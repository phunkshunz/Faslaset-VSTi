// =====================================================================
//  MASKINHALLEN : MOTSTÅND — studio effects (step 4c)
//  Phunkshunz AUDIO
//
//    Varvshall   8-line feedback delay network reverb: input diffusion,
//                slowly modulated lines (no metallic ringing), frequency-
//                dependent decay, predelay, low cut, FRYS (freeze).
//    Eko         stereo tape echo synced to the machine's OWN drifting
//                tempo; wow & flutter, saturating loop, tone filter,
//                ping-pong. Time changes glide like real tape.
//    Isolator    DJ-mixer 3-band isolator, Linkwitz-Riley 4th order
//                (bands sum flat), with full kills.
//    Press       stereo bus compressor, soft knee, auto make-up.
//    Pump        kick-triggered ducking for everything except the kick.
//
//  Allocation happens only in prepare(); process is real-time safe.
// =====================================================================
#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include "Voices.h"

namespace motstand {

// ---------------------------------------------------------------------
//  Shared helpers
// ---------------------------------------------------------------------
struct DelayLine {
    std::vector<float> buf;
    int mask = 0, w = 0;
    void prepare(int maxSamples) {
        int n = 1; while (n < maxSamples + 4) n <<= 1;
        buf.assign((size_t)n, 0.f); mask = n - 1; w = 0;
    }
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); }
    void push(float x) { buf[(size_t)w] = x; w = (w + 1) & mask; }
    // sample written k pushes ago (k = 1 is the newest)
    float at(int k) const { return buf[(size_t)((w - k) & mask)]; }
    // delay in samples (>= 1), linear interpolation
    float readLin(float d) const {
        const int k = (int)d; const float f = d - (float)k;
        const float a = at(k), b = at(k + 1);
        return a + (b - a) * f;
    }
    // 4-point Hermite (for the tape echo, where pitch glides are audible); d >= 2
    float readHermite(float d) const {
        const int k = (int)d; const float f = d - (float)k;
        const float xm1 = at(k - 1), x0 = at(k), x1 = at(k + 1), x2 = at(k + 2);
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
};

struct Allpass {
    DelayLine d; int len = 1; float g = 0.6f;
    void prepare(int samples, float gain) { len = std::max(1, samples); g = gain; d.prepare(len + 2); }
    float process(float x) {
        const float y = d.readLin((float)len);
        const float v = x + g * y;
        d.push(v);
        return y - g * v;
    }
};

// ---------------------------------------------------------------------
//  VARVSHALL
// ---------------------------------------------------------------------
struct Varvshall {
    // knobs
    float size = 0.6f;      // 0..1  -> 0.4x .. 1.6x room
    float decay = 2.5f;     // seconds (T60)
    float damping = 0.5f;   // 0..1  (1 = dark)
    float predelayMs = 18.f;
    bool  freeze = false;

    void prepare(double s) {
        sr = s;
        pre.prepare((int)(0.16 * sr));
        const int diffMs[4] = { 5, 7, 11, 17 };   // short, prime-ish
        for (int c = 0; c < 2; ++c)
            for (int k = 0; k < 4; ++k)
                diff[(size_t)c][(size_t)k].prepare((int)((diffMs[k] + c * 1.3) * 0.001 * sr * 0.61), 0.62f);
        for (size_t i = 0; i < 8; ++i) line[i].prepare((int)(kLenMs[i] * 1.7 * 0.001 * sr) + 64);
        lowCut.setLP(110.f, sr);
        for (size_t i = 0; i < 8; ++i) { damp[i].reset(); lfoPh[i] = (float)i / 8.f; }
        updateCoefs(true);
    }
    void clear() {
        pre.clear();
        for (auto& c : diff) for (auto& a : c) a.d.clear();
        for (auto& l : line) l.clear();
        for (auto& d : damp) d.reset();
    }

