#pragma once
// VEVHUS: ånglok, kompressor och knackande maskin. Inga samplingar, allt räknas fram:
// filtrerat brus, modala metallresonanser och ett eget rum (FDN).
#include <cmath>
#include <cstdint>
#include <array>
#include <vector>
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
    float ic1 = 0, ic2 = 0, a1 = 0, a2 = 0, a3 = 0, k = 1;
    void set (float freq, float q, float sr)
    {
        freq = std::clamp (freq, 20.0f, sr * 0.45f);
        const float g = std::tan (3.14159265f * freq / sr);
        k = 1.0f / std::max (0.05f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick (float x, float& lp, float& bp, float& hp)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        lp = v2; bp = v1; hp = x - k * v1 - v2;
    }
};

// Modal resonator: en ringande metallton. Impulssvaret har amplitud 1.
struct Mode
{
    float y1 = 0, y2 = 0, c1 = 0, c2 = 0, in = 0, amp = 1;
    void set (float freq, float decaySec, float sr, float a)
    {
        freq = std::min (freq, sr * 0.45f);
        const float w = kTwoPi * freq / sr, r = std::exp (-1.0f / (std::max (0.0005f, decaySec) * sr));
        c1 = 2.0f * r * std::cos (w); c2 = r * r; in = std::sin (w); amp = a;
    }
    float tick (float x) { const float y = in * x + c1 * y1 - c2 * y2; y2 = y1; y1 = y; return y * amp; }
};

enum Kind { Anga = 0, Pys, Knack, Klick, Skrammel, Vissla, NumKinds };

// Typer per källa (valbara i GUI och presets)
inline constexpr int kAngaTyper = 4;      // Torr, Bred, Våt, Tung
inline constexpr int kPysTyper = 4;       // Kort, Lång, Sprätt, Vissel
inline constexpr int kKnackTyper = 5;     // Vevstake, Ventil, Hammare, Kedja, Rör
inline constexpr int kKlickTyper = 4;     // Relä, Tick, Trä, Spärr
inline constexpr int kSkrammelTyper = 3;  // Broms, Koppel, Droppe

struct Params
{
    float tryck = 0.5f, fart = 0.5f, rytt = 0.4f, lacka = 0.2f, liv = 0.5f, bredd = 0.6f;
    float anga = 0.8f, pys = 0.5f, kompressor = 0.4f, knack = 0.5f, klick = 0.3f, skrammel = 0.2f, vissla = 0.1f;
    float ton = 0.5f, rum = 0.3f, volym = 0.8f;
    int angaTyp = 0, pysTyp = 0, knackTyp = 0, klickTyp = 0, skrammelTyp = 0;
    bool avgang = false;
};

struct Voice
{
    bool on = false;
    int kind = 0, typ = 0;
    int delay = 0, age = 0, life = 0;
    float vel = 1, panL = 0.7f, panR = 0.7f;
    // brus-envelope
    float att = 0, attInc = 1, env = 0, dec = 0.999f;
    float f0 = 500, fEnd = 300, fNow = 500, glide = 0.999f, q = 1;
    Svf f[3];
    // metall
    Mode m[6];
    int nModes = 0, exc = 0, excLen = 1;
    int reps = 0, repWait = 0, repMin = 1, repMax = 1;
    float repAmp = 1, excAmp = 1, hard = 0.5f;
    // toner
    float ph[3] { 0, 0, 0 }, sub = 0, subDec = 0.999f, subPh = 0, subF = 60;
    float vib = 0, gate = 1, gateT = 0;
    int puff2 = -1;
};

class Engine
{
public:
    void prepare (double sampleRate)
    {
        sr = (float) sampleRate;
        for (auto& v : voices) v.on = false;
        lastStep = -1; avgangPos = 0;
        const float sc = sr / 48000.0f;
        static const int lens[4] { 1153, 1559, 1867, 2269 };
        for (int i = 0; i < 4; ++i)
        {
            fdnLen[i] = std::max (16, (int) (lens[i] * sc));
            fdn[i].assign ((size_t) fdnLen[i], 0.0f);
            fdnPos[i] = 0; fdnLp[i] = 0;
        }
        for (auto& d : early) d.assign ((size_t) (0.05f * sr) + 4, 0.0f);
        earlyPos = 0;
        dcL = dcR = dcxL = dcxR = 0;
    }

