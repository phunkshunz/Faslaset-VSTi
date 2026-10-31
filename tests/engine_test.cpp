// Test av DSP-motorn utan JUCE. Byggs och körs av GitHub Actions:
//   g++ -std=c++17 -O2 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test
//
// Bevisar det som FASLÅSET lovar:
//   - kick och bas låter aldrig i samma sample
//   - basen är exakt 0 på samplet före varje kick
//   - basen startar på nollgenomgången (första samplet nära 0, går uppåt)
//   - inga NaN, inga toppar över 1, även med skräp-indata och hopp i tidslinjen

#include "Engine.h"

#include <cstdio>
#include <limits>
#include <random>
#include <vector>

using namespace fl;

static int fails = 0;
#define CHECK(x) do { if (! (x)) { std::printf ("FEL: %s (rad %d)\n", #x, __LINE__); ++fails; } } while (0)

struct Result
{
    long overlaps = 0, bassBeforeKick = 0, badStarts = 0, nonFinite = 0, overOne = 0;
    long kicks = 0, bassStarts = 0;
    double peak = 0.0;
};

// Kör motorn i block av varierande storlek, som en riktig DAW.
static Result run (Params prm, double sr, double bpm, double seconds, unsigned seed = 1, bool jumps = false)
{
    Engine e;
    e.prepare (sr);
    e.setParams (prm);

    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> blockDist (1, 1024);

    const long total = (long) (seconds * sr);
    std::vector<float> out (total), kick (total), bass (total);
    double ppq = 0.0;
    const double ppqPerSample = bpm / 60.0 / sr;

    long pos = 0;
    while (pos < total)
    {
        int n = std::min ((long) blockDist (rng), total - pos);
        Transport t { true, bpm, ppq };
        e.process (out.data() + pos, n, t, kick.data() + pos, bass.data() + pos);
        pos += n;
        ppq += n * ppqPerSample;
        if (jumps && (rng() % 200) == 0) ppq = std::floor (ppq / 4.0) * 4.0;   // loop tillbaka till takten
    }

    Result r;
    r.overlaps = e.overlapCount.load();
    for (long i = 0; i < total; ++i)
    {
        if (! std::isfinite (out[i])) ++r.nonFinite;
        if (std::fabs (out[i]) > 1.0f) ++r.overOne;
        r.peak = std::max (r.peak, (double) std::fabs (out[i]));
        if (kick[i] != 0.0f && bass[i] != 0.0f) ++r.overlaps;

        const bool kickStart = kick[i] != 0.0f && (i == 0 || kick[i - 1] == 0.0f);
        const bool bassStart = bass[i] != 0.0f && (i == 0 || bass[i - 1] == 0.0f);
        if (kickStart)
        {
            ++r.kicks;
            if (i > 0 && bass[i - 1] != 0.0f) ++r.bassBeforeKick;
        }
        if (bassStart)
        {
            ++r.bassStarts;
            // fas 0: första samplet litet och positivt (sinus/såg på väg upp från noll)
            const float maxFirst = 0.2f;
            if (! (bass[i] > 0.0f && bass[i] < maxFirst)) ++r.badStarts;
        }
    }
    return r;
}

static void expectClean (const char* name, const Result& r, bool expectBass = true)
{
    std::printf ("%-28s kickar %5ld  basstarter %6ld  topp %.3f  överlapp %ld\n",
                 name, r.kicks, r.bassStarts, r.peak, r.overlaps);
    CHECK (r.overlaps == 0);
    CHECK (r.bassBeforeKick == 0);
    CHECK (r.badStarts == 0);
    CHECK (r.nonFinite == 0);
    CHECK (r.overOne == 0);
    CHECK (r.kicks > 0);
    if (expectBass) CHECK (r.bassStarts > 0);
}

