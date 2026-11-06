// =====================================================================
//  MASKINHALLEN : MOTSTÅND — UNDERGÅNG (the end of the world, step 4f)
//  Phunkshunz AUDIO
//
//    Muller   hard-techno rumble: the kick is fed into a huge dark hall,
//             low-passed, driven and ducked by the next kick
//    Motor    a V8 with cross-plane firing: uneven pulses, left bank in
//             the left speaker, right bank in the right, misfires (RASSEL)
//    Storm    sandstorm (noise through drifting resonant band-passes)
//             + LARM, an air-raid siren far away (also fed to the hall)
//
//  All of it is off by default. Real-time safe; allocation in prepare().
// =====================================================================
#pragma once
#include "Fx.h"
#include "Vinkelslip.h"
#include "MotstandCore.h"

namespace motstand {

// ---------------------------------------------------------------------
//  MULLER
// ---------------------------------------------------------------------
struct Muller {
    float amount = 0.f;     // 0..1 send from the kick
    float depth = 6.f;      // hall time, seconds
    float freq = 120.f;     // low-pass cut-off, Hz
    float dirt = 0.4f;      // drive 0..1
    float pump = 0.85f;     // how hard the next kick ducks the rumble

    void prepare(double s) {
        sr = s;
        verb.lowCutOn = false; verb.size = 1.f; verb.damping = 0.9f; verb.predelayMs = 4.f;
        verb.prepare(sr);
        hp.set(28.f, 0.7f, sr);
        duck.prepare(sr);
        lastFreq = -1.f; idle = 0;
    }
    void onKick(float vel) { duck.trigger(vel); }

    float process(float kickIn) {
        duck.amount = pump;
        const float g = duck.next();
        if (amount <= 0.0001f && idle > (int)(sr * (depth + 1.0))) return 0.f;   // fully decayed: skip
        idle = amount <= 0.0001f ? idle + 1 : 0;
        if (freq != lastFreq) { for (auto& f : lp) f.set(freq, 0.75f, sr); lastFreq = freq; }
        verb.decay = depth;
        float l, r;
        const float x = kickIn * amount;
        verb.process(x, x, l, r);
        float m = 0.5f * (l + r);
        for (auto& f : lp) { f.tick(m); m = f.lp; }
        hp.tick(m); m = hp.hp;
        const float drive = 2.f + dirt * 14.f;
        m = fastTanh(m * drive * 3.f) / std::sqrt(drive) * 1.1f;
        return m * g;            // mono: lows belong in the middle of the club
    }

private:
    double sr = 48000;
    Varvshall verb;
    std::array<Svf, 2> lp;
    Svf hp;
    Pump duck;
    float lastFreq = -1.f;
    int idle = 0;
};

// ---------------------------------------------------------------------
//  MOTOR — V8, cross-plane
// ---------------------------------------------------------------------
struct Motor {
    float gas = 0.f;        // level 0..1
    float rpm = 1200.f;     // 500..7000
    float tone = 140.f;     // exhaust resonance, Hz
    float rattle = 0.3f;    // irregularity + misfires 0..1

    void prepare(double s) {
        sr = s; n.seed(0x5EEDu);
        for (auto& b : bank) b.set(tone, 5.f, sr);
        out.setLP(2400.f, sr);
        rpmNow = rpm; ph = 0.0; next = 0; huntPh = 0.f; lastTone = -1.f;
        thump.fill(0.f); thumpPh.fill(0.f);
        thumpC = decayCoef(0.018f, sr); outR.setLP(2400.f, sr);
    }

    std::array<long, 2> fired{};    // firings per bank (for tests and a future rev meter)