    void process (const Params& p, double bpm, double ppq, bool playing, float* L, float* R, int n)
    {
        const double beatsPerSample = bpm / 60.0 / sr;
        const float fartEff = fartNu (p, playing);
        const int perBeat = fartEff < 0.08f ? 0 : (fartEff < 0.35f ? 1 : (fartEff < 0.7f ? 2 : 4));
        const float stepSamples = (float) (sr * 60.0 / bpm / 4.0);

        for (int i = 0; i < n; ++i)
        {
            const double pos = ppq + i * beatsPerSample;
            if (playing)
            {
                const long step = (long) std::floor (pos * 4.0);
                if (step != lastStep) { lastStep = step; onStep (p, step, perBeat, stepSamples); }
                avgangPos = pos;
            }
            else
            {
                // transporten står: loket pustar ändå lite då och då
                if (rng.uni() < 1.0f / (sr * 1.2f)) trig (p, Pys, 0.35f + 0.4f * rng.uni(), 0);
                if (rng.uni() < 1.0f / (sr * 2.5f)) trig (p, Klick, 0.5f, 0);
            }

            float l = 0, r = 0;
            renderVoices (p, l, r);
            renderKompressor (p, pos, l, r);
            renderLacka (p, l, r);
            rum (p, l, r);
            // DC-spärr och mjuk mättning
            const float g = p.volym * 0.9f;
            l *= g; r *= g;
            dcL = l - dcxL + 0.9995f * dcL; dcxL = l;
            dcR = r - dcxR + 0.9995f * dcR; dcxR = r;
            L[i] = soft (dcL);
            R[i] = soft (dcR);
        }
    }

    // MIDI: spela ett slag för hand
    void hit (const Params& p, int kind, float vel) { trig (p, kind, vel, 0.0f); }

private:
    static float soft (float x)
    {
        // linjär upp till 0.6, sedan mjukt mot 1
        const float a = std::fabs (x);
        if (a < 0.6f) return x;
        const float y = 0.6f + 0.4f * std::tanh ((a - 0.6f) / 0.4f);
        return x < 0 ? -y : y;
    }

    float fartNu (const Params& p, bool playing)
    {
        if (! p.avgang) return p.fart;
        if (! playing) return 0.0f;
        const float t = (float) std::clamp (avgangPos / 32.0, 0.0, 1.0);
        return p.fart * t * t;
    }

    // euklidisk rytm: k slag jämnt fördelade på 16 steg, roterad
    static bool euklid (int step, int k, int rot)
    {
        if (k <= 0) return false;
        const int s = (step + rot) & 15;
        return ((s * k) % 16) < k;
    }

    void onStep (const Params& p, long step, int perBeat, float stepSamples)
    {
        const int inBeat = (int) (step & 3), inBar = (int) (step & 15);
        if (inBar == 0) { knackRot = (int) (rng.uni() * 3.0f) + 1; klickRot = (int) (rng.uni() * 16.0f); }

        // ÅNGA: tjoff i hjulets takt, med betoning
        const bool chuff = perBeat == 4 || (perBeat == 2 && (inBeat & 1) == 0) || (perBeat == 1 && inBeat == 0);
        if (chuff && p.anga > 0.01f)
        {
            static const float acc[4] { 1.0f, 0.55f, 0.8f, 0.5f };
            trig (p, Anga, acc[(step >> (perBeat == 4 ? 0 : 1)) & 3], stepSamples);
        }
        const float still = perBeat == 0 ? 0.45f : 1.0f;
        // KNACK: euklidiskt mönster, RYTT ger fler slag. Ibland en misständning.
        if (p.knack > 0.01f && euklid (inBar, 2 + (int) std::lround (p.rytt * 7.0f), knackRot) && rng.uni() < 0.88f * still)
            trig (p, Knack, 0.55f + 0.45f * rng.uni(), stepSamples);
        // KLICK: småtickande på de steg knacket lämnar
        if (p.klick > 0.01f && euklid (inBar, 3 + (int) std::lround (p.rytt * 9.0f), klickRot) && rng.uni() < 0.75f)
            trig (p, Klick, 0.4f + 0.6f * rng.uni(), stepSamples);
        // PYS: ventilen släpper, mest i slutet av halvtakten
        if (p.pys > 0.01f && (inBar == 15 || inBar == 7) && rng.uni() < 0.25f + 0.6f * p.tryck)
            trig (p, Pys, 0.6f + 0.4f * rng.uni(), stepSamples);
        if (p.pys > 0.01f && rng.uni() < 0.04f * p.tryck)
            trig (p, Pys, 0.4f + 0.3f * rng.uni(), stepSamples);
        // SKRAMMEL: broms, koppel och droppar, glest och oregelbundet
        if (p.skrammel > 0.01f)
        {
            const float pr = p.skrammelTyp == 2 ? 0.12f : 0.035f;
            if (rng.uni() < pr) trig (p, Skrammel, 0.6f + 0.4f * rng.uni(), stepSamples);
        }
        // VISSLA: sällsynt, på ettan
        if (p.vissla > 0.01f && inBar == 0 && rng.uni() < 0.3f)
            trig (p, Vissla, 0.8f, stepSamples);
    }