int main()
{
    // ---- psy: K-B-B-B, ska ge tre basslag per slag
    {
        Params p;
        p.mode = Mode::Spikrakt;
        auto r = run (p, 48000.0, 145.0, 30.0);
        expectClean ("spikrakt 145 / 48k", r);
        CHECK (r.bassStarts >= r.kicks * 3 - 3);
    }
    // ---- olika samplingsfrekvenser och tempon
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double bpm : { 128.0, 138.0, 150.0, 175.0 })
        {
            Params p;
            auto r = run (p, sr, bpm, 8.0, (unsigned) (sr + bpm));
            char name[64];
            std::snprintf (name, sizeof name, "spikrakt %.0f / %.0fk", bpm, sr / 1000.0);
            expectClean (name, r);
        }
    // ---- techno: släp, en bas per slag
    {
        Params p;
        p.mode = Mode::Slap;
        p.kickLengthMs = 140.0f;
        p.bassForm = 0.0f;
        p.bassDecayMs = 400.0f;
        auto r = run (p, 48000.0, 130.0, 30.0, 7);
        expectClean (u8"släp 130 / 48k", r);
        CHECK (r.bassStarts >= r.kicks - 2);
        CHECK (r.bassStarts <= r.kicks + 1);
    }
    // ---- lång kick i spikrakt: basen väntar in nollgenomgången
    {
        Params p;
        p.kickLengthMs = 180.0f;
        auto r = run (p, 44100.0, 150.0, 10.0, 3);
        expectClean ("spikrakt lång kick", r);
    }
    // ---- max mättning och resonans
    {
        Params p;
        p.kickDrive = 8.0f;
        p.bassDrive = 8.0f;
        p.bassReso = 0.9f;
        p.bassEnv = 1.0f;
        p.bassForm = 1.0f;
        p.outGain = 1.0f;
        auto r = run (p, 48000.0, 145.0, 10.0, 11);
        expectClean ("max drive + reso", r);
    }
    // ---- hopp i tidslinjen (loopar)
    {
        Params p;
        auto r = run (p, 48000.0, 145.0, 20.0, 5, true);
        expectClean ("loopar / hopp", r);
    }
    // ---- slumpade parametrar, 300 rundor
    {
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        long allOverlaps = 0, allBad = 0;
        for (int k = 0; k < 300; ++k)
        {
            Params p;
            p.mode = u (rng) < 0.5f ? Mode::Spikrakt : Mode::Slap;
            p.kickStartHz = 60.0f + u (rng) * 400.0f;
            p.kickEndHz = 30.0f + u (rng) * 60.0f;
            p.kickPitchMs = 2.0f + u (rng) * 60.0f;
            p.kickLengthMs = 20.0f + u (rng) * 300.0f;
            p.kickDrive = 1.0f + u (rng) * 7.0f;
            p.bassNote = 21.0f + u (rng) * 30.0f;
            p.bassForm = u (rng);
            p.bassCutoff = 30.0f + u (rng) * 3000.0f;
            p.bassReso = u (rng) * 0.9f;
            p.bassEnv = u (rng);
            p.bassEnvMs = 5.0f + u (rng) * 300.0f;
            p.bassDecayMs = 10.0f + u (rng) * 1000.0f;
            p.bassDrive = 1.0f + u (rng) * 7.0f;
            p.guillotineMs = 0.2f + u (rng) * 4.0f;
            const double bpm = 90.0 + u (rng) * 100.0;
            auto r = run (p, u (rng) < 0.5f ? 44100.0 : 48000.0, bpm, 2.0, (unsigned) k);
            allOverlaps += r.overlaps + r.bassBeforeKick;
            allBad += r.badStarts + r.nonFinite + r.overOne;
        }
        std::printf ("%-28s överlapp %ld  fel %ld\n", "300 slumpade inställningar", allOverlaps, allBad);
        CHECK (allOverlaps == 0);
        CHECK (allBad == 0);
    }
    // ---- skräp-indata: NaN, Inf, negativa värden. Får inte ge ljud som spränger något.
    {
        Engine e;
        e.prepare (std::numeric_limits<double>::quiet_NaN());
        Params p;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        p.kickStartHz = nan; p.kickEndHz = inf; p.kickLengthMs = -5.0f; p.kickDrive = nan;
        p.bassNote = inf; p.bassCutoff = nan; p.bassReso = 99.0f; p.bassDecayMs = -inf; p.outGain = 50.0f;
        e.setParams (p);
        std::vector<float> out (4096);
        bool okAll = true;
        double ppq = 0.0;
        for (int b = 0; b < 200; ++b)
        {
            Transport t { true, 145.0, ppq };
            if (b == 50) t.bpm = std::numeric_limits<double>::quiet_NaN();
            if (b == 60) t.ppqAtStart = inf;
            if (b == 70) t.bpm = 0.0;
            e.process (out.data(), 4096, t);
            for (float v : out) okAll = okAll && std::isfinite (v) && std::fabs (v) <= 1.0f;
            ppq += 4096.0 * 145.0 / 60.0 / 48000.0;
        }
        std::printf ("%-28s %s\n", u8"skräp-indata", okAll ? "ok" : "FEL");
        CHECK (okAll);
        CHECK (e.overlapCount.load() == 0);
    }
    // ---- stoppad transport = tystnad
    {
        Engine e;
        e.prepare (48000.0);
        std::vector<float> out (512, 1.0f);
        Transport t { false, 145.0, 0.0 };
        e.process (out.data(), 512, t);
        bool silent = true;
        for (float v : out) silent = silent && v == 0.0f;
        CHECK (silent);
    }

    if (fails == 0) std::printf ("\nALLA TESTER GICK IGENOM. Noll fasfel.\n");
    else std::printf ("\n%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
