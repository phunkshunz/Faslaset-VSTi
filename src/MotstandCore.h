// =====================================================================
//  MASKINHALLEN : MOTSTÅND  —  core engine (step 1 of the build)
//  Phunkshunz AUDIO
//
//  Pure C++17, header-only, no JUCE dependency.
//  Everything here is real-time safe: no allocation, no locks.
//
//  Contents:
//    Rng            fast seedable RNG with Gaussian output
//    DieZeit        free-running drift clock + jitter + DJ nudge
//    DrunkRider     asynchronous Brownian stepper for decay/tune
//    PolyTrack      per-instrument step loop with its own length
//    DetroitSmog    master-bus VCA collapse / diode smear
// =====================================================================
#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <numeric>

namespace motstand {

// ---------------------------------------------------------------------
//  Rng — xorshift128+, Box-Muller Gaussian. Seedable so a "bad take"
//  can be recalled exactly (store the seed in the preset).
// ---------------------------------------------------------------------
class Rng {
public:
    explicit Rng(uint64_t seed = 0x909909909ull) { setSeed(seed); }

    void setSeed(uint64_t seed) {
        s0 = splitmix(seed);
        s1 = splitmix(s0);
        hasSpare = false;
    }

    uint64_t next() {
        uint64_t x = s0;
        const uint64_t y = s1;
        s0 = y;
        x ^= x << 23;
        s1 = x ^ y ^ (x >> 17) ^ (y >> 26);
        return s1 + y;
    }

    // [0, 1)
    double uniform() { return (next() >> 11) * (1.0 / 9007199254740992.0); }

    // mean 0, sigma 1
    double gaussian() {
        if (hasSpare) { hasSpare = false; return spare; }
        double u, v, s;
        do {
            u = uniform() * 2.0 - 1.0;
            v = uniform() * 2.0 - 1.0;
            s = u * u + v * v;
        } while (s >= 1.0 || s == 0.0);
        const double m = std::sqrt(-2.0 * std::log(s) / s);
        spare = v * m;
        hasSpare = true;
        return u * m;
    }

private:
    static uint64_t splitmix(uint64_t z) {
        z += 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }
    uint64_t s0 = 1, s1 = 2;
    double spare = 0.0;
    bool hasSpare = false;
};

// ---------------------------------------------------------------------
//  DieZeit — the clock that refuses the grid.
//
//  Two separate kinds of instability, because they sound different:
//   1. Jitter: per-step Gaussian timing error (sigma ~0.6 ms, hard
//      clamp ±1.5 ms). Does NOT accumulate — it's the "loose hands".
//   2. Drift: a slow random walk on the tempo itself (±1.5 % max).
//      This DOES accumulate — the 909 floats away from the decks.
//
//  Nudge works like a DJ jog/pitch bend: while held, the machine runs
//  up to ±4 % faster/slower. Slip (in ms) tells you how far the machine
//  has wandered from the reference tempo, so you can fight it back.
// ---------------------------------------------------------------------
struct StepEvent {
    int     sampleOffset;   // position inside the current block
    int64_t stepIndex;      // global 16th-note counter since start
};

class DieZeit {
public:
    // ---- parameters (set from the processor, any time) ----
    double baseBpm      = 133.0;
    double jitterSigmaMs= 0.6;    // Gaussian sigma
    double jitterMaxMs  = 1.5;    // hard clamp
    double driftAmount  = 0.5;    // 0..1, speed of tempo wander
    double driftMaxPct  = 1.5;    // max tempo offset in percent
    double nudge        = 0.0;    // -1..1, held by the user

    void prepare(double sr, uint64_t seed) {
        sampleRate = sr;
        rng.setSeed(seed);
        reset();
    }

    void reset() {
        now = 0.0;
        stepIndex = 0;
        tempoOffsetPct = 0.0;
        slipSamples = 0.0;
        nextIdeal = 0.0;
        nextFire = 0.0;  // first hit lands immediately, no jitter
    }

    // Fills `out` with the steps that fire inside this block.
    // Returns how many events were written.
    int process(int numSamples, StepEvent* out, int maxOut) {
        // Nudge: shift the future of the clock, exactly like pushing a
        // record. Positive nudge = the machine catches up / rushes.
        const double push = nudge * 0.04 * numSamples;
        nextIdeal   -= push;
        nextFire    -= push;
        slipSamples -= push;

        int n = 0;
        const double blockEnd = now + numSamples;
        while (nextFire < blockEnd && n < maxOut) {
            int off = (int)std::floor(nextFire - now);
            off = std::clamp(off, 0, numSamples - 1);
            out[n++] = { off, stepIndex++ };
            advanceGrid();
        }
        now = blockEnd;
        return n;
    }

    // How far the machine is behind (+) or ahead (-) of the reference.
    double slipMs() const { return slipSamples / sampleRate * 1000.0; }
    double currentBpm() const { return baseBpm * (1.0 + tempoOffsetPct / 100.0); }

private:
    double stepLength(double bpm) const { return 60.0 / bpm / 4.0 * sampleRate; }

