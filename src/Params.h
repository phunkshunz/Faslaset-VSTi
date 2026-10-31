#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Bassline.h"

namespace fl
{
// Ordningen här är också ordningen i presets.
enum P
{
    PLage, PKick, PKStam, PKLangd, PKDriv, PKNiva,
    PBTon, PBSkala, PBSlag, PBStil, PBForm, PBFilter, PBReso, PBEnv, PBEnvTid, PBDecay, PBLangd, PBBett, PBNiva,
    PUtkling, PVolym, PTempo,
    NUM_P
};

inline const char* paramId (int i)
{
    static const char* ids[NUM_P] = { "lage", "kick", "k_stam", "k_langd", "k_driv", "k_niva",
                                      "b_ton", "b_skala", "b_slag", "b_stil", "b_form", "b_filter", "b_reso", "b_env", "b_envtid",
                                      "b_decay", "b_langd", "b_bett", "b_niva", "utkling", "volym", "tempo" };
    return ids[i];
}

constexpr int kNumKicks = 29;   // BETONG:s kickar, K01..K29

inline juce::StringArray kickNames()
{
    const char* n[kNumKicks] = { "909 PUNCH", "DEEP LONG", "DISTORTED", "MINIMAL TIGHT", "LOFI 12BIT", "INDUSTRIAL CRUSHED",
                                 "HARD CLIPPED", "RUMBLE COMBO", "SUB BOOM", "CLICK THUMP", u8"ÅNGVÄLT", "RUMBLE TUNNEL",
                                 "STAMPEN", "ROST", "HARDGROOVE 909", u8"ÅNGHAMMARE", "BUNKER LONG", "TUNGA VALS", "TECH HOUSE",
                                 "ROUND DEEP", "TRIBAL TOM", "GRUND TECHNO", "PEAK PUNCH", "BERLIN DEEP", "HARDGROOVE TIGHT",
                                 "RUND MINIMAL", "909 CLASSIC", u8"LÅNG SVANS", u8"STÅD" };
    juce::StringArray a;
    for (auto* s : n) a.add (juce::String::fromUTF8 (s));
    return a;
}

inline juce::StringArray scaleNames()
{
    return { "FRYGISK", "MOLL", "HARM. MOLL", juce::String::fromUTF8 ("FRYGISK DUR"), "DORISK", "PENTATON" };
}
inline juce::StringArray styleNames() { return { "RULL", "GALOPP", "GLES", "VILD" }; }

inline juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 4); }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout L;
    const auto U = [] (const char* s) { return String::fromUTF8 (s); };
    auto hz = [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (roundToInt (v)) + " Hz"; };
    auto ms = [] (float v, int) { return v < 10.0f ? String (v, 1) + " ms" : String (roundToInt (v)) + " ms"; };
    auto pct = [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; };
    auto x = [] (float v, int) { return v < 1.01f ? String ("REN") : String (v, 1) + "x"; };
    auto skew = [] (float lo, float hi, float centre) { NormalisableRange<float> r (lo, hi, 0.01f); r.setSkewForCentre (centre); return r; };
    auto F = [&] (int id, const String& name, NormalisableRange<float> r, float def, std::function<String (float, int)> fmt) {
        L.add (std::make_unique<AudioParameterFloat> (ParameterID { paramId (id), 1 }, name, r, def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (fmt)));
    };
    auto C = [&] (int id, const String& name, const StringArray& items, int def) {
        L.add (std::make_unique<AudioParameterChoice> (ParameterID { paramId (id), 1 }, name, items, def));
    };

    C (PLage, U ("LÄGE"), StringArray { "SPIKRAKT", U ("SLÄP") }, 0);

    C (PKick, "KICK", kickNames(), 22);
    F (PKStam, U ("KICK STÄMNING"), NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
       [] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " st"; });
    F (PKLangd, U ("KICK LÄNGD"), skew (20.0f, 600.0f, 110.0f), 80.0f, ms);
    F (PKDriv, "KICK DRIV", NormalisableRange<float> (1.0f, 8.0f, 0.01f), 1.4f, x);
    F (PKNiva, U ("KICK NIVÅ"), NormalisableRange<float> (0.0f, 1.5f, 0.01f), 1.0f, pct);

    L.add (std::make_unique<AudioParameterInt> (ParameterID { paramId (PBTon), 1 }, "BAS TON", 21, 48, 29,
                                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    C (PBSkala, "BAS SKALA", scaleNames(), 0);
    L.add (std::make_unique<AudioParameterInt> (ParameterID { paramId (PBSlag), 1 }, "BAS SLAG", 1, kMaxBeats, 4,
                                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return String (v) + (v == 1 ? " SLAG" : " SLAG"); })));
    C (PBStil, "BAS STIL", styleNames(), 0);
    F (PBForm, "BAS FORM", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.85f,
       [] (float v, int) { return v < 0.02f ? String ("SINUS") : v > 0.98f ? String::fromUTF8 ("SÅG") : String (roundToInt (v * 100.0f)) + " %"; });
    F (PBFilter, "BAS FILTER", skew (30.0f, 4000.0f, 300.0f), 200.0f, hz);
    F (PBReso, "BAS RESO", NormalisableRange<float> (0.0f, 0.9f, 0.01f), 0.3f, pct);
    F (PBEnv, "BAS ENV", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.65f, pct);
    F (PBEnvTid, "BAS ENV TID", skew (5.0f, 600.0f, 70.0f), 55.0f, ms);
    F (PBDecay, "BAS DECAY", skew (10.0f, 2000.0f, 150.0f), 140.0f, ms);
    F (PBLangd, U ("BAS LÄNGD"), NormalisableRange<float> (0.2f, 1.0f, 0.01f), 0.85f, pct);
    F (PBBett, "BAS BETT", NormalisableRange<float> (1.0f, 8.0f, 0.01f), 1.5f, x);
    F (PBNiva, U ("BAS NIVÅ"), NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f, pct);

    F (PUtkling, "UTKLING", NormalisableRange<float> (1.0f, 12.0f, 0.1f), 3.0f, ms);
    F (PVolym, "VOLYM", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f, pct);
    F (PTempo, "TEMPO", NormalisableRange<float> (90.0f, 190.0f, 0.1f), 145.0f, [] (float v, int) { return String (v, 1) + " BPM"; });

    L.add (std::make_unique<AudioParameterBool> (ParameterID { "spela", 1 }, "SPELA", false));
    return L;
}

