// =====================================================================
//  MASKINHALLEN : MOTSTÅND  —  VINKELSLIP (per-channel grinder) + ELNÄT
//  Phunkshunz AUDIO
//
//  VINKELSLIP: six non-linear modes. The shapers run at 4x the sample
//  rate (two linear-phase halfband stages) AND use first-order
//  antiderivative anti-aliasing (ADAA). That pair is what keeps heavy
//  drive clean and expensive-sounding instead of fizzy.
//
//    DIOD        asymmetric diode pair, warm even harmonics
//    GERMANIUM   biased fuzz with a starving gate: spits and sputters
//    TEJP        soft algebraic saturation + HF loss
//    VIKNING     sine wavefolder, metallic, goes wild with drive
//    LIKRIKTARE  full-wave rectifier blend: octave up, razor sharp
//    ROM         bit + sample-rate crush at base rate (aliasing is the point)
//
//  MATNING: how much the hit's velocity feeds the drive. Hard hits
//  grind more than soft ones, so the pattern's dynamics become dirt.
//
//  Every channel has the SAME fixed latency (kLatency), bypassed or not,
//  so the ten voices stay sample-aligned and dry/wet mixing never combs.
// =====================================================================
#pragma once
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include "Voices.h"

namespace motstand {

enum SlipMode { DIOD = 0, GERMANIUM, TEJP, VIKNING, LIKRIKTARE, ROMKROSS, kNumSlipModes };

// ---------------------------------------------------------------------
//  Halfband FIR (31 taps, Kaiser), linear phase. Half the taps are zero
//  so only the non-zero ones are computed.
// ---------------------------------------------------------------------
constexpr int kHbTaps = 31;

struct HalfbandKernel {
    std::array<float, kHbTaps> h{};
    std::array<int, kHbTaps> nz{};
    int numNz = 0;
    HalfbandKernel() {
        const int M = (kHbTaps - 1) / 2;
        const double beta = 8.0;
        auto bessel0 = [](double x) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
        const double i0b = bessel0(beta);
        double sum = 0;
        for (int n = 0; n < kHbTaps; ++n) {
            const int k = n - M;
            double v;
            if (k == 0) v = 0.5;
            else if (k % 2 == 0) v = 0.0;
            else v = std::sin(3.14159265358979 * k / 2.0) / (3.14159265358979 * k);
            const double r = (double)k / M;
            v *= bessel0(beta * std::sqrt(std::max(0.0, 1.0 - r * r))) / i0b;
            h[(size_t)n] = (float)v; sum += v;
        }
        for (auto& x : h) x = (float)(x / sum);          // unity DC gain
        for (int n = 0; n < kHbTaps; ++n) if (h[(size_t)n] != 0.f) nz[(size_t)numNz++] = n;
    }
    static const HalfbandKernel& get() { static const HalfbandKernel k; return k; }
};

struct HbFilter {           // runs at the higher of its two rates
    std::array<float, 2 * kHbTaps> buf{};
    int pos = 0;
    void reset() { buf.fill(0.f); pos = 0; }
    void push(float v) { buf[(size_t)pos] = buf[(size_t)(pos + kHbTaps)] = v; pos = (pos + 1) % kHbTaps; }
    float conv() const {
        const auto& K = HalfbandKernel::get();
        const int newest = pos + kHbTaps - 1;
        float s = 0.f;
        for (int i = 0; i < K.numNz; ++i) { const int t = K.nz[(size_t)i]; s += K.h[(size_t)t] * buf[(size_t)(newest - t)]; }
        return s;
    }
    void up(float x, float& a, float& b) { push(x * 2.f); a = conv(); push(0.f); b = conv(); }
    float down(float a, float b) { push(a); push(b); return conv(); }
};

// 4x up/down. Latency: 15 samples at every stage = 22.5 base samples,
// padded with 2 samples at 4x to an exact 23.
struct Os4 {
    static constexpr int kLatency = 23;
    HbFilter u1, u2, d2, d1;
    std::array<float, 2> pad{};
    void reset() { u1.reset(); u2.reset(); d2.reset(); d1.reset(); pad.fill(0.f); }
    void up(float x, float out[4]) {
        float a, b; u1.up(x, a, b);
        u2.up(a, out[0], out[1]); u2.up(b, out[2], out[3]);
    }
    float down(const float in[4]) {
        // 2-sample pad at 4x (half a base sample) -> integer total latency
        const float s0 = pad[0], s1 = pad[1];
        const float c0 = s0, c1 = s1, c2 = in[0], c3 = in[1];
        pad[0] = in[2]; pad[1] = in[3];
        const float e0 = d2.down(c0, c1), e1 = d2.down(c2, c3);
        return d1.down(e0, e1);
    }
};

// ---------------------------------------------------------------------
//  VINKELSLIP
// ---------------------------------------------------------------------
struct Vinkelslip {
    static constexpr int kLatency = Os4::kLatency;

