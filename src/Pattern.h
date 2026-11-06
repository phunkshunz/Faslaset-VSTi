// =====================================================================
//  MASKINHALLEN : MOTSTÅND — step data and the 8-pattern bank (step 4d)
//
//  One step packs into 32 bits so the GUI and the audio thread can share
//  it lock-free (one std::atomic<uint32_t> per step):
//    bits  0-6   velocity       0 = rest, 1..127
//    bits  7-13  chance         0..100 %, 101 = only while FILL, 102 = only without FILL
//    bits 14-15  ratchet - 1    1..4 hits inside the step (RULLE)
//    bits 16-21  tune lock + 32 -24..+24 (knob steps of 2 %), 0 = no lock
//    bits 22-27  decay lock + 32 -24..+24 (knob steps of 2 %), 0 = no lock
// =====================================================================
#pragma once
#include <array>
#include <cstdint>
#include <algorithm>
#include "Voices.h"
#include "MotstandCore.h"

namespace motstand {

constexpr int kNumPatterns = 8;
constexpr int kChanceFill = 101, kChanceNoFill = 102;
constexpr int kLockRange = 24;

struct Step {
    int vel = 0;
    int chance = 100;
    int ratchet = 1;
    int tune = 0;     // lock offset, -24..24
    int decay = 0;    // lock offset, -24..24

    uint32_t pack() const {
        const uint32_t v = (uint32_t) std::clamp(vel, 0, 127);
        const uint32_t c = (uint32_t) std::clamp(chance, 0, 127);
        const uint32_t r = (uint32_t) (std::clamp(ratchet, 1, 4) - 1);
        const uint32_t t = (uint32_t) (std::clamp(tune, -kLockRange, kLockRange) + 32);
        const uint32_t d = (uint32_t) (std::clamp(decay, -kLockRange, kLockRange) + 32);
        return v | (c << 7) | (r << 14) | (t << 16) | (d << 22);
    }
    static Step unpack(uint32_t x) {
        Step s;
        s.vel = (int) (x & 127u);
        s.chance = (int) ((x >> 7) & 127u);
        s.ratchet = (int) ((x >> 14) & 3u) + 1;
        s.tune = (int) ((x >> 16) & 63u) - 32;
        s.decay = (int) ((x >> 22) & 63u) - 32;
        return s;
    }
    static uint32_t rest() { return Step{}.pack(); }   // vel 0, chance 100, ratchet 1, no locks

