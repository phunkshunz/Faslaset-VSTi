// =====================================================================
//  MASKINHALLEN : MOTSTÅND  —  909-style voices (step 2)
//  Phunkshunz AUDIO
//
//  Pure C++17, header-only, real-time safe (no allocation, no locks).
//
//  Ten voices, modelled on how the TR-909 makes sound:
//    KICK          sine + pitch drop + click, soft-shaped like the 909 VCA
//    SNARE         two tuned oscillators + noise ("snappy")
//    CLAP          bandpassed noise, three fast bursts + tail
//    RIM           two short pings, very dry
//    TOM L/M/H     sine with pitch drop + a little noise
//    CH / OH       six metallic squares -> 6-bit quantised (909 hats were
//    RIDE          6-bit samples) -> VCA envelope. CH chokes OH.
//
//  Each trigger takes tuneMod/decayMod (-1..1) from the DrunkRider,
//  so no two hits are identical.
// =====================================================================
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

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

// Exponential decay with time constant in seconds
inline float decayCoef(float seconds, double sr) {
    return std::exp(-1.f / (std::max(0.0005f, seconds) * (float)sr));
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
};

// How much the DrunkRider may move a hit at full RIDE amount
constexpr float kRideTuneSemis = 0.25f;   // ±25 cents
constexpr float kRideDecayPct  = 0.08f;   // ±8 % decay

// ---------------------------------------------------------------------
//  KICK
// ---------------------------------------------------------------------
struct Kick {
    VoiceParams p;
    double sr = 48000;
    float phase = 0, freq = 50, amp = 0, ampC = 0, pEnv = 0, pC = 0;
    float click = 0, clickC = 0, gain = 0;
    Svf clickF; Noise n;

    void prepare(double s) { sr = s; clickF.set(3500, 0.7f, sr); n.seed(0x909A); }

    void trigger(float vel, float tuneMod, float decayMod) {
        freq  = (42.f + 24.f * p.tune) * semis(tuneMod * kRideTuneSemis);
        const float dec = (0.12f + 0.9f * p.decay * p.decay) * (1.f + decayMod * kRideDecayPct);
        ampC  = decayCoef(dec, sr);
        pC    = decayCoef(0.012f + 0.02f * (1.f - p.tone), sr);
        clickC= decayCoef(0.0015f, sr);
        amp = 1.f; pEnv = 1.f; click = 0.4f + 0.6f * p.tone;
        phase = 0.25f;                     // start on the peak: the 909 thump
        gain  = velGain(vel) * p.level;
    }

    float process() {
        if (amp < 1e-5f && click < 1e-5f) return 0.f;
        const float f = freq * (1.f + 3.5f * pEnv);   // ~2 octaves of drop
        phase += f / (float)sr; phase -= std::floor(phase);
        float body = std::sin(kTau * phase) * amp;
        body = std::tanh(body * 1.6f) / std::tanh(1.6f); // 909 waveshaping
        clickF.tick(n.next());
        const float c = clickF.bp * click * 0.6f;
        amp *= ampC; pEnv *= pC; click *= clickC;
        return (body + c) * gain;
    }
};

// ---------------------------------------------------------------------
//  SNARE  (tone = snappy amount)
// ---------------------------------------------------------------------
struct Snare {
    VoiceParams p;
    double sr = 48000;
    float ph1 = 0, ph2 = 0, f1 = 185, f2 = 330;
    float toneEnv = 0, toneC = 0, noiseEnv = 0, noiseC = 0, pEnv = 0, pC = 0, gain = 0;
    Svf hp, lp; Noise n;

    void prepare(double s) {
        sr = s; n.seed(0x5EA7);
        hp.set(1800, 0.7f, sr); lp.set(9000, 0.7f, sr);
    }
    void trigger(float vel, float tuneMod, float decayMod) {
        const float t = semis((p.tune - 0.5f) * 8.f + tuneMod * kRideTuneSemis);
        f1 = 180.f * t; f2 = 330.f * t;
        const float dm = 1.f + decayMod * kRideDecayPct;
        toneC  = decayCoef(0.045f * dm, sr);
        noiseC = decayCoef((0.06f + 0.25f * p.decay) * dm, sr);
        pC     = decayCoef(0.008f, sr);
        toneEnv = 1.f; noiseEnv = 1.f; pEnv = 1.f;
        gain = velGain(vel) * p.level;
    }
    float process() {
        if (toneEnv < 1e-5f && noiseEnv < 1e-5f) return 0.f;
        const float bend = 1.f + 0.4f * pEnv;
        ph1 += f1 * bend / (float)sr; ph1 -= std::floor(ph1);
        ph2 += f2 * bend / (float)sr; ph2 -= std::floor(ph2);
        // 909 snare oscillators are closer to triangles than sines
        auto tri = [](float ph) { return 4.f * std::fabs(ph - 0.5f) - 1.f; };
        const float body = (tri(ph1) * 0.6f + tri(ph2) * 0.4f) * toneEnv;
        hp.tick(n.next()); lp.tick(hp.hp);
        const float snap = lp.lp * noiseEnv;
        const float mix = body * (1.f - 0.6f * p.tone) + snap * (0.4f + 0.9f * p.tone);
        toneEnv *= toneC; noiseEnv *= noiseC; pEnv *= pC;
        return mix * gain;
    }
};