    // ---- knobs ----
    int   mode    = DIOD;
    float drive   = 0.f;   // 0..1  (0 = bypass)
    float matning = 0.5f;  // 0..1  velocity -> drive
    float mix     = 1.f;   // 0..1  dry/wet

    void prepare(double s) {
        sr = s;
        const double os = sr * 4.0;
        dc.setLP(15.f, os); tape.setLP(9000.f, os);
        gateC = decayCoef(0.004f, os);
        smoothC = 1.f - decayCoef(0.01f, sr);
        engageStep = 1.f / (float)(0.01 * sr);
        reset();
    }
    void reset() {
        os4.reset(); delay.fill(0.f); dpos = 0; xPrev = 0.f; gateEnv = 0.f;
        holdPhase = 0.f; held = 0.f; smooth = 0.f; engage = 0.f; active = false; dc.reset(); tape.reset();
    }
    void onTrigger(float vel01) { lastVel = vel01; }

    float process(float x) {
        // dry path: always delayed by kLatency so every channel lines up
        delay[(size_t)dpos] = x;
        const float dry = delay[(size_t)((dpos + 1) % (kLatency + 1))];
        dpos = (dpos + 1) % (kLatency + 1);

        const float target = std::clamp(drive * (1.f + matning * (lastVel - 0.6f) * 1.6f), 0.f, 1.f);
        smooth += (target - smooth) * smoothC;

        const bool want = drive > 0.f;
        if (want && !active) { os4.reset(); xPrev = 0.f; active = true; }
        engage = want ? std::min(1.f, engage + engageStep) : std::max(0.f, engage - engageStep);
        if (!active) return dry;
        if (!want && engage <= 0.f) { active = false; return dry; }

        const float g = 1.f + smooth * smooth * 30.f;            // up to ~+30 dB
        makeup = 1.f / (1.f + smooth * 1.3f);
        float wet;
        if (mode == ROMKROSS) {
            wet = limit(romCrush(dry * g));                       // base rate, aligned with dry
        } else {
            float up[4], sh[4];
            os4.up(x * g, up);
            for (int i = 0; i < 4; ++i) sh[i] = limit(shape(up[i]));   // everything non-linear at 4x
            wet = os4.down(sh);
        }
        return dry + (wet - dry) * mix * engage;
    }

private:
    float makeup = 1.f;
    float limit(float y) const { return std::tanh(y * makeup * 1.4f) / 1.4f * 1.25f; }
    float shape(float u) {
        float y;
        switch (mode) {
            case DIOD:       y = adaa(u, fDiod, FDiod) * 1.2f; break;
            case GERMANIUM:  y = germanium(u); break;
            case TEJP:       y = tape.lp(adaa(u, fTape, FTape)) * 1.1f; break;
            case VIKNING:    y = adaa(u * 1.3f, fFold, FFold); break;
            default:         y = rectifier(u); break;
        }
        return dc.hp(y);
    }