    Voice& freeVoice()
    {
        Voice* oldest = &voices[0];
        for (auto& v : voices)
        {
            if (! v.on) return v;
            if (v.age > oldest->age) oldest = &v;
        }
        return *oldest;
    }

    float level (const Params& p, int kind) const
    {
        switch (kind)
        {
            case Anga: return p.anga; case Pys: return p.pys; case Knack: return p.knack;
            case Klick: return p.klick; case Skrammel: return p.skrammel; case Vissla: return p.vissla;
            default: return 1.0f;
        }
    }

    int typFor (const Params& p, int kind) const
    {
        switch (kind)
        {
            case Anga: return std::clamp (p.angaTyp, 0, kAngaTyper - 1);
            case Pys: return std::clamp (p.pysTyp, 0, kPysTyper - 1);
            case Knack: return std::clamp (p.knackTyp, 0, kKnackTyper - 1);
            case Klick: return std::clamp (p.klickTyp, 0, kKlickTyper - 1);
            case Skrammel: return std::clamp (p.skrammelTyp, 0, kSkrammelTyper - 1);
            default: return 0;
        }
    }

    void setModes (Voice& v, const float* ratios, const float* amps, int n, float base, float decay, float decaySpread)
    {
        v.nModes = n;
        for (int k = 0; k < n; ++k)
        {
            const float det = 1.0f + 0.012f * rng.bi();                         // varje slag lite ostämt
            const float d = decay * (1.0f - decaySpread * (float) k / (float) n); // höga toner dör fortare
            v.m[k] = Mode();
            v.m[k].set (base * ratios[k] * det, d, sr, amps[k]);
        }
    }

