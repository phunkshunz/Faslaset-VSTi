// MOTSTÅND — effect measurements.  g++ -std=c++17 -O2 -Isrc tests/fx_test.cpp -o fx_test && ./fx_test
#include "Fx.h"
#include <cstdio>
#include <vector>

using namespace motstand;
static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

static float noise() { static uint32_t s = 22222; s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(int32_t)s / 2147483648.f; }

int main() {
    const double sr = 48000;

    // ---- VARVSHALL: measured T60 matches the knob; freeze holds and never explodes ----
    for (float t60 : { 1.0f, 2.5f, 6.0f }) {
        Varvshall h; h.prepare(sr); h.decay = t60; h.size = 0.6f; h.damping = 0.2f; h.predelayMs = 0.f;
        const int n = (int)(sr * (t60 * 1.6f + 0.5f));
        std::vector<double> e((size_t)n);
        for (int i = 0; i < n; ++i) { float l, r; h.process(i < 64 ? noise() : 0.f, 0.f, l, r); e[(size_t)i] = l * l + r * r; }
        // Schroeder backward integration, fit the -5..-25 dB slope
        std::vector<double> sch((size_t)n); double acc = 0;
        for (int i = n - 1; i >= 0; --i) { acc += e[(size_t)i]; sch[(size_t)i] = acc; }
        int i5 = -1, i25 = -1;
        for (int i = 0; i < n; ++i) {
            const double db = 10 * std::log10(sch[(size_t)i] / sch[0] + 1e-30);
            if (i5 < 0 && db < -5) i5 = i;
            if (i25 < 0 && db < -25) { i25 = i; break; }
        }
        const double measured = (i25 - i5) / sr * 3.0;   // 20 dB span * 3 = 60 dB
        std::printf("HALL    T60 knob %.1f s -> measured %.2f s\n", t60, measured);
        CHECK(i5 > 0 && i25 > 0 && std::fabs(measured - t60) < t60 * 0.3, "T60 off (%.2f vs %.1f)", measured, t60);
    }
    {
        Varvshall h; h.prepare(sr); h.decay = 3.f;
        double before = 0, late = 0; float peak = 0;
        for (int i = 0; i < (int)(sr * 0.5); ++i) { float l, r; h.process(noise() * 0.5f, noise() * 0.5f, l, r); }
        h.freeze = true;
        for (int i = 0; i < (int)(sr * 12); ++i) {
            float l, r; h.process(noise(), noise(), l, r);   // input is ignored while frozen
            peak = std::max(peak, std::fabs(l));
            if (i < (int)sr) before += l * l; else if (i > (int)(sr * 11)) late += l * l;
        }
        const double ratio = 10 * std::log10(late / before);
        std::printf("FRYS    12 s frozen: level change %.1f dB, peak %.2f\n", ratio, peak);
        CHECK(std::fabs(ratio) < 6.0 && peak < 4.f, "freeze unstable (%.1f dB, peak %.2f)", ratio, peak);
    }

    // ---- EKO: lands exactly on the beat division; runaway feedback stays bounded ----
    {
        Eko e; e.bpm = 120.0; e.division = 1; e.feedback = 0.f; e.wear = 0.f; e.pingPong = true; e.prepare(sr);
        int peakAt = -1; float best = 0;
        for (int i = 0; i < 20000; ++i) { float l, r; e.process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, l, r); if (std::fabs(l) > best) { best = std::fabs(l); peakAt = i; } }
        std::printf("EKO     1/8 at 120 BPM: echo at sample %d (expect 12000)\n", peakAt);
        CHECK(std::abs(peakAt - 12000) <= 3, "echo time wrong (%d)", peakAt);
        Eko f; f.bpm = 133; f.division = 3; f.feedback = 1.05f; f.wear = 1.f; f.prepare(sr);
        float pk = 0; bool finite = true;
        for (int i = 0; i < (int)(sr * 20); ++i) { float l, r; f.process(noise(), noise(), l, r); pk = std::max(pk, std::fabs(l)); if (!std::isfinite(l)) finite = false; }
        std::printf("EKO     max feedback + max wear for 20 s: peak %.2f\n", pk);
        CHECK(finite && pk < 3.f, "echo runaway (%.2f)", pk);
    }

    // ---- ISOLATOR: flat at unity, deep kills ----
    {
        Isolator iso; iso.prepare(sr);
        double ein = 0, eout = 0;
        for (int i = 0; i < (int)sr * 2; ++i) { float x = noise() * 0.3f, l = x, r = x; iso.process(l, r); if (i > 4800) { ein += x * x; eout += l * l; } }
        const double flat = 10 * std::log10(eout / ein);
        std::printf("ISO     unity: %.2f dB (noise in/out)\n", flat);
        CHECK(std::fabs(flat) < 0.25, "isolator not flat (%.2f dB)", flat);
        auto killTest = [&](float hz, int band) {
            Isolator k; k.prepare(sr);
            if (band == 0) k.low = 0.f; else if (band == 1) k.mid = 0.f; else k.high = 0.f;
            double a = 0, b = 0;
            for (int i = 0; i < (int)sr; ++i) { float x = std::sin(kTau * hz * i / (float)sr) * 0.5f, l = x, r = x; k.process(l, r); if (i > 9600) { a += x * x; b += l * l; } }
            return 10 * std::log10(b / a);
        };
        const double kl = killTest(60.f, 0), km = killTest(800.f, 1), kh = killTest(10000.f, 2);
        std::printf("ISO     kill LOW at 60 Hz %.1f dB, MID at 800 Hz %.1f dB, HIGH at 10 kHz %.1f dB\n", kl, km, kh);
        CHECK(kl < -30 && km < -20 && kh < -30, "kills not deep enough");
    }

    // ---- PRESS + PUMP ----
    {
        Press p; p.amount = 0.6f; p.prepare(sr);
        for (int i = 0; i < (int)sr; ++i) { float l = std::sin(kTau * 100.f * i / (float)sr) * 0.7f, r = l; p.process(l, r); }
        std::printf("PRESS   amount 0.6 on a -3 dBFS sine: %.1f dB reduction\n", p.gainReductionDb());
        CHECK(p.gainReductionDb() > 4.f && p.gainReductionDb() < 20.f, "compressor not compressing sensibly");
        Pump u; u.amount = 1.f; u.prepare(sr); u.trigger(1.f);
        float minG = 1.f, gAt300 = 0.f;
        for (int i = 0; i < (int)(sr * 0.5); ++i) { const float g = u.next(); minG = std::min(minG, g); if (i == (int)(sr * 0.3)) gAt300 = g; }
        std::printf("PUMP    full: dips to %.1f dB, back to %.1f dB after 300 ms\n", 20 * std::log10(minG), 20 * std::log10(gAt300));
        CHECK(minG < 0.3f && gAt300 > 0.6f, "pump shape wrong");
    }

    std::printf(failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
