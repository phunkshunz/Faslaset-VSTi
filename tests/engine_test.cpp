// Test av DSP-motorn och basgångs-generatorn utan JUCE. Körs av GitHub Actions:
//   g++ -std=c++17 -O2 -Isrc tests/engine_test.cpp -o engine_test && ./engine_test
//
// Bevisar det som FASLÅSET lovar:
//   - kick och bas låter aldrig i samma sample
//   - basen är exakt 0 på samplet före varje kick
//   - varje basnot startar på nollgenomgången och tonar ut mjukt till 0 (inga klick)
//   - kicken stängs på en nollgenomgång
//   - en DAW som hackar lite i positionen ger inga dubbelslag
//   - inga NaN, inga toppar över 1, även med skräp-indata och hopp i tidslinjen

#include "Engine.h"
#include "Bassline.h"

#include <cstdio>
#include <limits>
#include <random>
#include <vector>

using namespace fl;

static int fails = 0;
#define CHECK(x) do { if (! (x)) { std::printf ("FEL: %s (rad %d)\n", #x, __LINE__); ++fails; } } while (0)

// Konstgjord kick som liknar ett riktigt sample: pitch-svep, avklingning, lite DC i svansen.
static std::vector<float> makeKick (double sr, double lenSec, double startHz, double endHz, double dc)
{
    std::vector<float> v ((size_t) (lenSec * sr));
    double ph = 0.0;
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = (double) i / sr;
        const double hz = endHz + (startHz - endHz) * std::exp (-t / 0.02);
        ph += hz / sr;
        const double amp = std::exp (-t / 0.08);
        double x = std::sin (kTwoPi * ph) * amp + dc * amp;
        const double fadeStart = lenSec - 0.004;
        if (t > fadeStart) x *= std::max (0.0, (lenSec - t) / 0.004);
        v[i] = (float) x;
    }
    return v;
}

struct Result
{
    long overlaps = 0, bassBeforeKick = 0, badStarts = 0, badEnds = 0, kickBadEnds = 0;
    long nonFinite = 0, overOne = 0, kicks = 0, bassStarts = 0;
    double peak = 0.0, maxBassStep = 0.0;
};

static Result run (Params prm, const Pattern* pat, const KickSample& ks, double sr, double bpm, double seconds,
                   unsigned seed = 1, bool jumps = false, bool jitter = false)
{
    Engine e;
    e.prepare (sr);
    e.setParams (prm);
    e.setPattern (pat);
    e.setKick (ks);

    std::mt19937 rng (seed);
    std::uniform_int_distribution<int> blockDist (1, 1024);
    std::uniform_real_distribution<double> jit (-1.0, 1.0);

    const long total = (long) (seconds * sr);
    std::vector<float> out (total), kick (total), bass (total);
    double ppq = 0.0;
    const double ppqPerSample = bpm / 60.0 / sr;

    long pos = 0;
    while (pos < total)
    {
        const int n = (int) std::min ((long) blockDist (rng), total - pos);
        double hostPpq = ppq;
        if (jitter) hostPpq += jit (rng) * 1.5 * ppqPerSample;   // värden hackar ±1,5 sample
        Transport t { true, bpm, hostPpq };
        e.process (out.data() + pos, n, t, kick.data() + pos, bass.data() + pos);
        pos += n;
        ppq += n * ppqPerSample;
        if (jumps && (rng() % 200) == 0) ppq = std::floor (ppq / 4.0) * 4.0;
    }

    Result r;
    r.overlaps = e.overlapCount.load();
    for (long i = 0; i < total; ++i)
    {
        if (! std::isfinite (out[i])) ++r.nonFinite;
        if (std::fabs (out[i]) > 1.0f) ++r.overOne;
        r.peak = std::max (r.peak, (double) std::fabs (out[i]));
        if (kick[i] != 0.0f && bass[i] != 0.0f) ++r.overlaps;
        if (i > 0) r.maxBassStep = std::max (r.maxBassStep, (double) std::fabs (bass[i] - bass[i - 1]));

        const bool kickStart = kick[i] != 0.0f && (i == 0 || kick[i - 1] == 0.0f);
        const bool bassStart = bass[i] != 0.0f && (i == 0 || bass[i - 1] == 0.0f);
        const bool bassEnd = i > 0 && bass[i] == 0.0f && bass[i - 1] != 0.0f;
        const bool kickEnd = i > 0 && kick[i] == 0.0f && kick[i - 1] != 0.0f;
        if (kickStart)
        {
            ++r.kicks;
            if (i > 0 && bass[i - 1] != 0.0f) ++r.bassBeforeKick;
        }
        if (bassStart)
        {
            ++r.bassStarts;
            // startar på nollgenomgången: första samplet är högst ett steg från noll
            const float slope = i + 1 < total ? std::fabs (bass[i + 1] - bass[i]) : 1.0f;
            if (! (bass[i] > -0.002f && bass[i] <= slope * 1.5f + 0.01f)) ++r.badStarts;
        }
        if (bassEnd && std::fabs (bass[i - 1]) > 0.01f) ++r.badEnds;    // tonade ut till noll
        if (kickEnd && i > 1)   // stängdes på nollgenomgången: sista samplet högst ett steg från noll
        {
            const float slope = std::fabs (kick[i - 1] - kick[i - 2]);
            if (std::fabs (kick[i - 1]) > slope * 1.5f + 0.01f) ++r.kickBadEnds;
        }
    }
    return r;
}

