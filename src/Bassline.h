// =============================================================================
//  FASLÅSET — basgångs-generatorn. Ren C++17, testbar utan JUCE.
//
//  Slumpar inte vilt. Den bygger en basgång som en producent skulle:
//    - ett grundmotiv på ett slag (rytm + toner), mest grundtonen
//    - motivet upprepas över alla slag
//    - variation på frasens viktiga ställen: mitten och sista slaget
//    - toner väljs ur skalan, med tyngd på grundton, kvint, oktav och
//      (i frygisk) den lilla sekunden som ger psy-spänningen
// =============================================================================
#pragma once

#include "Engine.h"
#include <cstdint>

namespace fl
{

class Rng
{
public:
    explicit Rng (uint32_t seed) : s (seed ? seed : 0x9e3779b9u) {}
    uint32_t next() noexcept { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() noexcept { return (float) (next() & 0xffffff) / 16777216.0f; }
    int range (int n) noexcept { return n <= 1 ? 0 : (int) (next() % (uint32_t) n); }
    bool chance (float p) noexcept { return uni() < p; }

private:
    uint32_t s;
};

enum class BassStyle : int
{
    Rull = 0,     // psy: alla tre 16-delar, rullande
    Galopp = 1,   // psy: B-B-paus eller paus-B-B, studsigt
    Gles = 2,     // progg: färre noter, mer luft
    Vild = 3      // mer tonhopp och oktaver
};
constexpr int kNumStyles = 4;

struct BasslineGen
{
    // Toner som passar som "rörelse" i en basgång, skalsteg med vikter.
    static int movementDegree (Rng& r, int scale)
    {
        // b2 (steg 1) bara i frygiska skalor, där den är själva psy-ljudet
        const bool phrygian = scale == 0 || scale == 3;
        struct W { int deg; int w; };
        const W tbl[] = {
            { 7, 26 },                 // oktav upp
            { 4, 22 },                 // kvint
            { 1, phrygian ? 24 : 4 },  // liten sekund / sekund
            { 2, 14 },                 // ters
            { -3, 10 },                // kvint under
            { 6, 8 },                  // septima
            { 3, 6 },                  // kvart
        };
        int sum = 0;
        for (auto& t : tbl) sum += t.w;
        int x = r.range (sum);
        for (auto& t : tbl)
        {
            if (x < t.w) return t.deg;
            x -= t.w;
        }
        return 0;
    }

    // Rytm för ett slag (tre 16-delar efter kicken).
    static void rhythm (Rng& r, BassStyle st, bool out[3])
    {
        switch (st)
        {
            case BassStyle::Rull:
                out[0] = out[1] = out[2] = true;
                break;
            case BassStyle::Galopp:
                if (r.chance (0.6f)) { out[0] = true; out[1] = true; out[2] = false; }
                else { out[0] = false; out[1] = true; out[2] = true; }
                break;
            case BassStyle::Gles:
            {
                const int v = r.range (3);
                out[0] = v != 2; out[1] = v == 1; out[2] = v == 2 || v == 0;
                break;
            }
            case BassStyle::Vild:
                out[0] = true; out[1] = r.chance (0.8f); out[2] = r.chance (0.85f);
                break;
        }
        if (! out[0] && ! out[1] && ! out[2]) out[1] = true;
    }

    // Hela mönstret. beats = 1..8. Skriver alla 8 slag (resten fylls med upprepning).
    static void generate (Pattern& pat, uint32_t seed, int beats, int scale, BassStyle st, bool slap,
                          bool keepRhythm = false, bool keepNotes = false)
    {
        Rng r (seed);
        beats = std::clamp (beats, 1, kMaxBeats);

        // ---- grundmotivet
        bool motif[3] = { false, false, false };
        rhythm (r, st, motif);
        int motifDeg[3] = { 0, 0, 0 };
        if (st == BassStyle::Vild || r.chance (0.25f))
            motifDeg[r.range (3)] = movementDegree (r, scale);   // lite melodi redan i motivet
        const bool pingOct = r.chance (st == BassStyle::Vild ? 0.5f : 0.2f);   // sista 16-delen en oktav upp

        // ---- var variationerna sitter: sista slaget alltid (om mer än ett), mitten ibland
        auto isVar = [&] (int b) {
            if (beats == 1) return false;
            if (b == beats - 1) return true;
            if (beats >= 4 && b == beats / 2 - 1) return r.chance (0.55f);
            return st == BassStyle::Vild && r.chance (0.3f);
        };

        for (int b = 0; b < kMaxBeats; ++b)
        {
            const int src = b % beats;   // slag efter mönsterlängden upprepar början
            if (b >= beats)
            {
                for (int s = 0; s < kSlots; ++s) pat.set (b, s, pat.get (src, s));
                continue;
            }

            bool on[3] = { motif[0], motif[1], motif[2] };
            int deg[3] = { motifDeg[0], motifDeg[1], motifDeg[2] };
            int oct[3] = { 0, 0, 0 };
            if (pingOct && ! slap) oct[2] = 1;

            if (isVar (b))
            {
                // en till två noter flyttas, rytmen kan fyllas ut
                const int n = 1 + (r.chance (0.45f) ? 1 : 0);
                for (int k = 0; k < n; ++k)
                {
                    const int s = slap ? 0 : r.range (3);
                    deg[s] = movementDegree (r, scale);
                    on[s] = true;
                }
                if (r.chance (0.3f) && ! slap) { on[0] = on[1] = on[2] = true; }   // fill
                if (r.chance (0.15f)) oct[slap ? 0 : 2] = 1;
            }

            // techno: en not per slag, sällan paus (aldrig på första slaget)
            if (slap)
            {
                on[0] = b == 0 || ! r.chance (st == BassStyle::Gles ? 0.25f : 0.08f);
                on[1] = on[2] = false;
            }

            for (int s = 0; s < kSlots; ++s)
            {
                Cell old = pat.get (b, s);
                Cell c;
                c.on = keepRhythm ? old.on : on[s];
                c.degree = keepNotes ? old.degree : deg[s];
                c.oct = keepNotes ? old.oct : oct[s];
                c.vel = s == 0 ? 120 : (st == BassStyle::Rull ? 100 : 110);
                if (keepRhythm && ! c.on) c.vel = old.vel;
                pat.set (b, s, c);
            }
        }

        // ---- grundtonen ska bära: minst hälften av noterna. Annars flyttas
        //      noter tillbaka till grundtonen, först utanför variationsslaget.
        if (keepNotes) return;
        int notes = 0, root = 0;
        for (int b = 0; b < beats; ++b)
            for (int s = 0; s < kSlots; ++s)
            {
                const Cell c = pat.get (b, s);
                if (c.on) { ++notes; if (c.degree == 0) ++root; }
            }
        for (int pass = 0; pass < 2 && root * 2 < notes; ++pass)
            for (int b = 0; b < beats && root * 2 < notes; ++b)
            {
                if (pass == 0 && b == beats - 1 && beats > 1) continue;
                for (int s = 0; s < kSlots && root * 2 < notes; ++s)
                {
                    Cell c = pat.get (b, s);
                    if (c.on && c.degree != 0) { c.degree = 0; c.oct = 0; pat.set (b, s, c); ++root; }
                }
            }
        for (int b = beats; b < kMaxBeats; ++b)
            for (int s = 0; s < kSlots; ++s) pat.set (b, s, pat.get (b % beats, s));
    }
};

} // namespace fl
