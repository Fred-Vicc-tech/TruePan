# TruePan v1.1 – Psychoacoustic Stereo Panner

VST2-effekt (64-bit) för Ableton Live 9.6 på Windows 7 och senare.
Byggd på samma sätt som TrueSub: eget VST2-gränssnitt (ingen JUCE), CMake,
GitHub Actions på windows-2022 med Visual Studio 2022 (toolset v143),
statisk runtime och `_WIN32_WINNT=0x0601`.

## Funktioner

| Del | Vad den gör |
|---|---|
| **PAN** | Flyttar ljudet från helt vänster (L 100) via mitten (C) till helt höger (R 100). Dubbelklick = mitten. |
| **PAN LAW** | 0 / −3 / −4,5 / −6 dB. Mitten är alltid 0 dB, så pluginet ändrar inte nivån när PAN står i mitten. Standard −3 dB = ett monoljud låter lika starkt var det än står. |
| **MODE** | **STEREO** (standard): båda kanalerna följer med när du panorerar – stereobilden smalnar av mot sidan, inget försvinner. **BALANCE**: som en vanlig balansratt – den bortre kanalen sänks. För monoljud låter de två lika. |
| **WIDTH** | 0 % (mono) … 100 % (oförändrat) … 200 % (extra brett). |
| **LOW MONO** | Off eller 40–300 Hz. Basen under frekvensen blir mono och **stannar i mitten** även när du panorerar (bra med TrueSub). 24 dB/oktav Linkwitz-Riley, platt nivå. |
| **AUTO-PAN** | Tre lägen: **PAN** (ljudet åker mellan vänster och höger), **TREM** (tremolo: nivån svajar i takt) och **LESLIE** (roterande högtalare: horn och bastrumma som snurrar, dopplereffekt, SLOW/FAST med naturlig uppvarvning). DEPTH = hur mycket. För PAN/TREM: RATE (4 takter … 1/32, även trioler) och vågform (klicka på rutan: SINE / TRIANGLE / SQUARE). Följer Lives takt när Live spelar (”SYNC” visas). |
| **OUTPUT** | −24 … +12 dB. ”Over 0 dB” (uppe till höger) lyser rött om ljudet går över 0 dBFS. Väljaren under ratten: **STEREO** (normalt), **MONO**, **MID** (bara mitten) eller **SIDES** (bara sidorna). MONO och MID låter likadant – MID finns som tydligt namn för mid/side-lyssning. Valet sparas i projektet. |
| **STEREO FIELD** | Visar var ljudet är just nu (ringen = PAN-ratten, punkten = verklig position med AUTO-PAN) och nivåmätare för L och R. |
| **PRESET** | 13 fabrikspresets (syns även i Lives presetlista), sedan dina egna. **SAVE** sparar en egen preset (.tpp) i mappen *Dokument\TruePan Presets*, som skapas automatiskt. **LOAD** öppnar en preset. Pilarna bläddrar genom fabrikspresets och dina egna. |
| **PAN LAW-grafen** | Verkliga kurvor för vänster (cyan), höger (orange) och total ljudstyrka (streckad). |

Alla rattar: dra upp/ner, Shift = finjustering, mushjul = små steg, dubbelklick = standardvärde.
Alla inställningar kan automatiseras i Live och sparas i projektet.

## Installera

1. Gå till **Actions** i repot → senaste gröna körningen.
2. Ladda ner **TruePan-win64** (en zip-fil) och packa upp `TruePan.dll`.
3. Lägg `TruePan.dll` i din VST-mapp (samma mapp som TrueSub).
4. Starta Live → webbläsaren → **Plug-ins** → dra TruePan till ett ljudspår
   (eller efter TrueSub på samma spår).

## Automatiska tester (körs i varje bygge)

- **TruePanTests** (ljudmotorn): oförändrat ljud i standardläget, alla PAN LAW,
  BALANCE/STEREO, WIDTH, LOW MONO (bas mono, platt nivå, inget klick),
  AUTO-PAN (tid, takt, mjuka hopp), TREMOLO, LESLIE (fart, stereo, doppler, inget klick),
  OUTPUT, MONITOR, presetfiler, konstiga värden, processortid.
- **HostTests** (låtsas vara Live): stereoeffekt 2 in/2 ut, parametrar och texter,
  tempo/position från värden, spara/öppna projekt (även från v1.0), trasig data, presets.
- Bygget kontrollerar också att `TruePan.dll` inte behöver några DLL-filer som
  saknas i Windows 7.

## Filer

| Fil | Innehåll |
|---|---|
| `TruePanDSP.h` | Ljudmotorn (inga Windows-delar, testas på alla datorer) |
| `TruePan.cpp` | VST2-delen och fönstret (GDI+) |
| `vst2.h` | Eget VST2-gränssnitt (samma som i TrueSub) |
| `TruePanTests.cpp`, `HostTests.cpp` | Automatiska tester |
| `CMakeLists.txt`, `.github/workflows/build.yml` | Bygget |
