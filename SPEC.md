# MASKINHALLEN : MOTSTÅND — spec

En 909-maskin för Jeff Mills-misshandel. Inte ett 16-stegs-grid som följer DAW:en,
utan en maskin med egen instabilitet som man måste kämpa mot.

## 1. Die Zeit (klockan) — `DieZeit` i `src/MotstandCore.h`
- Egen intern klocka, följer inte Bitwigs grid.
- **Jitter:** Gaussiskt brus per steg, sigma 0,6 ms, max ±1,5 ms. Samlas inte på hög.
- **Drift:** tempot vandrar slumpmässigt (max ±1,5 %) och drar sig hem på ca 10 s.
  Den här ackumuleras, maskinen glider ur fas.
- **Nudge:** som pitch/jog på en skivspelare, ±4 % fart medan den hålls.
- **Slip:** visar i ms hur långt maskinen glidit från referenstempot.
- **Seed** sparas i presetet, så en bra "dålig tagning" går att återskapa.

## 2. Drunk walk på decay och tune — `DrunkRider`
- Dold, asynkron (egen klocka, 37–211 ms slumpintervall), Brownsk rörelse.
- Hoppar i hårda trappsteg 1–3 % per rörelse, aldrig mjuka svep.
- Extra steg vid varje slag. Max påverkan: ±8 % decay, ±25 cent tune.
- Full styrka på hats och ride, svagare på resten (justerbart).

## 3. Der Tom-Schwindel (polymeter) — `PolyTrack`
- Varje instrument har egen looplängd 1–32 steg.
- Kick 16. Toms/perc förvalt 7, 11, 13 (primtal; 15 är inget primtal).
- 16/7/11 möts på ettan först efter 77 takter. 16/7/11/13 efter 1001 takter.

## 4. Detroit Smog (VCA-kollaps) — `DetroitSmog`
- Summerar transientenergi inom 1 ms. Upp till 3 fulla slag: rent.
- Fler: spänningsfall (rail sag), asymmetrisk diodklippning, slew-utsmetning.
- Mjuk övergång, klickar aldrig. Körs i 2x oversampling i pluginet.

## Röster — `src/Voices.h`
KICK, SNARE, CLAP, RIM, TOM L/M/H, CH, OH, RIDE.
Cymbalerna är 6-bit + ~32 kHz sample-and-hold som 909:ans billiga ROM. CH chokar OH.

## Byggordning
1. ✅ Motorkärna (klocka, drunk walk, polymeter, Smog)
2. ✅ 909-rösterna + offline demo-rendering
3. ✅ PluginProcessor: allt ihopkopplat, separata utgångar per instrument, 2x oversampling på Smog
4. GUI: steggrid per instrument, slip-mätare, Nudge, Smog-mätare, tärningar per sektion, SV/EN-knapp
5. Actions för Mac, Windows, Linux (zippar), licensnyckel, demo-version

## Pluginet (steg 3) — `src/PluginProcessor.*`, `src/Params.h`
- **Transport:** startar när DAW:en trycker play (första slaget landar direkt), tyst när den står still.
  Utan DAW (standalone) går den på egen hand.
- **TEMPO:** DAW = DAW:ens tempo är grundtempo, drift och jitter läggs ovanpå. EGET = eget BPM-reglage.
- **JITTER MAX** 0–1,5 ms (sigma = 40 % av max), **DRIFT**, **NUDGE** (±1, automatiserbar),
  **FYLLA** (hur hårt drunk walk rider), **DETROIT SMOG** + **SMOG TRÖSKEL**, **SEED**, **MASTER**.
- **Per instrument:** TUNE, DECAY, TON, NIVÅ, LÄNGD (1–32 steg), TYST.
- **Utgångar:** MIX (stereo) + en extra stereoutgång per instrument (av som standard).
  Slås en på i DAW:en går den rösten dit torr, utan Smog, och lämnar MIX.
- **MIDI in:** GM 909-map (36 kick, 38 virvel, 39 klapp, 37 kant, 41/45/48 toms, 42/46 hats, 51 ride).
- **State:** parametrar + mönster sparas i projektet/presetet. Samma seed = exakt samma tagning.
- Latens från oversamplingen rapporteras till DAW:en (3 samples i 48 kHz).
- GUI just nu: JUCE:s enkla reglagelista. Riktiga panelen kommer i steg 4.

## Test
    g++ -std=c++17 -O2 -Isrc tests/core_test.cpp -o core_test && ./core_test
    g++ -std=c++17 -O2 -Isrc tests/render_demo.cpp -o render_demo && ./render_demo demo.wav
    cmake -B build -DMOTSTAND_TESTS=ON && cmake --build build --target MotstandTest   # plugintest