    void process(float inL, float inR, float& outL, float& outR) {
        updateCoefs(false);
        // low cut + predelay on the mono-ish input (keeps the kick out of the hall)
        float in = 0.5f * (inL + inR);
        in = lowCut.hp(in);
        pre.push(in);
        const float pd = std::max(1.f, predelayMs * 0.001f * (float)sr);
        const float x = freeze ? 0.f : pre.readLin(pd);
        float a = x, b = x * 0.98f;
        for (auto& ap : diff[0]) a = ap.process(a);
        for (auto& ap : diff[1]) b = ap.process(b);

        // read the 8 modulated lines
        std::array<float, 8> v{};
        for (size_t i = 0; i < 8; ++i) {
            lfoPh[i] += lfoInc[i]; if (lfoPh[i] >= 1.f) lfoPh[i] -= 1.f;
            const float mod = freeze ? 0.f : std::sin(kTau * lfoPh[i]) * modDepth;   // frozen = exact, lossless
            v[i] = line[i].readLin(lineLen[i] + mod);
        }
        // Householder mix: y = v - 2/N * sum(v)
        float sum = 0.f; for (float q : v) sum += q;
        const float h = sum * (2.f / 8.f);
        for (size_t i = 0; i < 8; ++i) {
            float y = v[i] - h;
            if (!freeze) y = damp[i].lp(y);
            const float inj = (i < 4 ? a : b) * ((i & 1) ? -1.f : 1.f);
            line[i].push(y * fb[i] + inj);
        }
        outL = (v[0] - v[2] + v[4] - v[6] + 0.5f * (v[1] - v[7])) * 0.42f;
        outR = (v[1] - v[3] + v[5] - v[7] + 0.5f * (v[2] - v[4])) * 0.42f;
    }

private:
    static constexpr float kLenMs[8] = { 29.7f, 37.1f, 41.1f, 43.7f, 53.1f, 59.3f, 67.7f, 73.1f };

    void updateCoefs(bool force) {
        if (!force && size == lastSize && decay == lastDecay && damping == lastDamp && freeze == lastFreeze) return;
        lastSize = size; lastDecay = decay; lastDamp = damping; lastFreeze = freeze;
        const float scale = 0.4f + 1.2f * std::clamp(size, 0.f, 1.f);
        // the damping filters also eat energy, so the loop gain is set for a
        // slightly longer time: measured T60 then matches the knob (within ~5 %)
        const float t60 = std::max(0.1f, decay) * 1.28f;
        for (size_t i = 0; i < 8; ++i) {
            lineLen[i] = std::round(kLenMs[i] * scale * 0.001f * (float)sr);
            fb[i] = freeze ? 1.f : std::pow(10.f, -3.f * lineLen[i] / (t60 * (float)sr));
            damp[i].setLP(16000.f * std::pow(0.12f, std::clamp(damping, 0.f, 1.f)), sr);   // 16k .. ~2k
            lfoInc[i] = (0.11f + 0.07f * (float)i) / (float)sr;
        }
        modDepth = 0.0012f * (float)sr * (0.5f + 0.5f * scale);    // ~±1 ms of slow chorus
    }

    double sr = 48000;
    DelayLine pre;
    std::array<std::array<Allpass, 4>, 2> diff;
    std::array<DelayLine, 8> line;
    std::array<OnePole, 8> damp;
    std::array<float, 8> lineLen{}, fb{}, lfoPh{}, lfoInc{};
    float modDepth = 0.f;
    OnePole lowCut;
    float lastSize = -1, lastDecay = -1, lastDamp = -1; bool lastFreeze = false;
};

// ---------------------------------------------------------------------
//  EKO  (tape echo)
// ---------------------------------------------------------------------
struct Eko {
    // knobs
    int   division = 2;       // index into kDiv
    float feedback = 0.45f;   // 0..1.05 (just over 1 = runaway, tamed by the tape)
    float tone = 0.5f;        // 0 dark .. 1 bright
    float wear = 0.3f;        // wow, flutter, tape saturation
    bool  pingPong = true;
    double bpm = 133.0;