    void trig (const Params& p, int kind, float vel, float stepSamples)
    {
        auto& v = freeVoice();
        v = Voice();
        v.on = true; v.kind = kind; v.typ = typFor (p, kind);
        const float lv = level (p, kind);
        v.vel = std::clamp (vel * lv * (1.0f + 0.45f * p.liv * rng.bi()), 0.0f, 1.6f);
        // utjämning så att alla typer ligger på ungefär samma nivå
        static const float kG[NumKinds][5] {
            { 1.0f, 1.25f, 0.9f, 0.9f, 1 }, { 1.0f, 1.0f, 1.1f, 0.85f, 1 }, { 1.4f, 0.6f, 0.75f, 0.55f, 0.85f },
            { 1.3f, 1.5f, 0.95f, 1.05f, 1 }, { 0.45f, 1.1f, 1.3f, 1, 1 }, { 1.2f, 1, 1, 1, 1 } };
        v.vel *= kG[kind][v.typ];
        const float center = (kind == Anga || kind == Vissla) ? 0.35f : 1.0f;
        const float pan = std::clamp (0.5f + 0.48f * p.bredd * center * rng.bi() * (0.35f + 0.65f * p.liv), 0.0f, 1.0f);
        v.panL = std::cos (pan * 1.5707963f); v.panR = std::sin (pan * 1.5707963f);
        const float spread = kind == Vissla ? 0.6f : (kind == Anga ? 2.0f : 3.0f);
        const float tonMul = std::pow (2.0f, ((p.ton - 0.5f) * 24.0f + spread * p.liv * rng.bi()) / 12.0f);
        v.delay = (int) (stepSamples * 0.16f * p.liv * rng.uni());   // släpar, rusar aldrig
        const float tr = p.tryck;
        const int t = v.typ;

        switch (kind)
        {
            case Anga:
            {
                static const float base[4] { 900.0f, 520.0f, 700.0f, 300.0f };
                static const float len[4] { 0.06f, 0.16f, 0.12f, 0.22f };
                v.f0 = base[t] * (0.6f + 0.8f * tr) * tonMul; v.fEnd = v.f0 * (t == 0 ? 0.6f : 0.42f);
                v.q = (t == 0 ? 2.2f : 1.3f) + 2.0f * tr;
                v.attInc = 1.0f / ((t == 0 ? 0.002f : 0.006f) * sr);
                v.dec = std::exp (-1.0f / ((len[t] * (0.7f + 0.6f * (1.0f - p.fart)) + 0.03f * rng.uni()) * sr));
                v.glide = std::exp (-1.0f / (0.05f * sr));
                v.f[1].set (v.f0 * 0.5f, 0.7f, sr);
                v.f[2].set (4500.0f, 0.7f, sr);
                if (t == 1) v.puff2 = (int) ((0.035f + 0.03f * rng.uni()) * sr);
                if (t == 3) { v.sub = 1.0f; v.subF = 52.0f * tonMul; v.subDec = std::exp (-1.0f / (0.11f * sr)); }
                v.life = (int) (2.5f * sr);
                break;
            }
            case Pys:
            {
                v.f0 = (3200.0f + 3500.0f * tr) * (0.8f + 0.4f * rng.uni()); v.fEnd = v.f0 * 0.65f; v.q = 0.8f;
                const float len = t == 0 ? 0.12f + 0.12f * rng.uni() : (t == 1 ? 0.6f + 1.0f * rng.uni() : 0.3f + 0.5f * rng.uni());
                v.attInc = 1.0f / ((t == 1 ? 0.07f : 0.012f) * sr);
                v.dec = std::exp (-1.0f / (len * sr));
                v.glide = std::exp (-1.0f / (0.7f * sr));
                v.f[1].set (900.0f, 0.6f, sr);
                v.ph[1] = kTwoPi * (2400.0f + 1800.0f * rng.uni()) * tonMul / sr;    // vissel-ton
                v.vib = (25.0f + 20.0f * rng.uni()) / sr;                           // sprätt-takt
                v.life = (int) (4.0f * sr);
                break;
            }
            case Knack:
            {
                static const float rVev[4] { 1.0f, 2.76f, 5.40f, 8.93f },       aVev[4] { 1.0f, 0.5f, 0.3f, 0.15f };
                static const float rVen[5] { 1.0f, 1.59f, 2.14f, 2.30f, 2.65f }, aVen[5] { 0.8f, 1.0f, 0.6f, 0.5f, 0.3f };
                static const float rHam[5] { 1.0f, 2.57f, 4.21f, 5.90f, 7.4f },  aHam[5] { 1.0f, 0.7f, 0.45f, 0.3f, 0.2f };
                static const float rKed[4] { 1.0f, 1.47f, 2.09f, 2.56f },       aKed[4] { 1.0f, 0.8f, 0.6f, 0.4f };
                static const float rRor[5] { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f }, aRor[5] { 1.0f, 0.8f, 0.5f, 0.3f, 0.15f };
                const float hardDec = 0.6f + 0.8f * p.rytt;
                switch (t)
                {
                    case 0: setModes (v, rVev, aVev, 4, (280.0f + 160.0f * rng.uni()) * tonMul, 0.07f * hardDec, 0.6f); v.excLen = (int) (0.0015f * sr); break;
                    case 1: setModes (v, rVen, aVen, 5, (1700.0f + 900.0f * rng.uni()) * tonMul, 0.03f * hardDec, 0.5f); v.excLen = (int) (0.0004f * sr); break;
                    case 2: setModes (v, rHam, aHam, 5, (150.0f + 70.0f * rng.uni()) * tonMul, 0.22f * hardDec, 0.7f); v.excLen = (int) (0.002f * sr); break;
                    case 3: setModes (v, rKed, aKed, 4, (1100.0f + 500.0f * rng.uni()) * tonMul, 0.025f * hardDec, 0.4f); v.excLen = (int) (0.0005f * sr);
                            v.reps = 2 + (int) (rng.uni() * 4.0f); v.repMin = (int) (0.008f * sr); v.repMax = (int) (0.026f * sr); break;
                    default: setModes (v, rRor, aRor, 5, (520.0f + 260.0f * rng.uni()) * tonMul, 0.35f * hardDec, 0.75f); v.excLen = (int) (0.0008f * sr); break;
                }
                v.hard = 0.3f + 0.7f * tr;
                v.f[0].set (1500.0f + 9000.0f * v.hard, 0.7f, sr);    // excitationens ljushet
                v.life = (int) (2.0f * sr);
                break;
            }
            case Klick:
            {
                static const float rRel[2] { 1.0f, 1.71f }, aRel[2] { 1.0f, 0.6f };
                static const float rTick[2] { 1.0f, 1.43f }, aTick[2] { 1.0f, 0.5f };
                static const float rTra[3] { 1.0f, 2.32f, 4.25f }, aTra[3] { 1.0f, 0.4f, 0.15f };
                static const float rSpa[3] { 1.0f, 1.8f, 2.7f }, aSpa[3] { 1.0f, 0.6f, 0.3f };
                switch (t)
                {
                    case 0: setModes (v, rRel, aRel, 2, (2600.0f + 1400.0f * rng.uni()) * tonMul, 0.006f, 0.3f);
                            v.reps = 1; v.repMin = (int) (0.004f * sr); v.repMax = (int) (0.009f * sr); break;
                    case 1: setModes (v, rTick, aTick, 2, (5200.0f + 2000.0f * rng.uni()) * tonMul, 0.0025f, 0.2f); break;
                    case 2: setModes (v, rTra, aTra, 3, (700.0f + 400.0f * rng.uni()) * tonMul, 0.03f, 0.6f); break;
                    default: setModes (v, rSpa, aSpa, 3, (1700.0f + 600.0f * rng.uni()) * tonMul, 0.008f, 0.3f);
                             v.reps = 4 + (int) (rng.uni() * 5.0f); v.repMin = (int) (0.011f * sr); v.repMax = (int) (0.019f * sr); break;
                }
                v.excLen = std::max (1, (int) (0.0003f * sr));
                v.f[0].set (9000.0f, 0.7f, sr);
                v.life = (int) (0.6f * sr);
                break;
            }
            case Skrammel:
            {
                if (t == 0)   // BROMS: gnisslande smalt brus med vibrato
                {
                    v.f0 = (2500.0f + 1800.0f * rng.uni()) * tonMul; v.q = 35.0f;
                    v.attInc = 1.0f / (0.12f * sr);
                    v.dec = std::exp (-1.0f / ((0.5f + 0.9f * rng.uni()) * sr));
                    v.vib = (5.0f + 2.5f * rng.uni()) / sr;
                    v.f[1].set (v.f0 * 1.5f, 30.0f, sr);
                    v.life = (int) (6.0f * sr);
                }
                else if (t == 1)   // KOPPEL: tunga metallslag med skrammel efter
                {
                    static const float rK[5] { 1.0f, 2.57f, 4.21f, 5.90f, 8.1f }, aK[5] { 1.0f, 0.8f, 0.6f, 0.4f, 0.25f };
                    setModes (v, rK, aK, 5, (210.0f + 120.0f * rng.uni()) * tonMul, 0.18f, 0.6f);
                    v.excLen = (int) (0.002f * sr);
                    v.reps = 2 + (int) (rng.uni() * 3.0f); v.repMin = (int) (0.03f * sr); v.repMax = (int) (0.09f * sr);
                    v.hard = 0.8f; v.f[0].set (6000.0f, 0.7f, sr);
                    v.life = (int) (2.5f * sr);
                }
                else   // DROPPE: vattendroppe som faller i ton
                {
                    v.f0 = (1300.0f + 900.0f * rng.uni()) * tonMul; v.fEnd = v.f0 * 0.3f; v.fNow = v.f0;
                    v.glide = std::exp (-1.0f / (0.012f * sr));
                    v.attInc = 1.0f;
                    v.dec = std::exp (-1.0f / ((0.03f + 0.03f * rng.uni()) * sr));
                    v.life = (int) (0.5f * sr);
                }
                break;
            }
            case Vissla:
            {
                v.f0 = 520.0f * tonMul; v.fNow = v.f0 * 0.85f; v.fEnd = v.f0;
                v.attInc = 1.0f / (0.09f * sr);
                v.dec = std::exp (-1.0f / ((0.7f + 0.6f * rng.uni()) * sr));
                v.glide = std::exp (-1.0f / (0.1f * sr));
                v.vib = 5.5f / sr;
                v.f[0].set (v.f0 * 2.0f, 3.0f, sr);
                v.life = (int) (5.0f * sr);
                break;
            }
            default: break;
        }
        if (kind != Skrammel || t != 2) v.fNow = (kind == Vissla) ? v.fNow : v.f0;
    }

