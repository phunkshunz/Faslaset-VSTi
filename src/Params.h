// MASKINHALLEN : MOTSTÅND — parameters and default pattern
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Voices.h"

namespace motstand {

// Display names (SV). The EN set is used by the SV/EN toggle in step 4.
inline const char* const kInstNameSV[kNumInstruments] = {
    "KICK", "VIRVEL", "KLAPP", "KANT", u8"TOM LÅG", "TOM MELLAN", u8"TOM HÖG",
    u8"STÄNGD HAT", u8"ÖPPEN HAT", "RIDE" };
inline const char* const kInstNameEN[kNumInstruments] = {
    "KICK", "SNARE", "CLAP", "RIM", "TOM LOW", "TOM MID", "TOM HIGH",
    "CLOSED HAT", "OPEN HAT", "RIDE" };
// ASCII ids, used for parameter ids and bus names
inline const char* const kInstId[kNumInstruments] = {
    "kick", "snare", "clap", "rim", "tomlo", "tommid", "tomhi", "ch", "oh", "ride" };

// GM-style 909 note map for MIDI input
inline int noteToInstrument(int note) {
    switch (note) {
        case 35: case 36: return KICK;
        case 38: case 40: return SNARE;
        case 39: return CLAP;
        case 37: return RIM;
        case 41: case 43: return TOM_LO;
        case 45: case 47: return TOM_MID;
        case 48: case 50: return TOM_HI;
        case 42: case 44: return CH;
        case 46: return OH;
        case 51: case 59: return RIDE;
        default: return -1;
    }
}

// How strongly the drunk walk rides each voice (x the global RIDE knob)
inline constexpr float kRideBase[kNumInstruments] = {
    0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 0.35f, 1.f, 1.f, 1.f };

namespace pid {
    inline juce::String inst(int i, const char* what) { return juce::String(kInstId[i]) + "_" + what; }
    inline const juce::String tempoSrc = "tempoSrc", bpm = "bpm", jitter = "jitter", drift = "drift",
        nudge = "nudge", ride = "ride", smog = "smog", smogThr = "smogThr", seed = "seed", master = "master";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction(
        [](float v, int) { return String(juce::roundToInt(v * 100.f)) + " %"; });

    // ---- global ----
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{ pid::tempoSrc, 1 }, "TEMPO",
          StringArray{ "DAW", "EGET" }, 0));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::bpm, 1 }, "EGET BPM",
          NormalisableRange<float>(90.f, 160.f, 0.01f), 133.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::jitter, 1 }, "JITTER MAX",
          NormalisableRange<float>(0.f, 1.5f, 0.01f), 1.5f,
          AudioParameterFloatAttributes().withLabel("ms")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::drift, 1 }, "DRIFT",
          NormalisableRange<float>(0.f, 1.f, 0.01f), 0.4f, pct));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::nudge, 1 }, "NUDGE",
          NormalisableRange<float>(-1.f, 1.f, 0.01f), 0.f));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::ride, 1 }, "FYLLA",   // drunk walk amount
          NormalisableRange<float>(0.f, 1.f, 0.01f), 0.7f, pct));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::smog, 1 }, "DETROIT SMOG",
          NormalisableRange<float>(0.f, 1.f, 0.01f), 0.8f, pct));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::smogThr, 1 }, "SMOG TRÖSKEL",
          NormalisableRange<float>(1.5f, 6.f, 0.1f), 3.f));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{ pid::seed, 1 }, "SEED", 1, 9999, 909));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::master, 1 }, "MASTER",
          NormalisableRange<float>(-24.f, 6.f, 0.1f), 0.f,
          AudioParameterFloatAttributes().withLabel("dB")));

    // ---- per instrument ----
    for (int i = 0; i < kNumInstruments; ++i) {
        const String n = String::fromUTF8(kInstNameSV[i]);
        const int defLen = i == TOM_LO ? 7 : i == TOM_MID ? 11 : i == TOM_HI ? 13 : i == RIM ? 11 : 16;
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::inst(i, "tune"), 1 }, n + " TUNE",
              NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f, pct));
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::inst(i, "decay"), 1 }, n + " DECAY",
              NormalisableRange<float>(0.f, 1.f, 0.001f), i == KICK ? 0.55f : 0.45f, pct));
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::inst(i, "tone"), 1 }, n + " TON",
              NormalisableRange<float>(0.f, 1.f, 0.001f), 0.5f, pct));
        l.add(std::make_unique<AudioParameterFloat>(ParameterID{ pid::inst(i, "level"), 1 }, n + " NIVÅ",
              NormalisableRange<float>(0.f, 1.f, 0.001f), i == KICK ? 0.85f : 0.55f, pct));
        l.add(std::make_unique<AudioParameterInt>(ParameterID{ pid::inst(i, "len"), 1 }, n + " LÄNGD",
              1, kMaxSteps, defLen));
        l.add(std::make_unique<AudioParameterBool>(ParameterID{ pid::inst(i, "mute"), 1 }, n + " TYST", false));
    }
    return l;
}

// Default lanes: a Mills-ish loop. Toms/rim on 7, 11, 13 against the 16 kick.
inline void defaultPattern(std::array<std::array<uint8_t, kMaxSteps>, kNumInstruments>& p) {
    for (auto& lane : p) lane.fill(0);
    auto set = [&](int inst, std::initializer_list<std::pair<int, int>> hits) {
        for (auto& h : hits) p[(size_t)inst][(size_t)h.first] = (uint8_t)h.second;
    };
    set(KICK, { {0,120},{4,120},{8,120},{12,120} });
    set(CH,   { {0,60},{1,45},{2,100},{3,45},{4,60},{5,45},{6,105},{7,50},{8,60},{9,45},{10,100},{11,45},{12,60},{13,50},{14,110},{15,55} });
    set(OH,   { {2,100},{6,100},{10,100},{14,110} });
    set(RIDE, { {0,70},{2,90},{4,70},{6,95},{8,70},{10,90},{12,70},{14,100} });
    set(CLAP, { {4,100},{12,105} });
    set(TOM_LO,  { {0,110},{3,80} });
    set(TOM_MID, { {0,100},{4,70},{7,90} });
    set(TOM_HI,  { {2,90},{9,70} });
    set(RIM,     { {1,90},{5,70},{8,100} });
}

} // namespace motstand
