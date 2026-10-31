// =============================================================================
//  FASLÅSET — DSP-kärnan, version 2. Samplad kick + genererad bas, faslåsta.
//
//  Header-only, ren C++17, inga JUCE-beroenden: testerna kompilerar den med g++.
//  Ingen allokering i process(). Inga lås. Allt som kommer utifrån saneras.
//
//  Grundregel: kick och bas låter ALDRIG samtidigt.
//    1. Kicken (ett sample) stängs först när vågformen passerar noll uppåt.
//    2. Basen startar med fas 0 exakt på den nollgenomgången (subsample-precision).
//    3. Varje basnot tonar ut med en mjuk cosinus-release som landar på exakt 0
//       innan nästa händelse. Inga hugg, inga klick.
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace fl
{

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;
constexpr int kMaxBeats = 8;
constexpr int kSlots = 3;   // basnoter per slag (16-del 2, 3, 4)

// -----------------------------------------------------------------------------
//  Basmönstret. Varje ruta är ett atomärt 32-bitarsord, så GUI:t kan ändra
//  medan ljudet spelar utan lås.
// -----------------------------------------------------------------------------
struct Cell
{
    bool on = false;
    int degree = 0;   // skalsteg från grundtonen, -7..+14
    int oct = 0;      // -1, 0, +1
    int vel = 110;    // 1..127

    uint32_t pack() const noexcept
    {
        return (on ? 1u : 0u) | ((uint32_t) (std::clamp (degree, -7, 14) + 64) << 8)
             | ((uint32_t) (std::clamp (oct, -1, 1) + 1) << 16) | ((uint32_t) std::clamp (vel, 1, 127) << 24);
    }
    static Cell unpack (uint32_t w) noexcept
    {
        Cell c;
        c.on = (w & 1u) != 0;
        c.degree = std::clamp ((int) ((w >> 8) & 0xffu) - 64, -7, 14);
        c.oct = std::clamp ((int) ((w >> 16) & 0x3u) - 1, -1, 1);
        c.vel = std::clamp ((int) ((w >> 24) & 0xffu), 1, 127);
        return c;
    }
};

class Pattern
{
public:
    Pattern() { clear(); }
    Cell get (int beat, int slot) const noexcept
    {
        return Cell::unpack (cells[idx (beat, slot)].load (std::memory_order_relaxed));
    }
    void set (int beat, int slot, const Cell& c) noexcept
    {
        cells[idx (beat, slot)].store (c.pack(), std::memory_order_relaxed);
    }
    void clear() noexcept
    {
        for (auto& c : cells) c.store (Cell().pack(), std::memory_order_relaxed);
    }

private:
    static size_t idx (int b, int s) noexcept
    {
        return (size_t) (std::clamp (b, 0, kMaxBeats - 1) * kSlots + std::clamp (s, 0, kSlots - 1));
    }
    std::array<std::atomic<uint32_t>, kMaxBeats * kSlots> cells;
};

// -----------------------------------------------------------------------------
//  Skalor (7 steg, halvtoner från grundtonen)
// -----------------------------------------------------------------------------
constexpr int kNumScales = 6;
inline const int* scaleSteps (int s) noexcept
{
    static const int t[kNumScales][7] = {
        { 0, 1, 3, 5, 7, 8, 10 },   // FRYGISK (psy-klassikern)
        { 0, 2, 3, 5, 7, 8, 10 },   // MOLL
        { 0, 2, 3, 5, 7, 8, 11 },   // HARMONISK MOLL
        { 0, 1, 4, 5, 7, 8, 10 },   // FRYGISK DUR (österländsk)
        { 0, 2, 3, 5, 7, 9, 10 },   // DORISK
        { 0, 3, 5, 7, 10, 12, 15 }, // PENTATON MOLL
    };
    return t[std::clamp (s, 0, kNumScales - 1)];
}

inline int degreeToSemis (int scale, int degree) noexcept
{
    const int* st = scaleSteps (scale);
    int o = 0;
    while (degree < 0) { degree += 7; --o; }
    while (degree >= 7) { degree -= 7; ++o; }
    return st[degree] + 12 * o;
}

// -----------------------------------------------------------------------------
//  Kick-sample: mono, oföränderligt när det väl laddats.
// -----------------------------------------------------------------------------
struct KickSample
{
    const float* data = nullptr;
    int length = 0;
    double sampleRate = 48000.0;
};

// -----------------------------------------------------------------------------
//  Oscilloskop: lock-free ring. Ljudtråden skriver, GUI-tråden läser.
// -----------------------------------------------------------------------------
enum ScopeFlags : uint32_t
{
    kKickStart = 1u,
    kBassStart = 2u
};

struct ScopeFrame
{
    float kick = 0.0f;
    float bass = 0.0f;
    uint32_t flags = 0;
};

class ScopeFifo
{
public:
    static constexpr uint32_t kSize = 16384;
    static constexpr uint32_t kMask = kSize - 1;

    inline void push (float k, float b, uint32_t flags) noexcept
    {
        const uint32_t w = writeIdx.load (std::memory_order_relaxed);
        const uint32_t r = readIdx.load (std::memory_order_acquire);
        if (w - r >= kSize) return;
        auto& f = buffer[w & kMask];
        f.kick = k;
        f.bass = b;
        f.flags = flags;
        writeIdx.store (w + 1, std::memory_order_release);
    }

    inline uint32_t pop (ScopeFrame* dest, uint32_t maxFrames) noexcept
    {
        if (dest == nullptr || maxFrames == 0) return 0;
        const uint32_t r = readIdx.load (std::memory_order_relaxed);
        const uint32_t w = writeIdx.load (std::memory_order_acquire);
        const uint32_t n = std::min (w - r, maxFrames);
        for (uint32_t i = 0; i < n; ++i)
            dest[i] = buffer[(r + i) & kMask];
        readIdx.store (r + n, std::memory_order_release);
        return n;
    }

private:
    std::array<ScopeFrame, kSize> buffer {};
    std::atomic<uint32_t> writeIdx { 0 };
    std::atomic<uint32_t> readIdx { 0 };
};

// -----------------------------------------------------------------------------
//  Indata
// -----------------------------------------------------------------------------
struct Transport
{
    bool isPlaying = false;
    double bpm = 0.0;
    double ppqAtStart = 0.0;
};

enum class Mode : int
{
    Spikrakt = 0,   // psy: K-B-B-B, en basnot per 16-del enligt mönstret
    Slap = 1        // techno: en lång basnot per slag, startar på kickens nollgenomgång
};

struct Params
{
    Mode mode = Mode::Spikrakt;

    // KICK
    float kickTune = 0.0f;        // halvtoner
    float kickLengthMs = 85.0f;   // efter detta stängs kicken på nästa nollgenomgång
    float kickDrive = 1.0f;
    float kickLevel = 1.0f;

    // BAS
    int rootNote = 29;            // MIDI (29 = F1)
    int scale = 0;
    int beats = 4;                // mönsterlängd i slag, 1..8
    float bassForm = 0.85f;       // 0 = sinus, 1 = såg
    float bassCutoff = 180.0f;
    float bassReso = 0.3f;
    float bassEnv = 0.7f;
    float bassEnvMs = 60.0f;
    float bassDecayMs = 120.0f;
    float bassGate = 0.85f;       // notlängd, andel av platsen
    float bassDrive = 1.4f;
    float bassLevel = 0.8f;

    // MASTER
    float releaseMs = 3.0f;       // mjuk utklingning i slutet av varje basnot
    float outGain = 0.8f;
};

inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// Mjuk mättning som bevarar nollgenomgångar: f(0) = 0, f(-x) = -f(x).
inline double shape (double x, double drive, double norm) noexcept
{
    return drive <= 1.001 ? x : std::tanh (drive * x) * norm;
}

// PolyBLEP: tar bort vikningsdistorsionen från sågens hopp.
inline double polyBlep (double t, double dt) noexcept
{
    if (t < dt) { const double x = t / dt; return x + x - x * x - 1.0; }
    if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
    return 0.0;
}

// -----------------------------------------------------------------------------
//  Motorn
// -----------------------------------------------------------------------------
class Engine
{
public:
    // ---------------------------------------------------------------- livscykel
    void prepare (double sampleRate) noexcept
    {
        if (! std::isfinite (sampleRate) || sampleRate < 8000.0 || sampleRate > 768000.0)
            sampleRate = 48000.0;
        sr = sampleRate;
        invSr = 1.0 / sr;
        prepared = true;
        setParams (p);
        reset();
    }

    void reset() noexcept
    {
        hardSilence();
        synced = false;
        expectedStep = 0.0;
        lastStep = 0;
        holdoff = 0;
    }

    void setPattern (const Pattern* pat) noexcept { pattern = pat; }

    // Kicken byts bara mellan block (anropas från ljudtråden före process).
    void setKick (const KickSample& k) noexcept
    {
        const bool valid = k.data != nullptr && k.length > 16 && std::isfinite (k.sampleRate)
                        && k.sampleRate >= 8000.0 && k.sampleRate <= 384000.0;
        if (valid && k.data == kick.data && k.length == kick.length) return;
        kick = valid ? k : KickSample {};
        if (kickState != KickState::Idle) startKickRelease (std::max (8, releaseSamples / 2));
    }

    // ---------------------------------------------------------------- parametrar
    void setParams (const Params& in) noexcept
    {
        const float nyq = (float) (sr * 0.45);
        auto f = [] (float v, float lo, float hi, float fb) {
            if (! std::isfinite (v)) return fb;
            return std::min (hi, std::max (lo, v));
        };

        Params s;
        s.mode = in.mode == Mode::Slap ? Mode::Slap : Mode::Spikrakt;
        s.kickTune = f (in.kickTune, -12.0f, 12.0f, 0.0f);
        s.kickLengthMs = f (in.kickLengthMs, 20.0f, 1000.0f, 85.0f);
        s.kickDrive = f (in.kickDrive, 1.0f, 8.0f, 1.0f);
        s.kickLevel = f (in.kickLevel, 0.0f, 1.5f, 1.0f);
        s.rootNote = std::clamp (in.rootNote, 21, 48);
        s.scale = std::clamp (in.scale, 0, kNumScales - 1);
        s.beats = std::clamp (in.beats, 1, kMaxBeats);
        s.bassForm = f (in.bassForm, 0.0f, 1.0f, 0.0f);
        s.bassCutoff = f (in.bassCutoff, 30.0f, std::min (8000.0f, nyq), 180.0f);
        s.bassReso = f (in.bassReso, 0.0f, 0.9f, 0.0f);
        s.bassEnv = f (in.bassEnv, 0.0f, 1.0f, 0.0f);
        s.bassEnvMs = f (in.bassEnvMs, 5.0f, 1000.0f, 60.0f);
        s.bassDecayMs = f (in.bassDecayMs, 10.0f, 4000.0f, 120.0f);
        s.bassGate = f (in.bassGate, 0.2f, 1.0f, 0.85f);
        s.bassDrive = f (in.bassDrive, 1.0f, 8.0f, 1.0f);
        s.bassLevel = f (in.bassLevel, 0.0f, 1.0f, 0.8f);
        s.releaseMs = f (in.releaseMs, 1.0f, 12.0f, 3.0f);
        s.outGain = f (in.outGain, 0.0f, 1.0f, 0.8f);
        p = s;

        bassAmpCoeff = std::exp (-1.0 / (p.bassDecayMs * 0.001 * sr));
        bassEnvCoeff = std::exp (-1.0 / (p.bassEnvMs * 0.001 * sr));
        svfK = 2.0 - 2.0 * p.bassReso;
        kickNorm = p.kickDrive > 1.001 ? 1.0 / std::tanh ((double) p.kickDrive) : 1.0;
        bassNorm = p.bassDrive > 1.001 ? 1.0 / std::tanh ((double) p.bassDrive) : 1.0;
        releaseSamples = std::max (16, (int) (p.releaseMs * 0.001 * sr));
        kickLenParam = (int64_t) (p.kickLengthMs * 0.001 * sr);
        kickRateMul = std::pow (2.0, p.kickTune / 12.0);
    }

    const Params& params() const noexcept { return p; }

    // ---------------------------------------------------------------- ljud
    bool process (float* out, int numSamples, const Transport& t,
                  float* kickOut = nullptr, float* bassOut = nullptr) noexcept
    {
        if (out == nullptr || numSamples <= 0) return true;

        const bool ok = prepared && t.isPlaying
                     && std::isfinite (t.bpm) && t.bpm >= 30.0 && t.bpm <= 300.0
                     && std::isfinite (t.ppqAtStart) && t.ppqAtStart > -1.0e7 && t.ppqAtStart < 1.0e9;
        if (! ok)
        {
            // stopp: tona ut det som låter i stället för att hugga
            if (kickState == KickState::Idle && bassState == BassState::Idle)
            {
                silence (out, kickOut, bassOut, 0, numSamples);
                synced = false;
                return true;
            }
            startKickRelease (releaseSamples);
            startBassRelease (releaseSamples);
            for (int i = 0; i < numSamples; ++i)
            {
                double dummy = 0.0;
                renderKick (dummy);
                const double k = lastKick, b = renderBass();
                writeSample (out, kickOut, bassOut, i, k, b, 0);
            }
            synced = false;
            return true;
        }

        // ---- klockan: 16-delar per sample
        const double sps = (t.bpm / 60.0) * 4.0 * invSr;
        const double invSps = 1.0 / sps;
        const double stepAtStart = t.ppqAtStart * 4.0;

        // Riktigt hopp (loop, scrub)? Mer än en 32-del fel. Små avvikelser från värden
        // ignoreras: klockan går bara framåt, så inget slag kan triggas två gånger.
        if (synced && std::fabs (stepAtStart - expectedStep) > 0.5)
        {
            startKickRelease (releaseSamples / 2);
            startBassRelease (releaseSamples / 2);
            bassPending = false;
            synced = false;
            holdoff = releaseSamples / 2 + 2;
        }
        if (! synced)
        {
            const int64_t s = (int64_t) std::floor (stepAtStart);
            const double frac = stepAtStart - (double) s;
            lastStep = (frac < sps && holdoff == 0) ? s - 1 : s;
            synced = true;
        }

        kickLenSamples = std::min (kickLenParam, (int64_t) (0.75 * 4.0 * invSps));
        const bool spik = p.mode == Mode::Spikrakt;
        const double R = (double) releaseSamples;

        for (int i = 0; i < numSamples; ++i)
        {
            uint32_t flags = 0;
            const double stepPos = stepAtStart + (double) i * sps;
            const double stepFloor = std::floor (stepPos);
            const int64_t step = (int64_t) stepFloor;
            const double sinceStep = (stepPos - stepFloor) * invSps;
            const int64_t beatNo = (int64_t) std::floor (stepFloor * 0.25);
            const int patBeat = (int) (((beatNo % p.beats) + p.beats) % p.beats);

            if (holdoff > 0)
            {
                --holdoff;
                lastStep = std::max (lastStep, step);
            }
            else if (step > lastStep)
            {
                lastStep = step;
                const int inBeat = (int) (((step % 4) + 4) % 4);
                currentBeat.store (patBeat, std::memory_order_relaxed);
                if (inBeat == 0)
                {
                    triggerKick (sinceStep);
                    flags |= kKickStart;
                    if (! kickPlaying()) armSlapBass (patBeat, sinceStep);
                }
                else if (spik)
                {
                    const Cell c = cellAt (patBeat, inBeat - 1);
                    if (c.on) arm (c, sinceStep);
                }
            }

            const double samplesToStep = (stepFloor + 1.0 - stepPos) * invSps;
            const double samplesToBeat = ((std::floor (stepPos * 0.25) + 1.0) * 4.0 - stepPos) * invSps;
            const double samplesToTarget = spik ? samplesToStep : samplesToBeat;

            // basen startar när kicken är död och det finns plats för noten + releasen
            if (bassPending && ! kickPlaying() && bassState == BassState::Idle)
            {
                if (samplesToTarget > 3.0 * R)
                {
                    startBass (pendingOffset, samplesToTarget);
                    flags |= kBassStart;
                }
                bassPending = false;
            }

            // sista utposten: releasen måste vara klar före nästa händelse
            if (bassState == BassState::Running && samplesToTarget <= R + 1.0)
                startBassRelease (std::max (1, (int) std::floor (samplesToTarget) - 1));
            if (kickState != KickState::Idle && kickState != KickState::Release && samplesToBeat <= R + 1.0)
                startKickRelease (std::max (1, (int) std::floor (samplesToBeat) - 1));

            double kickEndOffset = 0.0;
            const bool kickEnded = renderKick (kickEndOffset);
            const double k = lastKick;

            if (kickEnded)
            {
                if (spik) { if (bassPending) pendingOffset = kickEndOffset; }
                else armSlapBass (patBeat, kickEndOffset);
            }

            const double b = renderBass();
            if (! writeSample (out, kickOut, bassOut, i, k, b, flags))
            {
                silence (out, kickOut, bassOut, i, numSamples);
                hardSilence();
                synced = false;
                faultCount.fetch_add (1, std::memory_order_relaxed);
                return false;
            }
        }

        expectedStep = stepAtStart + (double) numSamples * sps;
        return true;
    }

    ScopeFifo& scope() noexcept { return scopeFifo; }

    std::atomic<uint32_t> faultCount { 0 };
    std::atomic<uint32_t> overlapCount { 0 };
    std::atomic<int> currentBeat { 0 };

private:
    enum class KickState { Idle, Running, WaitZero, Release };
    enum class BassState { Idle, Running, Release };

    bool kickPlaying() const noexcept { return kickState != KickState::Idle; }

    Cell cellAt (int beat, int slot) const noexcept
    {
        if (pattern == nullptr)
        {
            Cell c;
            c.on = true;   // utan mönster: grundtonen på alla platser
            return c;
        }
        return pattern->get (beat, slot);
    }

    void arm (const Cell& c, double offset) noexcept
    {
        bassPending = true;
        pendingOffset = offset;
        pendingCell = c;
    }

    void armSlapBass (int beat, double offset) noexcept
    {
        if (p.mode != Mode::Slap) return;
        const Cell c = cellAt (beat, 0);
        if (c.on) arm (c, offset);
    }

    static void silence (float* out, float* k, float* b, int from, int to) noexcept
    {
        std::fill (out + from, out + to, 0.0f);
        if (k) std::fill (k + from, k + to, 0.0f);
        if (b) std::fill (b + from, b + to, 0.0f);
    }

    bool writeSample (float* out, float* kickOut, float* bassOut, int i, double k, double b, uint32_t flags) noexcept
    {
        if (k != 0.0 && b != 0.0)
            overlapCount.fetch_add (1, std::memory_order_relaxed);   // ska aldrig hända

        gainSmooth += (p.outGain - gainSmooth) * 0.002;
        double y = (k * p.kickLevel + b * p.bassLevel) * gainSmooth;
        if (! std::isfinite (y) || ! std::isfinite (k) || ! std::isfinite (b)) return false;

        // mjuk mättning över 0 dBFS i stället för ett hårt tak
        if (std::fabs (y) > 0.9) y = (y > 0 ? 1.0 : -1.0) * (0.9 + 0.1 * std::tanh ((std::fabs (y) - 0.9) * 10.0));
        out[i] = (float) y;
        if (kickOut) kickOut[i] = (float) k;
        if (bassOut) bassOut[i] = (float) b;
        scopeFifo.push ((float) (k * p.kickLevel), (float) (b * p.bassLevel), flags);
        return true;
    }

    void hardSilence() noexcept
    {
        kickState = KickState::Idle;
        kickPos = lastKick = kickPrevRaw = 0.0;
        kickAge = kickRelLeft = kickRelLen = 0;
        stopBass();
        bassPending = false;
    }

    void stopBass() noexcept
    {
        bassState = BassState::Idle;
        bassPhase = bassAmp = bassEnvV = 0.0;
        bassRelLeft = bassRelLen = 0;
        ic1 = ic2 = 0.0;
    }

    // ---------------------------------------------------------------- kick
    void triggerKick (double offset) noexcept
    {
        bassPending = false;
        if (bassState != BassState::Idle)
        {
            overlapCount.fetch_add (1, std::memory_order_relaxed);
            stopBass();
        }
        if (kick.data == nullptr) { kickState = KickState::Idle; return; }
        kickState = KickState::Running;
        kickRate = kick.sampleRate * invSr * kickRateMul;
        kickPos = std::min (0.5, offset * kickRate);
        kickAge = 0;
        kickPrevRaw = 0.0;
        kickRelLeft = kickRelLen = 0;
    }

    void startKickRelease (int len) noexcept
    {
        if (kickState == KickState::Idle) return;
        len = std::max (1, len);
        if (kickState == KickState::Release && kickRelLeft <= len) return;
        kickState = KickState::Release;
        kickRelLen = kickRelLeft = len;
    }

    // Returnerar true när kicken stängdes på en nollgenomgång i detta sample.
    // endOffset = hur långt efter nollgenomgången nästa sample ligger (0..1).
    bool renderKick (double& endOffset) noexcept
    {
        lastKick = 0.0;
        if (kickState == KickState::Idle) return false;

        if (kick.data == nullptr || kickPos >= (double) (kick.length - 1))
        {
            kickState = KickState::Idle;   // samplet slut (laddaren tonar ut svansen)
            endOffset = 0.0;
            return true;
        }

        const int i0 = (int) kickPos;
        const double fr = kickPos - (double) i0;
        const double raw = kick.data[i0] + (kick.data[i0 + 1] - kick.data[i0]) * fr;

        if (kickState == KickState::WaitZero && kickPrevRaw <= 0.0 && raw > 0.0)
        {
            // nollgenomgång uppåt mellan förra samplet och detta: stäng här
            const double x = kickPrevRaw / (kickPrevRaw - raw);   // 0..1 sedan förra samplet
            endOffset = std::clamp (1.0 - x, 0.0, 1.0);
            kickState = KickState::Idle;
            return true;
        }

        double g = 1.0;
        if (kickState == KickState::Release)
        {
            g = 0.5 * (1.0 + std::cos (kPi * (double) (kickRelLen - kickRelLeft) / (double) kickRelLen));
            if (--kickRelLeft <= 0)
            {
                kickState = KickState::Idle;
                endOffset = 0.0;
                return true;
            }
        }

        lastKick = shape (raw, p.kickDrive, kickNorm) * g;
        kickPrevRaw = raw;
        kickPos += kickRate;
        ++kickAge;

        if (kickState == KickState::Running && kickAge >= kickLenSamples)
            kickState = KickState::WaitZero;
        // väntar den för länge (sample med DC eller tystnad): tona ut i stället
        if (kickState == KickState::WaitZero && kickAge >= kickLenSamples + (int64_t) (0.03 * sr))
            startKickRelease (releaseSamples);
        return false;
    }

    // ---------------------------------------------------------------- bas
    void startBass (double offset, double samplesToTarget) noexcept
    {
        const Cell c = pendingCell;
        const int semis = degreeToSemis (p.scale, c.degree) + 12 * c.oct;
        const double note = std::clamp ((double) (p.rootNote + semis), 12.0, 84.0);
        bassHz = std::min (sr * 0.45, midiToHz (note));
        bassInc = bassHz * invSr;

        bassState = BassState::Running;
        bassPhase = std::min (0.25, offset * bassInc);   // fas 0 ligger exakt på nollpunkten
        bassAmp = 0.35 + 0.65 * (double) c.vel / 127.0;
        bassEnvV = 1.0;
        ic1 = ic2 = 0.0;

        // notlängd: andel av platsen fram till nästa händelse, minus releasen
        const double room = std::max (0.0, samplesToTarget - (double) releaseSamples - 1.0);
        bassGateLeft = (int64_t) std::max (1.0, room * p.bassGate);
        bassRelLeft = bassRelLen = 0;
    }

    void startBassRelease (int len) noexcept
    {
        if (bassState == BassState::Idle) return;
        len = std::max (1, len);
        if (bassState == BassState::Release && bassRelLeft <= len) return;
        bassState = BassState::Release;
        bassRelLen = bassRelLeft = len;
    }

    double renderBass() noexcept
    {
        if (bassState == BassState::Idle) return 0.0;

        // sinus -> bandbegränsad såg. Båda börjar på 0 och går uppåt.
        const double ph = bassPhase;
        double psi = ph + 0.5;
        if (psi >= 1.0) psi -= 1.0;
        const double saw = (2.0 * psi - 1.0) - polyBlep (psi, bassInc);
        const double osc = (1.0 - p.bassForm) * std::sin (kTwoPi * ph) + p.bassForm * saw;

        // TPT-tillståndsfilter (lågpass), nollställt vid varje start
        const double fc = std::min (sr * 0.45, (double) p.bassCutoff * std::exp2 (5.0 * p.bassEnv * bassEnvV));
        const double g = std::tan (kPi * fc * invSr);
        const double a1 = 1.0 / (1.0 + g * (g + svfK));
        const double a2 = g * a1;
        const double a3 = g * a2;
        const double v3 = osc - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;

        double rel = 1.0;
        if (bassState == BassState::Release)
            rel = 0.5 * (1.0 + std::cos (kPi * (double) (bassRelLen - bassRelLeft) / (double) bassRelLen));

        const double y = shape (v2, p.bassDrive, bassNorm) * bassAmp * rel;

        bassPhase += bassInc;
        if (bassPhase >= 1.0) bassPhase -= 1.0;
        bassAmp *= bassAmpCoeff;
        bassEnvV *= bassEnvCoeff;

        if (! std::isfinite (ic1) || ! std::isfinite (ic2))
        {
            stopBass();
            faultCount.fetch_add (1, std::memory_order_relaxed);
            return 0.0;
        }

        if (bassState == BassState::Release)
        {
            if (--bassRelLeft <= 0) stopBass();
        }
        else if (--bassGateLeft <= 0 || bassAmp < 1.0e-4)
        {
            startBassRelease (releaseSamples);
        }
        return y;
    }

    // miljö
    double sr = 48000.0, invSr = 1.0 / 48000.0;
    bool prepared = false;

    // parametrar
    Params p {};
    double bassAmpCoeff = 0.0, bassEnvCoeff = 0.0, svfK = 2.0, kickNorm = 1.0, bassNorm = 1.0;
    double kickRateMul = 1.0;
    int64_t kickLenParam = 0, kickLenSamples = 0;
    int releaseSamples = 144;
    double gainSmooth = 0.0;
    const Pattern* pattern = nullptr;
    KickSample kick {};

    // klocka
    bool synced = false;
    double expectedStep = 0.0;
    int64_t lastStep = 0;
    int holdoff = 0;

    // kick
    KickState kickState = KickState::Idle;
    double kickPos = 0.0, kickRate = 1.0, lastKick = 0.0, kickPrevRaw = 0.0;
    int64_t kickAge = 0;
    int kickRelLeft = 0, kickRelLen = 0;

    // bas
    BassState bassState = BassState::Idle;
    bool bassPending = false;
    double pendingOffset = 0.0;
    Cell pendingCell {};
    double bassPhase = 0.0, bassAmp = 0.0, bassEnvV = 0.0, bassHz = 43.65, bassInc = 0.001;
    int64_t bassGateLeft = 0;
    int bassRelLeft = 0, bassRelLen = 0;
    double ic1 = 0.0, ic2 = 0.0;

    ScopeFifo scopeFifo;
};

} // namespace fl