    bool hasLocks() const { return tune != 0 || decay != 0; }
    bool plays(float roll01, bool fill) const {
        if (vel <= 0) return false;
        if (chance == kChanceFill) return fill;
        if (chance == kChanceNoFill) return ! fill;
        return roll01 * 100.f < (float) chance;
    }
};

using Lane = std::array<uint32_t, kMaxSteps>;
using PatternData = std::array<Lane, kNumInstruments>;
using Bank = std::array<PatternData, kNumPatterns>;

// ---------------------------------------------------------------------
//  Factory bank: A is the Mills-ish base; B..H are variations that show
//  off what the sequencer can do. All of them run at the same lengths.
// ---------------------------------------------------------------------
inline Bank factoryBank() {
    Bank b{};
    for (auto& p : b) for (auto& l : p) l.fill(Step::rest());
    auto hit = [](int vel, int chance = 100, int ratchet = 1, int tune = 0, int decay = 0) {
        Step s; s.vel = vel; s.chance = chance; s.ratchet = ratchet; s.tune = tune; s.decay = decay; return s.pack(); };

    // ---- A: base ----
    PatternData A{}; for (auto& l : A) l.fill(Step::rest());
    for (int i : { 0, 4, 8, 12 }) A[KICK][(size_t) i] = hit(120);
    const int ch[16] = { 60, 45, 100, 45, 60, 45, 105, 50, 60, 45, 100, 45, 60, 50, 110, 55 };
    for (int i = 0; i < 16; ++i) A[CH][(size_t) i] = hit(ch[i]);
    for (int i : { 2, 6, 10 }) A[OH][(size_t) i] = hit(100);
    A[OH][14] = hit(110);
    const int rd[8] = { 70, 90, 70, 95, 70, 90, 70, 100 };
    for (int i = 0; i < 8; ++i) A[RIDE][(size_t) (i * 2)] = hit(rd[i]);
    A[CLAP][4] = hit(100); A[CLAP][12] = hit(105);
    A[TOM_LO][0] = hit(110); A[TOM_LO][3] = hit(80);
    A[TOM_MID][0] = hit(100); A[TOM_MID][4] = hit(70); A[TOM_MID][7] = hit(90);
    A[TOM_HI][2] = hit(90); A[TOM_HI][9] = hit(70);
    A[RIM][1] = hit(90); A[RIM][5] = hit(70); A[RIM][8] = hit(100);
    b[0] = A;

    // ---- B: alive — chance on the hats, ghost kicks, rolls on the rim ----
    PatternData B = A;
    for (int i = 0; i < 16; ++i) if (i % 4 != 2) B[CH][(size_t) i] = hit(ch[i], 75);
    B[KICK][14] = hit(70, 35);
    B[RIM][8] = hit(100, 100, 3);
    B[CH][15] = hit(80, 60, 2);
    b[1] = B;

    // ---- C: breakdown — no kick, open hats ring longer, toms detuned down ----
    PatternData C = A;
    C[KICK].fill(Step::rest()); C[CLAP].fill(Step::rest());
    for (int i : { 2, 6, 10, 14 }) C[OH][(size_t) i] = hit(105, 100, 1, 0, 14);
    C[TOM_LO][0] = hit(110, 100, 1, -8); C[TOM_MID][0] = hit(100, 100, 1, -6);
    b[2] = C;

    // ---- D: tom rolls — Mills hands on the toms ----
    PatternData D = A;
    D[TOM_LO][0] = hit(115, 100, 2); D[TOM_MID][4] = hit(95, 100, 3); D[TOM_HI][9] = hit(90, 100, 4);
    D[TOM_HI][2] = hit(90, 100, 1, 6);
    b[3] = D;

    // ---- E: ride drive — 16th ride with accents, hats out ----
    PatternData E = A;
    E[CH].fill(Step::rest());
    for (int i = 0; i < 16; ++i) E[RIDE][(size_t) i] = hit(i % 4 == 2 ? 110 : (i % 2 ? 55 : 80), i % 2 ? 85 : 100);
    b[4] = E;

    // ---- F: FILL-ready — extra hits only play while FILL is held ----
    PatternData F = A;
    for (int i : { 13, 14, 15 }) F[SNARE][(size_t) i] = hit(80 + (i - 13) * 15, kChanceFill, i == 15 ? 4 : 2);
    F[TOM_HI][5] = hit(95, kChanceFill, 3);
    F[CLAP][12] = hit(105, kChanceNoFill);
    b[5] = F;

    // ---- G: stripped — kick, rim and the hall's worth of space ----
    PatternData G{}; for (auto& l : G) l.fill(Step::rest());
    for (int i : { 0, 4, 8, 12 }) G[KICK][(size_t) i] = hit(120);
    G[RIM][1] = hit(90); G[RIM][5] = hit(70, 70); G[RIM][8] = hit(100);
    for (int i : { 2, 6, 10, 14 }) G[CH][(size_t) i] = hit(90);
    b[6] = G;

    // ---- H: peak — everything, accents, rolls ----
    PatternData H = B;
    for (int i : { 0, 4, 8, 12 }) H[KICK][(size_t) i] = hit(127);
    H[SNARE][12] = hit(100); H[SNARE][15] = hit(70, 100, 2);
    H[OH][14] = hit(120, 100, 1, 0, 10);
    H[TOM_MID][7] = hit(110, 100, 2);
    b[7] = H;
    return b;
}

} // namespace motstand