// ---------------------------------------------------------------------
//  CLAP  (three bursts 11 ms apart, then the tail)
// ---------------------------------------------------------------------
struct Clap {
    VoiceParams p;
    double sr = 48000;
    int   counter = 1 << 30, burstLen = 0;
    float burstEnv = 0, burstC = 0, tail = 0, tailC = 0, gain = 0;
    Svf bp; Noise n;

    void prepare(double s) { sr = s; n.seed(0xC1A9); burstLen = (int)(0.011 * sr); }
    void trigger(float vel, float tuneMod, float decayMod) {
        bp.set(1000.f * semis((p.tune - 0.5f) * 10.f + tuneMod * kRideTuneSemis), 1.4f, sr);
        burstC = decayCoef(0.0035f, sr);
        tailC  = decayCoef((0.08f + 0.35f * p.decay) * (1.f + decayMod * kRideDecayPct), sr);
        counter = 0; burstEnv = 1.f; tail = 0.f;
        gain = velGain(vel) * p.level * 1.6f;
    }
    float process() {
        if (counter > burstLen * 3 && tail < 1e-5f) return 0.f;
        if (counter < burstLen * 3 && counter % burstLen == 0) burstEnv = 1.f;
        if (counter == burstLen * 3) tail = 0.7f;
        bp.tick(n.next());
        const float out = bp.bp * (burstEnv + tail);
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
    float ph1 = 0, ph2 = 0, f1 = 500, f2 = 1700, env = 0, envC = 0, gain = 0;
    Svf hp;

    void prepare(double s) { sr = s; hp.set(350, 0.7f, sr); }
    void trigger(float vel, float tuneMod, float decayMod) {
        const float t = semis((p.tune - 0.5f) * 8.f + tuneMod * kRideTuneSemis);
        f1 = 500.f * t; f2 = 1700.f * t;
        envC = decayCoef((0.006f + 0.02f * p.decay) * (1.f + decayMod * kRideDecayPct), sr);
        env = 1.f; ph1 = ph2 = 0.f;
        gain = velGain(vel) * p.level;
    }
    float process() {
        if (env < 1e-5f) return 0.f;
        ph1 += f1 / (float)sr; ph1 -= std::floor(ph1);
        ph2 += f2 / (float)sr; ph2 -= std::floor(ph2);
        float x = (std::sin(kTau * ph1) + 0.8f * std::sin(kTau * ph2)) * env;
        hp.tick(std::tanh(x * 2.f));
        env *= envC;
        return hp.hp * gain * 0.7f;
    }
};

// ---------------------------------------------------------------------
//  TOM  (base frequency set per instance: low / mid / high)
// ---------------------------------------------------------------------
struct Tom {
    VoiceParams p;
    double sr = 48000;
    float baseHz = 100;
    float phase = 0, freq = 100, amp = 0, ampC = 0, pEnv = 0, pC = 0;
    float nEnv = 0, nC = 0, gain = 0;
    Svf nf; Noise n;

