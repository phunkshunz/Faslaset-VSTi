// =====================================================================
//  MASKINHALLEN : MOTSTÅND — factory presets (step 4e)
//
//  65 presets: FABRIK (init) + 8 categories x 6 + UNDERGÅNG x 8 + PSYKOS x 8. Each preset is a style
//  description; a deterministic generator (seeded per preset) turns it
//  into a kit (sound models, tune/decay), VINKELSLIP, sends, effects,
//  clock settings and a full A..H pattern bank. Same preset = same
//  result every time, on every machine.
// =====================================================================
#pragma once
#include "Params.h"
#include <functional>

namespace motstand {

struct Style {
    const char* name;
    const char* cat;
    uint32_t seed;
    std::array<int8_t, kNumInstruments> models;   // KICK SNARE CLAP RIM TL TM TH CH OH RIDE
    int8_t kick;      // 0 four, 1 ghosts, 2 broken, 3 rolling, 4 psycho (ghost rolls + a roll into the one)
    int8_t hats;      // 0 16ths with accents, 1 offbeats only, 2 eighths, 3 sparse / random
    int8_t ride;      // 0 none, 1 eighths, 2 sixteenths, 3 quarters
    float perc;       // toms / rim / clap density 0..1
    float chance;     // how many steps get a CHANS value 0..1
    float rolls;      // how many steps get a RULLE 0..1
    float slip;       // VINKELSLIP amount 0..1
    float hall, eko;  // send amounts 0..1
    float hallDecay;  // seconds
    float smog, press, pump;
    float jitter, drift;
    float kickDecay;  // 0..1
    int8_t rom;       // 1 = 909 6-bit cymbals
    float elnat;
    // UNDERGÅNG (left out = off)
    float muller = 0.f, mDepth = 6.f, mFreq = 120.f, mDirt = 0.4f;
    float motor = 0.f, rpm = 1100.f, rattle = 0.3f;
    float storm = 0.f, stormColor = 1500.f, alarm = 0.f;
    float doom = 0.f;
};

//                 name                cat           seed   models KICK..RIDE                 k  h  r  perc chan roll slip hall eko  hDec smog press pump jit  drift kDec rom  elnat
inline const std::array<Style, 64>& styles() {
    static const std::array<Style, 64> s {{
        // ---- DETROIT: dry, driving, 909 hands ----
        { "MOTORSTAD",        "DETROIT",     101, {0,0,0,0,0,0,0,0,0,0},                0, 0, 1, 0.55f, 0.20f, 0.10f, 0.15f, 0.15f, 0.00f, 1.6f, 0.30f, 0.25f, 0.00f, 1.5f, 0.40f, 0.50f, 0, 0.0f },
        { u8"LÖPANDE BAND",   "DETROIT",     102, {0,0,0,1,0,0,0,0,0,0},                1, 0, 0, 0.70f, 0.30f, 0.15f, 0.20f, 0.10f, 0.05f, 1.4f, 0.40f, 0.30f, 0.00f, 1.5f, 0.50f, 0.45f, 0, 0.0f },
        { "NATTSKIFT",        "DETROIT",     103, {0,0,0,0,0,0,0,0,0,0},                0, 2, 1, 0.45f, 0.15f, 0.20f, 0.10f, 0.25f, 0.10f, 2.2f, 0.30f, 0.20f, 0.10f, 1.2f, 0.40f, 0.55f, 0, 0.0f },
        { u8"KRÖM OCH OLJA",  "DETROIT",     104, {0,0,0,0,1,1,1,0,0,0},                0, 0, 2, 0.80f, 0.25f, 0.30f, 0.25f, 0.20f, 0.00f, 1.8f, 0.50f, 0.35f, 0.00f, 1.5f, 0.60f, 0.45f, 0, 0.0f },
        { "FABRIKSGOLV",      "DETROIT",     105, {2,0,0,0,0,0,0,0,0,0},                1, 1, 1, 0.60f, 0.20f, 0.20f, 0.30f, 0.10f, 0.00f, 1.2f, 0.40f, 0.30f, 0.15f, 1.5f, 0.50f, 0.35f, 0, 0.0f },
        { u8"ÅTTA MIL",       "DETROIT",     106, {0,0,0,2,0,0,0,0,0,0},                0, 0, 3, 0.50f, 0.35f, 0.10f, 0.10f, 0.20f, 0.20f, 2.0f, 0.30f, 0.20f, 0.00f, 1.0f, 0.30f, 0.55f, 0, 0.0f },
        // ---- BERLIN: cold, long, concrete ----
        { "BETONGVALV",       "BERLIN",      201, {0,0,2,0,2,2,2,0,0,0},                0, 1, 1, 0.40f, 0.20f, 0.10f, 0.25f, 0.45f, 0.10f, 4.5f, 0.30f, 0.30f, 0.20f, 0.8f, 0.30f, 0.65f, 0, 0.0f },
        { "KRAFTVERK",        "BERLIN",      202, {3,0,0,0,0,0,0,3,3,0},                0, 0, 0, 0.35f, 0.15f, 0.10f, 0.40f, 0.30f, 0.00f, 3.0f, 0.50f, 0.40f, 0.30f, 0.8f, 0.20f, 0.70f, 0, 0.0f },
        { "TUNNELBANA",       "BERLIN",      203, {0,0,0,0,0,0,0,0,0,1},                0, 2, 2, 0.30f, 0.30f, 0.05f, 0.20f, 0.40f, 0.15f, 5.0f, 0.30f, 0.30f, 0.20f, 0.6f, 0.25f, 0.70f, 0, 0.0f },
        { "KALLHUS",          "BERLIN",      204, {1,0,0,0,0,0,0,2,2,0},                0, 1, 0, 0.30f, 0.10f, 0.05f, 0.15f, 0.50f, 0.10f, 6.0f, 0.20f, 0.30f, 0.35f, 0.5f, 0.20f, 0.80f, 0, 0.0f },
        { u8"SPÅRVÄXEL",      "BERLIN",      205, {0,0,0,1,0,0,0,0,0,0},                2, 0, 1, 0.55f, 0.25f, 0.20f, 0.25f, 0.30f, 0.20f, 3.5f, 0.40f, 0.35f, 0.15f, 1.0f, 0.40f, 0.60f, 0, 0.0f },
        { u8"GRÅZON",         "BERLIN",      206, {0,3,1,0,0,0,0,0,0,0},                0, 3, 1, 0.40f, 0.40f, 0.10f, 0.20f, 0.35f, 0.10f, 4.0f, 0.30f, 0.30f, 0.25f, 0.8f, 0.30f, 0.65f, 0, 0.0f },
        // ---- MINIMAL: space, clicks, small moves ----
        { "TOMRUMMET",        "MINIMAL",     301, {2,0,3,1,3,3,3,0,0,0},                0, 3, 0, 0.35f, 0.45f, 0.05f, 0.05f, 0.30f, 0.20f, 2.5f, 0.10f, 0.15f, 0.10f, 1.5f, 0.50f, 0.30f, 0, 0.0f },
        { u8"MIKROSPÅN",      "MINIMAL",     302, {2,0,0,3,1,1,1,0,0,0},                0, 1, 0, 0.45f, 0.50f, 0.10f, 0.10f, 0.20f, 0.25f, 1.8f, 0.10f, 0.15f, 0.00f, 1.5f, 0.60f, 0.25f, 0, 0.0f },
        { u8"KLICKSPÅR",      "MINIMAL",     303, {2,1,3,1,0,0,0,2,2,0},                0, 3, 0, 0.40f, 0.55f, 0.05f, 0.10f, 0.15f, 0.15f, 1.5f, 0.10f, 0.10f, 0.00f, 1.5f, 0.50f, 0.25f, 0, 0.0f },
        { "GLES",             "MINIMAL",     304, {0,0,0,1,0,0,0,0,0,0},                0, 1, 0, 0.20f, 0.35f, 0.00f, 0.00f, 0.25f, 0.10f, 3.0f, 0.10f, 0.10f, 0.00f, 1.0f, 0.40f, 0.40f, 0, 0.0f },
        { "PRECISION",        "MINIMAL",     305, {2,1,1,1,1,1,1,1,1,0},                0, 2, 0, 0.30f, 0.25f, 0.05f, 0.00f, 0.10f, 0.05f, 1.2f, 0.00f, 0.20f, 0.00f, 0.2f, 0.05f, 0.25f, 0, 0.0f },
        { u8"VIT VÄGG",       "MINIMAL",     306, {1,0,0,1,0,0,0,0,0,2},                0, 3, 3, 0.25f, 0.45f, 0.00f, 0.05f, 0.40f, 0.30f, 4.0f, 0.10f, 0.10f, 0.15f, 1.2f, 0.50f, 0.60f, 0, 0.0f },
        // ---- HARDGROOVE: loopy, rolling, tom hands ----
        { "RULLGARDIN",       "HARDGROOVE",  401, {0,0,0,0,1,1,1,0,0,0},                3, 0, 2, 0.85f, 0.30f, 0.35f, 0.20f, 0.10f, 0.05f, 1.4f, 0.50f, 0.40f, 0.20f, 1.5f, 0.60f, 0.40f, 0, 0.0f },
        { "TROPP",            "HARDGROOVE",  402, {0,0,0,2,0,0,0,0,0,0},                1, 0, 2, 0.90f, 0.25f, 0.40f, 0.25f, 0.10f, 0.00f, 1.2f, 0.60f, 0.45f, 0.25f, 1.5f, 0.60f, 0.40f, 0, 0.0f },
        { "SVETTIG",          "HARDGROOVE",  403, {2,0,0,0,1,1,1,0,0,0},                3, 0, 1, 0.80f, 0.35f, 0.30f, 0.30f, 0.15f, 0.10f, 1.6f, 0.50f, 0.40f, 0.30f, 1.5f, 0.70f, 0.35f, 0, 0.0f },
        { "GUNGA",            "HARDGROOVE",  404, {0,0,0,1,1,1,1,0,0,0},                1, 2, 2, 0.75f, 0.40f, 0.25f, 0.15f, 0.10f, 0.05f, 1.2f, 0.40f, 0.35f, 0.20f, 1.5f, 0.80f, 0.40f, 0, 0.0f },
        { "TOMRULLE",         "HARDGROOVE",  405, {0,0,0,0,0,0,0,0,0,0},                0, 0, 1, 1.00f, 0.20f, 0.55f, 0.20f, 0.15f, 0.00f, 1.6f, 0.50f, 0.40f, 0.20f, 1.5f, 0.50f, 0.45f, 0, 0.0f },
        { u8"HÅRT ARBETE",    "HARDGROOVE",  406, {3,0,0,0,0,0,0,0,0,0},                3, 0, 2, 0.90f, 0.30f, 0.40f, 0.45f, 0.10f, 0.05f, 1.2f, 0.70f, 0.55f, 0.30f, 1.5f, 0.60f, 0.35f, 0, 0.0f },
        // ---- INDUSTRI: grind, metal, collapse ----
        { u8"STÅLVERK",       "INDUSTRI",    501, {3,3,0,2,0,0,0,3,3,0},                2, 0, 0, 0.60f, 0.20f, 0.30f, 0.70f, 0.20f, 0.05f, 2.0f, 0.80f, 0.60f, 0.30f, 1.5f, 0.50f, 0.40f, 0, 0.3f },
        { "PRESSHALL",        "INDUSTRI",    502, {3,0,0,0,2,2,2,3,3,0},                0, 0, 1, 0.50f, 0.20f, 0.20f, 0.60f, 0.35f, 0.00f, 3.5f, 0.70f, 0.80f, 0.40f, 1.0f, 0.30f, 0.55f, 0, 0.2f },
        { u8"SMÄLTUGN",       "INDUSTRI",    503, {1,2,2,0,2,2,2,2,2,1},                0, 2, 1, 0.45f, 0.20f, 0.20f, 0.65f, 0.40f, 0.10f, 4.0f, 0.70f, 0.60f, 0.35f, 1.0f, 0.40f, 0.70f, 0, 0.4f },
        { "NITMASKIN",        "INDUSTRI",    504, {2,1,0,3,3,3,3,3,0,0},                3, 0, 0, 0.80f, 0.25f, 0.50f, 0.60f, 0.10f, 0.05f, 1.5f, 0.80f, 0.60f, 0.25f, 1.5f, 0.60f, 0.30f, 0, 0.2f },
        { "SKROT",            "INDUSTRI",    505, {3,3,3,2,0,0,0,2,2,3},                2, 3, 2, 0.60f, 0.40f, 0.30f, 0.80f, 0.25f, 0.15f, 2.5f, 0.90f, 0.50f, 0.20f, 1.5f, 0.80f, 0.45f, 1, 0.5f },
        { "HAVERI",           "INDUSTRI",    506, {3,0,2,2,2,2,2,3,3,1},                2, 0, 2, 0.70f, 0.35f, 0.35f, 0.90f, 0.35f, 0.20f, 3.0f, 1.00f, 0.70f, 0.40f, 1.5f, 1.00f, 0.50f, 1, 0.6f },
        // ---- DUB: echoes, halls, distance ----
        { "DIMMA",            "DUB",         601, {1,0,2,0,0,0,0,0,0,0},                0, 1, 0, 0.40f, 0.20f, 0.10f, 0.10f, 0.50f, 0.45f, 5.0f, 0.20f, 0.20f, 0.25f, 1.0f, 0.40f, 0.70f, 0, 0.0f },
        { "HAMNEN",           "DUB",         602, {1,0,0,2,0,0,0,0,0,0},                0, 1, 3, 0.45f, 0.25f, 0.10f, 0.15f, 0.40f, 0.55f, 4.0f, 0.20f, 0.25f, 0.30f, 1.0f, 0.50f, 0.65f, 0, 0.2f },
        { "EKOKAMMARE",       "DUB",         603, {0,0,2,1,0,0,0,0,0,0},                0, 2, 0, 0.50f, 0.25f, 0.15f, 0.10f, 0.30f, 0.70f, 3.0f, 0.20f, 0.25f, 0.20f, 1.2f, 0.40f, 0.55f, 0, 0.0f },
        { u8"BÅTHUS",         "DUB",         604, {1,0,0,3,2,2,2,0,0,0},                0, 1, 0, 0.55f, 0.20f, 0.10f, 0.20f, 0.45f, 0.50f, 4.5f, 0.20f, 0.25f, 0.30f, 1.0f, 0.40f, 0.70f, 0, 0.3f },
        { "UNDERVATTEN",      "DUB",         605, {1,0,2,0,0,0,0,2,2,0},                0, 1, 0, 0.30f, 0.20f, 0.05f, 0.25f, 0.60f, 0.40f, 7.0f, 0.10f, 0.20f, 0.35f, 0.8f, 0.30f, 0.80f, 0, 0.0f },
        { u8"LÅNGT BORT",     "DUB",         606, {0,0,2,1,0,0,0,0,0,2},                0, 3, 3, 0.35f, 0.30f, 0.05f, 0.10f, 0.65f, 0.60f, 8.0f, 0.10f, 0.15f, 0.20f, 1.0f, 0.40f, 0.60f, 0, 0.0f },
        // ---- ROM-KORN: lo-fi, 6-bit, tape ----
        { "6 BITAR",          "ROM-KORN",    701, {0,0,0,0,0,0,0,0,0,0},                0, 0, 1, 0.55f, 0.20f, 0.15f, 0.35f, 0.15f, 0.05f, 1.5f, 0.40f, 0.30f, 0.10f, 1.5f, 0.50f, 0.45f, 1, 0.2f },
        { "BILLIG LÅDA",      "ROM-KORN",    702, {2,1,1,1,3,3,3,2,2,0},                1, 0, 0, 0.60f, 0.25f, 0.15f, 0.40f, 0.10f, 0.05f, 1.2f, 0.40f, 0.30f, 0.10f, 1.5f, 0.60f, 0.35f, 1, 0.3f },
        { "MAGNETBAND",       "ROM-KORN",    703, {0,0,0,0,0,0,0,0,0,0},                0, 2, 1, 0.45f, 0.20f, 0.10f, 0.45f, 0.20f, 0.30f, 2.0f, 0.30f, 0.45f, 0.15f, 1.5f, 0.70f, 0.50f, 1, 0.4f },
        { "DAMMIG",           "ROM-KORN",    704, {1,0,0,1,0,0,0,1,1,0},                0, 1, 1, 0.40f, 0.30f, 0.10f, 0.35f, 0.25f, 0.20f, 2.5f, 0.30f, 0.30f, 0.15f, 1.5f, 0.60f, 0.55f, 1, 0.5f },
        { "KASSETT",          "ROM-KORN",    705, {0,1,1,0,0,0,0,0,0,0},                1, 0, 2, 0.60f, 0.25f, 0.20f, 0.50f, 0.15f, 0.20f, 1.5f, 0.40f, 0.50f, 0.20f, 1.5f, 0.90f, 0.45f, 1, 0.3f },
        { "SLITEN",           "ROM-KORN",    706, {3,1,3,3,0,0,0,2,2,3},                2, 3, 1, 0.50f, 0.40f, 0.20f, 0.60f, 0.20f, 0.20f, 2.0f, 0.60f, 0.40f, 0.20f, 1.5f, 1.00f, 0.45f, 1, 0.6f },
        // ---- LABB: polymeter, probability, broken machines ----
        { "PRIMTAL",          "LABB",        801, {0,0,0,1,1,1,1,0,0,0},                0, 0, 0, 1.00f, 0.20f, 0.15f, 0.20f, 0.20f, 0.20f, 2.0f, 0.30f, 0.30f, 0.10f, 1.5f, 0.50f, 0.40f, 0, 0.0f },
        { u8"FASFÖRSKJUTNING","LABB",        802, {2,0,0,3,1,1,1,0,0,0},                2, 3, 0, 0.90f, 0.35f, 0.20f, 0.20f, 0.20f, 0.30f, 2.0f, 0.30f, 0.30f, 0.10f, 1.5f, 1.00f, 0.40f, 0, 0.0f },
        { "SANNOLIKHET",      "LABB",        803, {0,0,0,1,0,0,0,0,0,0},                1, 3, 2, 0.70f, 0.80f, 0.20f, 0.15f, 0.25f, 0.20f, 2.5f, 0.30f, 0.30f, 0.15f, 1.5f, 0.50f, 0.40f, 0, 0.0f },
        { "MASKINFEL",        "LABB",        804, {3,2,3,2,2,2,2,3,3,3},                2, 3, 2, 0.80f, 0.50f, 0.50f, 0.70f, 0.30f, 0.30f, 3.0f, 0.90f, 0.50f, 0.30f, 1.5f, 1.00f, 0.40f, 1, 0.4f },
        { "FRYST",            "LABB",        805, {1,0,2,0,2,2,2,2,2,1},                0, 1, 3, 0.40f, 0.30f, 0.10f, 0.20f, 0.70f, 0.20f, 10.f, 0.20f, 0.20f, 0.30f, 1.0f, 0.40f, 0.70f, 0, 0.0f },
        { "KAOS",             "LABB",        806, {3,3,3,3,3,3,3,2,2,3},                2, 3, 2, 1.00f, 0.60f, 0.60f, 0.50f, 0.35f, 0.40f, 3.0f, 0.80f, 0.50f, 0.20f, 1.5f, 1.00f, 0.40f, 0, 0.2f },
        // ---- UNDERGÅNG: Mad Max, the end of the world ----                                                                                                               muller dep  freq dirt  motor rpm   rat   storm col    alarm doom
        { u8"ÖKENVÄGEN",      u8"UNDERGÅNG", 901, {3,0,2,2,2,2,2,3,3,1},                0, 0, 0, 0.55f, 0.20f, 0.20f, 0.50f, 0.30f, 0.20f, 4.0f, 0.60f, 0.50f, 0.30f, 1.5f, 0.70f, 0.55f, 0, 0.2f, 0.50f, 6.f, 110.f, 0.5f, 0.50f, 2200.f, 0.5f, 0.40f, 1100.f, 0.00f, 0.0f },
        { "BENSINKRIG",       u8"UNDERGÅNG", 902, {3,3,0,3,0,0,0,3,3,0},                3, 0, 2, 0.80f, 0.20f, 0.40f, 0.60f, 0.20f, 0.10f, 2.5f, 0.70f, 0.60f, 0.35f, 1.5f, 0.80f, 0.45f, 0, 0.3f, 0.60f, 5.f, 130.f, 0.8f, 0.60f, 4200.f, 0.7f, 0.20f, 1800.f, 0.00f, 0.0f },
        { "SISTA DAGEN",      u8"UNDERGÅNG", 903, {1,0,2,0,2,2,2,0,0,1},                0, 1, 0, 0.40f, 0.20f, 0.10f, 0.30f, 0.50f, 0.20f, 7.0f, 0.40f, 0.40f, 0.30f, 1.0f, 0.50f, 0.75f, 0, 0.0f, 0.70f, 9.f, 90.f,  0.5f, 0.00f, 1100.f, 0.3f, 0.30f, 900.f,  0.60f, 0.0f },
        { "ASKREGN",          u8"UNDERGÅNG", 904, {1,3,2,0,0,0,0,2,2,3},                0, 3, 1, 0.40f, 0.40f, 0.10f, 0.30f, 0.60f, 0.30f, 8.0f, 0.40f, 0.40f, 0.25f, 1.0f, 0.60f, 0.70f, 0, 0.3f, 0.50f, 8.f, 100.f, 0.4f, 0.00f, 1100.f, 0.3f, 0.70f, 800.f,  0.00f, 0.0f },
        { "RADIOAKTIV",       u8"UNDERGÅNG", 905, {3,3,3,2,0,0,0,2,2,3},                2, 0, 2, 0.60f, 0.40f, 0.30f, 0.60f, 0.30f, 0.40f, 3.0f, 0.70f, 0.50f, 0.30f, 1.5f, 0.90f, 0.45f, 1, 0.6f, 0.40f, 5.f, 140.f, 0.7f, 0.00f, 1100.f, 0.3f, 0.30f, 3500.f, 0.30f, 0.0f },
        { u8"DÖDSKÖRNING",    u8"UNDERGÅNG", 906, {2,0,0,3,1,1,1,3,3,0},                3, 0, 2, 0.90f, 0.20f, 0.50f, 0.60f, 0.15f, 0.20f, 2.0f, 0.70f, 0.60f, 0.60f, 1.5f, 0.70f, 0.35f, 0, 0.2f, 0.50f, 4.f, 150.f, 0.6f, 0.70f, 6000.f, 0.6f, 0.20f, 2500.f, 0.00f, 0.0f },
        { u8"EVIGT MÖRKER",   u8"UNDERGÅNG", 907, {1,0,2,1,2,2,2,2,2,1},                0, 1, 0, 0.30f, 0.30f, 0.10f, 0.40f, 0.50f, 0.20f, 10.f, 0.50f, 0.40f, 0.30f, 1.0f, 0.50f, 0.85f, 0, 0.2f, 0.90f, 12.f, 70.f, 0.6f, 0.00f, 1100.f, 0.3f, 0.20f, 600.f,  0.00f, 0.3f },
        { u8"VÄRLDENS ÄNDE",  u8"UNDERGÅNG", 908, {3,3,3,2,2,2,2,3,3,1},                3, 0, 2, 0.90f, 0.40f, 0.50f, 0.70f, 0.40f, 0.30f, 5.0f, 0.90f, 0.60f, 0.40f, 1.5f, 1.00f, 0.55f, 1, 0.6f, 0.70f, 7.f, 120.f, 0.9f, 0.40f, 3200.f, 0.8f, 0.50f, 1500.f, 0.40f, 0.8f },
        // ---- PSYKOS: rysligt fet psykos-techno ----
        { "PSYKOS",           "PSYKOS",     1001, {3,0,0,0,0,0,0,0,0,0},                4, 0, 2, 0.70f, 0.30f, 0.30f, 0.50f, 0.15f, 0.10f, 2.0f, 0.60f, 0.60f, 0.40f, 1.5f, 0.60f, 0.50f, 0, 0.0f, 0.70f, 5.f, 130.f, 0.7f },
        { u8"FEBERDRÖM",      "PSYKOS",     1002, {3,0,2,1,2,2,2,0,0,0},                4, 2, 1, 0.60f, 0.60f, 0.20f, 0.40f, 0.30f, 0.20f, 3.0f, 0.50f, 0.50f, 0.30f, 1.5f, 0.80f, 0.55f, 0, 0.0f, 0.60f, 5.f, 110.f, 0.6f },
        { "PARANOIA",         "PSYKOS",     1003, {2,0,0,3,0,0,0,3,3,0},                4, 0, 2, 0.80f, 0.40f, 0.50f, 0.50f, 0.15f, 0.30f, 2.0f, 0.60f, 0.60f, 0.40f, 1.5f, 0.70f, 0.40f, 0, 0.0f, 0.60f, 4.f, 140.f, 0.7f },
        { u8"SÖMNLÖS",        "PSYKOS",     1004, {2,0,0,0,0,0,0,0,0,0},                4, 0, 1, 0.60f, 0.25f, 0.30f, 0.60f, 0.10f, 0.10f, 1.5f, 0.70f, 0.70f, 0.50f, 1.5f, 0.50f, 0.40f, 0, 0.0f, 0.80f, 4.f, 160.f, 0.8f },
        { "HALLUCINOS",       "PSYKOS",     1005, {3,0,0,1,2,2,2,3,3,1},                4, 3, 2, 0.70f, 0.60f, 0.30f, 0.40f, 0.30f, 0.40f, 3.5f, 0.50f, 0.50f, 0.30f, 1.5f, 0.90f, 0.50f, 0, 0.0f, 0.60f, 6.f, 120.f, 0.6f },
        { "MANI",             "PSYKOS",     1006, {3,0,0,0,1,1,1,0,0,0},                3, 0, 2, 0.90f, 0.30f, 0.50f, 0.60f, 0.15f, 0.10f, 2.0f, 0.70f, 0.70f, 0.40f, 1.5f, 0.70f, 0.40f, 0, 0.0f, 0.50f, 5.f, 130.f, 0.6f },
        { "ISOLERING",        "PSYKOS",     1007, {2,0,0,1,0,0,0,1,1,0},                4, 1, 0, 0.40f, 0.30f, 0.20f, 0.40f, 0.20f, 0.10f, 2.0f, 0.50f, 0.50f, 0.40f, 1.0f, 0.40f, 0.45f, 0, 0.0f, 0.90f, 6.f, 100.f, 0.7f },
        { "SKRIK",            "PSYKOS",     1008, {3,3,3,2,0,0,0,3,3,0},                4, 0, 2, 0.80f, 0.40f, 0.40f, 0.80f, 0.20f, 0.20f, 2.5f, 0.80f, 0.60f, 0.40f, 1.5f, 0.80f, 0.45f, 0, 0.3f, 0.60f, 5.f, 140.f, 0.8f, 0.00f, 1100.f, 0.3f, 0.00f, 1500.f, 0.20f, 0.2f },
    }};
    return s;
}

inline int numPresets() { return 1 + (int) styles().size(); }

inline juce::String presetName (int index)
{
    if (index <= 0) return "FABRIK";
    const auto& st = styles()[(size_t) juce::jlimit (0, (int) styles().size() - 1, index - 1)];
    return juce::String::fromUTF8 (st.cat) + " : " + juce::String::fromUTF8 (st.name);
}

// ---------------------------------------------------------------------
//  Pattern generator
// ---------------------------------------------------------------------
namespace presetgen {

inline uint32_t hit (int vel, int chance = 100, int ratchet = 1, int tune = 0, int decay = 0)
{ Step s; s.vel = vel; s.chance = chance; s.ratchet = ratchet; s.tune = tune; s.decay = decay; return s.pack(); }

struct Gen {
    Rng r;
    explicit Gen (uint32_t seed) : r ((uint64_t) seed * 0x9E3779B97F4A7C15ull + 7) {}
    float f()             { return (float) r.uniform(); }
    int   i (int lo, int hi) { return lo + (int) (r.uniform() * (hi - lo + 1)); }
    bool  p (float prob)  { return f() < prob; }
};

// A: the base groove of the style. Also returns the lane lengths.
inline PatternData base (const Style& st, Gen& g, std::array<int, kNumInstruments>& len)
{
    PatternData A {}; for (auto& l : A) l.fill (Step::rest());
    len.fill (16);
    auto chanceFor = [&] (int fallback) { return g.p (st.chance) ? g.i (45, 85) : fallback; };
    auto rollFor   = [&] () { return g.p (st.rolls) ? g.i (2, 4) : 1; };

    // kick
    for (int s : { 0, 4, 8, 12 }) A[KICK][(size_t) s] = hit (g.i (115, 124));
    if (st.kick == 1) { A[KICK][(size_t) (g.p (0.5f) ? 14 : 10)] = hit (g.i (60, 80), g.i (40, 70)); if (g.p (0.5f)) A[KICK][3] = hit (g.i (55, 70), 50); }
    if (st.kick == 2) { A[KICK][8] = Step::rest(); A[KICK][(size_t) (g.p (0.5f) ? 10 : 11)] = hit (118); if (g.p (0.6f)) A[KICK][7] = hit (90, 70); }
    if (st.kick == 3) { for (int s : { 3, 7, 11, 15 }) if (g.p (0.55f)) A[KICK][(size_t) s] = hit (g.i (55, 80), chanceFor (100)); }
    if (st.kick == 4)   // PSYKOS: rolling ghost kicks between the four, and a roll into the one
    {
        for (int s : { 2, 3, 6, 7, 10, 11, 14 }) if (g.p (0.6f)) A[KICK][(size_t) s] = hit (g.i (45, 70), g.i (55, 90));
        A[KICK][15] = hit (g.i (70, 90), 100, g.p (0.5f) ? 2 : 3);
    }

    // hats
    if (st.hats == 0)
        for (int s = 0; s < 16; ++s)
        {
            if (s % 2 && g.p (0.5f)) continue;               // thin the weak 16ths: air between the hats
            A[CH][(size_t) s] = s % 4 == 2 ? hit (g.i (98, 115)) : hit (s % 2 ? g.i (30, 50) : g.i (50, 70), s % 2 ? chanceFor (100) : 100, s % 4 == 3 ? rollFor() : 1);
        }
    else if (st.hats == 1)
        for (int s : { 2, 6, 10, 14 }) A[CH][(size_t) s] = hit (g.i (90, 112));
    else if (st.hats == 2)
        for (int s = 0; s < 16; s += 2) A[CH][(size_t) s] = hit (s % 4 == 2 ? g.i (95, 110) : g.i (55, 75), chanceFor (100));
    else
        for (int s = 0; s < 16; ++s) if (g.p (0.45f)) A[CH][(size_t) s] = hit (g.i (40, 100), chanceFor (100), rollFor());

    // never three full cymbal layers at once: with 16th hats the ride drops to quarters
    int rideStyle = st.ride;
    if (st.hats == 0 && (rideStyle == 1 || rideStyle == 2)) rideStyle = 3;
    // open hat on the offbeats unless the closed hats already sit there (and not on top of a busy ride)
    if (st.hats != 1 && rideStyle != 1 && rideStyle != 2)
        for (int s : { 2, 6, 10, 14 }) { if (g.p (0.8f)) A[OH][(size_t) s] = hit (g.i (90, 112)); }
    if (st.hats == 1 && g.p (0.5f)) A[OH][14] = hit (100);

    // ride
    if (rideStyle == 1) for (int s = 0; s < 16; s += 2) A[RIDE][(size_t) s] = hit (g.i (55, 90));
    if (rideStyle == 2) for (int s = 0; s < 16; ++s) A[RIDE][(size_t) s] = hit (s % 4 == 2 ? g.i (90, 105) : g.i (40, 65), s % 2 ? chanceFor (100) : 100);
    if (rideStyle == 3) for (int s = 2; s < 16; s += 4) A[RIDE][(size_t) s] = hit (g.i (60, 90));

    // clap / snare
    if (st.perc > 0.2f) { A[CLAP][4] = hit (g.i (95, 110)); A[CLAP][12] = hit (g.i (98, 112)); }
    if (st.perc > 0.65f && g.p (0.5f)) A[CLAP][(size_t) (g.p (0.5f) ? 7 : 15)] = hit (g.i (55, 80), chanceFor (100));
    if (g.p (st.perc * 0.4f)) A[SNARE][(size_t) (g.p (0.5f) ? 12 : 14)] = hit (g.i (60, 100), chanceFor (100), rollFor());

    // toms + rim on polymeters
    const int primes[] = { 5, 7, 9, 11, 13 };
    for (int inst : { TOM_LO, TOM_MID, TOM_HI, RIM })
    {
        const int L = primes[g.i (0, 4)];
        len[(size_t) inst] = (inst == RIM && g.p (0.3f)) ? 16 : L;
        const int hits = (int) std::round (st.perc * (inst == RIM ? 3.5f : 2.6f) + g.f() * 0.8f);
        for (int n = 0; n < hits; ++n)
            A[(size_t) inst][(size_t) g.i (0, len[(size_t) inst] - 1)] = hit (g.i (65, 115), chanceFor (100), rollFor());
    }
    return A;
}

// B..H: the same moves as the factory bank, applied to any A
inline Bank derive (const PatternData& A, Gen& g)
{
    Bank b {}; b[0] = A;
    auto set = [] (PatternData& p, int inst, int s, uint32_t v) { p[(size_t) inst][(size_t) s] = v; };
    // B alive
    PatternData B = A;
    for (int s = 0; s < 16; ++s) { auto st = Step::unpack (B[CH][(size_t) s]); if (st.vel && s % 4 != 2 && s % 2) { st.chance = g.i (75, 95); B[CH][(size_t) s] = st.pack(); } }
    set (B, KICK, 14, hit (70, 35));
    for (int s = 0; s < kMaxSteps; ++s) { auto st = Step::unpack (B[RIM][(size_t) s]); if (st.vel) { st.ratchet = 3; B[RIM][(size_t) s] = st.pack(); break; } }
    b[1] = B;
    // C breakdown
    PatternData C = A; C[KICK].fill (Step::rest()); C[CLAP].fill (Step::rest());
    for (int s : { 2, 6, 10, 14 }) set (C, OH, s, hit (105, 100, 1, 0, 14));
    b[2] = C;
    // D tom rolls
    PatternData D = A; int r = 2;
    for (int inst : { TOM_LO, TOM_MID, TOM_HI })
        for (int s = 0; s < kMaxSteps; ++s) { auto st = Step::unpack (D[(size_t) inst][(size_t) s]); if (st.vel) { st.ratchet = r++; D[(size_t) inst][(size_t) s] = st.pack(); break; } }
    if (Step::unpack (D[TOM_LO][0]).vel == 0) set (D, TOM_LO, 0, hit (110, 100, 2));
    b[3] = D;
    // E ride drive
    PatternData E = A; E[CH].fill (Step::rest());
    E[OH].fill (Step::rest());
    for (int s = 0; s < 16; ++s) set (E, RIDE, s, hit (s % 4 == 2 ? 105 : (s % 2 ? 45 : 70), s % 2 ? 85 : 100));
    b[4] = E;
    // F fill-ready
    PatternData F = A;
    for (int s : { 13, 14, 15 }) set (F, SNARE, s, hit (80 + (s - 13) * 15, kChanceFill, s == 15 ? 4 : 2));
    set (F, TOM_HI, 4, hit (95, kChanceFill, 3));
    b[5] = F;
    // G stripped
    PatternData G {}; for (auto& l : G) l.fill (Step::rest());
    G[KICK] = A[KICK]; G[RIM] = A[RIM];
    for (int s : { 2, 6, 10, 14 }) set (G, CH, s, hit (90));
    b[6] = G;
    // H peak
    PatternData H = B;
    for (int s : { 0, 4, 8, 12 }) set (H, KICK, s, hit (127));
    set (H, SNARE, 12, hit (100)); set (H, SNARE, 15, hit (70, 100, 2));
    set (H, OH, 14, hit (120, 100, 1, 0, 10));
    b[7] = H;
    return b;
}

} // namespace presetgen

// ---------------------------------------------------------------------
//  Apply a style. `set` takes a parameter id and a PLAIN value.
// ---------------------------------------------------------------------
// The clean profile: every preset is generated from a tamed copy of its style,
// so the machine stays heavy but clean - air between the hits, no hiss, no chaos.
inline Style cleaned (Style st)
{
    const bool lofi = juce::String::fromUTF8 (st.cat) == "ROM-KORN";
    st.slip   *= 0.45f;
    st.chance *= 0.5f;
    st.rolls  *= 0.5f;
    st.smog   *= 0.6f;
    st.jitter  = std::min (st.jitter, 0.9f);
    st.drift  *= 0.5f;
    st.hall   *= 0.6f;
    st.eko    *= 0.6f;
    st.hallDecay = std::min (st.hallDecay, 4.f);
    st.elnat   = lofi ? st.elnat * 0.4f : 0.f;
    st.rom     = lofi ? st.rom : 0;
    st.muller *= 0.8f;  st.mDepth = std::min (st.mDepth, 5.f);  st.mDirt *= 0.6f;
    st.motor  *= 0.5f;  st.rattle *= 0.5f;
    st.storm  *= 0.25f; st.alarm  *= 0.5f;
    st.doom   *= 0.4f;
    if (! lofi)
    {   // swap the noisy sound models for clean ones
        if (st.models[SNARE] == 3) st.models[SNARE] = 2;            // BRUS -> FET
        for (int h : { CH, OH }) if (st.models[(size_t) h] == 2) st.models[(size_t) h] = 1;   // BRUS -> 808
        if (st.models[RIDE] == 3) st.models[RIDE] = 0;              // SPRAK -> 909
    }
    return st;
}

inline Bank applyStyle (const Style& original, const std::function<void (const juce::String&, float)>& set)
{
    using namespace presetgen;
    const Style st = cleaned (original);
    Gen g (st.seed);
    std::array<int, kNumInstruments> len {};
    const auto A = base (st, g, len);
    const auto bank = derive (A, g);

    // kit
    for (int i = 0; i < kNumInstruments; ++i)
    {
        set (pid::inst (i, "model"), (float) st.models[(size_t) i]);
        set (pid::inst (i, "len"), (float) len[(size_t) i]);
        set (pid::inst (i, "tune"), juce::jlimit (0.f, 1.f, 0.5f + (g.f() - 0.5f) * 0.24f));
        set (pid::inst (i, "decay"), juce::jlimit (0.f, 1.f, (i == KICK ? st.kickDecay : 0.45f) + (g.f() - 0.5f) * 0.2f));
        set (pid::inst (i, "tone"), juce::jlimit (0.f, 1.f, 0.5f + (g.f() - 0.5f) * 0.3f));
    }
    // cymbals: short tails, a little lower - clean tops
    set (pid::inst (OH, "decay"), 0.25f + g.f() * 0.15f);
    set (pid::inst (RIDE, "decay"), 0.2f + g.f() * 0.15f);
    set (pid::inst (CH, "decay"), 0.3f + g.f() * 0.2f);
    // VINKELSLIP: each lane gets the mode that suits it, scaled by the style
    const int   slipMode[kNumInstruments] = { DIOD, GERMANIUM, GERMANIUM, ROMKROSS, VIKNING, VIKNING, VIKNING, LIKRIKTARE, TEJP, LIKRIKTARE };
    const float slipW[kNumInstruments]    = { 0.5f, 0.4f, 0.6f, 0.5f, 0.6f, 0.55f, 0.5f, 0.3f, 0.5f, 0.4f };
    for (int i = 0; i < kNumInstruments; ++i)
    {
        set (pid::inst (i, "slipMode"), (float) slipMode[i]);
        const float d = st.slip * slipW[i] * (0.6f + 0.8f * g.f());
        set (pid::inst (i, "slipDrive"), d > 0.05f ? juce::jlimit (0.f, 1.f, d) : 0.f);
    }
    // sends: the kick stays dry
    const float hallW[kNumInstruments] = { 0.f, 0.5f, 0.7f, 0.6f, 0.5f, 0.5f, 0.5f, 0.0f, 0.2f, 0.2f };
    const float ekoW[kNumInstruments]  = { 0.f, 0.3f, 0.4f, 0.7f, 0.3f, 0.4f, 0.6f, 0.0f, 0.1f, 0.1f };
    for (int i = 0; i < kNumInstruments; ++i)
    {
        set (pid::inst (i, "sendHall"), juce::jlimit (0.f, 1.f, st.hall * hallW[i] * (0.5f + g.f())));
        set (pid::inst (i, "sendEko"), g.p (0.6f) ? juce::jlimit (0.f, 1.f, st.eko * ekoW[i] * (0.5f + g.f())) : 0.f);
    }
    // effects + clock
    set (pid::hallDecay, st.hallDecay);
    set (pid::hallSize, juce::jlimit (0.f, 1.f, 0.35f + st.hallDecay / 12.f + g.f() * 0.2f));
    set (pid::hallDamp, 0.45f + g.f() * 0.3f);          // darker halls: no fizz in the tail
    set (pid::hallPre, 8.f + g.f() * 40.f);
    set (pid::ekoTime, (float) g.i (1, 4));
    set (pid::ekoFeedback, 0.25f + st.eko * 0.3f);
    set (pid::ekoTone, 0.3f + g.f() * 0.4f);
    set (pid::ekoWear, st.rom ? 0.4f : 0.1f + g.f() * 0.1f);
    set (pid::smog, st.smog); set (pid::pressAmt, st.press); set (pid::pump, st.pump);
    set (pid::jitter, st.jitter); set (pid::drift, st.drift);
    set (pid::ride, 0.3f + g.f() * 0.3f);
    set (pid::romMode, (float) st.rom);
    set (pid::elnat, st.elnat);
    set (pid::seed, (float) (1 + st.seed % 9999));
    // UNDERGÅNG
    set (pid::mullerAmt, st.muller); set (pid::mullerDepth, st.mDepth); set (pid::mullerFreq, st.mFreq); set (pid::mullerDirt, st.mDirt);
    set (pid::motorGas, st.motor); set (pid::motorRpm, st.rpm); set (pid::motorRattle, st.rattle);
    set (pid::stormAmt, st.storm); set (pid::stormColor, st.stormColor); set (pid::alarm, st.alarm);
    set (pid::doom, st.doom);
    return bank;
}

} // namespace motstand
