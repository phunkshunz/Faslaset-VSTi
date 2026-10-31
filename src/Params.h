#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"

namespace fl
{
// Ordningen här är också ordningen i presets och tärningar.
enum P
{
    PLage, PKStart, PKSlut, PKSvep, PKLangd, PKDriv, PKNiva,
    PBTon, PBForm, PBFilter, PBReso, PBEnv, PBEnvTid, PBDecay, PBBett, PBNiva,
    PGiljotin, PVolym, PTempo,
    NUM_P
};

inline const char* paramId (int i)
{
    static const char* ids[NUM_P] = { "lage", "k_start", "k_slut", "k_svep", "k_langd", "k_driv", "k_niva",
                                      "b_ton", "b_form", "b_filter", "b_reso", "b_env", "b_envtid", "b_decay", "b_bett", "b_niva",
                                      "giljotin", "volym", "tempo" };
    return ids[i];
}

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

    L.add (std::make_unique<AudioParameterChoice> (ParameterID { paramId (PLage), 1 }, U ("LÄGE"), StringArray { "SPIKRAKT", U ("SLÄP") }, 0));

    F (PKStart, "KICK START", skew (60.0f, 800.0f, 200.0f), 220.0f, hz);
    F (PKSlut, "KICK SLUT", skew (30.0f, 120.0f, 55.0f), 50.0f, hz);
    F (PKSvep, "KICK SVEP", skew (2.0f, 120.0f, 20.0f), 18.0f, ms);
    F (PKLangd, U ("KICK LÄNGD"), skew (30.0f, 400.0f, 110.0f), 90.0f, ms);
    F (PKDriv, "KICK DRIV", NormalisableRange<float> (1.0f, 8.0f, 0.01f), 1.5f, x);
    F (PKNiva, U ("KICK NIVÅ"), NormalisableRange<float> (0.0f, 1.0f, 0.01f), 1.0f, pct);

    L.add (std::make_unique<AudioParameterInt> (ParameterID { paramId (PBTon), 1 }, "BAS TON", 21, 52, 29,
                                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return noteName (v); })));
    F (PBForm, "BAS FORM", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.85f,
       [] (float v, int) { return v < 0.02f ? String ("SINUS") : v > 0.98f ? String::fromUTF8 ("SÅG") : String (roundToInt (v * 100.0f)) + " %"; });
    F (PBFilter, "BAS FILTER", skew (30.0f, 4000.0f, 300.0f), 180.0f, hz);
    F (PBReso, "BAS RESO", NormalisableRange<float> (0.0f, 0.9f, 0.01f), 0.3f, pct);
    F (PBEnv, "BAS ENV", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f, pct);
    F (PBEnvTid, "BAS ENV TID", skew (5.0f, 600.0f, 70.0f), 60.0f, ms);
    F (PBDecay, "BAS DECAY", skew (10.0f, 2000.0f, 150.0f), 90.0f, ms);
    F (PBBett, "BAS BETT", NormalisableRange<float> (1.0f, 8.0f, 0.01f), 1.4f, x);
    F (PBNiva, U ("BAS NIVÅ"), NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f, pct);

    F (PGiljotin, "GILJOTIN", NormalisableRange<float> (0.2f, 5.0f, 0.01f), 0.7f, ms);
    F (PVolym, "VOLYM", NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.8f, pct);
    F (PTempo, "TEMPO", NormalisableRange<float> (90.0f, 190.0f, 0.1f), 145.0f, [] (float v, int) { return String (v, 1) + " BPM"; });

    L.add (std::make_unique<AudioParameterBool> (ParameterID { "spela", 1 }, "SPELA", false));
    return L;
}

// ---------------------------------------------------------------- presets
struct Preset
{
    const char* name;
    float v[NUM_P];
};