static void expectClean (const char* name, const Result& r, bool expectBass = true)
{
    std::printf ("%-30s kickar %5ld  basnoter %6ld  topp %.3f  basens största steg %.3f  överlapp %ld\n",
                 name, r.kicks, r.bassStarts, r.peak, r.maxBassStep, r.overlaps);
    CHECK (r.overlaps == 0);
    CHECK (r.bassBeforeKick == 0);
    CHECK (r.badStarts == 0);
    CHECK (r.badEnds == 0);
    CHECK (r.kickBadEnds == 0);
    CHECK (r.nonFinite == 0);
    CHECK (r.overOne == 0);
    CHECK (r.kicks > 0);
    if (expectBass) CHECK (r.bassStarts > 0);
}

int main()
{
    const auto kick44 = makeKick (44100.0, 0.45, 220.0, 50.0, 0.0);
    const auto kickDc = makeKick (48000.0, 0.6, 180.0, 45.0, 0.08);   // svans med DC
    const KickSample K44 { kick44.data(), (int) kick44.size(), 44100.0 };
    const KickSample KDC { kickDc.data(), (int) kickDc.size(), 48000.0 };

    Pattern roll;
    BasslineGen::generate (roll, 1234, 4, 0, BassStyle::Rull, false);

    // ---- psy: K-B-B-B
    {
        Params p;
        auto r = run (p, &roll, K44, 48000.0, 145.0, 30.0);
        expectClean ("spikrakt 145 / 48k", r);
        CHECK (r.bassStarts >= r.kicks * 3 - 3);
    }
    // ---- samplingsfrekvenser och tempon
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (double bpm : { 128.0, 138.0, 145.0, 150.0, 175.0 })
        {
            Params p;
            auto r = run (p, &roll, K44, sr, bpm, 6.0, (unsigned) (sr + bpm));
            char name[64];
            std::snprintf (name, sizeof name, "spikrakt %.0f / %.0fk", bpm, sr / 1000.0);
            expectClean (name, r);
        }
    // ---- techno: släp
    {
        Pattern sl;
        BasslineGen::generate (sl, 77, 4, 1, BassStyle::Rull, true);
        Params p;
        p.mode = Mode::Slap;
        p.kickLengthMs = 140.0f;
        p.bassForm = 0.0f;
        p.bassDecayMs = 400.0f;
        auto r = run (p, &sl, KDC, 48000.0, 130.0, 30.0, 7);
        expectClean (u8"släp 130 / 48k (DC-kick)", r);
        CHECK (r.bassStarts >= (r.kicks * 3) / 4);
    }
    // ---- DAW som hackar i positionen: inga dubbelslag
    {
        Params p;
        auto r = run (p, &roll, K44, 48000.0, 145.0, 20.0, 9, false, true);
        expectClean ("hackande DAW-position", r);
        const long expectedKicks = (long) (20.0 * 145.0 / 60.0);
        CHECK (r.kicks >= expectedKicks - 1 && r.kicks <= expectedKicks + 1);
    }
    // ---- lång kick, max mättning, loopar, ingen kick alls
    {
        Params p;
        p.kickLengthMs = 400.0f;
        expectClean ("spikrakt lång kick", run (p, &roll, K44, 44100.0, 150.0, 10.0, 3));
    }
    {
        Params p;
        p.kickDrive = 8.0f; p.bassDrive = 8.0f; p.bassReso = 0.9f; p.bassEnv = 1.0f; p.bassForm = 1.0f; p.outGain = 1.0f;
        auto r = run (p, &roll, KDC, 48000.0, 145.0, 10.0, 11);
        expectClean ("max drive + reso", r);
    }
    expectClean ("loopar / hopp", run (Params(), &roll, K44, 48000.0, 145.0, 20.0, 5, true));
    {
        Params p;
        auto r = run (p, &roll, KickSample {}, 48000.0, 145.0, 4.0, 2);
        std::printf ("%-30s basnoter %ld  överlapp %ld\n", "utan kick-sample", r.bassStarts, r.overlaps);
        CHECK (r.overlaps == 0 && r.bassStarts > 0 && r.badEnds == 0);
    }

    // ---- alla stilar, skalor och längder, slumpade ljudinställningar
    {
        std::mt19937 rng (42);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        long errs = 0, overl = 0;
        for (int k = 0; k < 300; ++k)
        {
            Params p;
            p.mode = u (rng) < 0.5f ? Mode::Spikrakt : Mode::Slap;
            p.kickTune = -12.0f + u (rng) * 24.0f;
            p.kickLengthMs = 20.0f + u (rng) * 300.0f;
            p.kickDrive = 1.0f + u (rng) * 7.0f;
            p.rootNote = 21 + (int) (u (rng) * 27.0f);
            p.scale = (int) (u (rng) * kNumScales);
            p.beats = 1 + (int) (u (rng) * 8.0f);
            p.bassForm = u (rng);
            p.bassCutoff = 30.0f + u (rng) * 3000.0f;
            p.bassReso = u (rng) * 0.9f;
            p.bassEnv = u (rng);
            p.bassEnvMs = 5.0f + u (rng) * 300.0f;
            p.bassDecayMs = 10.0f + u (rng) * 1000.0f;
            p.bassGate = 0.2f + u (rng) * 0.8f;
            p.bassDrive = 1.0f + u (rng) * 7.0f;
            p.releaseMs = 1.0f + u (rng) * 11.0f;
            Pattern pat;
            BasslineGen::generate (pat, (uint32_t) rng(), p.beats, p.scale, (BassStyle) (k % kNumStyles), p.mode == Mode::Slap);
            const double bpm = 90.0 + u (rng) * 100.0;
            auto r = run (p, &pat, k % 2 ? K44 : KDC, u (rng) < 0.5f ? 44100.0 : 48000.0, bpm, 2.0, (unsigned) k, false, k % 3 == 0);
            overl += r.overlaps + r.bassBeforeKick;
            errs += r.badStarts + r.badEnds + r.kickBadEnds + r.nonFinite + r.overOne;
            if (r.badStarts + r.badEnds + r.kickBadEnds + r.nonFinite + r.overOne)
                std::printf ("  runda %d: start %ld slut %ld kickslut %ld nan %ld over1 %ld  (läge %d drive %.1f/%.1f reso %.2f tune %.1f rel %.1f)\n",
                             k, r.badStarts, r.badEnds, r.kickBadEnds, r.nonFinite, r.overOne, (int) p.mode, p.kickDrive, p.bassDrive, p.bassReso, p.kickTune, p.releaseMs);
        }
        std::printf ("%-30s överlapp %ld  fel %ld\n", "300 slumpade inställningar", overl, errs);
        CHECK (overl == 0);
        CHECK (errs == 0);
    }

    // ---- generatorn: regler för en vettig basgång
    {
        long bad = 0;
        for (uint32_t seed = 1; seed < 2000; ++seed)
            for (int st = 0; st < kNumStyles; ++st)
            {
                const int beats = 1 + (int) (seed % 8);
                const bool slap = seed % 3 == 0;
                Pattern pat;
                BasslineGen::generate (pat, seed, beats, (int) (seed % kNumScales), (BassStyle) st, slap);
                int notes = 0, root = 0;
                for (int b = 0; b < beats; ++b)
                    for (int s = 0; s < kSlots; ++s)
                    {
                        const Cell c = pat.get (b, s);
                        if (! c.on) continue;
                        ++notes;
                        if (c.degree == 0) ++root;
                        if (slap && s > 0) ++bad;                 // techno: en not per slag
                        if (c.degree < -7 || c.degree > 14) ++bad;
                    }
                if (notes == 0) ++bad;                            // aldrig helt tomt
                if (root * 2 < notes) ++bad;                      // grundtonen dominerar
                if (slap && ! pat.get (0, 0).on) ++bad;           // techno: första slaget spelar
                // upprepning: slag efter mönsterlängden kopierar början
                for (int b = beats; b < kMaxBeats; ++b)
                    for (int s = 0; s < kSlots; ++s)
                        if (pat.get (b, s).pack() != pat.get (b % beats, s).pack()) ++bad;
                // samma frö = samma mönster
                Pattern again;
                BasslineGen::generate (again, seed, beats, (int) (seed % kNumScales), (BassStyle) st, slap);
                for (int b = 0; b < kMaxBeats; ++b)
                    for (int s = 0; s < kSlots; ++s)
                        if (pat.get (b, s).pack() != again.get (b, s).pack()) ++bad;
            }
        std::printf ("%-30s regelbrott %ld\n", u8"basgångs-generatorn", bad);
        CHECK (bad == 0);
    }

    // ---- skräp-indata
    {
        Engine e;
        e.prepare (std::numeric_limits<double>::quiet_NaN());
        e.setPattern (&roll);
        std::vector<float> junk (1000, std::numeric_limits<float>::quiet_NaN());
        e.setKick ({ junk.data(), 0, 48000.0 });   // ogiltig längd: ska ignoreras
        e.setKick ({ kick44.data(), (int) kick44.size(), -5.0 });
        Params p;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float inf = std::numeric_limits<float>::infinity();
        p.kickTune = nan; p.kickLengthMs = -5.0f; p.kickDrive = nan; p.rootNote = 9999; p.scale = -4; p.beats = 0;
        p.bassCutoff = nan; p.bassReso = 99.0f; p.bassDecayMs = -inf; p.bassGate = nan; p.releaseMs = inf; p.outGain = 50.0f;
        e.setParams (p);
        e.setKick (K44);
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
        std::printf ("%-30s %s\n", u8"skräp-indata", okAll ? "ok" : "FEL");
        CHECK (okAll);
        CHECK (e.overlapCount.load() == 0);
    }
    // ---- stopp tonar ut, sen tystnad
    {
        Engine e;
        e.prepare (48000.0);
        e.setPattern (&roll);
        e.setKick (K44);
        std::vector<float> out (512);
        Transport t { true, 145.0, 0.0 };
        e.process (out.data(), 512, t);
        t.isPlaying = false;
        e.process (out.data(), 512, t);
        float maxStep = 0.0f;
        for (int i = 1; i < 512; ++i) maxStep = std::max (maxStep, std::fabs (out[i] - out[i - 1]));
        e.process (out.data(), 512, t);
        bool silent = true;
        for (float v : out) silent = silent && v == 0.0f;
        CHECK (silent);
        CHECK (maxStep < 0.2f);
    }

    if (fails == 0) std::printf ("\nALLA TESTER GICK IGENOM. Noll fasfel, inga klick.\n");
    else std::printf ("\n%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
