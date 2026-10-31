# FASLÅSET

Faslåst kick + basgång, som VST3 och AU för macOS (VST3 för Windows). Phunkshunz AUDIO, psy-serien.

Kick och bas som krockar är den vanligaste orsaken till en grötig botten. Sidechain är silvertejp. FASLÅSET spelar en samplad kick (BETONG:s 29 kickar) och en genererad basgång i samma maskin: kicken stängs först när vågformen passerar noll, och varje basnot startar med fas 0 på exakt den punkten, ner på bråkdelen av ett sample. Varje basnot tonar ut mjukt till exakt noll före nästa händelse. Ingen överlappning, inga utsläckta frekvenser, inga klick.

Manualen (engelska, PDF) ligger i zip-filen. Hela specen finns i [SPEC.md](SPEC.md).

## Ladda ner

Mac: https://github.com/phunkshunz/Faslaset-VSTi/releases/latest/download/FASLASET-macOS.zip

Windows (VST3): https://github.com/phunkshunz/Faslaset-VSTi/releases/latest/download/FASLASET-Windows.zip

Ladda inte ner via den gröna knappen **Code**. Den ger bara källkoden, inte pluginet.

## Installera

**Mac:** dubbelklicka på zip-filen i Hämtade filer. Öppna sedan Terminal och kör:

```
bash ~/Downloads/FASLASET/installera.command
```

**Stäng Bitwig helt och starta igen.** FASLASET finns bland instrumenten. Kräver macOS 11 eller nyare, Intel eller Apple Silicon.

**Windows:** packa upp och lägg `FASLASET.vst3` i `C:\Program Files\Common Files\VST3`.

## Kom igång på en minut

1. Lägg FASLASET på ett instrumentspår och tryck play. Den följer tempot och spelar själv, ingen MIDI behövs. Ingen DAW? Tryck **SPELA**.
2. Bläddra bland de 14 presetsen med pilarna, eller klicka på namnet.
3. Välj **SPIKRAKT** (psy) eller **SLÄP** (techno).
4. Ställ **GRUNDTON** och **SKALA** i låtens tonart. Hela basgången följer med.
5. Slå tärningen i BASGÅNG-panelen tills en basgång fastnar. **TONER** byter toner men behåller rytmen, **RYTM** tvärtom, **KLANG** slumpar basens ljud. **DICE** uppe till höger slumpar allt. **ÅNGRA** tar dig tillbaka (40 steg).
6. Kolla oscilloskopet: grönt och rosa rör aldrig varandra. Brickan visar **NOLL FASFEL** så länge inget sample någonsin krockat.

## Basgången

Rutnätet har 1–8 slag (**SLAG**) med tre basnoter per slag: 16-del 2, 3 och 4 efter kicken.

| Gör så här | Så händer |
|---|---|
| Klicka | Noten på/av |
| Dra upp/ner (eller scrolla) | Byt ton i skalan |
| Dubbelklicka | Tillbaka till grundtonen |
| Högerklicka | Oktav upp/ner och betoning |

Tärningen slumpar inte vilt. Den bygger ett motiv som mest är grundtonen, upprepar det, och lägger rörelse där en fras behöver det (sista slaget alltid, mitten ibland). Tonerna väljs med tyngd på oktav, kvint, ters och i frygisk den lilla sekunden. Minst hälften av noterna är alltid grundtonen. **STIL** styr känslan: RULL (klassisk rullande psy), GALOPP (studs), GLES (progg, mer luft), VILD (mer hopp och oktaver).

## Lägena

| Läge | Vad det gör |
|---|---|
| **SPIKRAKT** | Psy. Kick på slaget, basnoter på 16-delarna efter (K-B-B-B) enligt rutnätet. Varje not startar på fas 0 och tonar ut före nästa 16-del. |
| **SLÄP** | Techno. En lång basnot per slag (översta raden) som startar på kickens nollgenomgång och rullar efter. |

## Rattarna

| Panel | Rattar |
|---|---|
| **KICK** (syragrön) | SAMPLE (29 kickar från BETONG), STÄMNING (±12 halvtoner), LÄNGD (sen stängs den på nästa nollgenomgång), DRIV, NIVÅ |
| **BASGÅNG** (magenta) | GRUNDTON, SLAG, SKALA, STIL, FORM (sinus till såg), FILTER, RESO, ENV och ENV TID (psy-plucket), DECAY, LÄNGD, BETT, NIVÅ |
| **MASTER** | UTKLING (mjuk release i slutet av varje basnot, 1–12 ms), VOLYM |

## Säkerhet

All indata kläms och kontrolleras. Om något någonsin skulle ge ett ogiltigt tal (NaN eller Inf) tystas blocket, motorn nollställs och räknaren SPÄRR går upp. Utgången har ett mjukt tak strax under 0 dBFS. Klockan går bara framåt, så en DAW som hackar lite i positionen kan aldrig trigga ett slag två gånger. Testerna i `tests/` kör 300 slumpade inställningar, tre samplingsfrekvenser, loopar, hackande DAW-position och skräp-indata, och kräver noll krockande samples, att varje basnot startar på noll och tonar ut till noll, och att kicken stängs på en nollgenomgång.

## Bygga själv

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8 hämtas automatiskt. GitHub Actions bygger Mac och Windows vid varje push till `main` och lägger zipparna under Releases.