// lage, kStart, kSlut, kSvep, kLangd, kDriv, kNiva, bTon, bForm, bFilter, bReso, bEnv, bEnvTid, bDecay, bBett, bNiva, giljotin, volym, tempo
inline const Preset* presets (int& count)
{
    static const Preset p[] = {
        { "FULLON F",        { 0, 220, 50, 18,  90, 1.5f, 1.0f, 29, 0.85f, 180, 0.30f, 0.70f,  60,  90, 1.4f, 0.80f, 0.7f, 0.8f, 145 } },
        { "MORGONSOL G",     { 0, 240, 55, 20,  85, 1.3f, 1.0f, 31, 0.70f, 240, 0.25f, 0.60f,  70, 100, 1.3f, 0.75f, 0.7f, 0.8f, 142 } },
        { "PROGG A",         { 0, 200, 52, 22,  95, 1.2f, 1.0f, 33, 0.50f, 260, 0.20f, 0.40f,  80, 110, 1.2f, 0.75f, 0.8f, 0.8f, 138 } },
        { "GOA RULL",        { 0, 250, 55, 18,  90, 1.4f, 1.0f, 30, 0.75f, 220, 0.35f, 0.80f,  70,  95, 1.6f, 0.80f, 0.7f, 0.8f, 146 } },
        { "DARK G",          { 0, 190, 48, 15,  80, 2.5f, 1.0f, 31, 1.00f, 140, 0.45f, 0.90f,  40,  70, 2.5f, 0.85f, 0.6f, 0.8f, 150 } },
        { "FOREST E",        { 0, 170, 44, 12,  75, 2.0f, 1.0f, 28, 1.00f, 120, 0.50f, 1.00f,  35,  60, 3.0f, 0.80f, 0.5f, 0.8f, 155 } },
        { "HITECH D#",       { 0, 260, 58, 10,  60, 2.2f, 1.0f, 27, 1.00f, 160, 0.55f, 1.00f,  25,  45, 3.5f, 0.80f, 0.4f, 0.8f, 175 } },
        { "SUMPMARK F#",     { 0, 160, 46, 16,  85, 3.0f, 1.0f, 30, 0.90f, 110, 0.60f, 0.85f,  50,  80, 2.8f, 0.85f, 0.6f, 0.8f, 148 } },
        { "SLÄP TUNNEL",     { 1, 160, 42, 30, 140, 1.8f, 1.0f, 28, 0.00f, 400, 0.00f, 0.00f, 100, 350, 1.3f, 0.90f, 0.8f, 0.8f, 130 } },
        { "INDUSTRI E",      { 1, 180, 45, 20, 120, 4.0f, 1.0f, 28, 0.30f, 200, 0.30f, 0.50f, 120, 300, 3.5f, 0.85f, 0.8f, 0.8f, 135 } },
        { "SUBTRYCK D",      { 1, 150, 40, 35, 160, 1.2f, 1.0f, 26, 0.00f, 300, 0.00f, 0.00f, 100, 600, 1.0f, 0.90f, 1.0f, 0.8f, 128 } },
        { "BETONGRULL F",    { 1, 200, 47, 25, 130, 2.5f, 1.0f, 29, 0.20f, 250, 0.20f, 0.30f, 150, 450, 2.0f, 0.85f, 0.8f, 0.8f, 132 } },
    };
    count = (int) (sizeof (p) / sizeof (p[0]));
    return p;
}

inline int numPresets() { int n = 0; presets (n); return n; }

// ---------------------------------------------------------------- tärningar
// Slumpar inom musikaliska ramar. Kicken hålls kortare än en 16-del i SPIKRAKT.
struct DiceRanges
{
    static void kick (float* v, juce::Random& r, bool spik, float bpm)
    {
        const float sixteenthMs = 60000.0f / juce::jmax (90.0f, bpm) / 4.0f;
        v[PKStart] = 140.0f + r.nextFloat() * 160.0f;
        v[PKSlut] = 40.0f + r.nextFloat() * 20.0f;
        v[PKSvep] = 8.0f + r.nextFloat() * 25.0f;
        v[PKLangd] = spik ? sixteenthMs * (0.6f + r.nextFloat() * 0.3f) : 100.0f + r.nextFloat() * 90.0f;
        v[PKDriv] = 1.0f + r.nextFloat() * r.nextFloat() * 4.0f;
    }
    static void bass (float* v, juce::Random& r, bool spik)
    {
        static const int notes[] = { 26, 27, 28, 29, 30, 31, 32, 33 };
        v[PBTon] = (float) notes[r.nextInt (8)];
        v[PBForm] = spik ? 0.5f + r.nextFloat() * 0.5f : r.nextFloat() * 0.4f;
        v[PBFilter] = spik ? 100.0f + r.nextFloat() * 220.0f : 150.0f + r.nextFloat() * 300.0f;
        v[PBReso] = r.nextFloat() * (spik ? 0.6f : 0.35f);
        v[PBEnv] = spik ? 0.4f + r.nextFloat() * 0.6f : r.nextFloat() * 0.5f;
        v[PBEnvTid] = spik ? 25.0f + r.nextFloat() * 70.0f : 60.0f + r.nextFloat() * 150.0f;
        v[PBDecay] = spik ? 45.0f + r.nextFloat() * 80.0f : 200.0f + r.nextFloat() * 500.0f;
        v[PBBett] = 1.0f + r.nextFloat() * r.nextFloat() * 3.5f;
    }
};
} // namespace fl
