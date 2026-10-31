// =============================================================================
//  FASLÅSET — DSP-kärnan. Kick + bas, faslåsta, sample-exakt.
//
//  Header-only, ren C++17, inga JUCE-beroenden: testerna kompilerar den med g++.
//  Ingen allokering i process(). Inga lås. Allt som kommer utifrån saneras.
//
//  Grundregel: kick och bas låter ALDRIG samtidigt.
//    1. Kicken stängs först när dess sinus passerar noll uppåt.
//    2. Basen startar med fas 0 exakt på den nollgenomgången (med subsample-precision).
//    3. Giljotinen rampar ner basen så att den är exakt 0 innan nästa händelse.
// =============================================================================
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

namespace fl
{

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

// -----------------------------------------------------------------------------
//  Oscilloskop: lock-free ring. Ljudtråden skriver, GUI-tråden läser.
//  Full ring = ramar kastas. Ljudtråden väntar aldrig på grafiken.
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
    static constexpr uint32_t kSize = 16384;   // 2^n
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
    double ppqAtStart = 0.0;   // kvartsnoter vid blockets första sample
};

enum class Mode : int
{
    Spikrakt = 0,   // psy: K-B-B-B, varje 16-del huggs av före nästa
    Slap = 1        // techno: en lång sub per slag, startar på kickens nollgenomgång
};

struct Params
{
    Mode mode = Mode::Spikrakt;

    // KICK
    float kickStartHz = 220.0f;
    float kickEndHz = 50.0f;
    float kickPitchMs = 18.0f;    // tidskonstant för pitch-svepet
    float kickLengthMs = 90.0f;   // tid till -60 dB
    float kickDrive = 1.5f;       // 1 = ren
    float kickLevel = 1.0f;

    // BAS
    float bassNote = 29.0f;       // MIDI-not (29 = F1)
    float bassForm = 0.85f;       // 0 = sinus, 1 = såg
    float bassCutoff = 180.0f;    // Hz
    float bassReso = 0.3f;        // 0..0.9
    float bassEnv = 0.7f;         // filterenvelope, 0..1 (upp till 5 oktaver)
    float bassEnvMs = 60.0f;      // filterenvelopens tidskonstant
    float bassDecayMs = 90.0f;    // volymens tidskonstant
    float bassDrive = 1.4f;       // 1 = ren
    float bassLevel = 0.8f;

    // MASTER
    float guillotineMs = 0.7f;
    float outGain = 0.8f;
};

inline double midiToHz (double note) { return 440.0 * std::pow (2.0, (note - 69.0) / 12.0); }

