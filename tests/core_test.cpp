// MOTSTÅND — engine tests. Run:  g++ -std=c++17 -O2 -Isrc tests/core_test.cpp -o core_test && ./core_test
#include "MotstandCore.h"
#include "Voices.h"
#include "Vinkelslip.h"
#include <cstdio>
#include <vector>

using namespace motstand;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static const char* names[kNumInstruments] = { "KICK","SNARE","CLAP","RIM","TOM L","TOM M","TOM H","CH","OH","RIDE" };

int main() {
    const double sr = 48000; const int bs = 256;

    // ---- DieZeit: jitter stays inside ±1.5 ms of the drifting grid ----
    {
        DieZeit z; z.prepare(sr, 42); z.driftAmount = 0.0;    // drift off: test jitter alone
        StepEvent ev[16]; double start = 0; std::vector<double> t;
        for (int b = 0; b < sr * 30 / bs; ++b) {
            int n = z.process(bs, ev, 16);
            for (int i = 0; i < n; ++i) t.push_back(start + ev[i].sampleOffset);
            start += bs;
        }
        const double len = 60.0 / 133.0 / 4.0 * sr; double worst = 0, sumAbs = 0;
        for (size_t i = 0; i < t.size(); ++i) {
            const double dev = std::fabs(t[i] - i * len) / sr * 1000.0;
            worst = std::max(worst, dev); sumAbs += dev;
        }
        std::printf("CLOCK   jitter mean %.2f ms, worst %.2f ms over %zu steps\n", sumAbs / t.size(), worst, t.size());
        CHECK(worst <= 1.5 + 0.03, "jitter %.2f ms exceeds 1.5 ms", worst);   // +1 sample rounding
        CHECK(sumAbs / t.size() > 0.2, "jitter too small, clock is too tight");
    }
    // ---- DieZeit: drift wanders, nudge pulls it back ----
    {
        DieZeit z; z.prepare(sr, 7); StepEvent ev[16];
        for (int b = 0; b < sr * 120 / bs; ++b) z.process(bs, ev, 16);
        const double before = z.slipMs();
        z.nudge = before > 0 ? 1.0 : -1.0;
        int blocks = 0;
        while (std::fabs(z.slipMs()) > 1.0 && blocks < 100000 && ((z.slipMs() > 0) == (before > 0))) { z.process(bs, ev, 16); ++blocks; }
        std::printf("DRIFT   slip after 2 min %.1f ms, nudged back to %.1f ms in %.2f s\n", before, z.slipMs(), blocks * bs / sr);
        CHECK(std::fabs(before) > 0.5, "clock did not drift at all");
        CHECK(std::fabs(z.slipMs()) < 2.0, "nudge failed to recover");
    }
    // ---- DrunkRider: stays in range and keeps moving ----
    {
        DrunkRider r; r.prepare(sr, 3); float prev = 0, mn = 1, mx = -1; int same = 0;
        for (int i = 0; i < 5000; ++i) {
            r.advance(bs); float v = r.onTrigger();
            if (v == prev) { ++same; }
            prev = v; mn = std::min(mn, v); mx = std::max(mx, v);
        }
        std::printf("DRUNK   range %.2f .. %.2f\n", mn, mx);
        CHECK(mn >= -1.f && mx <= 1.f, "rider out of range");
        CHECK(same < 10, "rider got stuck");
    }
    // ---- PolyTrack ----
    {
        std::array<PolyTrack, 3> tr; tr[0].length = 16; tr[1].length = 7; tr[2].length = 11;
        CHECK(fullCycleSteps(tr) == 1232, "lcm wrong");
        std::printf("POLY    16/7/11 meets on the one after %lld bars\n", (long long)fullCycleSteps(tr) / 16);
    }
    // ---- DetroitSmog: clean up to 3 hits, chokes above ----
    {
        DetroitSmog s; s.prepare(sr);
        for (int hits : { 3, 4 }) {
            s.reset(); std::vector<float> L(bs, 0.f), R(bs, 0.f);
            for (int h = 0; h < hits; ++h) s.addTransient(10 + h * 4, 1.f);
            s.process(L.data(), R.data(), bs);
            std::printf("SMOG    %d hits in 1 ms -> collapse %.2f\n", hits, s.currentCollapse());
            if (hits == 3) CHECK(s.currentCollapse() < 0.01f, "smog fired on 3 hits");
            else           CHECK(s.currentCollapse() > 0.5f, "smog did not fire on 4 hits");
        }
    }
    // ---- Voices: every voice sounds, stays sane, and dies away ----
    {
        Kit kit; kit.prepare(sr);
        for (int inst = 0; inst < kNumInstruments; ++inst) {
            kit.prepare(sr);
            kit.trigger(inst, 1.f, 0.f, 0.f);
            float out[kNumInstruments]; float peak = 0, tailPeak = 0; bool bad = false;
            const int total = (int)(sr * (inst >= CH ? 12.0 : 4.0));   // cymbals ring longer
            for (int i = 0; i < total; ++i) {
                kit.process(out);
                const float x = out[inst];
                if (!std::isfinite(x)) bad = true;
                peak = std::max(peak, std::fabs(x));
                if (i > total - (int)sr / 10) tailPeak = std::max(tailPeak, std::fabs(x));
            }
            std::printf("VOICE   %-6s peak %.2f\n", names[inst], peak);
            CHECK(!bad, "%s produced NaN/inf", names[inst]);
            CHECK(peak > 0.05f, "%s is silent", names[inst]);
            CHECK(peak < 1.5f, "%s is too hot (%.2f)", names[inst], peak);
            CHECK(tailPeak < 0.001f, "%s never dies away", names[inst]);
        }
        // CH chokes OH
        kit.prepare(sr); kit.trigger(OH, 1.f, 0, 0);
        float out[kNumInstruments];
        for (int i = 0; i < 2000; ++i) kit.process(out);
        kit.trigger(CH, 1.f, 0, 0);
        float ohAfter = 0;
        for (int i = 0; i < 4800; ++i) { kit.process(out); if (i > 2400) ohAfter = std::max(ohAfter, std::fabs(out[OH])); }
        CHECK(ohAfter < 0.01f, "closed hat did not choke open hat (%.3f)", ohAfter);
        // drunk mods actually change the sound
        auto energy = [&](float decayMod) {
            kit.prepare(sr); kit.trigger(RIDE, 1.f, 0.f, decayMod); double e = 0;
            for (int i = 0; i < (int)sr; ++i) { kit.process(out); e += out[RIDE] * out[RIDE]; }
            return e;
        };
        const double e0 = energy(-1.f), e1 = energy(1.f);
        std::printf("RIDER   ride energy at decayMod -1: %.1f, +1: %.1f\n", e0, e1);
        CHECK(e1 > e0 * 1.03, "decayMod has no audible effect");
    }

    // ---- VINKELSLIP: bypass is a clean fixed delay; every mode stays sane ----
    {
        Vinkelslip v; v.prepare(sr); v.drive = 0.f;
        bool exact = true;
        std::vector<float> in(2000); for (int i = 0; i < 2000; ++i) in[(size_t)i] = std::sin(i * 0.05f) * 0.5f;
        for (int i = 0; i < 2000; ++i) {
            const float y = v.process(in[(size_t)i]);
            const float ref = i >= Vinkelslip::kLatency ? in[(size_t)(i - Vinkelslip::kLatency)] : 0.f;
            if (std::fabs(y - ref) > 1e-6f) exact = false;
        }
        CHECK(exact, "bypassed VINKELSLIP is not a clean %d-sample delay", Vinkelslip::kLatency);
        for (int m = 0; m < kNumSlipModes; ++m) {
            Vinkelslip w; w.prepare(sr); w.mode = m; w.drive = 1.f; w.onTrigger(1.f);
            float peak = 0; bool ok = true;
            for (int i = 0; i < 48000; ++i) {
                const float y = w.process(std::sin(i * 0.02f) * 0.8f);
                if (!std::isfinite(y)) ok = false;
                peak = std::max(peak, std::fabs(y));
            }
            std::printf("SLIP    mode %d full drive peak %.2f\n", m, peak);
            CHECK(ok && peak < 1.4f && peak > 0.2f, "slip mode %d misbehaves (peak %.2f)", m, peak);
        }
    }

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
