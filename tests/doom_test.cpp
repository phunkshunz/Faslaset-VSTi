// MOTSTÅND — UNDERGÅNG measurements.  g++ -std=c++17 -O2 -Isrc tests/doom_test.cpp -o doom_test && ./doom_test
#include "Doom.h"
#include "Voices.h"
#include <cstdio>
#include <vector>
#include <complex>

using namespace motstand;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

// energy share below a frequency, via a plain DFT on a decimated copy (cheap enough for a test)
static double lowShare(const std::vector<float>& x, double sr, double cut) {
    double lo = 0, all = 0; const int N = 4096; const int hop = (int)x.size() / N;
    for (int k = 1; k < N / 2; ++k) {
        std::complex<double> acc = 0;
        for (int i = 0; i < N; ++i) acc += (double)x[(size_t)(i * hop)] * std::polar(1.0, -6.283185307 * k * i / N);
        const double e = std::norm(acc), f = k * (sr / hop) / N;
        all += e; if (f < cut) lo += e;
    }
    return lo / all;
}

int main() {
    const double sr = 48000;
    // ---- MULLER: lives in the lows, ducked by the kick, bounded ----
    {
        Kit kit; kit.prepare(sr);
        Muller m; m.prepare(sr); m.amount = 1.f; m.depth = 6.f; m.freq = 120.f; m.dirt = 0.6f; m.pump = 0.9f;
        const int n = (int)(sr * 8), beat = (int)(sr * 60.0 / 138.0);
        std::vector<float> out((size_t)n); float peak = 0; bool fin = true;
        double rightAfter = 0, midBeat = 0;
        for (int i = 0; i < n; ++i) {
            if (i % beat == 0) { kit.trigger(KICK, 1.f, 0, 0); m.onKick(1.f); }
            float o[kNumInstruments]; kit.process(o);
            const float y = m.process(o[KICK]);
            out[(size_t)i] = y; peak = std::max(peak, std::fabs(y)); if (!std::isfinite(y)) fin = false;
            const int ph = i % beat;
            if (i > (int)sr * 3) { if (ph < (int)(0.02 * sr)) rightAfter += y * y; else if (std::abs(ph - beat * 3 / 4) < (int)(0.02 * sr)) midBeat += y * y; }
        }
        const double ls = lowShare(out, sr, 300.0);
        std::printf("MULLER  peak %.2f, energy under 300 Hz %.0f %%, duck: %.1f dB quieter right after the kick\n",
                    peak, ls * 100, 10 * std::log10(midBeat / (rightAfter + 1e-12)));
        CHECK(fin && peak < 1.5f && peak > 0.05f, "rumble level wrong (%.2f)", peak);
        CHECK(ls > 0.85, "rumble not in the lows (%.0f %%)", ls * 100);
        CHECK(midBeat > rightAfter * 2.0, "kick does not duck the rumble");
    }
    // ---- MOTOR: fires at rpm/60*4, two banks, bounded ----
    {
        Motor mo; mo.prepare(sr); mo.gas = 1.f; mo.rpm = 1200.f; mo.rattle = 0.f; mo.tone = 140.f;
        const int n = (int)(sr * 4); std::vector<float> L((size_t)n); float peak = 0; double diff = 0; bool fin = true;
        for (int i = 0; i < n; ++i) { float l, r; mo.process(l, r); L[(size_t)i] = l; peak = std::max(peak, std::fabs(l)); diff += (l - r) * (l - r); if (!std::isfinite(l) || !std::isfinite(r)) fin = false; }
        // count firings: envelope peaks in the left channel (left bank fires 4 of 8 per cycle)
        const long fires = mo.fired[0] + mo.fired[1];
        const double perSec = fires / 4.0, expect = 1200.0 / 60.0 / 2.0 * 8.0;   // 8 firings per 2 revolutions
        std::printf("MOTOR   1200 rpm: %.0f firings/s (expect %.0f), banks L/R %ld/%ld, peak %.2f, L/R differ: %s\n",
                    perSec, expect, mo.fired[0], mo.fired[1], peak, diff > 1.0 ? "yes" : "no");
        CHECK(fin && peak < 1.2f && peak > 0.05f, "motor level wrong");
        CHECK(std::fabs(perSec - expect) < expect * 0.05, "firing rate wrong");
        CHECK(std::labs(mo.fired[0] - mo.fired[1]) <= 1, "banks do not fire equally");   // ±1: the run can stop mid-cycle
        CHECK(diff > 1.0, "banks are not split left/right");
        Motor wild; wild.prepare(sr); wild.gas = 1.f; wild.rpm = 7000.f; wild.rattle = 1.f;
        float pk = 0; for (int i = 0; i < (int)sr * 4; ++i) { float l, r; wild.process(l, r); pk = std::max(pk, std::fabs(l)); }
        CHECK(pk < 1.2f, "motor at 7000 rpm full rattle too hot (%.2f)", pk);
    }
    // ---- STORM + LARM ----
    {
        Storm st; st.prepare(sr); st.amount = 1.f; st.sweep = 1.f; st.color = 2000.f; st.alarm = 1.f;
        float pk = 0, spk = 0; bool fin = true; double sMin = 1e9, sMax = 0, acc = 0; int cnt = 0;
        for (int i = 0; i < (int)(sr * 20); ++i) {
            float l, r, s; st.process(l, r, s);
            pk = std::max(pk, std::max(std::fabs(l), std::fabs(r))); spk = std::max(spk, std::fabs(s));
            if (!std::isfinite(l) || !std::isfinite(s)) fin = false;
            acc += s * s; if (++cnt == 4800) { sMin = std::min(sMin, acc); sMax = std::max(sMax, acc); acc = 0; cnt = 0; }
        }
        std::printf("STORM   20 s: peak %.2f, siren peak %.2f, siren swell %.0f dB\n", pk, spk, 10 * std::log10(sMax / (sMin + 1e-12)));
        CHECK(fin && pk < 1.2f && spk < 0.6f && spk > 0.05f, "storm levels wrong");
        CHECK(sMax > sMin * 3.0, "siren does not rise and fall");
    }
    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
