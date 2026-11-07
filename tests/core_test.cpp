// LÄCKAGET motor test: g++ -std=c++17 -O2 -Isrc tests/core_test.cpp -o core_test && ./core_test
#include "Lackaget.h"
#include "TestLoop.h"
#include <chrono>
#include <cstdio>
#include <functional>

using namespace lackaget;
using testloop::Stereo;

static int failures = 0;
#define CHECK(c, msg) do { if (! (c)) { std::printf ("  FAIL: %s\n", msg); ++failures; } } while (0)

static constexpr double SR = 48000.0;

// runs a whole buffer through the machine with the host transport playing
static Stereo run (Lackaget& m, const Stereo& in, double bpm, std::function<void (size_t)> perBlock = {})
{
    Stereo out = in;
    const int bs = 256;
    const double ppqPerSample = bpm / 60.0 / SR;
    for (size_t p = 0; p < in.size(); p += bs)
    {
        const int n = (int) std::min<size_t> (bs, in.size() - p);
        if (perBlock) perBlock (p);
        m.setTransport (bpm, (double) p * ppqPerSample, true);
        m.process (out.L.data() + p, out.R.data() + p, n);
    }
    return out;
}

static double rms (const std::vector<float>& x, size_t a = 0, size_t b = 0)
{
    if (b == 0 || b > x.size()) b = x.size();
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / std::max<size_t> (1, b - a));
}
static double peak (const std::vector<float>& x) { double p = 0; for (float v : x) p = std::max (p, (double) std::fabs (v)); return p; }
static bool finite (const Stereo& s) { for (size_t i = 0; i < s.size(); ++i) if (! std::isfinite (s.L[i]) || ! std::isfinite (s.R[i])) return false; return true; }
static double db (double x) { return 20.0 * std::log10 (std::max (x, 1.0e-12)); }

static Settings transparent()
{
    Settings s; s.hunting = 0; s.behorighet = 0; s.last = 0; s.ventil = 0; s.forangare = 0; s.ton = 1.f;
    s.kompressor = 0; s.siktglas = 0; s.oljeretur = 0; s.spridning = 0; return s;
}

