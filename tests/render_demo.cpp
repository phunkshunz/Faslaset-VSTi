// MOTSTÅND — offline demo render (no JUCE needed).
// Builds 32 bars at 136 BPM with polymeter toms, drifting clock,
// drunk-ridden cymbals and Detroit Smog on the master, then writes a WAV.
//   g++ -std=c++17 -O2 -Isrc tests/render_demo.cpp -o render_demo && ./render_demo motstand_demo.wav
#include "MotstandCore.h"
#include "Voices.h"
#include "Vinkelslip.h"
#include <cstdio>
#include <vector>
#include <cstring>
#include <string>

using namespace motstand;

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr) {
    FILE* f = std::fopen(path, "wb"); if (!f) { std::perror(path); return; }
    const uint32_t n = (uint32_t)L.size(), data = n * 4, riff = 36 + data;
    const uint16_t pcm = 1, ch = 2, bits = 16, align = 4; const uint32_t rate = sr, bps = sr * 4, fmt = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&fmt, 4, 1, f); std::fwrite(&pcm, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&rate, 4, 1, f); std::fwrite(&bps, 4, 1, f); std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) {
        const int16_t l = (int16_t)std::lround(std::clamp(L[i], -1.f, 1.f) * 32767.f);
        const int16_t r = (int16_t)std::lround(std::clamp(R[i], -1.f, 1.f) * 32767.f);
        std::fwrite(&l, 2, 1, f); std::fwrite(&r, 2, 1, f);
    }
    std::fclose(f);
}