    void process(float& L, float& R) {
        if (gas <= 0.0001f) { L = R = 0.f; return; }
        if (tone != lastTone) { for (auto& b : bank) b.set(tone, 5.f, sr); lastTone = tone; }
        // the idle hunts a little, like a real engine
        huntPh += 0.6f / (float)sr; if (huntPh >= 1.f) huntPh -= 1.f;
        rpmNow += (rpm - rpmNow) * 0.0004f;
        const float r = rpmNow * (1.f + 0.015f * std::sin(kTau * huntPh) * (0.5f + rattle));
        // one cycle = two revolutions = eight firings
        ph += r / 60.0 / 2.0 / sr;
        if (ph >= 1.0) { ph -= 1.0; next = 0; }
        float exc[2] = { 0.f, 0.f };
        while (next < 8 && ph >= (next + kOffset[next] * (0.4 + rattle)) / 8.0) {
            const float u = (float)(n.next() * 0.5 + 0.5);
            const bool misfire = u < rattle * 0.12f;
            if (!misfire) {
                const float a = 1.f - rattle * 0.6f * (float)(n.next() * 0.5 + 0.5);
                const int side = kBank[next];
                exc[side] += a;
                ++fired[(size_t)side];
                thump[(size_t)side] = std::max(thump[(size_t)side], a);
            }
            ++next;
        }
        float y[2];
        for (int sd = 0; sd < 2; ++sd) {
            bank[(size_t)sd].tick(exc[sd] * 4.f);
            thumpPh[(size_t)sd] += tone * 0.5f / (float)sr; if (thumpPh[(size_t)sd] >= 1.f) thumpPh[(size_t)sd] -= 1.f;
            const float body = std::sin(kTau * thumpPh[(size_t)sd]) * thump[(size_t)sd];
            thump[(size_t)sd] *= thumpC;
            y[sd] = bank[(size_t)sd].bp + body * 0.6f;
        }
        const float drive = 2.f + gas * 4.f;
        const float mono = 0.5f * (y[0] + y[1]);
        L = out.lp(fastTanh((0.7f * y[0] + 0.3f * mono) * drive)) * gas * 0.45f;
        R = fastTanh((0.7f * y[1] + 0.3f * mono) * drive) * gas * 0.45f;
        R = outR.lp(R);
    }
private:
    // cross-plane firing: uneven spacing inside the cycle, and which bank (L/R) fires
    static constexpr float kOffset[8] = { 0.f, 0.22f, -0.12f, 0.30f, 0.05f, -0.20f, 0.35f, -0.08f };
    static constexpr int   kBank[8]   = { 0, 1, 1, 0, 1, 0, 0, 1 };
    double sr = 48000, ph = 0.0;
    int next = 0;
    float rpmNow = 1200.f, huntPh = 0.f, lastTone = -1.f, thumpC = 0.999f;
    std::array<Svf, 2> bank;
    std::array<float, 2> thump{}, thumpPh{};
    OnePole out, outR;
    Noise n;
};

// ---------------------------------------------------------------------
//  STORM + LARM
// ---------------------------------------------------------------------
struct Storm {
    float amount = 0.f;     // sand level 0..1
    float sweep = 0.3f;     // how fast the wind moves 0..1
    float color = 1500.f;   // centre frequency, Hz
    float alarm = 0.f;      // siren level 0..1

    void prepare(double s) {
        sr = s; nl.seed(0xD05Eu); nr.seed(0x5A2Du); walk.prepare(sr, 0x57u);
        wind.setLP(240.f, sr); windR.setLP(240.f, sr);
        w = 0.f; count = 0; sirenPh = 0.f; o1 = o2 = 0.f;
        bpL.set(color, 1.2f, sr); bpR.set(color * 1.1f, 1.2f, sr);
    }

    // returns the siren separately so it can be sent far away into the hall
    void process(float& L, float& R, float& siren) {
        L = R = siren = 0.f;
        if (amount > 0.0001f) {
            if (++count >= 32) {                       // move the wind every 32 samples
                count = 0;
                walk.stepMin = 0.002f + sweep * 0.02f; walk.stepMax = walk.stepMin * 3.f;
                walk.advance(32);
                w += (walk.onTrigger() - w) * 0.2f;
                const float c = std::clamp(color * std::exp2(w * 1.6f), 80.f, (float)(sr * 0.4));
                bpL.set(c, 1.2f, sr); bpR.set(c * 1.13f, 1.2f, sr);
            }
            const float a = nl.next(), b = nr.next();
            bpL.tick(a); bpR.tick(b);
            const float gust = 0.6f + 0.4f * (w * 0.5f + 0.5f);
            L = (bpL.bp * 0.7f + wind.lp(a) * 1.6f) * amount * gust * 0.5f;
            R = (bpR.bp * 0.7f + windR.lp(b) * 1.6f) * amount * gust * 0.5f;
        }
        if (alarm > 0.0001f) {
            // air-raid siren: rises over ~3 s, holds, falls, ~9 s per cycle
            sirenPh += 1.f / (9.f * (float)sr); if (sirenPh >= 1.f) sirenPh -= 1.f;
            const float t = sirenPh < 0.35f ? sirenPh / 0.35f : sirenPh < 0.55f ? 1.f : std::max(0.f, 1.f - (sirenPh - 0.55f) / 0.45f);
            const float shape = t * t * (3.f - 2.f * t);
            const float f = 180.f + 420.f * shape;
            o1 += f / (float)sr; if (o1 >= 1.f) o1 -= 1.f;
            o2 += f * 1.015f / (float)sr; if (o2 >= 1.f) o2 -= 1.f;
            auto osc = [](float ph) { const float x = kTau * ph; return std::sin(x) + 0.25f * std::sin(3.f * x); };
            siren = (osc(o1) + osc(o2)) * 0.5f * alarm * 0.22f * (0.3f + 0.7f * shape);
        }
    }

private:
    double sr = 48000;
    Noise nl, nr;
    DrunkRider walk;
    Svf bpL, bpR;
    OnePole wind, windR;
    float w = 0.f, sirenPh = 0.f, o1 = 0.f, o2 = 0.f;
    int count = 0;
};

} // namespace motstand
