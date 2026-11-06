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
- Bussen är helt ren (0 % THD, mätt) tills den kollapsar. Standard: 30 %, tröskel 3,5.
- Summerar transientenergi inom 1 ms. Upp till tröskeln: rent.
- Fler: spänningsfall (rail sag), asymmetrisk diodklippning, slew-utsmetning.
- Mjuk övergång, klickar aldrig. Körs i 2x oversampling i pluginet.

## Röster — `src/Voices.h` (v2)
- **KICK:** tvåstegs pitch-fall, triangel-VCO genom waveshaper som blir fetare med accent,
  separat attackpuls + klick, VCA som håller öppet ~3 ms.
- **VIRVEL:** två sinusbaserade kroppar + snappy brus. **KLAPP:** fyra smällar (0/9/18/28 ms) + rum.
- **KANT:** dubbelping genom mättande brygga. **TOMS:** kropp + oharmonisk deltonsmod + stickbrus.
- **HATS/RIDE:** 909-ROM. En bandbegränsad metallvåg räknas fram vid start (32 kHz, full upplösning)
  och spelas med Hermite-interpolation + rekonstruktionsfilter. TUNE ändrar uppspelningstakten, som på 909.
  **CYMBAL ROM:** REN (standard) eller 909 6-BIT (originalets korn).
  Mätt: under -31 dB energi över 18 kHz, alltså inget digitalt skräp. CH chokar OH.
- **Komponenttolerans:** varje seed ger maskinen egna små fel (±0,7 % tune, ±4 % decay).
- **Accent** ändrar klangen (kickens form, virvelns brus), inte bara volymen.
- Rena röster som standard: ingen inbyggd mättning i virvel, toms och kant. Smutsen väljs till.

## VINKELSLIP — `src/Vinkelslip.h` (per kanal)
LÄGE: DIOD, GERMANIUM, TEJP, VIKNING, LIKRIKTARE, ROM. DRIVE, MATNING (hårda slag distar mer), MIX.
- 4x översampling (linjärfas halfband) + ADAA. Mätt mot vanlig distorsion vid extrem drive:
  20–33 dB mindre skräp (DIOD -41, GERMANIUM -40, TEJP -52, VIKNING -36, LIKRIKTARE -39 dB).
- Alla kanaler har samma fasta fördröjning (23 samples) även avslagna, så de ligger i fas.
- Uppmätt nivåkompensation per läge: 0 dB volymändring när DRIVE vrids, bara klangen ändras.
  Av på alla kanaler som standard.

## ELNÄT
50 Hz-brum med udda övertoner, som en dåligt jordad verkstad. Av som standard.

## MIDI UT
Varje steg skickas som GM 909-not på kanal 10 (30 ms långa), med drift och jitter inbakat.
Spela in den i Bitwig, så får du Mills-timingen som MIDI. Kan stängas av (MIDI UT).

## MOTSTÅND vs BETONG
BETONG (gratis) = sampler som hackar dina egna ljud. MOTSTÅND (premium) = helsyntetiserad 909
med studioeffekter, VINKELSLIP och liveprestanda. Inga samples, ingen överlappning.

## Studioeffekter (steg 4c) — `src/Fx.h`
Signalväg: röster → VINKELSLIP → (send HALL/EKO per kanal) → MIX → Detroit Smog → PRESS → ISOLATOR → MASTER.
- **VARVSHALL:** 8-linjers FDN-reverb, diffusion, långsamt modulerade linjer (ingen metallisk ringning),
  lågskärning (kicken hålls ute), STORLEK, SVANS 0,3–12 s, MÖRKER, AVSTÅND 0–120 ms, NIVÅ,
  **FRYS** (ringer för evigt). Mätt: efterklangstiden träffar ratten inom 6 %, frysen 0,0 dB på 12 s.