    // envelope för brusbaserade slag: linjär attack, sedan exponentiell utklingning
    static float envTick (Voice& v)
    {
        if (v.att < 1.0f) { v.att = std::min (1.0f, v.att + v.attInc); if (v.att >= 1.0f) v.env = 1.0f; return v.att; }
        v.env *= v.dec;
        return v.env;
    }

    float metal (Voice& v, float noise)
    {
        float x = 0;
        if (v.exc < v.excLen) { x = noise * v.excAmp; ++v.exc; }
        else if (v.reps > 0)
        {
            if (--v.repWait <= 0)
            {
                if (v.repWait < 0) v.repWait = v.repMin + (int) (rng.uni() * (float) (v.repMax - v.repMin));
                else { v.exc = 0; --v.reps; v.excAmp *= 0.6f + 0.3f * rng.uni(); v.repWait = v.repMin + (int) (rng.uni() * (float) (v.repMax - v.repMin)); }
            }
        }
        float lp, bp, hp;
        v.f[0].tick (x, lp, bp, hp);
        const float e = lp * 1.2f / std::sqrt ((float) std::max (1, v.excLen));
        float out = 0;
        for (int k = 0; k < v.nModes; ++k) out += v.m[k].tick (e);
        return out + hp * 0.3f * v.hard * (v.exc < v.excLen ? 1.0f : 0.0f);
    }