int main(int argc, char** argv) {
    const char* out = argc > 1 ? argv[1] : "motstand_demo.wav";
    const bool grind = argc > 2 && std::string(argv[2]) == "slip";   // second arg "slip" = VINKELSLIP on
    const int sr = 48000, bs = 256, bars = 32;
    const double bpm = 136.0;

    DieZeit clock; clock.prepare(sr, 0xD37A01); clock.baseBpm = bpm;
    Kit kit; kit.prepare(sr); kit.setTolerance(0xD37A01);
    std::array<Vinkelslip, kNumInstruments> slip;
    for (auto& v : slip) v.prepare(sr);
    if (grind) {
        auto set = [&](int i, int mode, float drive) { slip[(size_t)i].mode = mode; slip[(size_t)i].drive = drive; };
        set(KICK, DIOD, 0.35f); set(CLAP, GERMANIUM, 0.45f); set(RIM, ROMKROSS, 0.4f);
        set(TOM_LO, VIKNING, 0.5f); set(TOM_MID, VIKNING, 0.45f); set(TOM_HI, VIKNING, 0.4f);
        set(OH, TEJP, 0.5f); set(RIDE, LIKRIKTARE, 0.3f);
    }
    Elnat hum; hum.prepare(sr); hum.amount = grind ? 0.35f : 0.f;
    DetroitSmog smog; smog.prepare(sr);
    std::array<DrunkRider, kNumInstruments> rider;
    std::array<float, kNumInstruments> rideAmt{ 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 1.f, 1.f, 1.f };
    for (int i = 0; i < kNumInstruments; ++i) rider[i].prepare(sr, 100 + i);

    // knobs
    kit.params(KICK).decay = 0.55f; kit.params(KICK).tone = 0.6f;
    kit.params(CH).level = 0.55f;   kit.params(OH).decay = 0.35f; kit.params(OH).level = 0.5f;
    kit.params(RIDE).level = 0.45f; kit.params(RIDE).decay = 0.4f;
    kit.params(CLAP).level = 0.6f;
    kit.params(TOM_LO).level = 0.55f; kit.params(TOM_MID).level = 0.5f; kit.params(TOM_HI).level = 0.45f;
    kit.params(RIM).level = 0.45f;

    // ---- the lanes (velocity 0..127) ----
    std::array<PolyTrack, kNumInstruments> tr;
    auto set = [&](int inst, int len, std::initializer_list<std::pair<int,int>> hits) {
        tr[inst].length = len; for (auto& h : hits) tr[inst].velocity[h.first] = (uint8_t)h.second;
    };
    set(KICK, 16, { {0,120},{4,120},{8,120},{12,120} });
    set(CH,   16, { {0,60},{1,45},{2,100},{3,45},{4,60},{5,45},{6,105},{7,50},{8,60},{9,45},{10,100},{11,45},{12,60},{13,50},{14,110},{15,55} });
    set(OH,   16, { {2,100},{6,100},{10,100},{14,110} });
    set(RIDE, 16, { {0,70},{2,90},{4,70},{6,95},{8,70},{10,90},{12,70},{14,100} });
    set(CLAP, 16, { {4,100},{12,105} });
    set(TOM_LO, 7,  { {0,110},{3,80} });     // 7 against 16
    set(TOM_MID,11, { {0,100},{4,70},{7,90} }); // 11 against 16
    set(TOM_HI, 13, { {2,90},{9,70} });      // 13 against 16
    set(RIM,    11, { {1,90},{5,70},{8,100} });

    // arrangement: which lanes play from which bar
    std::array<int, kNumInstruments> fromBar{ 0, 99, 16, 20, 8, 8, 12, 0, 4, 16 };

    const double total = bars * 16 * (60.0 / bpm / 4.0) * sr;
    std::vector<float> L((size_t)total + sr * 2, 0.f), R(L.size(), 0.f);
    const float pan[kNumInstruments] = { 0.f, 0.f, 0.1f, -0.5f, -0.4f, 0.1f, 0.5f, 0.45f, -0.3f, 0.35f };

    StepEvent ev[16]; size_t pos = 0; int smogHits = 0; float maxCollapse = 0;
    while (pos + bs <= L.size()) {
        const int n = clock.process(bs, ev, 16);
        for (auto& r : rider) r.advance(bs);
        int e = 0;
        for (int i = 0; i < bs; ++i) {
            while (e < n && ev[e].sampleOffset == i) {
                const int64_t step = ev[e].stepIndex;
                const int bar = (int)(step / 16);
                if (bar < bars) {
                    for (int inst = 0; inst < kNumInstruments; ++inst) {
                        if (bar < fromBar[inst]) continue;
                        const int v = tr[inst].velocityAt(step);
                        if (!v) continue;
                        const float d = rider[inst].onTrigger() * rideAmt[inst];
                        const float t = rider[inst].onTrigger() * rideAmt[inst];
                        kit.trigger(inst, v / 127.f, t, d);
                        slip[(size_t)inst].onTrigger(v / 127.f);
                        smog.addTransient(i, v / 127.f);
                    }
                }
                ++e;
            }
            float o[kNumInstruments]; kit.process(o);
            for (int inst = 0; inst < kNumInstruments; ++inst) o[inst] = slip[(size_t)inst].process(o[inst]);
            const float h = hum.process(); L[pos + i] += h; R[pos + i] += h;
            for (int inst = 0; inst < kNumInstruments; ++inst) {
                L[pos + i] += o[inst] * (0.5f - 0.5f * pan[inst]);
                R[pos + i] += o[inst] * (0.5f + 0.5f * pan[inst]);
            }
        }
        smog.process(&L[pos], &R[pos], bs);
        if (smog.currentCollapse() > 0.3f) ++smogHits;
        maxCollapse = std::max(maxCollapse, smog.currentCollapse());
        pos += bs;
    }

    float peak = 0; for (size_t i = 0; i < L.size(); ++i) peak = std::max({ peak, std::fabs(L[i]), std::fabs(R[i]) });
    const float g = peak > 0 ? 0.89f / peak : 1.f;            // offline normalise to -1 dBFS
    for (size_t i = 0; i < L.size(); ++i) { L[i] *= g; R[i] *= g; }
    writeWav(out, L, R, sr);
    std::printf("Wrote %s: %.1f s, raw peak %.2f, final slip %.1f ms, bpm now %.2f, smog blocks %d (max collapse %.2f)\n",
                out, L.size() / (double)sr, peak, clock.slipMs(), clock.currentBpm(), smogHits, maxCollapse);
    return 0;
}
