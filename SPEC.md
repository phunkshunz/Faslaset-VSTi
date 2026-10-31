# FASLÅSET – spec

## Idé

Hybridmotor (samplad kick + genererad basgång) där basens oscillator alltid startar sin cykel i rätt fas, på rätt sample, utifrån kickens nollgenomgång. Kick och bas får aldrig låta i samma sample.

## Motorn (`src/Engine.h`)

- Header-only, ren C++17, inga JUCE-beroenden. Ingen allokering och inga lås i ljudtråden.
- **Klocka:** 16-delar per sample = BPM / 60 × 4 / samplerate, räknat från DAW:ens ppq varje block. Klockan går bara framåt: ett slag triggas bara när 16-delsnumret ökar. Avvikelse mer än en 32-del = riktigt hopp: allt tonar ut (halva UTKLING) och klockan låser om.
- **Kick:** sample (BETONG K01–K29, mono). Vid laddning: DC bort (högpass 8 Hz), tystnad i början bort, svans under −66 dB bort, toppnivå 0,97, 8 sampel intoning, 4 ms uttoning. Uppspelning med linjär interpolation, STÄMNING ±12 halvtoner. Efter LÄNGD väntar kicken på nollgenomgång uppåt och stängs där (subsample-position beräknas). Väntar den mer än 30 ms tonar den ut. LÄNGD begränsas till 3/4 slag.
- **Basmönster:** 8 slag × 3 rutor, varje ruta ett atomärt 32-bitarsord (på/av, skalsteg −7..+14, oktav −1..+1, betoning). Slag = floor(16-del / 4) mod SLAG, räknat från låtens början.
- **Bas:** sinus → PolyBLEP-såg (FORM), båda startar på 0 uppåt. TPT-lågpass nollställt vid varje not. Filterenvelope upp till 5 oktaver. Mättning med tanh. Not = GRUNDTON + skalsteg + 12 × oktav. Notlängd = LÄNGD × platsen fram till nästa händelse minus releasen. Slutet är en cosinus-release (UTKLING) som alltid hinner till exakt 0 före nästa händelse. Noten startar bara om det finns plats för minst tre releaser.
- **SPIKRAKT:** kick på 16-del 0, rutorna 1–3 på 16-del 1–3. **SLÄP:** ruta 1 startar på kickens nollgenomgång.
- **Skydd:** all indata kläms. NaN/Inf nollar blocket, återställer motorn, räknar `faultCount`. Mjukt tak strax under 0 dBFS. `overlapCount` räknar sampel där både kick och bas är ≠ 0 och ska alltid vara 0. Stopp i DAW:en tonar ut i stället för att hugga.

## Basgångs-generatorn (`src/Bassline.h`)

- Motiv på ett slag (rytm efter STIL + mest grundtonen), upprepat över SLAG.
- Variation på sista slaget (alltid om SLAG > 1) och mittslaget (55 % om SLAG ≥ 4). Rörelsetoner viktade: oktav, kvint, liten sekund (bara frygiska skalor), ters, kvint under, septima, kvart.
- Minst hälften av noterna är grundtonen (tvingas efteråt om det behövs). SLÄP: en not per slag, första slaget spelar alltid.
- Samma frö ger alltid samma mönster (presets lagrar ett frö). TONER / RYTM slumpar bara den ena delen.

## Pluginet

- Instrument, stereo ut. Följer DAW:ens play/tempo. SPELA + TEMPO ger egen klocka.
- 14 presets (10 psy, 4 techno). Tärningar: kick, basgång, toner, rytm, klang, allt. Ångra 40 steg. Mönstret sparas i projektet.
- GUI 1060 × 640: topprad, oscilloskop, KICK, BASGÅNG (rutnät 8 × 3), MASTER.
- Färger: UV-svart #07060B, panel #14121C, UV-violett #9B5CFF, syragrön #B6FF00 (kick), magenta #FF2BD6 (bas), cyan #00E5FF (fas 0, varning).

## Tester

- `tests/engine_test.cpp` (körs i Actions): alla lägen, 44,1/48/96 kHz, 128–175 BPM, lång kick, kick med DC, max mättning, loopar, hackande DAW-position, ingen kick, 300 slumpade inställningar, generatorns regler över 8000 mönster, NaN/Inf-indata. Kräver noll krockar, bas = 0 före varje kick, varje basnot startar på noll och slutar på noll, kicken stängs på nollgenomgång, inga dubbelslag, inga värden över 1.
- `tests/plugin_test.cpp`: 29 kickar laddas och låter, alla presets, för stora block, alla tärningar, ångra, state med mönster, stopp tonar ut, GUI till PNG, ljudprov till WAV.