    void advanceGrid() {
        // 1. tempo random walk, weakly pulled home so it never sticks
        // (pull of 0.99/step = drifts and returns over ~10 seconds)
        tempoOffsetPct += rng.gaussian() * driftAmount * 0.03;
        tempoOffsetPct *= 0.99;
        tempoOffsetPct  = std::clamp(tempoOffsetPct, -driftMaxPct, driftMaxPct);

        const double baseLen = stepLength(baseBpm);
        const double effLen  = stepLength(currentBpm());
        nextIdeal   += effLen;
        slipSamples += effLen - baseLen;

        // 2. per-step jitter around the (drifting) ideal grid
        double j = rng.gaussian() * jitterSigmaMs;
        j = std::clamp(j, -jitterMaxMs, jitterMaxMs);
        nextFire = nextIdeal + j * 0.001 * sampleRate;
    }

    Rng     rng;
    double  sampleRate     = 48000.0;
    double  now            = 0.0;   // absolute sample time of block start
    double  nextIdeal      = 0.0;   // where the step "should" be
    double  nextFire       = 0.0;   // where it actually lands
    double  tempoOffsetPct = 0.0;
    double  slipSamples    = 0.0;
    int64_t stepIndex      = 0;
};

// ---------------------------------------------------------------------
//  DrunkRider — the invisible hand on the decay and tune knobs.
//
//  Ticks on its OWN irregular clock (not the sequencer's), and moves
//  in small hard steps — never a smooth sweep. On top of that, every
//  trigger adds a micro step, so no two hits are identical.
//  Output is -1..1; the voice maps it to e.g. ±8 % decay.
// ---------------------------------------------------------------------
class DrunkRider {
public:
    float stepMin   = 0.01f;   // 1 % of range per move
    float stepMax   = 0.03f;   // 3 % of range per move
    float pullHome  = 0.015f;  // mean reversion
    float tickMinMs = 37.f;    // async clock, random interval
    float tickMaxMs = 211.f;

    void prepare(double sr, uint64_t seed) {
        sampleRate = sr;
        rng.setSeed(seed);
        value = 0.f;
        samplesToTick = nextInterval();
    }

    // Call once per block.
    void advance(int numSamples) {
        samplesToTick -= numSamples;
        while (samplesToTick <= 0.0) {
            stagger();
            samplesToTick += nextInterval();
        }
    }

    // Call when the voice fires. Returns the value to use for this hit.
    float onTrigger() {
        stagger();
        return value;
    }

    float current() const { return value; }

private:
    void stagger() {
        const float mag = stepMin + (stepMax - stepMin) * (float)rng.uniform();
        const float dir = rng.uniform() < 0.5 ? -1.f : 1.f;
        value += dir * mag - value * pullHome;
        // reflect at the walls instead of clipping (no "stuck" sound)
        if (value >  1.f) value =  2.f - value;
        if (value < -1.f) value = -2.f - value;
    }

    double nextInterval() {
        const double ms = tickMinMs + (tickMaxMs - tickMinMs) * rng.uniform();
        return ms * 0.001 * sampleRate;
    }

    Rng    rng;
    double sampleRate    = 48000.0;
    double samplesToTick = 0.0;
    float  value         = 0.f;
};

// ---------------------------------------------------------------------
//  PolyTrack — one instrument lane with its own loop length.
//  Position is derived from the global step counter, so all lanes stay
//  mathematically locked to each other even while the clock drifts.
// ---------------------------------------------------------------------
constexpr int kMaxSteps = 32;

struct PolyTrack {
    std::array<uint8_t, kMaxSteps> velocity{};   // 0 = rest, 1..127
    int length = 16;                             // 1..32

    int positionAt(int64_t globalStep) const {
        return (int)(globalStep % (int64_t)std::max(1, length));
    }
    uint8_t velocityAt(int64_t globalStep) const {
        return velocity[(size_t)positionAt(globalStep)];
    }
};

// How many 16ths until every lane lands on its "one" at the same time.
template <size_t N>
inline int64_t fullCycleSteps(const std::array<PolyTrack, N>& tracks) {
    int64_t l = 1;
    for (const auto& t : tracks) l = std::lcm(l, (int64_t)std::max(1, t.length));
    return l;
}

// ---------------------------------------------------------------------
//  DetroitSmog — master-bus VCA collapse.
//
//  Voices report their transients with addTransient(). Smog sums the
//  energy of all transients inside a ~1 ms window. Up to `threshold`
//  (default 3 full hits) nothing happens. Above it, the bus "chokes":
//    - rail sag (gain dips, like a starving VCA)
//    - asymmetric diode clipping with rising drive
//    - slew-rate smear that rounds off the attack
//  Collapse is a smoothed 0..1 value, so it never clicks on/off.
//
//  NOTE: in the plugin this runs inside 2x oversampling (step 3).
// ---------------------------------------------------------------------
class DetroitSmog {
public:
    float threshold  = 3.0f;   // "more than three transients"
    float windowMs   = 1.0f;
    float amount     = 1.0f;   // 0..1, user knob
    float baseline   = 0.0f;   // optional always-on dirt (0 = bus is clean until it chokes)

