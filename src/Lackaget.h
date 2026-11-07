#pragma once
// LÄCKAGET: ljudmotorn (bara C++17, inget JUCE, går att testa fristående).
//
// Ljudet går genom en kylkrets:
//   KOMPRESSOR (mättnad) -> EXPANSIONSVENTIL (svaj i tonhöjd + OLJERETUR-släp) -> FÖRÅNGARE (filter)
//   -> SIKTGLAS (bubblor) -> NÖDSTOPP (varvar ner / upp) -> HÖGTRYCKSPRESSOSTAT (limiter) -> MIX
//
// Allt styrs av en medvetet felinställd PI-regulator med dödtid. Börvärdet hoppar i takt
// (BÖRVÄRDE = 1/4 .. 1/32) och dödtiden är låst till samma steg, så pendlingen landar i groovet.
// Låtens egen nivå är "värmelast": mycket ljud in -> regulatorn tappar greppet.
//
// Viktigt: tyst in ger tyst ut. Allt brus i maskinen rör bara modulationen, aldrig ljudet direkt.

#include "Koldmedier.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace lackaget {

inline constexpr float kPi = 3.14159265358979f;

// ------------------------------------------------------------------ helpers
struct Rng
{
    uint32_t s = 0x9e3779b9u;
    explicit Rng (uint32_t seed = 1) { reseed (seed); }
    void reseed (uint32_t seed) { s = seed * 2654435761u + 0x6d2b79f5u; if (s == 0) s = 1; }
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }   // [0,1)
    float bip() { return uni() * 2.f - 1.f; }
    float gauss() { return (uni() + uni() + uni() + uni() - 2.f) * 1.7320508f; }
};

inline float dbToGain (float db) { return std::pow (10.f, db / 20.f); }
inline float clampf (float x, float a, float b) { return std::min (b, std::max (a, x)); }

// numerically safe log(cosh(x)) for ADAA
inline double logCosh (double x) { const double a = std::fabs (x); return a + std::log1p (std::exp (-2.0 * a)) - 0.69314718055994531; }

// tanh saturation. Only the non-linear part (tanh(x) - x) gets first-order antiderivative
// anti-aliasing, so a clean signal keeps all its top end (plain ADAA dulls everything above 10 kHz).
struct AdaaTanh
{
    double x1 = 0, f1 = 0;
    void reset() { x1 = 0; f1 = 0; }
    static double F (double x) { return logCosh (x) - 0.5 * x * x; }   // antiderivative of tanh(x) - x
    float process (float xf)
    {
        const double x = xf, f = F (x), d = x - x1;
        const double r = std::fabs (d) < 1.0e-5 ? std::tanh (0.5 * (x + x1)) - 0.5 * (x + x1) : (f - f1) / d;
        x1 = x; f1 = f;
        return (float) (x + r);
    }
};