    void renderVoices (const Params& p, float& l, float& r)
    {
        for (auto& v : voices)
        {
            if (! v.on) continue;
            if (v.delay > 0) { --v.delay; continue; }
            ++v.age;
            const float noise = rng.bi();
            float out = 0, lp, bp, hp;
            switch (v.kind)
            {
                case Anga:
                {
                    if (v.age == v.puff2) { v.att = 0.3f; v.env = 0; }     // dubbelpust
                    const float e = envTick (v);
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.f[0].set (v.fNow, v.q, sr);
                    v.f[0].tick (noise, lp, bp, hp);
                    float l2, b2, h2; v.f[1].tick (noise, l2, b2, h2);
                    out = (bp * 1.3f + l2 * 0.7f) * e;
                    if (v.typ == 2 && rng.uni() < 0.006f * (0.3f + e))  // våt: sprakande droppar
                    { float l3, b3, h3; v.f[2].tick (rng.bi() * 2.0f, l3, b3, h3); out += h3 * 0.5f; }
                    else if (v.typ == 2) { float l3, b3, h3; v.f[2].tick (0.0f, l3, b3, h3); out += h3 * 0.5f; }
                    if (v.sub > 0.0005f)
                    {
                        v.subPh += kTwoPi * v.subF * (1.0f + v.sub * 0.6f) / sr;
                        if (v.subPh > kTwoPi) v.subPh -= kTwoPi;
                        out += 0.7f * std::sin (v.subPh) * v.sub;
                        v.sub *= v.subDec;
                    }
                    out *= 0.9f;
                    if (v.att >= 1.0f && v.env < 0.0003f && v.sub < 0.0005f) v.on = false;
                    break;
                }
                case Pys:
                {
                    const float e = envTick (v);
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.f[0].set (v.fNow, v.q, sr);
                    v.f[0].tick (noise, lp, bp, hp);
                    float l2, b2, h2; v.f[1].tick (noise, l2, b2, h2);
                    out = hp * 0.5f + bp * 0.35f + h2 * 0.08f;
                    if (v.typ == 2)   // sprätt: ventilen fladdrar
                    {
                        v.gateT += v.vib;
                        if (v.gateT >= 1.0f) { v.gateT -= 1.0f; v.ph[2] = rng.uni() < 0.6f ? 1.0f : 0.15f; }
                        v.gate += (v.ph[2] - v.gate) * 0.004f;
                        out *= v.gate;
                    }
                    if (v.typ == 3) { v.ph[0] += v.ph[1] * (1.0f + 0.004f * std::sin (v.ph[0] * 0.0007f)); if (v.ph[0] > kTwoPi * 1000) v.ph[0] -= kTwoPi * 1000;
                                      out += 0.12f * std::sin (v.ph[0]); }
                    out *= e * 0.9f;
                    if (v.att >= 1.0f && v.env < 0.0003f) v.on = false;
                    break;
                }
                case Knack:
                case Klick:
                    out = metal (v, noise) * (v.kind == Klick ? 0.5f : 0.45f);
                    break;
                case Skrammel:
                    if (v.typ == 0)
                    {
                        const float e = envTick (v);
                        v.ph[0] += kTwoPi * v.vib; if (v.ph[0] > kTwoPi) v.ph[0] -= kTwoPi;
                        const float f = v.f0 * (1.0f + 0.015f * std::sin (v.ph[0]));
                        v.f[0].set (f, v.q, sr);
                        v.f[0].tick (noise, lp, bp, hp);
                        float l2, b2, h2; v.f[1].tick (noise, l2, b2, h2);
                        out = (bp + b2 * 0.5f) * e * 0.22f;
                        if (v.att >= 1.0f && v.env < 0.0003f) v.on = false;
                    }
                    else if (v.typ == 1) out = metal (v, noise) * 0.35f;
                    else
                    {
                        const float e = envTick (v);
                        v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                        v.ph[0] += kTwoPi * v.fNow / sr; if (v.ph[0] > kTwoPi) v.ph[0] -= kTwoPi;
                        out = 0.5f * std::sin (v.ph[0]) * e;
                        if (v.att >= 1.0f && v.env < 0.0003f) v.on = false;
                    }
                    break;
                case Vissla:
                {
                    const float e = envTick (v);
                    v.fNow = v.fEnd + (v.fNow - v.fEnd) * v.glide;
                    v.gateT += v.vib; if (v.gateT > 1.0f) v.gateT -= 1.0f;
                    const float f = v.fNow * (1.0f + 0.004f * std::sin (kTwoPi * v.gateT));
                    static const float ratio[3] { 1.0f, 1.26f, 1.5f };   // ångvisslans ackord
                    for (int k = 0; k < 3; ++k)
                    {
                        v.ph[k] += kTwoPi * f * ratio[k] / sr; if (v.ph[k] > kTwoPi) v.ph[k] -= kTwoPi;
                        out += std::sin (v.ph[k]) * (k == 0 ? 0.16f : 0.11f);
                    }
                    v.f[0].tick (noise, lp, bp, hp);
                    out = (out + bp * 0.12f) * e;
                    if (v.att >= 1.0f && v.env < 0.0003f) v.on = false;
                    break;
                }
                default: break;
            }
            out *= v.vel;
            l += out * v.panL; r += out * v.panR;
            if (--v.life <= 0) v.on = false;
        }
    }