    void prepare(double s, float base, uint32_t seed) {
        sr = s; baseHz = base; n.seed(seed);
    }
    void trigger(float vel, float tuneMod, float decayMod) {
        freq = baseHz * semis((p.tune - 0.5f) * 12.f + tuneMod * kRideTuneSemis);
        ampC = decayCoef((0.12f + 0.6f * p.decay) * (1.f + decayMod * kRideDecayPct), sr);
        pC   = decayCoef(0.05f, sr);
        nC   = decayCoef(0.02f, sr);
        nf.set(freq * 4.f, 0.8f, sr);
        amp = 1.f; pEnv = 1.f; nEnv = 0.3f * p.tone; phase = 0.f;
        gain = velGain(vel) * p.level;
    }
    float process() {
        if (amp < 1e-5f) return 0.f;
        phase += freq * (1.f + 0.6f * pEnv) / (float)sr; phase -= std::floor(phase);
        nf.tick(n.next());
        const float out = std::sin(kTau * phase) * amp + nf.lp * nEnv;
        amp *= ampC; pEnv *= pC; nEnv *= nC;
        return out * gain;
    }
};

// ---------------------------------------------------------------------
//  METAL  (closed hat, open hat, ride)
//  Six detuned squares -> bandpass -> 6-bit quantise + ~32 kHz
//  sample-and-hold (the 909's cheap cymbal ROM) -> VCA envelope.
// ---------------------------------------------------------------------
struct Metal {
    VoiceParams p;
    double sr = 48000;
    std::array<float, 6> ratios{ 1.f, 1.4826f, 1.8003f, 2.5461f, 2.6304f, 3.8968f };
    std::array<float, 6> ph{};
    float baseHz = 205.3f, filterHz = 8000.f, q = 0.9f;
    float minDecay = 0.02f, maxDecay = 0.4f;
    float env = 0, envC = 0, gain = 0, held = 0, holdPhase = 0, holdStep = 1;
    float chokeC = 0; bool choking = false;
    Svf bp, hp;

    void prepare(double s, float base, float filt, float qv, float dMin, float dMax) {
        sr = s; baseHz = base; filterHz = filt; q = qv; minDecay = dMin; maxDecay = dMax;
        holdStep = (float)(32000.0 / sr);
        hp.set(filt * 0.75f, 0.7f, sr);
        chokeC = decayCoef(0.004f, sr);
        for (size_t i = 0; i < ph.size(); ++i) ph[i] = 0.13f * (float)i;
    }
    void trigger(float vel, float tuneMod, float decayMod) {
        const float t = semis((p.tune - 0.5f) * 6.f + tuneMod * kRideTuneSemis);
        bp.set(filterHz * t * (0.8f + 0.4f * p.tone), q, sr);
        const float d = minDecay + (maxDecay - minDecay) * p.decay * p.decay;
        envC = decayCoef(d * (1.f + decayMod * kRideDecayPct), sr);
        env = 1.f; choking = false;
        gain = velGain(vel) * p.level;
        tuneScale = t;
    }
    void choke() { choking = true; }
    float process() {
        if (env < 1e-5f) return 0.f;
        float sq = 0.f;
        for (size_t i = 0; i < ph.size(); ++i) {
            ph[i] += baseHz * ratios[i] * tuneScale / (float)sr;
            ph[i] -= std::floor(ph[i]);
            sq += ph[i] < 0.5f ? 1.f : -1.f;
        }
        bp.tick(sq * (1.f / 6.f));
        hp.tick(bp.bp);
        // cheap ROM: sample-and-hold at ~32 kHz, then 6-bit quantise
        holdPhase += holdStep;
        if (holdPhase >= 1.f) {
            holdPhase -= 1.f;
            const float x = std::clamp(hp.hp * 3.f, -1.f, 1.f);
            held = std::round(x * 31.f) / 31.f;
        }
        const float out = held * env;
        env *= choking ? chokeC : envC;
        return out * gain;
    }
private:
    float tuneScale = 1.f;
};

// ---------------------------------------------------------------------
//  Kit — all ten voices. process() writes one sample per instrument,
//  so the processor can send each one to its own output bus.
// ---------------------------------------------------------------------
struct Kit {
    Kick  kick;
    Snare snare;
    Clap  clap;
    Rim   rim;
    Tom   tomLo, tomMid, tomHi;
    Metal ch, oh, ride;

    void prepare(double sr) {
        kick.prepare(sr); snare.prepare(sr); clap.prepare(sr); rim.prepare(sr);
        tomLo.prepare(sr, 82.f, 0x701u); tomMid.prepare(sr, 123.f, 0x702u); tomHi.prepare(sr, 174.f, 0x703u);
        //            base Hz  filter  Q     min dec  max dec
        ch.prepare  (sr, 205.3f, 9000.f, 0.9f, 0.015f, 0.12f);
        oh.prepare  (sr, 205.3f, 8500.f, 0.9f, 0.12f,  0.9f);
        ride.prepare(sr, 151.0f, 5200.f, 0.6f, 0.5f,   2.8f);
        ride.ratios = { 1.f, 1.341f, 1.751f, 2.183f, 2.887f, 3.412f };
    }

    VoiceParams& params(int inst) {
        switch (inst) {
            case KICK: return kick.p;   case SNARE: return snare.p; case CLAP: return clap.p;
            case RIM: return rim.p;     case TOM_LO: return tomLo.p; case TOM_MID: return tomMid.p;
            case TOM_HI: return tomHi.p; case CH: return ch.p;      case OH: return oh.p;
            default: return ride.p;
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