// ---------------------------------------------------------------- presets
// Varje preset = inställningar + ett frö till basgångs-generatorn,
// så samma preset alltid ger samma basgång.
struct Preset
{
    const char* name;
    uint32_t seed;
    float v[NUM_P];
};

// lage kick stam langd driv niva | ton skala slag stil form filter reso env envtid decay langd bett niva | utkling volym tempo
inline const Preset* presets (int& count)
{
    static const Preset p[] = {
        { "FULLON F",      101, { 0, 22,  0.0f,  80, 1.4f, 1.0f, 29, 0, 4, 0, 0.85f, 200, 0.30f, 0.65f,  55, 140, 0.85f, 1.5f, 0.80f, 3.0f, 0.8f, 145 } },
        { "MORGONSOL G",   202, { 0,  0,  0.0f,  85, 1.2f, 1.0f, 31, 1, 4, 1, 0.70f, 260, 0.25f, 0.55f,  70, 150, 0.80f, 1.3f, 0.75f, 3.0f, 0.8f, 142 } },
        { "PROGG A",       303, { 0,  3,  0.0f,  90, 1.1f, 1.0f, 33, 4, 8, 2, 0.50f, 280, 0.20f, 0.40f,  80, 180, 0.75f, 1.2f, 0.75f, 4.0f, 0.8f, 138 } },
        { "GOA RULL E",    404, { 0,  9,  0.0f,  75, 1.5f, 1.0f, 28, 3, 4, 0, 0.75f, 230, 0.35f, 0.75f,  65, 130, 0.90f, 1.6f, 0.80f, 3.0f, 0.8f, 146 } },
        { "DARK G",        505, { 0,  6, -1.0f,  70, 2.5f, 1.0f, 31, 0, 8, 3, 1.00f, 150, 0.45f, 0.90f,  40, 110, 0.85f, 2.5f, 0.85f, 2.5f, 0.8f, 150 } },
        { "FOREST E",      606, { 0,  2, -2.0f,  65, 2.0f, 1.0f, 28, 2, 8, 3, 1.00f, 130, 0.50f, 1.00f,  35, 100, 0.80f, 3.0f, 0.80f, 2.5f, 0.8f, 155 } },
        { "HITECH D#",     707, { 0,  9,  2.0f,  55, 2.2f, 1.0f, 27, 0, 2, 0, 1.00f, 170, 0.55f, 1.00f,  25,  80, 0.90f, 3.5f, 0.80f, 2.0f, 0.8f, 175 } },
        { "SUMPMARK F#",   808, { 0,  5,  0.0f,  75, 3.0f, 1.0f, 30, 0, 4, 1, 0.90f, 120, 0.60f, 0.85f,  50, 120, 0.85f, 2.8f, 0.85f, 3.0f, 0.8f, 148 } },
        { "GALOPP D",      909, { 0, 22,  0.0f,  80, 1.6f, 1.0f, 26, 1, 2, 1, 0.80f, 210, 0.40f, 0.70f,  60, 140, 0.80f, 1.8f, 0.80f, 3.0f, 0.8f, 144 } },
        { "OKTAVHOPP A",  1010, { 0,  0,  0.0f,  80, 1.4f, 1.0f, 33, 1, 8, 3, 0.85f, 240, 0.30f, 0.70f,  55, 130, 0.85f, 1.6f, 0.80f, 3.0f, 0.8f, 145 } },
        { u8"SLÄP TUNNEL", 1111, { 1, 11,  0.0f, 150, 1.6f, 1.0f, 28, 1, 4, 0, 0.00f, 400, 0.00f, 0.00f, 100, 400, 0.95f, 1.3f, 0.90f, 5.0f, 0.8f, 130 } },
        { "INDUSTRI E",   1212, { 1,  5,  0.0f, 130, 3.5f, 1.0f, 28, 0, 4, 1, 0.30f, 220, 0.30f, 0.50f, 120, 320, 0.90f, 3.5f, 0.85f, 4.0f, 0.8f, 135 } },
        { "BUNKER D",     1313, { 1, 16, -1.0f, 170, 1.3f, 1.0f, 26, 4, 8, 2, 0.00f, 300, 0.00f, 0.00f, 100, 600, 0.95f, 1.0f, 0.90f, 6.0f, 0.8f, 128 } },
        { "BERLIN F",     1414, { 1, 23,  0.0f, 140, 2.0f, 1.0f, 29, 1, 8, 0, 0.20f, 260, 0.20f, 0.30f, 150, 450, 0.90f, 2.0f, 0.85f, 5.0f, 0.8f, 132 } },
    };
    count = (int) (sizeof (p) / sizeof (p[0]));
    return p;
}

inline int numPresets() { int n = 0; presets (n); return n; }
} // namespace fl