    static constexpr float kDiv[6] = { 0.25f, 0.5f, 0.75f, 1.f, 1.5f, 2.f };   // in beats: 1/16 1/8 3/16 1/4 3/8 1/2

    void prepare(double s) {
        sr = s;
        for (auto& d : dl) d.prepare((int)(2.6 * sr));
        cur = target();
        wowPh = flutPh = 0.f;
        for (auto& f : hp) f.setLP(140.f, sr);
        for (auto& f : lp) f.reset();
        smoothC = 1.f - std::exp(-1.f / (0.12f * (float)sr));     // tape glide when time changes
    }
    void clear() { for (auto& d : dl) d.clear(); }

    void process(float inL, float inR, float& outL, float& outR) {
        cur += (target() - cur) * smoothC;
        wowPh  += 0.55f / (float)sr; if (wowPh  >= 1.f) wowPh  -= 1.f;
        flutPh += 6.9f  / (float)sr; if (flutPh >= 1.f) flutPh -= 1.f;
        const float wobble = 1.f + wear * (0.0035f * std::sin(kTau * wowPh) + 0.0009f * std::sin(kTau * flutPh));
        const float d = std::clamp(cur * wobble, 4.f, (float)(2.5 * sr));
        const float lpHz = 1800.f * std::pow(7.f, std::clamp(tone, 0.f, 1.f));   // 1.8k .. 12.6k
        lp[0].setLP(lpHz, sr); lp[1].setLP(lpHz, sr);

        const float rL = dl[0].readHermite(d), rR = dl[1].readHermite(pingPong ? d : d * 1.003f);
        auto loop = [&](float x, int c) {
            x = hp[(size_t)c].hp(x);
            x = lp[(size_t)c].lp(x);
            const float drive = 1.f + wear * 1.5f;
            return std::tanh(x * drive) / drive;                   // tape keeps runaway in check
        };
        const float fbk = std::clamp(feedback, 0.f, 1.05f);
        if (pingPong) {
            dl[0].push(0.5f * (inL + inR) + loop(rR, 1) * fbk);    // mono in -> left, bounce
            dl[1].push(loop(rL, 0) * fbk);
        } else {
            dl[0].push(inL + loop(rL, 0) * fbk);
            dl[1].push(inR + loop(rR, 1) * fbk);
        }
        outL = rL; outR = rR;
    }
    float delaySamples() const { return target(); }

private:
    float target() const {
        const float beats = kDiv[(size_t)std::clamp(division, 0, 5)];
        return (float)(beats * 60.0 / std::max(30.0, bpm) * sr);
    }
    double sr = 48000;
    std::array<DelayLine, 2> dl;
    std::array<OnePole, 2> hp, lp;
    float cur = 1000.f, smoothC = 0.001f, wowPh = 0.f, flutPh = 0.f;
};

// ---------------------------------------------------------------------
//  ISOLATOR — 3-band Linkwitz-Riley (24 dB/oct), bands sum flat
// ---------------------------------------------------------------------
struct Isolator {
    float low = 1.f, mid = 1.f, high = 1.f;   // linear gains (0 = kill)
    float f1 = 250.f, f2 = 2500.f;

