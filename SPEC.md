# FASLÅSET – spec

## Idé

Hybridmotor (kick + bas) där basens oscillator alltid startar sin cykel i rätt fas, på rätt sample, utifrån kickens nollgenomgång. Kick och bas får aldrig låta i samma sample.

## Motorn (`src/Engine.h`)

- Header-only, ren C++17, inga JUCE-beroenden. Ingen allokering och inga lås i ljudtråden.
- **Klocka:** 16-delar per sample = BPM / 60 × 4 / samplerate. Position räknas om från DAW:ens ppq varje block, så inget driver. Hopp i tidslinjen (mer än 2 sampel fel) ger tvärstopp och omsynk.
- **Kick:** sinus med exponentiellt pitch-svep (START → SLUT, tidskonstant SVEP) och exponentiell avklingning (LÄNGD = tid till −60 dB). När LÄNGD gått ut väntar kicken på att fasen slår runt (sinus passerar noll uppåt) och stängs där. LÄNGD begränsas till 3/4 slag.
- **Bas:** sinus → såg (FORM), båda startar på 0 uppåt. TPT-tillståndsfilter (lågpass) som nollställs vid varje start. Filterenvelope upp till 5 oktaver. Mättning med tanh (bevarar nollgenomgångar).
- **Faslås:** när kicken stängs räknas hur långt efter nollgenomgången nästa sample ligger (bråkdel av ett sample). Basen startar på nästa sample med fas = den tiden × basens fasökning. Samma subsample-korrigering när basen startar på en 16-delsgräns.
- **Giljotin:** när basen har färre än GILJOTIN-sampel kvar till nästa händelse rampar den ner linjärt. Rampen räknas med en heltalsräknare och landar på exakt 0 på samplet före händelsen. Basen startar bara om det finns plats för minst två ramper.
- **SPIKRAKT:** kick på 16-del 0, bas armad på 16-del 1, 2, 3. Giljotinens mål = nästa 16-del.
- **SLÄP:** bas armad när kicken stängs. Giljotinens mål = nästa slag.
- **Skydd:** all indata kläms. NaN/Inf i signalen eller filtertillståndet nollar blocket, återställer motorn och räknar upp `faultCount`. Hårt tak ±1. `overlapCount` räknar sampel där både kick och bas är ≠ 0 och ska alltid vara 0.
- **Oscilloskop:** lock-free SPSC-ring (16384 ramar) med kick, bas och flaggor (kickstart, basstart). Full ring = ramar kastas.

## Pluginet

- Instrument, stereo ut (mono kopierat till båda). Följer DAW:ens play/tempo. SPELA + TEMPO ger egen klocka (standalone startar med SPELA på).
- Block större än utlovat delas upp, ingen allokering i processBlock.
- 12 presets (8 psy, 4 techno). Tärning per sektion + DICE för båda.
- GUI 980 × 560: topprad, oscilloskop (ett slag per svep, triggat på kicken), KICK, BAS, MASTER. Statusbricka: NOLL FASFEL / FASFEL n · SPÄRR n.
- Färger: UV-svart #07060B, panel #14121C, UV-violett #9B5CFF (rattar, aktivt), syragrön #B6FF00 (kick), magenta #FF2BD6 (bas), cyan #00E5FF (fas 0, varning).

## Tester

- `tests/engine_test.cpp` (körs i Actions): alla lägen, 44,1/48/96 kHz, 128–175 BPM, lång kick, max mättning, loopar, 300 slumpade inställningar, NaN/Inf-indata. Kräver noll krockar, bas = 0 på samplet före varje kick, varje basstart nära 0 och uppåt, inga värden över 1.
- `tests/plugin_test.cpp`: alla presets, för stora block, tärningar, state, stopp = tystnad, GUI till PNG.