    // KOMPRESSOR: tvåcylindrig, dunkar i åttondelar med övertoner och ojämn gång
    void renderKompressor (const Params& p, double pos, float& l, float& r)
    {
        if (p.kompressor < 0.01f) return;
        const double cyc = pos * 2.0;
        const float ph = (float) (cyc - std::floor (cyc));
        if (ph < lastKompPh) { kompJitter = 0.04f * p.liv * rng.bi(); kompAlt = ! kompAlt; }
        lastKompPh = ph;
        const float hum = (46.0f + 6.0f * p.tryck) * std::pow (2.0f, (p.ton - 0.5f) * 0.5f) * (1.0f + kompJitter);
        humPh += kTwoPi * hum / sr; if (humPh > kTwoPi) humPh -= kTwoPi;
        const float tone = std::sin (humPh) + 0.45f * std::sin (2 * humPh) + 0.2f * std::sin (3 * humPh) + 0.08f * std::sin (5 * humPh);
        const float thump = std::exp (-ph * 10.0f) * (kompAlt ? 1.0f : 0.7f);
        float lp, bp, hp;
        kompF.set (160.0f + 1100.0f * p.tryck * thump, 0.9f, sr);
        kompF.tick (tone * (0.3f + 0.7f * thump) + 0.25f * rng.bi() * thump, lp, bp, hp);
        const float out = lp * 0.42f * p.kompressor;
        l += out; r += out;
    }