    void prepare(double s) {
        sr = s;
        for (auto& c : ch) {
            for (auto& f : c.lp1) f.set(f1, 0.70710678f, sr);
            for (auto& f : c.hp1) f.set(f1, 0.70710678f, sr);
            for (auto& f : c.lp2) f.set(f2, 0.70710678f, sr);
            for (auto& f : c.hp2) f.set(f2, 0.70710678f, sr);
            c.ap.set(f2, 0.70710678f, sr);
        }
        sl = low; sm = mid; sh = high;
        smoothC = 1.f - std::exp(-1.f / (0.008f * (float)sr));     // 8 ms: kills are fast but click-free
    }
    void process(float& L, float& R) {
        sl += (low - sl) * smoothC; sm += (mid - sm) * smoothC; sh += (high - sh) * smoothC;
        L = band(ch[0], L); R = band(ch[1], R);
    }
private:
    struct Chan { std::array<Svf, 2> lp1, hp1, lp2, hp2; Svf ap; };
    float band(Chan& c, float x) {
        // low = LR4 low @ f1, then all-passed at f2 so it lines up with mid + high
        float lo = x; for (auto& f : c.lp1) { f.tick(lo); lo = f.lp; }
        float rest = x; for (auto& f : c.hp1) { f.tick(rest); rest = f.hp; }
        c.ap.tick(lo); lo = c.ap.lp + c.ap.hp - c.ap.k * c.ap.bp;
        float mi = rest; for (auto& f : c.lp2) { f.tick(mi); mi = f.lp; }
        float hi = rest; for (auto& f : c.hp2) { f.tick(hi); hi = f.hp; }
        return lo * sl + mi * sm + hi * sh;
    }
    double sr = 48000;
    std::array<Chan, 2> ch;
    float sl = 1, sm = 1, sh = 1, smoothC = 0.01f;
};

// ---------------------------------------------------------------------
//  PRESS — stereo bus compressor (one knob)
// ---------------------------------------------------------------------
struct Press {
    float amount = 0.f;   // 0 = bypass
    void prepare(double s) {
        sr = s; envDb = -120.f; grDb = 0.f;
        att = 1.f - std::exp(-1.f / (0.006f * (float)sr));
        rel = 1.f - std::exp(-1.f / (0.140f * (float)sr));
    }
    void process(float& L, float& R) {
        if (amount <= 0.001f) { grDb += (0.f - grDb) * rel; if (grDb < -0.01f) apply(L, R); return; }
        const float peak = std::max(std::fabs(L), std::fabs(R));
        const float inDb = 20.f * std::log10(peak + 1e-9f);
        const float thr = -3.f - 24.f * amount, ratio = 2.f + 4.f * amount, knee = 6.f;
        float over = inDb - thr, target = 0.f;
        if (over > knee * 0.5f) target = -(over * (1.f - 1.f / ratio));
        else if (over > -knee * 0.5f) { const float t = over + knee * 0.5f; target = -(t * t / (2.f * knee)) * (1.f - 1.f / ratio); }
        grDb += (target - grDb) * (target < grDb ? att : rel);
        apply(L, R);
    }
    float gainReductionDb() const { return -grDb; }
private:
    void apply(float& L, float& R) {
        const float makeup = amount * 7.f;                      // auto make-up, a bit under the reduction
        const float g = std::pow(10.f, (grDb + makeup) / 20.f);
        L *= g; R *= g;
    }
    double sr = 48000;
    float envDb = -120.f, grDb = 0.f, att = 0.01f, rel = 0.001f;
};

// ---------------------------------------------------------------------
//  PUMP — every kick ducks the rest of the machine
// ---------------------------------------------------------------------
struct Pump {
    float amount = 0.f;   // 0..1 -> up to -14 dB
    void prepare(double s) {
        sr = s; env = 0.f; target = 0.f;
        att = 1.f - std::exp(-1.f / (0.004f * (float)sr));
        rel = 1.f - std::exp(-1.f / (0.170f * (float)sr));
    }
    void trigger(float vel) { target = std::max(target, 0.6f + 0.4f * vel); hold = (int)(0.012 * sr); }
    float next() {
        if (hold > 0) --hold; else target = 0.f;
        env += (target - env) * (target > env ? att : rel);
        return amount <= 0.f ? 1.f : std::pow(10.f, -amount * 14.f * env / 20.f);
    }
private:
    double sr = 48000;
    float env = 0.f, target = 0.f, att = 0.1f, rel = 0.001f;
    int hold = 0;
};

} // namespace motstand
