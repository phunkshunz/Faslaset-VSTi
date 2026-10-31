# FASLÅSET

Faslåst kick + bas, som VST3 och AU för macOS (VST3 för Windows). Phunkshunz AUDIO, psy-serien.

Kick och bas som krockar är den vanligaste orsaken till en grötig botten. Sidechain är silvertejp. FASLÅSET bygger dem i samma maskin: kicken stängs först när dess svängning passerar noll, och basen startar med fas 0 på exakt den punkten, ner på bråkdelen av ett sample. Innan nästa slag huggs basen av med en giljotin som landar på exakt noll. Ingen överlappning, inga utsläckta frekvenser.

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
2. Bläddra bland de 12 presetsen med pilarna, eller klicka på namnet.
3. Välj **SPIKRAKT** (psy) eller **SLÄP** (techno).
4. Ställ **TON** i låtens tonart.
5. Slå tärningarna i KICK- och BAS-panelen, eller **DICE** för båda.
6. Kolla oscilloskopet: grönt och rosa rör aldrig varandra. Brickan visar **NOLL FASFEL** så länge inget sample någonsin krockat.

## Lägena

| Läge | Vad det gör |
|---|---|
| **SPIKRAKT** | Psy. Kick på slaget, bas på de tre följande 16-delarna (K-B-B-B). Varje basnot startar på fas 0 och huggs av före nästa 16-del. |
| **SLÄP** | Techno. En lång sub per slag som startar på kickens nollgenomgång och rullar efter, tills giljotinen hugger av den före nästa kick. |

## Rattarna

| Panel | Rattar |
|---|---|
| **KICK** (syragrön) | START, SLUT och SVEP (pitch-svepet), LÄNGD (tid till −60 dB, sen stängs den på nästa nollgenomgång), DRIV, NIVÅ |
| **BAS** (magenta) | TON, FORM (sinus till såg), FILTER, RESO, ENV och ENV TID (psy-plucket), DECAY, BETT, NIVÅ |
| **MASTER** | GILJOTIN (rampen före nästa slag, 0,2–5 ms), VOLYM |

DRIV och BETT mättar utan att flytta nollgenomgångarna, så låsningen håller även med mycket mättning.

## Säkerhet

All indata kläms och kontrolleras. Om något någonsin skulle ge ett ogiltigt tal (NaN eller Inf) tystas blocket, motorn nollställs och räknaren SPÄRR går upp. Utgången har ett hårt tak på 0 dBFS. Testerna i `tests/` kör 300 slumpade inställningar, tre samplingsfrekvenser, loopar och skräp-indata, och kräver noll krockande samples.

## Bygga själv

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

JUCE 8 hämtas automatiskt. GitHub Actions bygger Mac och Windows vid varje push till `main` och lägger zipparna under Releases.
