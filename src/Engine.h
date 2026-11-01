#pragma once
// VEVHUS: ett ånglok som står och pustar på perrongen.
// Inga samplingar. Allt räknas fram: filtrerat brus, resonanser och metallklick.
#include <cmath>
#include <cstdint>
#include <array>
#include <algorithm>

namespace pr
{
constexpr float kTwoPi = 6.28318530718f;

struct Rng
{
    uint32_t s = 0x9e3779b9u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // 0..1
    float bi() { return uni() * 2.0f - 1.0f; }                             // -1..1
};

// Tillståndsvariabelfilter (Cytomic). Ger lågpass, bandpass och högpass.
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    void set (float freq, float q, float sr)
    {
        freq = std::clamp (freq, 20.0f, sr * 0.45f);
        g = std::tan (3.14159265f * freq / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

enum Kind { Anga = 0, Pys, Knack, Vissla, NumKinds };

struct Params
{
    float tryck = 0.5f, fart = 0.5f, rytt = 0.4f, lacka = 0.2f, liv = 0.5f;
    float anga = 0.8f, pys = 0.5f, kompressor = 0.4f, knack = 0.5f, vissla = 0.2f;
    float ton = 0.5f, rum = 0.3f, volym = 0.8f;
    bool avgang = false;
};

struct Voice
{
    bool on = false;
    int kind = 0;
    int delay = 0;          // samplar kvar innan slaget börjar (släpande tajming)
    float env = 0, decay = 0.999f, attack = 1.0f, att = 0;
    float panL = 0.7f, panR = 0.7f, vel = 1;
    float f0 = 500, fEnd = 300, fNow = 500, glide = 0.999f, q = 1;
    float phase = 0, phaseInc = 0;      // vissla
    Svf f[3];
    float fr[3] { 1, 1, 1 };            // knack: tre metallresonanser
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v.on = false;
        lackaF.reset(); kompF.reset();
        for (auto& d : rumL) d = 0;
        for (auto& d : rumR) d = 0;
        rumPos = 0; lastStep = -1; avgangPos = 0;
    }

    // ppq: position i taktslag, playing: om transporten går.
    void process (const Params& p, double bpm, double ppq, bool playing, float* L, float* R, int n)
    {
        const double beatsPerSample = bpm / 60.0 / sr;
        // FART: hur många tjoff per taktslag. 0 = tomgång (bara pys och kompressor).
        const float fartEff = fartNu (p, playing);
        const int perBeat = fartEff < 0.08f ? 0 : (fartEff < 0.35f ? 1 : (fartEff < 0.7f ? 2 : 4));

        for (int i = 0; i < n; ++i)
        {
            const double pos = ppq + i * beatsPerSample;
            if (playing)
            {
                const long step = (long) std::floor (pos * 4.0);   // sextondelar
                if (step != lastStep)
                {
                    lastStep = step;
                    onStep (p, step, perBeat, bpm);
                }
                avgangPos = pos;
            }
            else if (rng.uni() < 1.0f / (sr * 0.9f)) // tomgång utan transport: pys då och då
                trig (p, Pys, 0.4f + 0.4f * rng.uni(), 0);

            float l = 0, r = 0;
            renderVoices (p, l, r);
            renderKompressor (p, bpm, pos, l, r);
            renderLacka (p, l, r);
            rum (p, l, r);
            const float g = p.volym * 0.7f;
            L[i] = std::tanh (l * g);
            R[i] = std::tanh (r * g);
        }
    }

    // MIDI: spela ett slag för hand (C=ånga, D=pys, E=knack, F=vissla)
    void hit (const Params& p, int kind, float vel) { trig (p, kind, vel, 0.0f); }

private:
    float fartNu (const Params& p, bool playing)
    {
        if (! p.avgang) return p.fart;
        if (! playing) return 0.0f;
        // AVGÅNG: loket drar igång över 8 takter (32 slag) och accelererar till FART
        const float t = (float) std::clamp (avgangPos / 32.0, 0.0, 1.0);
        return p.fart * t * t;
    }

    void onStep (const Params& p, long step, int perBeat, double bpm)
    {
        const int inBeat = (int) (step & 3);
        const int inBar = (int) (step & 15);
        const float stepSamples = (float) (sr * 60.0 / bpm / 4.0);

        // ÅNGA: tjoff på hjulets varv, med betoning 1 0.55 0.8 0.5
        const bool chuff = perBeat == 4 || (perBeat == 2 && (inBeat == 0 || inBeat == 2)) || (perBeat == 1 && inBeat == 0);
        if (chuff && p.anga > 0.01f)
        {
            static const float acc[4] { 1.0f, 0.55f, 0.8f, 0.5f };
            trig (p, Anga, acc[(step >> (perBeat == 4 ? 0 : 1)) & 3] * p.anga, stepSamples);
        }
        // KNACK: på offbeats, oftare med RYTT. Ibland en misständning (tystnad).
        if (p.knack > 0.01f && (inBeat == 1 || inBeat == 3) && rng.uni() < 0.15f + 0.75f * p.rytt)
            trig (p, Knack, (0.5f + 0.5f * rng.uni()) * p.knack, stepSamples);
        if (p.knack > 0.01f && inBeat == 2 && rng.uni() < 0.35f * p.rytt)
            trig (p, Knack, 0.4f * p.knack, stepSamples);
        // PYS: ventilen släpper, oftast på sista sextondelen i takten, oftare med högt TRYCK
        if (p.pys > 0.01f && (inBar == 15 || inBar == 7) && rng.uni() < 0.25f + 0.6f * p.tryck)
            trig (p, Pys, (0.6f + 0.4f * rng.uni()) * p.pys, stepSamples);
        // VISSLA: sällsynt, på ettan i en takt
        if (p.vissla > 0.01f && inBar == 0 && rng.uni() < 0.35f * p.vissla)
            trig (p, Vissla, 0.8f * p.vissla, stepSamples);
    }

    Voice* freeVoice()
    {
        Voice* quietest = &voices[0];
        for (auto& v : voices)
        {
            if (! v.on) return &v;
            if (v.env < quietest->env) quietest = &v;
        }
        return quietest;
    }

    void trig (const Params& p, int kind, float vel, float stepSamples)
    {
        auto& v = *freeVoice();
        v = Voice();
        v.on = true; v.kind = kind;
        // LIV: anslag, panorering, ton och tajming varierar per slag
        v.vel = std::clamp (vel * (1.0f + 0.5f * p.liv * rng.bi()), 0.0f, 1.5f);
        const float pan = std::clamp (0.5f + 0.45f * p.liv * rng.bi() + (kind == Knack ? 0.15f * rng.bi() : 0.0f), 0.0f, 1.0f);
        v.panL = std::cos (pan * 1.5707963f); v.panR = std::sin (pan * 1.5707963f);
        const float tonSemi = (p.ton - 0.5f) * 24.0f + 4.0f * p.liv * rng.bi();
        const float tonMul = std::pow (2.0f, tonSemi / 12.0f);
        v.delay = (int) (stepSamples * 0.18f * p.liv * rng.uni());   // släpar, rusar aldrig
        const float tr = p.tryck;

        switch (kind)
        {
            case Anga:   // brusstöt genom en resonans som faller i ton
                v.f0 = (320.0f + 700.0f * tr) * tonMul; v.fEnd = v.f0 * 0.45f; v.q = 1.4f + 2.5f * tr;
                v.attack = 1.0f / (0.004f * sr);
                v.decay = std::exp (-1.0f / ((0.07f + 0.18f * (1.0f - p.fart) + 0.05f * rng.uni()) * sr));
                v.glide = std::exp (-1.0f / (0.06f * sr));
                break;
            case Pys:    // ljust brus som klingar ut långsamt
                v.f0 = (3500.0f + 3000.0f * tr) * (0.85f + 0.3f * rng.uni()); v.fEnd = v.f0 * 0.7f; v.q = 0.7f;
                v.attack = 1.0f / (0.03f * sr);
                v.decay = std::exp (-1.0f / ((0.35f + 1.1f * rng.uni()) * sr));
                v.glide = std::exp (-1.0f / (0.8f * sr));
                v.phaseInc = rng.uni() < 0.3f ? kTwoPi * (2400.0f + 1800.0f * rng.uni()) / sr : 0.0f; // liten vissling ibland
                break;
            case Knack:  // korta metalliska smällar: tre resonanser
            {
                static const float ratios[3] { 1.0f, 2.31f, 3.87f };
                const float base = (900.0f + 900.0f * rng.uni()) * tonMul;
                for (int k = 0; k < 3; ++k) { v.fr[k] = base * ratios[k]; v.f[k].set (v.fr[k], 18.0f + 20.0f * p.rytt, sr); }
                v.attack = 1.0f;
                v.decay = std::exp (-1.0f / ((0.025f + 0.05f * p.rytt) * sr));
                break;
            }
            case Vissla: // ångvisslan, glider upp till tonen
                v.f0 = 560.0f * tonMul; v.fEnd = v.f0; v.fNow = v.f0 * 0.82f;
                v.attack = 1.0f / (0.08f * sr);
                v.decay = std::exp (-1.0f / (0.9f * sr));
                v.glide = std::exp (-1.0f / (0.12f * sr));
                v.phaseInc = 0;
                break;
            default: break;
        }
        v.fNow = kind == Vissla ? v.fNow : v.f0;
        v.att = 0;
    }

    void renderVoices (const Params& p, float& l, float& r)
    {
        for (auto& v : voices)
        {
            if (! v.on) continue;
            if (v.delay > 0) { --v.delay; continue; }
            v.att = std::min (1.0f, v.att + v.attack);
            v.env = v.env * v.decay;
            if (v.att >= 1.0f && v.env == 0.0f) v.env = 1.0f;     // starta utklingningen när attacken är klar
            const float level = (v.att < 1.0f ? v.att : v.env) * v.vel;
            float out = 0, lp, bp, hp;
            const float noise = rng.bi();
            switch (v.kind)
            {
                case Anga:
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.f[0].set (v.fNow, v.q, sr);
                    v.f[0].tick (noise, lp, bp, hp);
                    v.f[1].set (v.fNow * 0.5f, 0.8f, sr);
                    { float l2, b2, h2; v.f[1].tick (noise, l2, b2, h2); out = (bp * 1.6f + l2 * 0.6f) * (0.6f + 0.6f * p.tryck); }
                    break;
                case Pys:
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.f[0].set (v.fNow, v.q, sr);
                    v.f[0].tick (noise, lp, bp, hp);
                    out = hp * 0.55f + bp * 0.35f;
                    if (v.phaseInc > 0) { v.phase += v.phaseInc; if (v.phase > kTwoPi) v.phase -= kTwoPi; out += 0.08f * std::sin (v.phase); }
                    break;
                case Knack:
                {
                    const float x = v.att < 1.0f || v.env > 0.9f ? noise : noise * 0.05f;
                    for (int k = 0; k < 3; ++k) { v.f[k].tick (x, lp, bp, hp); out += bp * (k == 0 ? 1.0f : 0.6f); }
                    out *= 4.0f / (18.0f + 20.0f * p.rytt);   // resonansen förstärker, kompensera
                    break;
                }
                case Vissla:
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.phase += kTwoPi * v.fNow / sr; if (v.phase > kTwoPi) v.phase -= kTwoPi;
                    out = 0.32f * std::sin (v.phase) + 0.12f * std::sin (2.0f * v.phase + 0.3f) + 0.06f * noise;
                    break;
                default: break;
            }
            out *= level;
            l += out * v.panL; r += out * v.panR;
            if (v.att >= 1.0f && v.env < 0.0005f) v.on = false;
        }
    }

    // KOMPRESSOR: lågt dunkande varv i takt (åttondelar), med övertoner och ojämn gång
    void renderKompressor (const Params& p, double bpm, double pos, float& l, float& r)
    {
        if (p.kompressor < 0.01f) return;
        const double cyc = pos * 2.0;                       // två varv per taktslag
        const float ph = (float) (cyc - std::floor (cyc));
        if (ph < lastKompPh) kompJitter = 0.04f * p.liv * rng.bi();
        lastKompPh = ph;
        const float hum = 48.0f + 4.0f * p.tryck + kompJitter * 40.0f;
        humPh += kTwoPi * hum / sr; if (humPh > kTwoPi) humPh -= kTwoPi;
        const float saw = std::sin (humPh) + 0.5f * std::sin (2 * humPh) + 0.25f * std::sin (3 * humPh);
        const float thump = std::exp (-ph * 9.0f);          // kolvslaget i varje varv
        float lp, bp, hp;
        kompF.set (180.0f + 900.0f * p.tryck * thump, 0.9f, sr);
        kompF.tick (saw * (0.35f + 0.65f * thump) + 0.15f * rng.bi() * thump, lp, bp, hp);
        const float out = lp * 0.5f * p.kompressor;
        l += out; r += out;
        (void) bpm;
    }

    // LÄCKA: ett ständigt väsande under allt
    void renderLacka (const Params& p, float& l, float& r)
    {
        if (p.lacka < 0.01f) return;
        float lp, bp, hp;
        lackaF.set (5200.0f + 2500.0f * p.tryck, 0.6f, sr);
        lackaF.tick (rng.bi(), lp, bp, hp);
        const float wobble = 0.8f + 0.2f * std::sin (lackaPh += 0.7f * kTwoPi / sr);
        const float out = hp * 0.12f * p.lacka * wobble;
        l += out * (0.8f + 0.2f * rng.uni()); r += out * (0.8f + 0.2f * rng.uni());
    }

    // RUM: kort stålhall, två korsade fördröjningar
    void rum (const Params& p, float& l, float& r)
    {
        if (p.rum < 0.01f) return;
        const int a = (rumPos + kRum - (int) (0.031f * sr)) % kRum, b = (rumPos + kRum - (int) (0.047f * sr)) % kRum;
        const float dl = rumL[(size_t) a], dr = rumR[(size_t) b];
        rumL[(size_t) rumPos] = l + dr * 0.55f;
        rumR[(size_t) rumPos] = r + dl * 0.55f;
        rumPos = (rumPos + 1) % kRum;
        l += dl * p.rum * 0.6f; r += dr * p.rum * 0.6f;
    }

    static constexpr int kRum = 8192;
    float sr = 48000;
    std::array<Voice, 24> voices;
    Rng rng;
    Svf lackaF, kompF;
    long lastStep = -1;
    double avgangPos = 0;
    float humPh = 0, lastKompPh = 0, kompJitter = 0, lackaPh = 0;
    std::array<float, kRum> rumL {}, rumR {};
    int rumPos = 0;
};
} // namespace pr