struct DcBlock
{
    float x1 = 0, y1 = 0, r = 0.9995f;
    void prepare (double sr) { r = (float) std::exp (-2.0 * kPi * 8.0 / sr); }
    void reset() { x1 = y1 = 0; }
    float process (float x) { const float y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
};

// TPT state variable filter (Zavalishin), low-pass out
struct Svf
{
    float ic1 = 0, ic2 = 0;
    void reset() { ic1 = ic2 = 0; }
    float lp (float x, float g, float k)
    {
        const float a1 = 1.f / (1.f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        return v2;
    }
};

// circular delay with 4-point Hermite read
struct Delay
{
    std::vector<float> buf; int mask = 0, w = 0;
    void prepare (int minLen) { int n = 1; while (n < minLen + 4) n <<= 1; buf.assign ((size_t) n, 0.f); mask = n - 1; w = 0; }
    void reset() { std::fill (buf.begin(), buf.end(), 0.f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // d = delay in samples (>= 1), measured back from the newest sample
    float read (double d) const
    {
        const double pos = (double) w - 1.0 - d;
        const double fl = std::floor (pos); const float t = (float) (pos - fl);
        const int i = (int) fl;
        const float y0 = buf[(size_t) ((i - 1) & mask)], y1 = buf[(size_t) (i & mask)],
                    y2 = buf[(size_t) ((i + 1) & mask)], y3 = buf[(size_t) ((i + 2) & mask)];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
};

// ------------------------------------------------------------------ settings
enum class Rate { Fjardedel, Attondel, Sextondel, Trettiotvaondel, AttondelTriol, SextondelTriol, PunkteradAttondel, Halv };
inline constexpr float kRateBeats[] = { 1.f, 0.5f, 0.25f, 0.125f, 1.f / 3.f, 1.f / 6.f, 0.75f, 2.f };
inline constexpr const char* kRateName[] = { "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/8.", "1/2" };
inline constexpr int kNumRates = 8;

struct Settings
{
    int   medium      = 0;      // index i kMedier
    int   borvarde    = 1;      // Rate
    float hunting     = 0.30f;  // EXPANSIONSVENTIL HUNTING: hur mycket regulatorn överkompenserar
    float behorighet  = 0.15f;  // BEHÖRIGHET: 0 kat 1-tekniker .. 1 lärling fredag 15:30
    float last        = 0.30f;  // VÄRMELAST: hur mycket låten stör regulatorn
    float ventil      = 0.25f;  // svaj i tonhöjd (djup)
    float forangare   = 0.20f;  // hur mycket filtret följer ventilen
    float ton         = 1.00f;  // filtrets grundläge (0 = 120 Hz .. 1 = 20 kHz)
    float resonans    = 0.20f;
    float kompressor  = 0.25f;  // mättnad
    float siktglas    = 0.10f;  // bubblor
    float oljeretur   = 0.00f;  // släp: off-beat-16-delar kommer sent + modulationen kletar
    float spridning   = 0.30f;  // vänster och höger förångare pendlar olika
    float pressostat  = -0.5f;  // tak i dBFS
    float mix         = 1.00f;
    float ut          = 0.00f;  // dB
    float stopptid    = 0.70f;  // sekunder
    float starttid    = 0.90f;  // sekunder
    uint32_t seed     = 1;
};

enum Larm : uint32_t
{
    LarmHogtryck   = 1u << 0,   // A1
    LarmLagtryck   = 1u << 1,   // A2
    LarmPressostat = 1u << 2,   // A3
    LarmFlashgas   = 1u << 3,   // A12
    LarmPendling   = 1u << 4,   // A17
    LarmBrand      = 1u << 5,   // A30
    LarmNodstopp   = 1u << 6,   // A99
};

// ------------------------------------------------------------------ the regulator ("anläggningen")
// First-order plant with dead time, controlled by a mis-tuned PI loop. Runs at control rate.
struct Anlaggning
{
    Rng rng { 7 };
    std::vector<float> deadBuf; int deadW = 0;
    float y = 0, ySens = 0, integ = 0, u = 0, r = 0, ePrev = 0;
    float spike = 0, peakErr = 0;

    void prepare (int maxDeadTicks) { deadBuf.assign ((size_t) std::max (8, maxDeadTicks + 2), 0.f); reset(); }
    void reset() { std::fill (deadBuf.begin(), deadBuf.end(), 0.f); deadW = 0; y = ySens = integ = u = r = ePrev = spike = peakErr = 0; }

    // dt seconds; tau, theta seconds; returns nothing, read y/u/err afterwards
    void tick (float dt, float target, float tau, float theta, const Koldmedium& m,
               float hunting, float beh, float load)
    {
        r = target;
        // dead time: the valve change reaches the sensor late
        const int n = (int) deadBuf.size();
        const int dTicks = std::clamp ((int) std::lround (theta / dt), 1, n - 1);
        deadBuf[(size_t) deadW] = u; deadW = (deadW + 1) % n;
        const float uLate = deadBuf[(size_t) ((deadW - 1 - dTicks + 2 * n) % n)];

        // plant: tau dy/dt = K*u + load - y   (+ spikes from flammable media)
        const float K = m.forstarkning;
        y += dt / tau * (K * uLate + load - y);
        y += spike; spike *= 0.f;
        y = clampf (y, -6.f, 6.f);

        // sensor lies: lag + noise (more for a sloppy technician)
        const float sTau = 0.004f + 0.05f * beh;
        ySens += (1.f - std::exp (-dt / sTau)) * (y - ySens);
        const float meas = ySens + m.brus * (0.15f + 1.2f * beh) * rng.gauss() * 0.25f;

        // SIMC PI tuning for FOPDT, then deliberately over-gained
        const float thetaEff = std::max (theta, dt);
        const float Kc = tau / (K * 2.f * thetaEff);
        const float tauI = std::min (tau, 8.f * thetaEff);
        const float g = 1.f + hunting * 1.9f * (1.5f - m.dampning) + beh * 1.6f * (1.2f - m.dampning);
        const float e = r - meas;
        integ += e * dt / tauI;
        if (beh < 0.5f) integ = clampf (integ, -2.f, 2.f);                 // a real technician has anti-windup
        else            integ = clampf (integ * (1.f - 0.05f * dt), -5.f, 5.f);   // the apprentice doesn't
        u = clampf (Kc * g * (e + integ), -2.5f, 2.5f);
        peakErr = std::max (std::fabs (e), peakErr * (1.f - dt * 2.f));
        ePrev = e;
    }
};

// ------------------------------------------------------------------ the machine
class Lackaget
{
public:
    static constexpr int kCtl = 16;               // control-rate divider
    static constexpr float kBaseMs = 8.f;         // fixed latency of the valve delay
    static constexpr float kWobbleMs = 5.f;       // max pitch-wobble excursion
    static constexpr float kDragMs = 28.f;        // max OLJERETUR drag

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        sr = sampleRate;
        const int maxDelay = (int) std::ceil ((kBaseMs + kWobbleMs + kDragMs + 4.f) * 0.001 * sr);
        for (auto& d : dly) d.prepare (maxDelay + (int) std::ceil (4.5 * sr));   // room for the full stop + restart lag
        for (auto& c : comb) c.prepare ((int) std::ceil (0.02 * sr));
        const float dt = kCtl / (float) sr;
        for (auto& a : plant) a.prepare ((int) std::ceil (2.0 / dt));
        for (auto& d : dc) d.prepare (sr);
        reset();
    }

    void reset()
    {
        for (auto& d : dly) d.reset();
        for (auto& c : comb) c.reset();
        for (auto& a : plant) a.reset();
        for (auto& f : filt) f.reset();
        for (auto& s : sat) s.reset();
        for (auto& d : dc) d.reset();
        plant[0].rng.reseed (cfg.seed * 3 + 1); plant[1].rng.reseed (cfg.seed * 7 + 5); rng.reseed (cfg.seed);
        envFast = envSlow = 0; gapEnv = gapRef = 0; ctlCount = 0; ppqFree = 0;
        modL = modR = modLp = modRp = 0; uL = uR = 0; dragNow = 0;
        gPrev = { 0, 0 }; gNow = { 0, 0 };
        bubbleLeft = 0; holdL = holdR = 0; holdCount = 0;
        speed = 1; lag = 0; stopState = Kor; xfade = 0; stopTimer = 0;
        limEnv = 0; grDb = 0; larm = 0; firstCtl = true;
        stepIdx = -1;
    }

    void setSettings (const Settings& s)
    {
        if (s.seed != cfg.seed) { plant[0].rng.reseed (s.seed * 3 + 1); plant[1].rng.reseed (s.seed * 7 + 5); rng.reseed (s.seed); }
        cfg = s;
        cfg.medium = std::clamp (cfg.medium, 0, kNumMedier - 1);
        cfg.borvarde = std::clamp (cfg.borvarde, 0, kNumRates - 1);
    }
    const Settings& settings() const { return cfg; }

    // host tempo/position for the next block. playing=false -> free running at bpm
    void setTransport (double bpm, double ppqAtBlockStart, bool hostPlaying)
    {
        tempo = std::clamp (bpm, 30.0, 300.0);
        if (hostPlaying) { ppq = ppqAtBlockStart; usingHost = true; }
        else             { ppq = ppqFree; usingHost = false; }
    }

    // NÖDSTOPP: true = varva ner och stanna, false = starta igen
    void nodstopp (bool on)
    {
        // start each move from the current motor speed, so a stop during a restart (or the reverse) never jumps
        if (on && (stopState == Kor || stopState == Startar || stopState == Kopplar))
        { if (stopState == Kopplar) { lag = 0.f; xfade = 0.f; } stopState = Stannar; stopTimer = 0; speed0 = speed; }
        if (! on && (stopState == Stannar || stopState == Stilla)) { stopState = Startar; stopTimer = 0; speed0 = speed; }
    }
    bool nodstoppAktiv() const { return stopState == Stannar || stopState == Stilla; }

    int latencySamples() const { return (int) std::lround (kBaseMs * 0.001 * sr); }

    // stereo in place
    void process (float* L, float* R, int n)
    {
        const Koldmedium& m = kMedier[cfg.medium];
        const float stepBeats = kRateBeats[cfg.borvarde];
        const double ppqPerSample = tempo / 60.0 / sr;
        const float beatSec = (float) (60.0 / tempo);
        const float dtCtl = kCtl / (float) sr;
        const float wet = clampf (cfg.mix, 0.f, 1.f);
        const float outGain = dbToGain (cfg.ut);
        const float ceil = dbToGain (std::min (cfg.pressostat, 0.f));
        const float drive = 1.f + 9.f * cfg.kompressor * (0.4f + 0.6f * m.tryck);
        const float makeup = 0.25f / std::tanh (drive * 0.25f);   // keeps a -12 dBFS signal at the same level
        const float asym = 0.25f * cfg.kompressor * m.tryck;
        // TON 1 = fully open whatever the medium; lower down, icy media sit brighter and dark ones darker
        const float tonEff = clampf (cfg.ton + (m.ljus - 0.5f) * 0.3f * (1.f - cfg.ton), 0.f, 1.f);
        const float baseCut = 120.f * std::pow (22000.f / 120.f, tonEff);
        const float kRes = 1.414f - 1.32f * clampf (cfg.resonans, 0.f, 1.f);   // 1/Q, Butterworth at 0
        const float wobble = cfg.ventil * kWobbleMs * 0.001f * (float) sr * clampf (std::sqrt (m.troghet / 0.3f), 0.6f, 1.5f);   // slow plants swing wider
        const float base = kBaseMs * 0.001f * (float) sr;
        const float dragMax = cfg.oljeretur * kDragMs * 0.001f * (float) sr;
        const float smear = 1.f - std::exp (-dtCtl / (0.004f + cfg.oljeretur * 0.15f * beatSec));
        const float envA = 1.f - std::exp (-1.f / (0.0015f * (float) sr)), envR = 1.f - std::exp (-1.f / (0.060f * (float) sr));
        const float envS = 1.f - std::exp (-1.f / (0.400f * (float) sr));
        const float limRel = 1.f - std::exp (-1.f / (0.080f * (float) sr));
        gapDecay = std::exp (-1.f / (0.012f * (float) sr)); gapRefDecay = std::exp (-1.f / (0.5f * (float) sr));
        uint32_t larmNow = 0;

        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            const float mono = 0.5f * (std::fabs (inL) + std::fabs (inR));
            envFast += (mono > envFast ? envA : envR) * (mono - envFast);
            envSlow += envS * (envFast - envSlow);

            // ---------------- control rate: the regulator
            if (ctlCount == 0)
            {
                const double beat = ppq;
                const long step = (long) std::floor (beat / stepBeats);
                const float stepPhase = (float) (beat / stepBeats - (double) step);
                if (step != stepIdx) { stepIdx = step; onStep (m); }
                static constexpr float pat[8] = { 1.f, -1.f, 0.55f, -1.f, 1.f, -0.45f, 0.3f, -1.f };
                float target = pat[(size_t) (((step % 8) + 8) % 8)];
                target += m.glid * 0.8f * std::sin (2.f * kPi * (float) (beat / 32.0));    // glide over 8 bars
                const float tau = std::max (0.004f, m.troghet * beatSec);
                const float theta = m.dodtid * 0.5f * stepBeats * beatSec * (1.f + cfg.behorighet * 0.6f * rng.bip());
                const float loadDb = 20.f * std::log10 (envSlow + 1.0e-6f);
                const float load = cfg.last * clampf ((loadDb + 40.f) / 34.f, 0.f, 1.4f) * 1.6f;

                plant[0].tick (dtCtl, target, tau, theta, m, cfg.hunting, cfg.behorighet, load);
                plant[1].tick (dtCtl, target, tau * 1.13f, theta * 0.92f, m, cfg.hunting, cfg.behorighet, load * 0.9f);

                // OLJERETUR: the modulation smears
                const float a = firstCtl ? 1.f : smear;
                modL += a * (plant[0].y - modL);
                const float rOwn = plant[1].y;
                modR += a * (modL + cfg.spridning * (rOwn - modL) - modR);
                uL += a * (std::tanh (plant[0].u / 1.5f) - uL);
                uR += a * (std::tanh ((plant[0].u + cfg.spridning * (plant[1].u - plant[0].u)) / 1.5f) - uR);
                firstCtl = false;

                // filter coefficients for this tick (interpolated per sample)
                gPrev = gNow; openPrev = openNow;
                for (int c = 0; c < 2; ++c)
                {
                    const float uu = c == 0 ? uL : uR;
                    float fc = baseCut * std::pow (2.f, cfg.forangare * 3.f * uu);
                    if (stopState != Kor) fc *= 0.15f + 0.85f * speed * speed;   // the evaporator goes dark when the motor dies
                    fc = clampf (fc, 30.f, 0.45f * (float) sr);
                    gNow[(size_t) c] = std::tan (kPi * fc / (float) sr);
                    openNow[(size_t) c] = clampf ((fc - 13000.f) / 7000.f, 0.f, 1.f);   // near the top: fade the filter out
                }
                if (gPrev[0] == 0.f) gPrev = gNow;

                // OLJERETUR drag: offbeat 16ths land late. Only moves freely in the gaps.
                const double sixteenth = beat * 4.0;
                const long s16 = (long) std::floor (sixteenth);
                const float ph16 = (float) (sixteenth - (double) s16);
                const long upcoming = ph16 > 0.35f ? s16 + 1 : s16;
                const bool off = (upcoming & 1) != 0;
                dragTarget = off ? dragMax * (1.f + 0.35f * cfg.behorighet * rng.bip()) : 0.f;

                // flammable media: rare pressure spikes on loud onsets
                if (m.brand > 0.f && envFast > 2.5f * envSlow + 0.02f && rng.uni() < m.brand * 0.02f * (0.3f + cfg.last))
                { const float sp = m.brand * 1.6f * rng.bip(); plant[0].spike = sp; plant[1].spike = sp * 0.8f; larmNow |= LarmBrand; }

                // alarms
                const float pe = std::max (plant[0].peakErr, plant[1].peakErr);
                if (plant[0].u >= 2.45f) larmNow |= LarmHogtryck;
                if (plant[0].u <= -2.45f) larmNow |= LarmLagtryck;
                if (pe > 1.6f) larmNow |= LarmPendling;
                (void) stepPhase;
            }
            if (++ctlCount >= kCtl) ctlCount = 0;
            const float ci = (float) ctlCount / (float) kCtl;

            // ---------------- KOMPRESSOR
            const float dPress = 1.f + 0.35f * std::fabs (plant[0].r - plant[0].y) * cfg.kompressor;
            float sL = inL, sR = inR;
            if (cfg.kompressor > 0.001f)
            {
                // symmetric tanh + a square term for warmth (even harmonics). Zero in = zero out.
                const float tl = sat[0].process (inL * drive * dPress), tr = sat[1].process (inR * drive * dPress);
                sL = dc[0].process ((tl + asym * tl * tl) * makeup);
                sR = dc[1].process ((tr + asym * tr * tr) * makeup);
            }

            // ---------------- EXPANSIONSVENTIL + OLJERETUR + NÖDSTOPP (one varispeed delay)
            gapEnv = std::max (mono, gapEnv * gapDecay);
            gapRef = std::max (gapEnv, gapRef * gapRefDecay);
            const bool quiet = gapEnv < 0.0005f || gapEnv < 0.06f * gapRef;
            const float slew = quiet ? 1.0f : 0.0004f;     // in a gap it may jump; under a sound it barely moves
            dragNow += clampf (dragTarget - dragNow, -slew, slew);
            dly[0].push (sL); dly[1].push (sR);
            advanceStop();
            const float dl = base + wobble * std::tanh (modL * 0.6f) + dragNow + lag;
            const float dr = base + wobble * std::tanh (modR * 0.6f) + dragNow + lag;
            float vL = dly[0].read (std::max (1.f, dl)), vR = dly[1].read (std::max (1.f, dr));
            if (xfade > 0.f)   // restart: crossfade from the lagging head back to real time
            {
                const float liveL = dly[0].read (std::max (1.f, dl - lag)), liveR = dly[1].read (std::max (1.f, dr - lag));
                const float t = 1.f - xfade;
                vL = vL * std::cos (t * 0.5f * kPi) + liveL * std::sin (t * 0.5f * kPi);
                vR = vR * std::cos (t * 0.5f * kPi) + liveR * std::sin (t * 0.5f * kPi);
            }

            // ---------------- FÖRÅNGARE
            const float gl = gPrev[0] + (gNow[0] - gPrev[0]) * ci, gr = gPrev[1] + (gNow[1] - gPrev[1]) * ci;
            float fL = filt[0].lp (vL, gl, kRes), fR = filt[1].lp (vR, gr, kRes);
            const float opL = openPrev[0] + (openNow[0] - openPrev[0]) * ci, opR = openPrev[1] + (openNow[1] - openPrev[1]) * ci;
            fL += opL * (vL - fL); fR += opR * (vR - fR);
            if (cfg.ton >= 0.999f && cfg.forangare < 0.001f && stopState == Kor) { fL = vL; fR = vR; }   // fully open: bypass

            // ---------------- metallic comb while the compressor seizes
            if (stopState != Kor)
            {
                const float amt = 1.f - speed;
                const float cd = (float) sr * (0.0021f + 0.0045f * speed);
                const float cL = comb[0].read (cd), cR = comb[1].read (cd * 1.07f);
                comb[0].push (fL + 0.86f * cL); comb[1].push (fR + 0.86f * cR);
                fL += amt * 0.45f * cL; fR += amt * 0.45f * cR;
                const float duck = std::sqrt (std::max (speed, 0.f));
                fL *= duck; fR *= duck;
            }

            // ---------------- SIKTGLAS: bubbles (flash gas) crush and choke the signal for a moment
            if (bubbleLeft <= 0 && cfg.siktglas > 0.001f)
            {
                const float flash = std::max (0.f, plant[0].y - plant[0].r) * 0.5f + std::max (0.f, -plant[0].u) * 0.15f;
                const float pPerMs = cfg.siktglas * (0.02f + 0.6f * m.bubblor + 0.5f * flash) * (0.3f + 3.f * envFast);
                if (rng.uni() < pPerMs * 1000.f / (float) sr)
                {
                    bubbleLen = bubbleLeft = (int) ((0.002f + 0.020f * rng.uni()) * (float) sr);
                    bubbleDecim = 2 + (int) (rng.uni() * (2.f + 14.f * cfg.siktglas));
                    bubbleDepth = 0.25f + 0.65f * cfg.siktglas * rng.uni();
                    larmNow |= LarmFlashgas;
                }
            }
            if (bubbleLeft > 0)
            {
                const float t = 1.f - (float) bubbleLeft / (float) bubbleLen;
                const float w = 0.5f - 0.5f * std::cos (2.f * kPi * t);          // smooth window: no clicks
                if (holdCount <= 0) { holdL = fL; holdR = fR; holdCount = bubbleDecim; }
                --holdCount;
                const float choke = 1.f - bubbleDepth * w;
                fL = (fL + w * (holdL - fL)) * choke;
                fR = (fR + w * (holdR - fR)) * choke;
                --bubbleLeft;
            }

            // ---------------- HÖGTRYCKSPRESSOSTAT
            float oL = fL * outGain, oR = fR * outGain;
            const float pk = std::max (std::fabs (oL), std::fabs (oR));
            limEnv = pk > limEnv ? pk : limEnv + limRel * (pk - limEnv);
            const float gainLim = limEnv > ceil ? ceil / limEnv : 1.f;
            oL *= gainLim; oR *= gainLim;
            if (gainLim < 0.5f) larmNow |= LarmPressostat;
            grDb = std::max (grDb * 0.9995f, -20.f * std::log10 (std::max (gainLim, 1.0e-4f)));
            oL = clampf (oL, -ceil, ceil); oR = clampf (oR, -ceil, ceil);

            // dry is delayed by the same fixed latency so MIX does not comb
            dly[2].push (inL); dly[3].push (inR);
            const float dryL = dly[2].read (base), dryR = dly[3].read (base);
            L[i] = dryL + wet * (oL - dryL);
            R[i] = dryR + wet * (oR - dryR);

            ppq += ppqPerSample;
        }
        if (! usingHost) ppqFree = ppq;
        if (stopState != Kor) larmNow |= LarmNodstopp;
        larm = larmNow;
    }

    // for the panel
    uint32_t larmBits() const { return larm; }
    float tryckL() const { return modL; }                 // "trycket" (process value)
    float tryckR() const { return modR; }
    float borvardeNu() const { return plant[0].r; }
    float ventilLage() const { return uL; }
    float motorVarv() const { return speed; }
    float gainReductionDb() const { return grDb; }

private:
    enum StopState { Kor, Stannar, Stilla, Startar, Kopplar };

    void onStep (const Koldmedium& m)
    {
        (void) m;
    }

    // motor speed and the lag it causes (varispeed)
    void advanceStop()
    {
        const float dt = 1.f / (float) sr;
        switch (stopState)
        {
            case Kor: speed = 1.f; lag = 0.f; xfade = 0.f; return;
            case Stannar:
            {
                stopTimer += dt;
                const float T = clampf (cfg.stopptid, 0.05f, 2.f);
                const float t = std::min (1.f, stopTimer / T);
                speed = speed0 * std::pow (1.f - t, 1.6f);        // the motor coasts down
                if (t >= 1.f) { stopState = Stilla; speed = 0.f; }
                break;
            }
            case Stilla: speed = 0.f; return;                     // lag frozen: nothing is read
            case Startar:
            {
                stopTimer += dt;
                const float T = clampf (cfg.starttid, 0.05f, 2.f);
                const float t = std::min (1.f, stopTimer / T);
                speed = speed0 + (1.f - speed0) * t * t * (3.f - 2.f * t);   // spins up
                if (t >= 1.f) { stopState = Kopplar; xfade = 1.f; speed = 1.f; }
                break;
            }
            case Kopplar:
                xfade -= dt / 0.03f;
                if (xfade <= 0.f) { stopState = Kor; xfade = 0.f; lag = 0.f; speed = 1.f; return; }
                break;
        }
        lag += 1.f - speed;
        const float maxLag = (float) (4.4 * sr);
        if (lag > maxLag) lag = maxLag;
    }

    double sr = 48000.0, tempo = 120.0, ppq = 0.0, ppqFree = 0.0;
    bool usingHost = false, firstCtl = true;
    Settings cfg;
    Rng rng { 1 };
    std::array<Delay, 4> dly;            // 0,1 wet varispeed; 2,3 dry latency match
    std::array<Delay, 2> comb;
    std::array<Anlaggning, 2> plant;
    std::array<Svf, 2> filt;
    std::array<AdaaTanh, 2> sat;
    std::array<DcBlock, 2> dc;
    std::array<float, 2> gPrev { 0, 0 }, gNow { 0, 0 }, openPrev { 0, 0 }, openNow { 0, 0 };
    float envFast = 0, envSlow = 0;
    int ctlCount = 0; long stepIdx = -1;
    float modL = 0, modR = 0, modLp = 0, modRp = 0, uL = 0, uR = 0;
    float dragNow = 0, dragTarget = 0, gapEnv = 0, gapRef = 0, gapDecay = 0.99f, gapRefDecay = 0.9999f;
    int bubbleLeft = 0, bubbleLen = 1, bubbleDecim = 2, holdCount = 0;
    float bubbleDepth = 0, holdL = 0, holdR = 0;
    StopState stopState = Kor;
    float speed = 1, speed0 = 1, lag = 0, xfade = 0, stopTimer = 0;
    float limEnv = 0, grDb = 0;
    uint32_t larm = 0;
};

} // namespace lackaget