int main()
{
    const Stereo loop = testloop::techno (SR, 128.0, 8.0);
    std::printf ("LÄCKAGET motor test, %d köldmedier\n", kNumMedier);

    // 1. Tyst in = tyst ut, även för lärlingen på max (bruset får aldrig läcka in i ljudet)
    {
        Stereo sil; sil.L.assign ((size_t) (SR * 4), 0.f); sil.R = sil.L;
        double worst = 0;
        for (int med = 0; med < kNumMedier; ++med)
        {
            Lackaget m; m.prepare (SR, 256);
            Settings s; s.medium = med; s.behorighet = 1; s.hunting = 1; s.siktglas = 1; s.kompressor = 1; s.oljeretur = 1; s.ventil = 1; s.forangare = 1; s.resonans = 1;
            m.setSettings (s);
            auto o = run (m, sil, 128.0);
            worst = std::max ({ worst, peak (o.L), peak (o.R) });
        }
        std::printf ("TYST     silence in, all media at max: peak out %.1f dB\n", db (worst));
        CHECK (worst < 1.0e-6, "the machine makes noise on its own");
    }

    // 2. Allt på noll = helt genomskinlig (bara den fasta latensen)
    {
        Lackaget m; m.prepare (SR, 256); m.setSettings (transparent());
        auto o = run (m, loop, 128.0);
        const int lat = m.latencySamples();
        double err = 0, ref = 0;
        for (size_t i = (size_t) lat; i < loop.size(); ++i)
        { const double d = o.L[i] - loop.L[i - (size_t) lat]; err += d * d; ref += (double) loop.L[i] * loop.L[i]; }
        std::printf ("NOLL     everything at zero: null %.1f dB, latency %d samples (%.1f ms)\n", 10 * std::log10 (err / ref + 1e-30), lat, lat * 1000.0 / SR);
        CHECK (err / ref < 1.0e-10, "not transparent with everything at zero");
    }

    // 3. Standardläget: samma nivå (+-1.5 dB), ingen överstyrning
    {
        Lackaget m; m.prepare (SR, 256); m.setSettings (Settings{});
        auto o = run (m, loop, 128.0);
        const double d = db (rms (o.L) / rms (loop.L));
        std::printf ("STANDARD default: level change %+.2f dB, peak %.2f\n", d, peak (o.L));
        CHECK (std::fabs (d) < 1.5, "default changes the level too much");
        CHECK (finite (o), "not finite");
    }

    // 4. Pendlingen är låst till takten: med kat 1-tekniker upprepar sig trycket exakt var 8:e steg
    {
        double corr[2] {}; double period[2] {};
        const double bpms[2] = { 120.0, 140.0 };
        for (int t = 0; t < 2; ++t)
        {
            Lackaget m; m.prepare (SR, 256);
            Settings s; s.behorighet = 0; s.hunting = 0.6f; s.last = 0; s.medium = 0; s.borvarde = (int) Rate::Attondel;
            m.setSettings (s);
            Stereo sil; sil.L.assign ((size_t) (SR * 12), 0.f); sil.R = sil.L;
            std::vector<float> y;
            run (m, sil, bpms[t], [&] (size_t) { y.push_back (m.tryckL()); });
            const double blocksPerBeat = 60.0 / bpms[t] * SR / 256.0;
            const size_t per = (size_t) std::lround (blocksPerBeat * 4.0);   // 8 eighths = 4 beats
            const size_t a = y.size() / 3;
            double sxy = 0, sxx = 0, syy = 0;
            for (size_t i = a; i + per < y.size(); ++i) { sxy += y[i] * y[i + per]; sxx += y[i] * y[i]; syy += y[i + per] * y[i + per]; }
            corr[t] = sxy / std::sqrt (sxx * syy + 1e-30);
            period[t] = per;
        }
        std::printf ("TAKT     pressure repeats every 4 beats: corr %.3f @120, %.3f @140\n", corr[0], corr[1]);
        CHECK (corr[0] > 0.95 && corr[1] > 0.95, "the hunting is not locked to the tempo");
        (void) period;
    }

    // 5. HUNTING ger mer pendling (överslängar)
    {
        double over[3] {};
        const float h[3] = { 0.f, 0.5f, 1.f };
        for (int k = 0; k < 3; ++k)
        {
            Lackaget m; m.prepare (SR, 256);
            Settings s; s.behorighet = 0; s.hunting = h[k]; s.last = 0; s.medium = 0; m.setSettings (s);
            Stereo sil; sil.L.assign ((size_t) (SR * 8), 0.f); sil.R = sil.L;
            double tv = 0, prev = 0;   // total movement of the pressure = how much it hunts
            run (m, sil, 128.0, [&] (size_t p) { const double y = m.tryckL(); if (p > SR * 2) tv += std::fabs (y - prev); prev = y; });
            over[k] = tv;
        }
        std::printf ("HUNTING  pressure movement: hunting 0 -> %.1f, 0.5 -> %.1f, 1 -> %.1f\n", over[0], over[1], over[2]);
        CHECK (over[1] > over[0] * 1.05 && over[2] > over[1] * 1.05, "HUNTING does not increase the overshoot");
    }

    // 6. Alla köldmedier, allt på max, i 8 s: ändligt och under taket
    {
        double worstPk = 0; bool ok = true;
        for (int med = 0; med < kNumMedier; ++med)
            for (int r = 0; r < kNumRates; ++r)
            {
                if (r != med % kNumRates) continue;
                Lackaget m; m.prepare (SR, 256);
                Settings s; s.medium = med; s.borvarde = r; s.behorighet = 1; s.hunting = 1; s.last = 1; s.siktglas = 1; s.kompressor = 1;
                s.oljeretur = 1; s.ventil = 1; s.forangare = 1; s.resonans = 1; s.ton = 0.5f; s.ut = 12.f; s.pressostat = -1.f;
                m.setSettings (s);
                auto o = run (m, loop, 128.0);
                ok = ok && finite (o);
                worstPk = std::max ({ worstPk, peak (o.L), peak (o.R) });
            }
        std::printf ("MAX      all media at max + 12 dB: finite %s, peak %.2f dB (ceiling -1.0)\n", ok ? "yes" : "NO", db (worstPk));
        CHECK (ok, "NaN/inf at extreme settings");
        CHECK (worstPk <= dbToGain (-1.f) + 1e-4, "PRESSOSTAT let a peak through");
    }

    // 7. NÖDSTOPP: tonhöjden dyker, ljudet dör, och efter ÅTERSTART är maskinen tillbaka i realtid
    {
        Lackaget m; m.prepare (SR, 256);
        Settings s = transparent(); s.stopptid = 0.5f; s.starttid = 0.6f; m.setSettings (s);
        Stereo sine; sine.L.resize ((size_t) (SR * 6)); for (size_t i = 0; i < sine.size(); ++i) sine.L[i] = 0.3f * (float) std::sin (2 * M_PI * 440.0 * i / SR);
        sine.R = sine.L;
        bool stopped = false, restarted = false;
        auto o = run (m, sine, 128.0, [&] (size_t p) {
            if (! stopped && p >= (size_t) (SR * 1.0)) { m.nodstopp (true); stopped = true; }
            if (! restarted && p >= (size_t) (SR * 2.5)) { m.nodstopp (false); restarted = true; }
        });
        auto zc = [&] (size_t a, size_t b) { int c = 0; for (size_t i = a + 1; i < b; ++i) if ((o.L[i - 1] < 0) != (o.L[i] < 0)) ++c; return c * SR / (2.0 * (b - a)); };
        const double fBefore = zc ((size_t) (SR * 0.5), (size_t) (SR * 0.9));
        const double fFalling = zc ((size_t) (SR * 1.25), (size_t) (SR * 1.35));
        const double stilla = rms (o.L, (size_t) (SR * 1.8), (size_t) (SR * 2.4));
        const int lat = m.latencySamples();
        double err = 0, ref = 0;
        for (size_t i = (size_t) (SR * 4.0); i < sine.size(); ++i) { const double d = o.L[i] - sine.L[i - (size_t) lat]; err += d * d; ref += (double) sine.L[i] * sine.L[i]; }
        std::printf ("NÖDSTOPP pitch %.0f Hz -> %.0f Hz while stopping, stopped %.1f dB, back in real time: null %.1f dB\n",
                     fBefore, fFalling, db (stilla / rms (sine.L)), 10 * std::log10 (err / ref + 1e-30));
        CHECK (fFalling < fBefore * 0.8, "the pitch does not fall when stopping");
        CHECK (stilla < 1e-3, "not silent when stopped");
        CHECK (err / ref < 1e-6, "not back in real time after the restart");
        CHECK (finite (o), "not finite");
    }

    // 8. OLJERETUR: offbeat-16-delar kommer sent, slagen på taktslagen ligger kvar
    {
        Lackaget m; m.prepare (SR, 256);
        Settings s = transparent(); s.oljeretur = 1.f; m.setSettings (s);
        const double bpm = 120.0, s16 = 60.0 / bpm * SR / 4.0;
        Stereo clicks; clicks.L.assign ((size_t) (SR * 6), 0.f);
        for (long k = 0; k * s16 < clicks.size() - 100; ++k) clicks.L[(size_t) (k * s16)] = 0.8f;
        clicks.R = clicks.L;
        auto o = run (m, clicks, bpm);
        const int lat = m.latencySamples();
        double onSum = 0, offSum = 0; int onN = 0, offN = 0;
        for (long k = 16; k < 80; ++k)
        {
            const size_t c = (size_t) (k * s16) + (size_t) lat;
            size_t best = c; float bv = 0;
            for (size_t i = c; i < c + (size_t) (0.04 * SR); ++i) if (std::fabs (o.L[i]) > bv) { bv = std::fabs (o.L[i]); best = i; }
            const double ms = (double) (best - c) * 1000.0 / SR;
            if (k % 2) { offSum += ms; ++offN; } else { onSum += ms; ++onN; }
        }
        std::printf ("OLJERETUR on-beat 16ths %.1f ms late, offbeat 16ths %.1f ms late (max %.0f)\n", onSum / onN, offSum / offN, (double) Lackaget::kDragMs);
        CHECK (onSum / onN < 1.0, "on-beat hits moved");
        CHECK (offSum / offN > 0.8 * Lackaget::kDragMs, "offbeat hits did not drag");
    }

    // 9. Köldmedierna låter olika
    {
        std::vector<Stereo> outs;
        for (int med = 0; med < kNumMedier; ++med)
        {
            Lackaget m; m.prepare (SR, 256);
            Settings s; s.medium = med; s.hunting = 0.6f; s.behorighet = 0.4f; s.ventil = 0.5f; s.forangare = 0.6f; s.ton = 0.6f; s.kompressor = 0.5f; s.siktglas = 0.3f;
            m.setSettings (s);
            auto o = run (m, loop, 128.0);
            outs.push_back (o);
        }
        int distinct = 0; double closest = 0;
        for (size_t a = 0; a < outs.size(); ++a)
        {
            double nearest = 1e9;
            for (size_t b = 0; b < outs.size(); ++b)
            {
                if (a == b) continue;
                double e = 0, r = 0;
                for (size_t i = 0; i < loop.size(); ++i) { const double d = outs[a].L[i] - outs[b].L[i]; e += d * d; r += (double) outs[a].L[i] * outs[a].L[i]; }
                nearest = std::min (nearest, 10 * std::log10 (e / r + 1e-30));
            }
            closest = std::max (closest == 0 ? -999 : closest, nearest);
            distinct += nearest > -20.0;
        }
        std::printf ("MEDIER   %d of %d media give a different result\n", distinct, kNumMedier);
        CHECK (distinct == kNumMedier, "some media sound identical");
    }

    // 10. CPU: hur mycket av en kärna
    {
        Lackaget m; m.prepare (SR, 256);
        Settings s; s.siktglas = 0.5f; s.oljeretur = 0.5f; s.kompressor = 0.6f; m.setSettings (s);
        Stereo big = testloop::techno (SR, 128.0, 30.0);
        const auto t0 = std::chrono::steady_clock::now();
        run (m, big, 128.0);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("CPU      %.2f %% of one core in real time (this machine)\n", 100.0 * sec / 30.0);
        CHECK (sec / 30.0 < 0.03, "too heavy");
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
