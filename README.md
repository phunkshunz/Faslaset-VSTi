# MASKINHALLEN : LÄCKAGET

**Phunkshunz AUDIO**: en effekt byggd som en kylanläggning som håller på att gå sönder.

Ljudet går genom en kylkrets: **KOMPRESSOR** (mättnad) → **EXPANSIONSVENTIL** (svaj i tonhöjd) →
**FÖRÅNGARE** (filter) → **SIKTGLAS** (bubblor) → **HÖGTRYCKSPRESSOSTAT** (limiter).
Allt styrs av en medvetet felinställd regulator med dödtid. Börvärdet hoppar i takt, så pendlingen landar i groovet.

- **BEHÖRIGHET**: från *Kat 1-tekniker* (stabilt och rent) till *Lärling, fredag 15:30* (allt går sönder).
- **KÖLDMEDIUM**: 20 medier med egen karaktär, från R744 (CO₂, nervös) till R718 (vatten, segt och djupt).
- **VÄRMELAST**: låten själv stör regulatorn. Mycket ljud in betyder att den tappar greppet, i breaket lugnar den sig.
- **OLJERETUR**: off-beat-16-delar kommer sent, ett släpande sväng direkt på ljudet.
- **NÖDSTOPP / ÅTERSTART**: kompressorn varvar ner, skär ihop och startar igen (går att trigga med MIDI).
- Tyst in = tyst ut. Allt brus i maskinen rör bara modulationen, aldrig ljudet.

Status: steg 1 av 4 (ljudmotorn, testad). Repot hette tidigare Motstand-VSTi och Faslaset-VSTi; den gamla koden finns i git-historiken.

## Testa motorn

```
g++ -std=c++17 -O2 -Isrc tests/core_test.cpp -o core_test && ./core_test
g++ -std=c++17 -O2 -Isrc tests/render_demo.cpp -o render_demo && ./render_demo .
```