    void prepare(double sr) {
        sampleRate = sr;
        windowSamples = std::max(1, (int)(windowMs * 0.001 * sr));
        attackCoef  = coefFor(0.2);    // ms
        releaseCoef = coefFor(20.0);   // ms
        sagAttack   = coefFor(0.3);
        sagRelease  = coefFor(15.0);
        reset();
    }

    void reset() {
        pendingCount = 0;
        ringCount = 0;
        absTime = 0;
        collapse = sag = 0.f;
        for (auto& s : ch) s = {};
    }

    // energy: 1.0 = a full-velocity hit
    void addTransient(int sampleOffset, float energy) {
        if (pendingCount < kMaxPending)
            pending[pendingCount++] = { sampleOffset, energy };
    }

    void process(float* left, float* right, int numSamples) {
        std::sort(pending.begin(), pending.begin() + pendingCount,
                  [](const Pending& a, const Pending& b){ return a.offset < b.offset; });
        int p = 0;

        for (int i = 0; i < numSamples; ++i, ++absTime) {
            // feed new transients into the 1 ms window
            while (p < pendingCount && pending[p].offset <= i) {
                pushRing(absTime, pending[p].energy);
                ++p;
            }
            // energy inside the window
            float e = 0.f;
            for (int k = 0; k < ringCount; ++k)
                if (absTime - ring[k].time < windowSamples) e += ring[k].energy;

            // smoothed collapse 0..1
            const float target = std::clamp(e - threshold, 0.f, 1.f) * amount;
            const float c = target > collapse ? attackCoef : releaseCoef;
            collapse += (target - collapse) * c;

            // rail sag follows the collapse
            const float sc = collapse > sag ? sagAttack : sagRelease;
            sag += (collapse - sag) * sc;

            const float drive  = 1.f + baseline * 2.f + collapse * 6.f;
            const float gain   = 1.f - 0.35f * sag;
            const float slew   = 1.f - 0.85f * collapse;  // 1 = no smear

            // how much of the choked circuit we hear: 0 = untouched, clean bus
            const float wet = std::clamp(collapse * 3.f + baseline, 0.f, 1.f);

            left[i]  = shade(left[i],  ch[0], drive, gain, slew, wet);
            right[i] = shade(right[i], ch[1], drive, gain, slew, wet);
        }

        pendingCount = 0;
        pruneRing();
    }

    float currentCollapse() const { return collapse; }  // for the GUI meter

private:
    struct ChanState { float slewed = 0.f, dcX = 0.f, dcY = 0.f; };

    float shade(float x, ChanState& s, float drive, float gain, float slew, float wet) {
        // slew limit: max change per sample shrinks as the bus chokes
        const float maxStep = 0.02f + slew * 2.0f;
        const float d = std::clamp(x - s.slewed, -maxStep, maxStep);
        s.slewed += d;

        // asymmetric diode pair (positive side clips harder)
        float u = s.slewed * drive;
        float y = u >= 0.f ? (1.f - std::exp(-u))
                           : -(1.f - std::exp(u * 0.7f)) / 0.7f;
        y = y / std::max(1.f, drive * 0.6f) * gain;

        // DC blocker (the asymmetry creates offset) on the choked path only
        const float out = y - s.dcX + 0.9995f * s.dcY;
        s.dcX = y;
        s.dcY = out;
        return x + (out - x) * wet;
    }

    float coefFor(double ms) const {
        return (float)(1.0 - std::exp(-1.0 / (ms * 0.001 * sampleRate)));
    }

    struct Pending { int offset; float energy; };
    struct Hit     { int64_t time; float energy; };
    static constexpr int kMaxPending = 64;
    static constexpr int kRing       = 32;

    void pushRing(int64_t t, float e) {
        if (ringCount < kRing) ring[ringCount++] = { t, e };
        else { std::rotate(ring.begin(), ring.begin() + 1, ring.end()); ring[kRing - 1] = { t, e }; }
    }
    void pruneRing() {
        int w = 0;
        for (int k = 0; k < ringCount; ++k)
            if (absTime - ring[k].time < windowSamples) ring[w++] = ring[k];
        ringCount = w;
    }

    std::array<Pending, kMaxPending> pending{};
    std::array<Hit, kRing> ring{};
    std::array<ChanState, 2> ch{};
    int     pendingCount = 0, ringCount = 0, windowSamples = 48;
    int64_t absTime = 0;
    double  sampleRate = 48000.0;
    float   collapse = 0.f, sag = 0.f;
    float   attackCoef = 0.f, releaseCoef = 0.f, sagAttack = 0.f, sagRelease = 0.f;
};

} // namespace motstand