    // LÄCKA: ständigt väsande, lite olika i vänster och höger
    void renderLacka (const Params& p, float& l, float& r)
    {
        if (p.lacka < 0.01f) return;
        float lp, bp, hp, lp2, bp2, hp2;
        lackaF.set (5000.0f + 2500.0f * p.tryck, 0.6f, sr);
        lackaF2.set (5600.0f + 2500.0f * p.tryck, 0.6f, sr);
        lackaF.tick (rng.bi(), lp, bp, hp);
        lackaF2.tick (rng.bi(), lp2, bp2, hp2);
        lackaPh += 0.37f * kTwoPi / sr; if (lackaPh > kTwoPi) lackaPh -= kTwoPi;
        const float w = 0.75f + 0.25f * std::sin (lackaPh);
        l += hp * 0.08f * p.lacka * w;
        r += hp2 * 0.08f * p.lacka * (1.75f - w) * 0.85f;
    }

    // RUM: tidiga reflexer + fyrkanalig FDN med dämpning. Låter som en stor plåthall.
    void rum (const Params& p, float& l, float& r)
    {
        if (p.rum < 0.01f) return;
        const int el = (int) early[0].size();
        early[0][(size_t) earlyPos] = l; early[1][(size_t) earlyPos] = r;
        auto tap = [&] (int ch, float sec) { int i = earlyPos - (int) (sec * sr); while (i < 0) i += el; return early[(size_t) ch][(size_t) i]; };
        const float erL = 0.5f * tap (1, 0.011f) + 0.35f * tap (0, 0.019f) + 0.25f * tap (1, 0.029f);
        const float erR = 0.5f * tap (0, 0.013f) + 0.35f * tap (1, 0.023f) + 0.25f * tap (0, 0.037f);
        earlyPos = (earlyPos + 1) % el;

        float d[4];
        for (int i = 0; i < 4; ++i) d[i] = fdn[i][(size_t) fdnPos[i]];
        const float fb = 0.6f + 0.33f * p.rum;
        const float damp = 0.35f;
        const float in = (erL + erR) * 0.5f;
        const float h0 = d[0] + d[1] + d[2] + d[3], h1 = d[0] - d[1] + d[2] - d[3];
        const float h2 = d[0] + d[1] - d[2] - d[3], h3 = d[0] - d[1] - d[2] + d[3];
        const float hs[4] { h0, h1, h2, h3 };
        for (int i = 0; i < 4; ++i)
        {
            fdnLp[i] += (hs[i] * 0.5f - fdnLp[i]) * (1.0f - damp);
            fdn[i][(size_t) fdnPos[i]] = fdnLp[i] * fb + in * (i & 1 ? -1.0f : 1.0f) + (i == 0 ? erL : (i == 1 ? erR : 0.0f)) * 0.3f;
            fdnPos[i] = (fdnPos[i] + 1) % fdnLen[i];
        }
        const float wet = p.rum * 0.55f;
        l += (erL * 0.4f + (d[0] + d[2]) * 0.5f) * wet;
        r += (erR * 0.4f + (d[1] + d[3]) * 0.5f) * wet;
    }

    float sr = 48000;
    std::array<Voice, 32> voices;
    Rng rng;
    Svf lackaF, lackaF2, kompF;
    long lastStep = -1;
    double avgangPos = 0;
    int knackRot = 1, klickRot = 5;
    float humPh = 0, lastKompPh = 0, kompJitter = 0, lackaPh = 0;
    bool kompAlt = false;
    std::vector<float> fdn[4];
    int fdnLen[4] { 1, 1, 1, 1 }, fdnPos[4] { 0, 0, 0, 0 };
    float fdnLp[4] { 0, 0, 0, 0 };
    std::vector<float> early[2];
    int earlyPos = 0;
    float dcL = 0, dcR = 0, dcxL = 0, dcxR = 0;
};
} // namespace pr