- **EKO:** stereo tejpeko synkat till maskinens EGET drivande tempo (1/16–1/2), ÅTERKOPPLING upp till
  105 % (tejpen tämjer självsvängningen), TON, SLITAGE (wow, flutter, mättnad), PINGPONG.
  Mätt: ekot landar på exakt rätt sample.
- **ISOLATOR:** DJ-mixerns 3-bands kill-EQ (Linkwitz-Riley 24 dB/okt vid 250 Hz och 2,5 kHz),
  LÅG/MITT/TOPP + KILL-knappar. Mätt: summerar till 0,00 dB, kills -34 till -53 dB.
- **PRESS:** bus-kompressor med en ratt och automatisk nivå. **PUMP:** kicken duckar allt annat (upp till -14 dB).
- Tärning för effekterna (smakfulla sends: kicken hålls torr).

## Byggordning
1. ✅ Motorkärna (klocka, drunk walk, polymeter, Smog)
2. ✅ 909-rösterna + offline demo-rendering
3. ✅ PluginProcessor: allt ihopkopplat, separata utgångar per instrument, 2x oversampling på Smog
4a. ✅ Ljud v2: nya röster, VINKELSLIP, ELNÄT, MIDI ut
4b. ✅ Panel i DOCKLANDS-stil (se nedan)
4c. ✅ Studioeffekter: VARVSHALL, EKO, ISOLATOR, PRESS + PUMP, sends per kanal
4d. Sequencer: CHANS (sannolikhet), RULLE (rullningar), steglås, 8 mönster A–H
4e. 40+ presets (kit + mönster)
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
- Latens (VINKELSLIP 23 + Smog-översampling 3 = 26 samples i 48 kHz) rapporteras till DAW:en.


## Panelen (steg 4b) — `src/Gui.h`, `src/PluginEditor.*`
DOCKLANDS: rostig varvsplåt med nitar, rostrinnor och repor, lysrörsgrönt sken, Dymo-etiketter.
Ritas i 1200 x 936 och skalas fritt (55–150 %) med låst proportion. Startar i 900 x 702 (passar en 13-tums laptop).
- **DIE ZEIT:** strobe-ring som på en Technics (prickarna står still i fas, kryper när maskinen glider),
  BPM + "I FAS / 12 MS EFTER" i mitten, **jogghjul** för NUDGE (dra höger/vänster, fjädrar tillbaka),
  JITTER, DRIFT, FYLLA, BPM, EGET TEMPO, tärning.
- **RINGARNA:** varje aktiv kanal är en ring, spelhuvudet lyser vitt. Mitten räknar
  "ETTAN OM X TAKTER" och blinkar "ETTAN!" när alla möts.
- **DETROIT SMOG:** analog visare (snabb upp, långsam ner) med rött fält, MÄNGD, TRÖSKEL, ELNÄT, MASTER, tärning.
- **10 kanalremsor:** tärning (stilriktigt slumpat mönster per instrument, polymeter-längder för toms/kant),
  Dymo-namn, 32-stegs grid (klick = slag, dra upp/ner = styrka, dra i sidled = fler slag,
  högerklick = accent/mjuk/av), LÄNGD, TUNE/DECAY/TON/NIVÅ, VINKELSLIP läge/DRIVE/MATNING/MIX, TYST.
  Tärning för VINKELSLIP på alla kanaler (smakfullt: ungefär hälften, aldrig fullt).
- **Toppen:** SEED + tärning, MIDI UT, CYMBAL ROM, **SV/EN** (sparas i projektet). Tips-rutor på båda språken.
- Panelbilder renderas utan fönster: `MotstandSnapshot bild.png [sv|en] [skala]`, även i Actions.

## Test
    g++ -std=c++17 -O2 -Isrc tests/core_test.cpp -o core_test && ./core_test
    g++ -std=c++17 -O2 -Isrc tests/render_demo.cpp -o render_demo && ./render_demo demo.wav
    cmake -B build -DMOTSTAND_TESTS=ON && cmake --build build --target MotstandTest   # plugintest
