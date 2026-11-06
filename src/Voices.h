// =====================================================================
//  MASKINHALLEN : MOTSTÅND  —  909-style voices (v2, step 4a)
//  Phunkshunz AUDIO
//
//  Real-time safe in process()/trigger(). The cymbal ROMs are built
//  once (static, on first use) outside the audio callback.
//
//    KICK        two-stage pitch drop, triangle VCO through a waveshaper
//                that fattens with accent, separate attack pulse + click
//    SNARE       two sine-based tuned bodies + snappy noise
//    CLAP        four bursts (0/9/18/28 ms) + bandpassed room tail
//    RIM         two short pings through a saturating bridge
//    TOM L/M/H   sine body + inharmonic partial + stick noise
//    CH/OH/RIDE  909-style ROM: a band-limited metallic recording, built
//                at 32 kHz (optionally read as 6-bit like the real ROM), played back with
//                interpolation and a reconstruction filter. Tune changes
//                the playback rate, exactly like the 909's tune knob.
//
//  Everything is band-limited by construction (sines, soft-shaped
//  triangles, ROMs), so no aliasing hash in the top end.
// =====================================================================
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <vector>

namespace motstand {

constexpr float kPi  = 3.14159265358979f;
constexpr float kTau = 6.28318530717959f;

enum Instrument {
    KICK = 0, SNARE, CLAP, RIM, TOM_LO, TOM_MID, TOM_HI, CH, OH, RIDE,
    kNumInstruments
};

// ---------------------------------------------------------------------
//  Small DSP helpers
// ---------------------------------------------------------------------
struct Noise {
    uint32_t s = 0x1234567u;
    void seed(uint32_t v) { s = v ? v : 1u; }
    float next() {                       // white, -1..1
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        return (float)(int32_t)s * (1.0f / 2147483648.0f);
    }
};

// Topology-preserving state variable filter (Simper)
struct Svf {
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0;
    float lp = 0, bp = 0, hp = 0;
    void set(float hz, float q, double sr) {
        hz = std::clamp(hz, 10.f, (float)(sr * 0.45));
        g  = std::tan(kPi * hz / (float)sr);
        k  = 1.f / std::max(0.05f, q);
        a1 = 1.f / (1.f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    void tick(float v0) {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0.f; }
};

struct OnePole {
    float a = 1.f, z = 0.f;
    void setLP(float hz, double sr) { a = 1.f - std::exp(-kTau * std::min(hz, (float)(sr * 0.49)) / (float)sr); }
    float lp(float x) { z += a * (x - z); return z; }
    float hp(float x) { return x - lp(x); }
    void reset() { z = 0.f; }
};

inline float decayCoef(float seconds, double sr) {
    return std::exp(-1.f / (std::max(0.0003f, seconds) * (float)sr));
}

// 909-ish accent curve: soft hits are clearly softer, accents push
inline float velGain(float vel01) {
    return 0.3f + 0.7f * std::pow(std::clamp(vel01, 0.f, 1.f), 1.3f);
}

inline float semis(float s) { return std::exp2(s / 12.f); }

// Common voice parameters (0..1 knobs, set by the processor)
struct VoiceParams {
    float tune  = 0.5f;
    float decay = 0.5f;
    float tone  = 0.5f;   // voice-specific colour (attack, snappy, ...)
    float level = 0.8f;
    // "component tolerance": set once per seed, never by the user
    float tolTune  = 1.f;
    float tolDecay = 1.f;
    int   model = 0;          // sound model 0..3 (0 = the 909 voice)
};

constexpr int kNumModels = 4;

// How much the DrunkRider may move a hit at full amount
constexpr float kRideTuneSemis = 0.25f;   // ±25 cents
constexpr float kRideDecayPct  = 0.08f;   // ±8 % decay

// ---------------------------------------------------------------------
//  KICK
// ---------------------------------------------------------------------
struct Kick {
    VoiceParams p;
    double sr = 48000;
    float phase = 0, base = 50, amp = 0, ampC = 0;
    float e1 = 0, e1C = 0, e2 = 0, e2C = 0;     // two-stage pitch envelope
    float pulse = 0, pulseC = 0, click = 0, clickC = 0;
    float shapeK = 1.5f, shapeNorm = 1.f, gain = 0, hold = 0;
    float amt1 = 3.f, amt2 = 0.9f, bodyNz = 0.f;
    int   holdN = 0;
    Svf clickF, bodyF; OnePole out; Noise n;

    void prepare(double s) {
        sr = s; clickF.set(2600, 0.9f, sr); bodyF.set(320, 0.7f, sr); out.setLP(9000, sr); n.seed(0x909A);
        amp = 0; pulse = click = 0;
    }

    void trigger(float vel, float tuneMod, float decayMod) {
        // models: 0 909, 1 BOOM (long sub), 2 STAMP (short punch), 3 INDUSTRI (squared + grit body)
        float lo = 38.f, range = 30.f, decMul = 1.f, snap = 1.f, clickMul = 1.f, kShape = 0.9f, kVel = 1.3f;
        amt1 = 3.f; amt2 = 0.9f; bodyNz = 0.f;
        switch (p.model) {
            case 1: lo = 31.f; range = 22.f; decMul = 1.9f; amt1 = 1.4f; amt2 = 0.5f; clickMul = 0.35f; kShape = 0.35f; kVel = 0.4f; break;
            case 2: lo = 48.f; range = 34.f; decMul = 0.42f; amt1 = 4.4f; amt2 = 1.5f; snap = 0.6f; clickMul = 1.6f; kShape = 1.6f; kVel = 1.6f; break;
            case 3: lo = 40.f; range = 28.f; decMul = 0.85f; amt1 = 3.4f; amt2 = 1.1f; clickMul = 1.1f; kShape = 3.2f; kVel = 2.0f; bodyNz = 0.14f; break;
            default: break;
        }
        base = (lo + range * p.tune) * p.tolTune * semis(tuneMod * kRideTuneSemis);
        const float dec = (0.10f + 1.2f * p.decay * p.decay) * decMul * p.tolDecay * (1.f + decayMod * kRideDecayPct)
                          * (1.f + 0.15f * vel);                   // accent rings a bit longer
        ampC  = decayCoef(dec, sr);
        e1C   = decayCoef((0.004f + 0.004f * (1.f - p.tone)) * snap, sr);   // fast snap
        e2C   = decayCoef((0.035f + 0.03f * (1.f - p.tone)) * snap, sr);    // slower settle
        pulseC= decayCoef(0.0004f, sr);
        clickC= decayCoef(0.0025f, sr);
        amp = 1.f; e1 = 1.f; e2 = 1.f;
        pulse = (0.5f + 0.5f * p.tone) * std::min(1.f, clickMul);
        click = (0.25f + 0.55f * p.tone) * clickMul;
        phase = 0.f;
        holdN = (int)(0.003 * sr);                                 // VCA holds ~3 ms open
        shapeK = kShape + kVel * vel;                              // accent rounds it fuller
        shapeNorm = 1.f / std::tanh(shapeK);
        gain = velGain(vel) * p.level;
    }

    float process() {
        if (amp < 1e-5f && click < 1e-5f && pulse < 1e-5f) return 0.f;
        const float f = base * (1.f + amt1 * e1 + amt2 * e2);
        phase += f / (float)sr; phase -= std::floor(phase);
        // triangle VCO, inverted so the hit starts on the positive peak (the 909 thump),
        // then soft-shaped toward a sine; accent pushes it squarer and fatter
        const float tri = 1.f - 4.f * std::fabs(phase - 0.5f);
        float body = std::tanh(shapeK * -tri) * shapeNorm * amp;
        const float w = n.next();
        if (bodyNz > 0.f) { bodyF.tick(w); body += bodyF.lp * bodyNz * amp * 2.f; }
        clickF.tick(w);
        const float att = pulse + clickF.bp * click;
        const float y = out.lp(body * 0.82f + att * 0.42f);
        if (holdN > 0) --holdN; else amp *= ampC;
        e1 *= e1C; e2 *= e2C; pulse *= pulseC; click *= clickC;
        return y * gain;
    }
};

// ---------------------------------------------------------------------
//  SNARE  (tone = snappy amount)
// ---------------------------------------------------------------------
struct Snare {
    VoiceParams p;
    double sr = 48000;
    float ph1 = 0, ph2 = 0, f1 = 185, f2 = 330;
    float b1 = 0, b1C = 0, b2 = 0, b2C = 0, noiseEnv = 0, noiseC = 0, pEnv = 0, pC = 0, gain = 0, vel = 1;
    float bodyMix = 1.f, noiseMix = 1.f;
    Svf hp, lp; OnePole air; Noise n;

    void prepare(double s) {
        sr = s; n.seed(0x5EA7);
        hp.set(1100, 0.6f, sr); lp.set(8500, 0.6f, sr); air.setLP(14000, sr);
    }
    void trigger(float v, float tuneMod, float decayMod) {
        vel = v;
        // models: 0 909, 1 TAJT (high, short), 2 FET (low, big body), 3 BRUS (almost all noise)
        float off = 0.f, bMul = 1.f, nMul = 1.f; bodyMix = 1.f; noiseMix = 1.f;
        switch (p.model) {
            case 1: off = 5.f;  bMul = 0.55f; nMul = 0.45f; bodyMix = 0.85f; noiseMix = 1.1f; break;
            case 2: off = -3.f; bMul = 1.8f;  nMul = 1.3f;  bodyMix = 1.4f;  noiseMix = 0.85f; break;
            case 3: off = 0.f;  bMul = 0.4f;  nMul = 1.6f;  bodyMix = 0.25f; noiseMix = 1.4f; break;
            default: break;
        }
        const float t = semis((p.tune - 0.5f) * 8.f + off + tuneMod * kRideTuneSemis) * p.tolTune;
        f1 = 178.f * t; f2 = 331.f * t;
        const float dm = (1.f + decayMod * kRideDecayPct) * p.tolDecay;
        b1C = decayCoef(0.060f * dm * bMul, sr);
        b2C = decayCoef(0.035f * dm * bMul, sr);
        noiseC = decayCoef((0.05f + 0.28f * p.decay) * dm * nMul, sr);
        pC = decayCoef(0.006f, sr);
        b1 = b2 = noiseEnv = pEnv = 1.f;
        ph1 = 0.f; ph2 = 0.13f;
        gain = velGain(v) * p.level;
    }
    float process() {
        if (b1 < 1e-5f && noiseEnv < 1e-5f) return 0.f;
        const float bend = 1.f + 0.35f * pEnv;
        ph1 += f1 * bend / (float)sr; ph1 -= std::floor(ph1);
        ph2 += f2 * bend / (float)sr; ph2 -= std::floor(ph2);
        const float s1 = std::sin(kTau * ph1), s2 = std::sin(kTau * ph2);
        // a touch of 2nd harmonic: the 909 bodies are slightly squashed sines
        const float body = (s1 + 0.18f * s1 * s1) * b1 * 0.6f + (s2 + 0.12f * s2 * s2) * b2 * 0.4f;
        hp.tick(n.next()); lp.tick(hp.hp);
        const float snap = air.lp(lp.lp) * noiseEnv * (0.8f + 0.4f * vel);
        const float mix = body * (1.f - 0.55f * p.tone) * bodyMix + snap * (0.45f + 1.0f * p.tone) * noiseMix;
        b1 *= b1C; b2 *= b2C; noiseEnv *= noiseC; pEnv *= pC;
        return mix * 0.8f * gain;
    }
};

// ---------------------------------------------------------------------
//  CLAP  (four bursts, then the room)
// ---------------------------------------------------------------------
struct Clap {
    VoiceParams p;
    double sr = 48000;
    std::array<int, 4> burstAt{};
    int counter = 1 << 30, tailAt = 0, nextBurst = 4, numBursts = 4;
    float burstEnv = 0, burstC = 0, tail = 0, tailC = 0, gain = 0;
    Svf bp, room; Noise n;

    void prepare(double s) {
        sr = s; n.seed(0xC1A9);
        burstAt = { 0, (int)(0.009 * sr), (int)(0.018 * sr), (int)(0.028 * sr) };
        tailAt = burstAt[3];
    }
    void trigger(float vel, float tuneMod, float decayMod) {
        // models: 0 909, 1 TAJT (two bursts, short), 2 SAL (big hall tail), 3 SNAPP (finger snap)
        float bpHz = 1150.f, q = 1.3f, tailMul = 1.f, g = 1.4f; numBursts = 4;
        switch (p.model) {
            case 1: numBursts = 2; tailMul = 0.35f; break;
            case 2: bpHz = 950.f; q = 0.9f; tailMul = 2.6f; g = 1.2f; break;
            case 3: numBursts = 1; bpHz = 2300.f; q = 2.2f; tailMul = 0.22f; g = 1.6f; break;
            default: break;
        }
        const float t = semis((p.tune - 0.5f) * 10.f + tuneMod * kRideTuneSemis) * p.tolTune;
        bp.set(bpHz * t, q, sr);
        room.set(bpHz * 0.78f * t, 0.7f, sr);
        burstC = decayCoef(0.0032f, sr);
        tailC  = decayCoef((0.07f + 0.4f * p.decay) * tailMul * (1.f + decayMod * kRideDecayPct) * p.tolDecay, sr);
        tailAt = burstAt[(size_t)(numBursts - 1)];
        counter = 0; nextBurst = 0; burstEnv = 0.f; tail = 0.f;
        gain = velGain(vel) * p.level * g;
    }
    float process() {
        if (counter > tailAt && tail < 1e-5f) return 0.f;
        if (nextBurst < numBursts && counter == burstAt[(size_t)nextBurst]) {
            burstEnv = 1.f - 0.1f * (float)nextBurst; ++nextBurst;
        }
        if (counter == tailAt) tail = 0.75f;
        const float w = n.next();
        bp.tick(w); room.tick(w);
        const float out = bp.bp * burstEnv + (bp.bp * 0.6f + room.bp * 0.4f) * tail;
        burstEnv *= burstC; tail *= tailC; ++counter;
        return out * gain;
    }
};

// ---------------------------------------------------------------------
//  RIM  (bridged-T style double ping)
// ---------------------------------------------------------------------
struct Rim {
    VoiceParams p;
    double sr = 48000;
    float ph1 = 0, ph2 = 0, f1 = 500, f2 = 1700, env = 0, envC = 0, env2 = 0, env2C = 0, gain = 0;
    float w2 = 0.8f, nzAmt = 0.08f; bool bell = false;
    Svf hp; Noise n;

    void prepare(double s) { sr = s; hp.set(380, 0.7f, sr); n.seed(0x219u); }
    void trigger(float vel, float tuneMod, float decayMod) {
        // models: 0 909 rim, 1 CLAVE (wood), 2 KOSKÄLLA (cowbell), 3 KNACK (low knock)
        const float t = semis((p.tune - 0.5f) * 8.f + tuneMod * kRideTuneSemis) * p.tolTune;
        float d = (0.006f + 0.025f * p.decay), d2 = 0.5f, g = 0.7f;
        f1 = 480.f; f2 = 1690.f; w2 = 0.8f; nzAmt = 0.08f * p.tone; bell = false;
        switch (p.model) {
            case 1: f1 = 2450.f; w2 = 0.f; d = 0.02f + 0.04f * p.decay; d2 = 1.f; g = 0.55f; nzAmt = 0.f; break;
            case 2: f1 = 540.f; f2 = 800.f; w2 = 1.f; d = 0.05f + 0.3f * p.decay; d2 = 0.6f; g = 0.42f; bell = true; break;
            case 3: f1 = 285.f; f2 = 610.f; w2 = 0.5f; d = 0.008f + 0.02f * p.decay; d2 = 0.4f; g = 1.1f; nzAmt = 0.25f; break;
            default: break;
        }
        f1 *= t; f2 *= t;
        d *= (1.f + decayMod * kRideDecayPct) * p.tolDecay;
        envC = decayCoef(d, sr); env2C = decayCoef(d * d2, sr);
        env = env2 = 1.f; ph1 = ph2 = 0.f;
        gain = velGain(vel) * p.level * g;
    }
    float process() {
        if (env < 1e-5f) return 0.f;
        ph1 += f1 / (float)sr; ph1 -= std::floor(ph1);
        ph2 += f2 / (float)sr; ph2 -= std::floor(ph2);
        // the cowbell's two oscillators are squarish (band-limited: fundamental + 3rd + 5th)
        auto osc = [&](float ph) { const float a = kTau * ph; return bell ? std::sin(a) + std::sin(3.f * a) / 3.f + std::sin(5.f * a) / 5.f : std::sin(a); };
        const float x = osc(ph1) * env + w2 * osc(ph2) * env2 + n.next() * env2 * nzAmt;
        hp.tick(x * 0.55f);
        env *= envC; env2 *= env2C;
        return hp.hp * gain;
    }
};

// ---------------------------------------------------------------------
//  TOM  (base frequency set per instance)
// ---------------------------------------------------------------------
struct Tom {
    VoiceParams p;
    double sr = 48000;
    float baseHz = 100;
    float phase = 0, ph2 = 0, freq = 100, amp = 0, ampC = 0, a2 = 0, a2C = 0, pEnv = 0, pC = 0;
    float nEnv = 0, nC = 0, gain = 0, pAmt = 0.55f;
    Svf nf; Noise n;

    void prepare(double s, float base, uint32_t seed) { sr = s; baseHz = base; n.seed(seed); }
    void trigger(float vel, float tuneMod, float decayMod) {
        // models: 0 909, 1 CONGA (tight, slap), 2 SYNT (long deep sweep), 3 BONGO (high, short)
        float fMul = 1.f, dMul = 1.f, pTau = 0.045f, a2s = 0.28f, nMul = 1.f; pAmt = 0.55f;
        switch (p.model) {
            case 1: fMul = 1.55f; dMul = 0.7f;  pAmt = 0.18f; pTau = 0.02f;  a2s = 0.15f; nMul = 1.6f; break;
            case 2: fMul = 0.9f;  dMul = 1.6f;  pAmt = 2.2f;  pTau = 0.16f;  a2s = 0.f;   nMul = 0.2f; break;
            case 3: fMul = 2.4f;  dMul = 0.45f; pAmt = 0.25f; pTau = 0.015f; a2s = 0.2f;  nMul = 1.2f; break;
            default: break;
        }
        freq = baseHz * fMul * semis((p.tune - 0.5f) * 12.f + tuneMod * kRideTuneSemis) * p.tolTune;
        const float d = (0.12f + 0.7f * p.decay) * dMul * (1.f + decayMod * kRideDecayPct) * p.tolDecay;
        ampC = decayCoef(d, sr); a2C = decayCoef(d * 0.3f, sr);
        pC   = decayCoef(pTau, sr);
        nC   = decayCoef(0.012f, sr);
        nf.set(std::min(freq * 5.f, (float)(sr * 0.4)), 0.8f, sr);
        amp = 1.f; a2 = a2s; pEnv = 1.f; nEnv = (0.15f + 0.35f * p.tone) * nMul; phase = ph2 = 0.f;
        gain = velGain(vel) * p.level;
    }
    float process() {
        if (amp < 1e-5f) return 0.f;
        const float f = freq * (1.f + pAmt * pEnv);
        phase += f / (float)sr; phase -= std::floor(phase);
        ph2   += f * 1.48f / (float)sr; ph2 -= std::floor(ph2);   // inharmonic head mode
        nf.tick(n.next());
        const float out = std::sin(kTau * phase) * amp + std::sin(kTau * ph2) * a2 + nf.lp * nEnv;
        amp *= ampC; a2 *= a2C; pEnv *= pC; nEnv *= nC;
        return out * 0.85f * gain;
    }
};

// ---------------------------------------------------------------------
//  CYMBAL ROM — built once, shared by every instance.
//  Band-limited additive squares (no aliasing at all), a stick-noise
//  attack, the ROM's own natural decay, then crushed to 6 bits.
// ---------------------------------------------------------------------
struct CymbalRom {
    static constexpr double kRate = 32000.0;
    std::vector<float> data;

    // squares = band-limited additive squares (909/808 metal), else pure sines (bells, FM-ish ring);
    // noiseMix blends in filtered white noise (crash, sizzle, noise hats); stick = attack noise burst
    static CymbalRom build(const std::array<float, 6>& freqs, float bpHz, float q, float hpHz,
                           float seconds, float naturalTau, uint32_t seed,
                           float noiseMix = 0.f, bool squares = true, float stickAmt = 0.6f) {
        CymbalRom r;
        const size_t n = (size_t)(seconds * kRate);
        r.data.assign(n, 0.f);
        Noise nz; nz.seed(seed);
        std::array<float, 6> ph{};
        for (size_t i = 0; i < 6; ++i) ph[i] = (float)(nz.next() * 0.5 + 0.5);
        const float nyq = (float)(kRate * 0.45);
        // additive, band-limited squares
        for (size_t o = 0; o < 6; ++o) {
            const float f = freqs[o];
            if (f <= 0.f) continue;
            for (int k = 1; (float)k * f < nyq; k += 2) {
                if (!squares && k > 1) break;
                const float a = (squares ? 4.f / (kPi * (float)k) : 1.f) / 6.f;
                const double w = kTau * (double)k * f / kRate;
                const double p0 = kTau * (double)k * ph[o];
                // rotate a phasor instead of calling sin() per sample
                double c = std::cos(w), s = std::sin(w), re = std::cos(p0), im = std::sin(p0);
                for (size_t i = 0; i < n; ++i) {
                    r.data[i] += a * (float)im;
                    const double nre = re * c - im * s; im = re * s + im * c; re = nre;
                }
            }
        }
        Svf bp, hp, stick; bp.set(bpHz, q, kRate); hp.set(hpHz, 0.7f, kRate); stick.set(bpHz * 1.2f, 0.8f, kRate);
        const float dc = decayCoef(naturalTau, kRate), sc = decayCoef(0.004f, kRate);
        float env = 1.f, se = stickAmt, peak = 1e-9f;
        for (size_t i = 0; i < n; ++i) {
            const float w = nz.next();
            bp.tick(r.data[i] * (1.f - noiseMix) + w * noiseMix * 0.8f); hp.tick(bp.bp);
            stick.tick(w);
            r.data[i] = (hp.hp * 3.f + stick.bp * se) * env;
            env *= dc; se *= sc;
            peak = std::max(peak, std::fabs(r.data[i]));
        }
        for (auto& x : r.data) x /= peak;                                // full-resolution ROM
        return r;
    }

    static const CymbalRom& hat() {
        static const CymbalRom r = build({ 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f }, 8800.f, 0.9f, 6500.f, 1.4f, 0.32f, 0xA7u);
        return r;
    }
    static const CymbalRom& ride() {
        static const CymbalRom r = build({ 151.f, 202.5f, 264.4f, 329.6f, 436.0f, 515.3f }, 5600.f, 0.55f, 3200.f, 3.5f, 1.1f, 0xB1Du);
        return r;
    }
    // ---- the extra models ----
    static const CymbalRom& hat808() {     // brighter, rawer square metal, no stick
        static const CymbalRom r = build({ 205.3f, 304.4f, 369.6f, 522.7f, 540.f, 800.f }, 7200.f, 0.55f, 6000.f, 1.2f, 0.22f, 0x808u, 0.f, true, 0.f);
        return r;
    }
    static const CymbalRom& hatNoise() {   // white-noise hat, like a cheap drum box
        static const CymbalRom r = build({ 0, 0, 0, 0, 0, 0 }, 9500.f, 0.5f, 7000.f, 1.2f, 0.25f, 0x4E0u, 1.f, true, 0.3f);
        return r;
    }
    static const CymbalRom& hatMetal() {   // inharmonic sine ring, FM-ish, industrial
        static const CymbalRom r = build({ 1210.f, 1793.f, 2613.f, 3301.f, 4877.f, 6453.f }, 6000.f, 0.4f, 2800.f, 1.4f, 0.35f, 0x3E7u, 0.f, false, 0.25f);
        return r;
    }
    static const CymbalRom& crash() {
        static const CymbalRom r = build({ 196.f, 263.f, 344.f, 428.f, 567.f, 670.f }, 6500.f, 0.4f, 3500.f, 4.5f, 1.4f, 0xC4Au, 0.25f, true, 0.5f);
        return r;
    }
    static const CymbalRom& bell() {        // ride bell: few strong partials
        static const CymbalRom r = build({ 612.f, 1438.f, 1872.f, 2604.f, 3470.f, 4211.f }, 3000.f, 0.3f, 500.f, 2.5f, 0.9f, 0xBE1u, 0.f, false, 0.35f);
        return r;
    }
    static const CymbalRom& sizzle() {
        static const CymbalRom r = build({ 151.f, 202.5f, 264.4f, 329.6f, 436.0f, 515.3f }, 7000.f, 0.5f, 4000.f, 4.f, 1.2f, 0x512u, 0.18f, true, 0.35f);
        return r;
    }
};

// ---------------------------------------------------------------------
//  ROM VOICE  (closed hat, open hat, ride)
// ---------------------------------------------------------------------
struct RomVoice {
    VoiceParams p;
    const CymbalRom* rom = nullptr;
    std::array<const CymbalRom*, kNumModels> roms{};
    std::array<float, kNumModels> decayMul{ 1.f, 1.f, 1.f, 1.f };
    double sr = 48000, pos = 1e12, step = 1;
    float minDecay = 0.02f, maxDecay = 0.4f;
    int   romBits = 0;            // 0 = clean full resolution, 6 = original 909 ROM grit
    float env = 0, envC = 0, gain = 0, chokeC = 0, reconHz = 12000.f;
    bool choking = false;
    Svf recon; OnePole dcb;

    void prepare(double s, const CymbalRom& r, float dMin, float dMax, float recon_) {
        sr = s; rom = &r; minDecay = dMin; maxDecay = dMax; reconHz = recon_;
        chokeC = decayCoef(0.003f, sr);
        dcb.setLP(20.f, sr);
        pos = 1e12;
    }
    void trigger(float vel, float tuneMod, float decayMod) {
        const int m = std::clamp(p.model, 0, kNumModels - 1);
        if (roms[(size_t)m]) rom = roms[(size_t)m];
        const float t = semis((p.tune - 0.5f) * 8.f + tuneMod * kRideTuneSemis) * p.tolTune;
        step = CymbalRom::kRate / sr * t;
        recon.set(std::min(reconHz * t * (0.7f + 0.6f * p.tone), (float)(sr * 0.45)), 0.6f, sr);
        const float d = (minDecay + (maxDecay - minDecay) * p.decay * p.decay) * p.tolDecay * decayMul[(size_t)m];
        envC = decayCoef(d * (1.f + decayMod * kRideDecayPct), sr);
        env = 1.f; choking = false; pos = 0.0;
        gain = velGain(vel) * p.level;
    }
    void choke() { choking = true; }
    float process() {
        if (!rom || env < 1e-5f) return 0.f;
        const size_t n = rom->data.size();
        if (pos >= (double)(n - 3)) { env = 0.f; return 0.f; }
        // 4-point Hermite interpolation through the 6-bit ROM
        const size_t i = (size_t)pos; const float fr = (float)(pos - (double)i);
        const float* d = rom->data.data();
        float xm1 = i ? d[i - 1] : d[i], x0 = d[i], x1 = d[i + 1], x2 = d[i + 2];
        if (romBits > 0) {
            const float q = (float)((1 << (romBits - 1)) - 1);
            xm1 = std::round(xm1 * q) / q; x0 = std::round(x0 * q) / q;
            x1 = std::round(x1 * q) / q;   x2 = std::round(x2 * q) / q;
        }
        const float c1 = 0.5f * (x1 - xm1), c2 = xm1 - 2.5f * x0 + 2.f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        const float s = ((c3 * fr + c2) * fr + c1) * fr + x0;
        pos += step;
        recon.tick(s);
        const float y = dcb.hp(recon.lp) * env;
        env *= choking ? chokeC : envC;
        return y * gain;
    }
};

// ---------------------------------------------------------------------
//  Kit — all ten voices
// ---------------------------------------------------------------------
struct Kit {
    Kick  kick;
    Snare snare;
    Clap  clap;
    Rim   rim;
    Tom   tomLo, tomMid, tomHi;
    RomVoice ch, oh, ride;

    void prepare(double sr) {
        kick.prepare(sr); snare.prepare(sr); clap.prepare(sr); rim.prepare(sr);
        tomLo.prepare(sr, 82.f, 0x701u); tomMid.prepare(sr, 123.f, 0x702u); tomHi.prepare(sr, 174.f, 0x703u);
        // reconstruction filters a bit lower than "open": the sparkle stays, the hiss goes
        ch.prepare  (sr, CymbalRom::hat(),  0.012f, 0.11f, 12500.f);
        oh.prepare  (sr, CymbalRom::hat(),  0.10f,  0.75f, 12500.f);
        ride.prepare(sr, CymbalRom::ride(), 0.45f,  2.6f,  11000.f);
        // all ROMs are built here (once), never on the audio thread
        ch.roms = oh.roms = { &CymbalRom::hat(), &CymbalRom::hat808(), &CymbalRom::hatNoise(), &CymbalRom::hatMetal() };
        ride.roms = { &CymbalRom::ride(), &CymbalRom::crash(), &CymbalRom::bell(), &CymbalRom::sizzle() };
        ride.decayMul = { 1.f, 1.5f, 0.6f, 1.2f };
    }

    VoiceParams& params(int inst) {
        switch (inst) {
            case KICK: return kick.p;   case SNARE: return snare.p; case CLAP: return clap.p;
            case RIM: return rim.p;     case TOM_LO: return tomLo.p; case TOM_MID: return tomMid.p;
            case TOM_HI: return tomHi.p; case CH: return ch.p;      case OH: return oh.p;
            default: return ride.p;
        }
    }

    // 0 = clean cymbals, 6 = the original 909's 6-bit ROM grit
    void setRomBits(int bits) { ch.romBits = oh.romBits = ride.romBits = bits; }

    // Every machine gets its own small "factory" errors, like two real 909s.
    void setTolerance(uint64_t seed) {
        Noise n; n.seed((uint32_t)(seed ^ (seed >> 32)) | 1u);
        for (int i = 0; i < kNumInstruments; ++i) {
            auto& p = params(i);
            p.tolTune  = 1.f + n.next() * 0.007f;   // ±0.7 %
            p.tolDecay = 1.f + n.next() * 0.04f;    // ±4 %
        }
    }

    void trigger(int inst, float vel01, float tuneMod, float decayMod) {
        switch (inst) {
            case KICK:    kick.trigger(vel01, tuneMod, decayMod); break;
            case SNARE:   snare.trigger(vel01, tuneMod, decayMod); break;
            case CLAP:    clap.trigger(vel01, tuneMod, decayMod); break;
            case RIM:     rim.trigger(vel01, tuneMod, decayMod); break;
            case TOM_LO:  tomLo.trigger(vel01, tuneMod, decayMod); break;
            case TOM_MID: tomMid.trigger(vel01, tuneMod, decayMod); break;
            case TOM_HI:  tomHi.trigger(vel01, tuneMod, decayMod); break;
            case CH:      ch.trigger(vel01, tuneMod, decayMod); oh.choke(); break; // 909 choke
            case OH:      oh.trigger(vel01, tuneMod, decayMod); break;
            case RIDE:    ride.trigger(vel01, tuneMod, decayMod); break;
            default: break;
        }
    }

    void process(float out[kNumInstruments]) {
        out[KICK]    = kick.process();
        out[SNARE]   = snare.process();
        out[CLAP]    = clap.process();
        out[RIM]     = rim.process();
        out[TOM_LO]  = tomLo.process();
        out[TOM_MID] = tomMid.process();
        out[TOM_HI]  = tomHi.process();
        out[CH]      = ch.process();
        out[OH]      = oh.process();
        out[RIDE]    = ride.process();
    }
};

} // namespace motstand