    // ---- static shapers and their antiderivatives ----
    static float fDiod(float x) { return x >= 0.f ? 1.f - std::exp(-x) : -(1.f - std::exp(0.7f * x)) / 0.7f; }
    static float FDiod(float x) {
        return x >= 0.f ? x + std::exp(-x) - 1.f
                        : -(x - std::exp(0.7f * x) / 0.7f) / 0.7f - 1.f / (0.7f * 0.7f);
    }
    static float fTape(float x) { return x / std::sqrt(1.f + x * x); }
    static float FTape(float x) { return std::sqrt(1.f + x * x) - 1.f; }
    static float fFold(float x) { return std::sin(x); }
    static float FFold(float x) { return 1.f - std::cos(x); }
    static float fTanhB(float x) { return std::tanh(x + 0.35f) - 0.33638f; }        // tanh(0.35)
    static float FTanhB(float x) {                                                   // log(cosh(x+b)) - tanh(b)x
        const float a = std::fabs(x + 0.35f);
        return a + std::log1p(std::exp(-2.f * a)) - 0.6931472f - 0.33638f * x;
    }
    static float fRect(float v) { return 0.35f * v + 0.9f * (std::fabs(v) / (1.f + std::fabs(v))); }
    static float FRect(float v) { const float a = std::fabs(v); return 0.175f * v * v + 0.9f * (a - std::log1p(a)) * (v >= 0.f ? 1.f : -1.f); }

    template <typename F, typename Fi>
    float adaa(float x, F f, Fi Fint) {
        const float d = x - xPrev;
        const float y = std::fabs(d) > 1e-4f ? (Fint(x) - Fint(xPrev)) / d : f(0.5f * (x + xPrev));
        xPrev = x;
        return y;
    }
    float germanium(float u) {
        // starving gate: below the bias the transistor stops conducting
        const float a = std::fabs(u);
        gateEnv = a > gateEnv ? a : gateEnv * gateC;
        const float gate = std::clamp((gateEnv - 0.08f) * 6.f, 0.f, 1.f);
        return adaa(u, fTanhB, FTanhB) * gate * 1.4f;
    }
    float rectifier(float u) { return adaa(u, fRect, FRect) * 1.3f; }
    float romCrush(float u) {
        const float rate = 0.5f - 0.45f * smooth;            // down to ~2.4 kHz at 48k
        holdPhase += rate;
        if (holdPhase >= 1.f) {
            holdPhase -= 1.f;
            const float bits = 8.f - 5.f * smooth;            // 8 -> 3 bits
            const float steps = std::exp2(bits) * 0.5f;
            held = std::round(std::clamp(u, -1.5f, 1.5f) / 1.5f * steps) / steps;
        }
        return held;
    }

    double sr = 48000;
    Os4 os4;
    std::array<float, kLatency + 1> delay{};
    int dpos = 0;
    float xPrev = 0.f, lastVel = 1.f, smooth = 0.f, smoothC = 0.01f, engage = 0.f, engageStep = 0.001f;
    float gateEnv = 0.f, gateC = 0.99f, holdPhase = 0.f, held = 0.f;
    bool active = false;
    OnePole dc, tape;
};

// ---------------------------------------------------------------------
//  ELNÄT — Swedish mains hum, 50 Hz with the odd harmonics a badly
//  grounded workshop gives you. Amount 0..1 (1 = about -42 dBFS).
// ---------------------------------------------------------------------
struct Elnat {
    float amount = 0.f;
    void prepare(double s) { sr = s; phase = 0.0; wobble = 0.0; }
    float process() {
        if (amount <= 0.f) return 0.f;
        phase += 50.0 / sr; if (phase >= 1.0) phase -= 1.0;
        wobble += 0.13 / sr; if (wobble >= 1.0) wobble -= 1.0;
        const float p = kTau * (float)phase;
        const float buzz = std::sin(p) + 0.25f * std::sin(2.f * p) + 0.5f * std::sin(3.f * p)
                         + 0.3f * std::sin(5.f * p) + 0.18f * std::sin(7.f * p);
        const float w = 0.85f + 0.15f * std::sin(kTau * (float)wobble);   // the grid breathes
        return buzz * w * amount * 0.0045f;
    }
private:
    double sr = 48000, phase = 0.0, wobble = 0.0;
};

} // namespace motstand
