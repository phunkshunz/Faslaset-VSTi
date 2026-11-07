#pragma once
// Shared test helpers: a synthetic techno loop (no samples needed) and a WAV writer.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace testloop {

struct Stereo { std::vector<float> L, R; size_t size() const { return L.size(); } };

// 4-on-the-floor kick, offbeat open hat, 16th closed hats, clap on 2 and 4, rolling bass
inline Stereo techno (double sr, double bpm, double seconds, uint32_t seed = 1)
{
    const size_t n = (size_t) (sr * seconds);
    Stereo s; s.L.assign (n, 0.f); s.R.assign (n, 0.f);
    const double spb = 60.0 / bpm * sr, s16 = spb / 4.0;
    uint32_t r = seed * 747796405u + 1;
    auto noise = [&] { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return (float) (r >> 8) / 8388608.f - 1.f; };
    float hp1 = 0, hp2 = 0, cl = 0;
    for (size_t i = 0; i < n; ++i)
    {
        const double pos16 = (double) i / s16;
        const long k = (long) pos16;
        const double t = ((double) i - (double) k * s16) / sr;    // seconds since this 16th
        float x = 0.f, hat = 0.f;
        if (k % 4 == 0)                                          // kick
        {
            const double f = 48.0 + 110.0 * std::exp (-t * 38.0);
            const double ph = 2.0 * M_PI * (48.0 * t + 110.0 / 38.0 * (1.0 - std::exp (-t * 38.0)));
            (void) f;
            x += (float) (0.85 * std::sin (ph) * std::exp (-t * 7.0));
        }
        const float nz = noise();
        hp1 = 0.6f * hp1 + 0.4f * nz; const float hpf = nz - hp1;
        if (k % 4 == 2) hat += hpf * (float) std::exp (-t * 18.0) * 0.22f;     // open hat
        else            hat += hpf * (float) std::exp (-t * 90.0) * 0.12f;     // closed hat
        if (k % 8 == 4)                                                       // clap
        {
            hp2 = 0.85f * hp2 + 0.15f * nz;
            cl = hp2 * (float) std::exp (-t * 22.0) * 0.9f;
            x += cl;
        }
        // bass on the offbeat 16ths, saw-ish through a simple decay
        if (k % 4 != 0)
        {
            const double fb = 55.0 * ((k / 16) % 2 ? 1.189 : 1.0);
            const double ph = std::fmod (t * fb, 1.0);
            x += (float) ((2.0 * ph - 1.0) * 0.22 * std::exp (-t * 14.0));
        }
        s.L[i] = 0.5f * (x + hat * 0.8f);   // peaks around -3 dBFS
        s.R[i] = 0.5f * (x + hat * 1.2f);
    }
    return s;
}

inline void writeWav (const std::string& path, const Stereo& s, int sr)
{
    FILE* f = std::fopen (path.c_str(), "wb");
    if (! f) return;
    const uint32_t n = (uint32_t) s.size(), bytes = n * 2 * 3;
    auto u32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto u16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); u32 (36 + bytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    u32 (16); u16 (1); u16 (2); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 6); u16 (6); u16 (24);
    std::fwrite ("data", 1, 4, f); u32 (bytes);
    for (uint32_t i = 0; i < n; ++i)
        for (float v : { s.L[i], s.R[i] })
        {
            const float c = v > 1.f ? 1.f : (v < -1.f ? -1.f : v);
            const int32_t q = (int32_t) std::lround (c * 8388607.0);
            const uint8_t b[3] = { (uint8_t) (q & 0xff), (uint8_t) ((q >> 8) & 0xff), (uint8_t) ((q >> 16) & 0xff) };
            std::fwrite (b, 1, 3, f);
        }
    std::fclose (f);
}

} // namespace testloop