// Mjuk mättning som bevarar nollgenomgångar: f(0) = 0, f(-x) = -f(x).
inline double shape (double x, double drive, double norm) noexcept
{
    return drive <= 1.001 ? x : std::tanh (drive * x) * norm;
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
    }

    // ---------------------------------------------------------------- parametrar (ljudtråden, före process)
    void setParams (const Params& in) noexcept
    {
        const float nyq = (float) (sr * 0.45);
        auto f = [] (float v, float lo, float hi, float fb) {
            if (! std::isfinite (v)) return fb;
            return std::min (hi, std::max (lo, v));
        };

        Params s;
        s.mode = in.mode == Mode::Slap ? Mode::Slap : Mode::Spikrakt;
        s.kickStartHz = f (in.kickStartHz, 30.0f, std::min (1000.0f, nyq), 220.0f);
        s.kickEndHz = f (in.kickEndHz, 25.0f, std::min (150.0f, nyq), 50.0f);
        s.kickPitchMs = f (in.kickPitchMs, 2.0f, 200.0f, 18.0f);
        s.kickLengthMs = f (in.kickLengthMs, 20.0f, 600.0f, 90.0f);
        s.kickDrive = f (in.kickDrive, 1.0f, 8.0f, 1.0f);
        s.kickLevel = f (in.kickLevel, 0.0f, 1.0f, 1.0f);
        s.bassNote = f (in.bassNote, 21.0f, 60.0f, 29.0f);
        s.bassForm = f (in.bassForm, 0.0f, 1.0f, 0.0f);
        s.bassCutoff = f (in.bassCutoff, 30.0f, std::min (8000.0f, nyq), 180.0f);
        s.bassReso = f (in.bassReso, 0.0f, 0.9f, 0.0f);
        s.bassEnv = f (in.bassEnv, 0.0f, 1.0f, 0.0f);
        s.bassEnvMs = f (in.bassEnvMs, 5.0f, 1000.0f, 60.0f);
        s.bassDecayMs = f (in.bassDecayMs, 10.0f, 4000.0f, 90.0f);
        s.bassDrive = f (in.bassDrive, 1.0f, 8.0f, 1.0f);
        s.bassLevel = f (in.bassLevel, 0.0f, 1.0f, 0.8f);
        s.guillotineMs = f (in.guillotineMs, 0.2f, 5.0f, 0.7f);
        s.outGain = f (in.outGain, 0.0f, 1.0f, 0.8f);
        p = s;

        kickPitchCoeff = std::exp (-1.0 / (p.kickPitchMs * 0.001 * sr));
        const double kickTau = (p.kickLengthMs * 0.001) / 6.907755278982137; // -60 dB
        kickAmpCoeff = std::exp (-1.0 / (kickTau * sr));
        kickLenParam = (int64_t) (p.kickLengthMs * 0.001 * sr);
        bassAmpCoeff = std::exp (-1.0 / (p.bassDecayMs * 0.001 * sr));
        bassEnvCoeff = std::exp (-1.0 / (p.bassEnvMs * 0.001 * sr));
        bassHz = std::min ((double) nyq, midiToHz (p.bassNote));
        bassInc = bassHz * invSr;
        svfK = 2.0 - 2.0 * p.bassReso;
        kickNorm = p.kickDrive > 1.001 ? 1.0 / std::tanh ((double) p.kickDrive) : 1.0;
        bassNorm = p.bassDrive > 1.001 ? 1.0 / std::tanh ((double) p.bassDrive) : 1.0;
        guillotineSamples = std::max (4, (int) (p.guillotineMs * 0.001 * sr));
    }

    const Params& params() const noexcept { return p; }

    // ---------------------------------------------------------------- ljud
    // Skriver mono till 'out'. kickOut/bassOut (valfria) får de separata signalerna
    // före master-gain, för tester. Returnerar false om felspärren löste ut.
    bool process (float* out, int numSamples, const Transport& t,
                  float* kickOut = nullptr, float* bassOut = nullptr) noexcept
    {
        if (out == nullptr || numSamples <= 0) return true;

        const bool ok = prepared && t.isPlaying
                     && std::isfinite (t.bpm) && t.bpm >= 30.0 && t.bpm <= 300.0
                     && std::isfinite (t.ppqAtStart) && t.ppqAtStart > -1.0e7 && t.ppqAtStart < 1.0e9;
        if (! ok)
        {
            silence (out, kickOut, bassOut, 0, numSamples);
            hardSilence();
            synced = false;
            return true;
        }

        // ---- klockan: 16-delar per sample
        const double sps = (t.bpm / 60.0) * 4.0 * invSr;
        const double invSps = 1.0 / sps;
        const double stepAtStart = t.ppqAtStart * 4.0;

        // hopp i tidslinjen (loop, scrub): tvärstopp och synka om
        if (synced && std::fabs (stepAtStart - expectedStep) > sps * 2.0)
        {
            hardSilence();
            synced = false;
        }
        if (! synced)
        {
            const int64_t s = (int64_t) std::floor (stepAtStart);
            const double frac = stepAtStart - (double) s;
            lastStep = frac < sps ? s - 1 : s;   // står vi på en gräns: trigga den
            synced = true;
        }

        // kicken får aldrig vara längre än 3/4 slag (svansen behöver plats att landa på noll)
        kickLenSamples = std::min (kickLenParam, (int64_t) (0.75 * 4.0 * invSps));

        const bool spik = p.mode == Mode::Spikrakt;
        const double G = (double) guillotineSamples;

        for (int i = 0; i < numSamples; ++i)
        {
            uint32_t flags = 0;
            const double stepPos = stepAtStart + (double) i * sps;
            const double stepFloor = std::floor (stepPos);
            const int64_t step = (int64_t) stepFloor;
            const double sinceStep = (stepPos - stepFloor) * invSps;   // samples sedan gränsen, 0..1

            if (step != lastStep)
            {
                lastStep = step;
                const int64_t inBeat = ((step % 4) + 4) % 4;
                if (inBeat == 0)
                {
                    triggerKick (sinceStep);
                    flags |= kKickStart;
                }
                else if (spik)
                {
                    bassPending = true;
                    pendingOffset = sinceStep;
                }
            }

            const double samplesToStep = (stepFloor + 1.0 - stepPos) * invSps;
            const double samplesToBeat = ((std::floor (stepPos * 0.25) + 1.0) * 4.0 - stepPos) * invSps;
            const double samplesToTarget = spik ? samplesToStep : samplesToBeat;

            // basen startar (kicken måste vara död, och det måste finnas plats för en hel ramp)
            if (bassPending && kickState == KickState::Idle && bassState == BassState::Idle)
            {
                if (samplesToTarget > 2.0 * G)
                {
                    startBass (pendingOffset);
                    flags |= kBassStart;
                }
                bassPending = false;
            }

            // giljotinen: rampen landar på 0 innan nästa händelse
            if (bassState == BassState::Running && samplesToTarget <= G)
            {
                bassState = BassState::Guillotine;
                bassGateLeft = std::max (1, (int) std::floor (samplesToTarget));
                bassGateStep = bassGate / (double) bassGateLeft;
            }
            // samma skydd för kicken, om den mot förmodan fortfarande lever vid nästa slag
            if (kickState != KickState::Idle && ! kickFading && samplesToBeat <= G)
            {
                kickFading = true;
                kickGateLeft = std::max (1, (int) std::floor (samplesToBeat));
                kickGateStep = kickGate / (double) kickGateLeft;
            }

            double kickEndOffset = 0.0;
            const bool kickEnded = renderKick (kickEndOffset);
            const double k = lastKick;

            if (kickEnded && (! spik || bassPending))
            {
                bassPending = true;
                pendingOffset = kickEndOffset;   // startar nästa sample, exakt fasrätt
            }

            const double b = renderBass();

            if (k != 0.0 && b != 0.0)
                overlapCount.fetch_add (1, std::memory_order_relaxed);   // ska aldrig hända

            gainSmooth += (p.outGain - gainSmooth) * 0.002;
            double y = (k * p.kickLevel + b * p.bassLevel) * gainSmooth;

            // felspärr: NaN/Inf nollar resten av blocket och återställer allt
            if (! std::isfinite (y) || ! std::isfinite (k) || ! std::isfinite (b))
            {
                silence (out, kickOut, bassOut, i, numSamples);
                hardSilence();
                synced = false;
                faultCount.fetch_add (1, std::memory_order_relaxed);
                return false;
            }

            y = std::min (1.0, std::max (-1.0, y));   // hårt tak, sista utposten
            out[i] = (float) y;
            if (kickOut) kickOut[i] = (float) k;
            if (bassOut) bassOut[i] = (float) b;
            scopeFifo.push ((float) (k * p.kickLevel), (float) (b * p.bassLevel), flags);
        }

        expectedStep = stepAtStart + (double) numSamples * sps;
        return true;
    }

    ScopeFifo& scope() noexcept { return scopeFifo; }

    std::atomic<uint32_t> faultCount { 0 };
    std::atomic<uint32_t> overlapCount { 0 };   // samples där båda låter. Ska alltid vara 0.

