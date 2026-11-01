#pragma once
// VEVHUS presets. Ordning: tryck fart rytt lacka liv bredd | anga pys komp knack klick skrammel vissla | ton rum volym
// | typer: anga pys knack klick skrammel | avgång
struct VevhusPreset { const char* namn; float v[16]; int typ[5]; bool avgang; };

inline const VevhusPreset kPresets[] = {
    { "Perrong 06:12",      { .55f,.00f,.30f,.35f,.55f,.70f,  .00f,.80f,.45f,.25f,.35f,.40f,.00f, .50f,.40f,.80f }, { 0,1,0,0,2 }, false },
    { "Avgång",             { .60f,.75f,.45f,.20f,.50f,.60f,  .85f,.55f,.35f,.45f,.25f,.15f,.45f, .50f,.35f,.80f }, { 1,0,0,0,1 }, true  },
    { "Godståget",          { .70f,.45f,.55f,.25f,.45f,.55f,  .90f,.45f,.50f,.60f,.30f,.30f,.10f, .40f,.30f,.80f }, { 3,0,2,0,1 }, false },
    { "Rangerbangården",    { .45f,.30f,.65f,.15f,.70f,.85f,  .60f,.40f,.20f,.55f,.50f,.55f,.15f, .50f,.50f,.78f }, { 1,2,3,0,1 }, false },
    { "Vevhuset",           { .50f,.55f,.70f,.10f,.50f,.50f,  .55f,.30f,.65f,.80f,.40f,.00f,.00f, .45f,.25f,.80f }, { 0,0,0,2,0 }, false },
    { "Ventilverkstan",     { .65f,.40f,.80f,.20f,.55f,.75f,  .40f,.60f,.25f,.70f,.60f,.00f,.00f, .55f,.30f,.80f }, { 0,0,1,1,0 }, false },
    { "Kedjekroken",        { .40f,.35f,.60f,.10f,.65f,.80f,  .45f,.30f,.30f,.75f,.35f,.30f,.00f, .50f,.40f,.80f }, { 1,0,3,3,1 }, false },
    { "Pannrummet",         { .80f,.20f,.40f,.55f,.45f,.60f,  .50f,.70f,.70f,.40f,.30f,.25f,.00f, .40f,.55f,.78f }, { 2,1,4,0,2 }, false },
    { "Smedjan",            { .60f,.25f,.50f,.05f,.40f,.45f,  .30f,.25f,.30f,.85f,.20f,.25f,.00f, .40f,.45f,.80f }, { 0,0,2,2,1 }, false },
    { "Rörgatan",           { .50f,.40f,.45f,.15f,.60f,.80f,  .50f,.35f,.20f,.70f,.25f,.00f,.00f, .55f,.60f,.78f }, { 1,0,4,1,0 }, false },
    { "Bromsbacken",        { .55f,.15f,.35f,.20f,.60f,.85f,  .40f,.45f,.30f,.30f,.25f,.75f,.00f, .50f,.50f,.78f }, { 1,1,0,0,0 }, false },
    { "Kopplingen",         { .45f,.10f,.45f,.15f,.55f,.75f,  .30f,.40f,.35f,.35f,.30f,.80f,.00f, .45f,.45f,.80f }, { 0,0,2,0,1 }, false },
    { "Droppstenen",        { .35f,.00f,.30f,.30f,.70f,.90f,  .00f,.50f,.25f,.15f,.30f,.70f,.00f, .55f,.60f,.80f }, { 0,1,1,1,2 }, false },
    { "Spärrhaken",         { .50f,.50f,.70f,.10f,.50f,.70f,  .55f,.30f,.25f,.45f,.70f,.00f,.00f, .50f,.30f,.80f }, { 0,0,0,3,0 }, false },
    { "Trästopp",           { .45f,.45f,.60f,.05f,.55f,.65f,  .50f,.25f,.30f,.40f,.70f,.10f,.00f, .50f,.25f,.80f }, { 0,0,0,2,2 }, false },
    { "Nattåget",           { .40f,.40f,.35f,.30f,.45f,.60f,  .70f,.45f,.40f,.30f,.20f,.15f,.30f, .35f,.65f,.78f }, { 1,1,4,0,0 }, false },
    { "Ångvisslan",         { .60f,.55f,.40f,.20f,.40f,.60f,  .75f,.50f,.30f,.35f,.20f,.10f,.90f, .55f,.50f,.78f }, { 0,3,0,0,1 }, false },
    { "Tunga tåget",        { .85f,.35f,.50f,.25f,.35f,.45f,  1.0f,.45f,.55f,.55f,.20f,.25f,.00f, .30f,.35f,.80f }, { 3,1,2,0,1 }, false },
    { "Våta kolvar",        { .55f,.55f,.45f,.40f,.55f,.65f,  .85f,.50f,.30f,.40f,.30f,.45f,.00f, .50f,.35f,.80f }, { 2,2,0,0,2 }, false },
    { "Sprättventil",       { .75f,.30f,.55f,.30f,.60f,.80f,  .40f,.85f,.30f,.35f,.35f,.00f,.00f, .50f,.35f,.80f }, { 0,2,1,1,0 }, false },
    { "Tomgång",            { .40f,.00f,.25f,.25f,.40f,.55f,  .00f,.45f,.75f,.35f,.30f,.10f,.00f, .45f,.30f,.80f }, { 0,0,0,0,2 }, false },
    { "Kompressorn",        { .70f,.00f,.55f,.15f,.45f,.50f,  .00f,.55f,1.0f,.50f,.45f,.00f,.00f, .40f,.30f,.80f }, { 0,2,1,0,0 }, false },
    { "Läckan",             { .65f,.20f,.30f,.90f,.60f,.85f,  .35f,.70f,.25f,.20f,.25f,.20f,.00f, .50f,.45f,.78f }, { 1,1,0,1,2 }, false },
    { "Minimal ånga",       { .45f,.50f,.30f,.05f,.35f,.50f,  .70f,.20f,.15f,.35f,.15f,.00f,.00f, .45f,.20f,.80f }, { 0,0,1,1,0 }, false },
    { "Berlin 04:00",       { .55f,.55f,.60f,.15f,.60f,.70f,  .65f,.35f,.45f,.60f,.40f,.15f,.00f, .40f,.55f,.80f }, { 0,0,0,0,1 }, false },
    { "Perlonspår",         { .50f,.45f,.75f,.10f,.75f,.85f,  .45f,.30f,.20f,.65f,.55f,.20f,.00f, .55f,.40f,.80f }, { 1,2,3,2,2 }, false },
    { "Bortglömt lok",      { .35f,.25f,.30f,.45f,.70f,.80f,  .55f,.50f,.30f,.25f,.20f,.45f,.25f, .35f,.75f,.78f }, { 2,1,4,2,0 }, false },
    { "Snabbtåget",         { .70f,.95f,.55f,.20f,.45f,.55f,  .80f,.35f,.30f,.45f,.25f,.10f,.20f, .55f,.25f,.80f }, { 0,0,0,0,1 }, false },
    { "Malmbanan",          { .80f,.40f,.50f,.20f,.45f,.55f,  .90f,.40f,.45f,.60f,.25f,.50f,.15f, .30f,.45f,.80f }, { 3,0,2,0,0 }, false },
    { "Stålverket",         { .75f,.30f,.65f,.20f,.55f,.70f,  .45f,.45f,.40f,.80f,.45f,.40f,.00f, .40f,.60f,.78f }, { 3,2,4,3,1 }, false },
    { "Maskinparken",       { .60f,.50f,.60f,.20f,.55f,.70f,  .60f,.45f,.45f,.55f,.45f,.25f,.10f, .50f,.40f,.80f }, { 1,0,1,0,1 }, false },
    { "Sista turen",        { .45f,.35f,.35f,.25f,.50f,.65f,  .70f,.55f,.30f,.30f,.20f,.35f,.60f, .45f,.55f,.78f }, { 1,1,0,0,0 }, true  },
};
inline constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