private:
    enum class KickState { Idle, Running, WaitZero };
    enum class BassState { Idle, Running, Guillotine };

    static void silence (float* out, float* k, float* b, int from, int to) noexcept
    {
        std::fill (out + from, out + to, 0.0f);
        if (k) std::fill (k + from, k + to, 0.0f);
        if (b) std::fill (b + from, b + to, 0.0f);
    }

    void hardSilence() noexcept
    {
        kickState = KickState::Idle;
        kickPhase = kickPitchEnv = kickAmp = lastKick = 0.0;
        kickGate = 1.0;
        kickFading = false;
        kickAge = 0;
        stopBass();
        bassPending = false;
    }

    void stopBass() noexcept
    {
        bassState = BassState::Idle;
        bassPhase = bassAmp = bassEnvV = bassGateStep = 0.0;
        bassGate = 0.0;
        ic1 = ic2 = 0.0;
    }

    // offset = hur många samples sedan den exakta starttidpunkten (0..1)
    void triggerKick (double offset) noexcept
    {
        bassPending = false;
        if (bassState != BassState::Idle)
        {
            overlapCount.fetch_add (1, std::memory_order_relaxed);
            stopBass();
        }
        kickState = KickState::Running;
        kickPitchEnv = 1.0;
        kickAmp = 1.0;
        kickGate = 1.0;
        kickFading = false;
        kickAge = 0;
        kickPhase = std::min (0.25, offset * p.kickStartHz * invSr);
    }

    void startBass (double offset) noexcept
    {
        bassState = BassState::Running;
        bassPhase = std::min (0.25, offset * bassInc);   // fas 0 ligger exakt på nollpunkten
        bassAmp = 1.0;
        bassEnvV = 1.0;
        bassGate = 1.0;
        bassGateStep = 0.0;
        ic1 = ic2 = 0.0;
    }

    // Returnerar true när kicken dog i detta sample. endOffset = hur långt efter
    // nollgenomgången nästa sample ligger (0..1 sample).
    bool renderKick (double& endOffset) noexcept
    {
        lastKick = 0.0;
        if (kickState == KickState::Idle) return false;

        lastKick = shape (std::sin (kTwoPi * kickPhase), p.kickDrive, kickNorm) * kickAmp * kickGate;

        const double hz = p.kickEndHz + (p.kickStartHz - p.kickEndHz) * kickPitchEnv;
        const double inc = hz * invSr;
        kickPhase += inc;
        kickPitchEnv *= kickPitchCoeff;
        kickAmp *= kickAmpCoeff;

        if (kickFading)
        {
            kickGate -= kickGateStep;
            if (--kickGateLeft <= 0)
            {
                kickState = KickState::Idle;
                kickPhase = kickAmp = 0.0;
                return false;   // nödstängd, ingen bas efter den
            }
        }

        if (kickPhase >= 1.0)
        {
            kickPhase -= 1.0;
            if (kickState == KickState::WaitZero)
            {
                endOffset = std::min (1.0, kickPhase / inc);
                kickState = KickState::Idle;
                kickPhase = kickAmp = 0.0;
                return true;
            }
        }

        if (kickState == KickState::Running && ++kickAge >= kickLenSamples)
            kickState = KickState::WaitZero;
        return false;
    }

    double renderBass() noexcept
    {
        if (bassState == BassState::Idle) return 0.0;

        // oscillator: sinus -> såg. Sågen startar också på 0 och går uppåt.
        const double ph = bassPhase;
        const double saw = ph < 0.5 ? 2.0 * ph : 2.0 * ph - 2.0;
        const double osc = (1.0 - p.bassForm) * std::sin (kTwoPi * ph) + p.bassForm * saw;

        // TPT-tillståndsfilter, lågpass. Nollställs vid varje start: utgången börjar på 0.
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

        const double y = shape (v2, p.bassDrive, bassNorm) * bassAmp * bassGate;

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

        if (bassState == BassState::Guillotine)
        {
            bassGate -= bassGateStep;
            if (--bassGateLeft <= 0) stopBass();   // heltalsräknare: landar exakt, inga flyttalsrester
        }
        else if (bassAmp < 1.0e-5)
        {
            stopBass();   // helt utklingad: inget att hugga av
        }
        return y;
    }

    // miljö
    double sr = 48000.0, invSr = 1.0 / 48000.0;
    bool prepared = false;

    // parametrar
    Params p {};
    double kickPitchCoeff = 0.0, kickAmpCoeff = 0.0, bassAmpCoeff = 0.0, bassEnvCoeff = 0.0;
    double bassHz = 43.65, bassInc = 0.0, svfK = 2.0, kickNorm = 1.0, bassNorm = 1.0;
    int64_t kickLenParam = 0, kickLenSamples = 0;
    int guillotineSamples = 32;
    double gainSmooth = 0.0;

    // klocka
    bool synced = false;
    double expectedStep = 0.0;
    int64_t lastStep = 0;

    // kick
    KickState kickState = KickState::Idle;
    double kickPhase = 0.0, kickPitchEnv = 0.0, kickAmp = 0.0, kickGate = 1.0, kickGateStep = 0.0, lastKick = 0.0;
    bool kickFading = false;
    int kickGateLeft = 0;
    int64_t kickAge = 0;

    // bas
    BassState bassState = BassState::Idle;
    bool bassPending = false;
    double pendingOffset = 0.0;
    double bassPhase = 0.0, bassAmp = 0.0, bassEnvV = 0.0, bassGate = 0.0, bassGateStep = 0.0;
    int bassGateLeft = 0;
    double ic1 = 0.0, ic2 = 0.0;

    ScopeFifo scopeFifo;
};

} // namespace fl
