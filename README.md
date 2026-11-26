# TruePan v1.8 – Psychoacoustic Stereo Panner

VST2-effekt (64-bit) för Ableton Live 9.6 på Windows 7 och senare.
Byggd på samma sätt som TrueSub: eget VST2-gränssnitt (ingen JUCE), CMake,
GitHub Actions på windows-2022 med Visual Studio 2022 (toolset v143),
statisk runtime och `_WIN32_WINNT=0x0601`.

## Funktioner

| Del | Vad den gör |
|---|---|
| **PAN** | Flyttar ljudet från helt vänster (L 100) via mitten (C) till helt höger (R 100). Dubbelklick = mitten. |
| **PAN LAW** | 0 / −3 / −4,5 / −6 dB. Mitten är alltid 0 dB, så pluginet ändrar inte nivån när PAN står i mitten. Standard −3 dB = ett monoljud låter lika starkt var det än står. |
| **HEAD** | Psykoakustisk huvudmodell (knappen HEAD i grafens rubrikrad). Ersätter PAN LAW med det som händer vid riktiga öron: **tidsskillnad** (Woodworth: upp till ca 0,66 ms rakt från sidan) och **huvudskugga** (Brown & Duda 1998: bortre örat tappar diskant, närmaste örat får mer; basen påverkas nästan inte). Huvudstorlek 12–30 cm (standard 17,5 cm = forskningens effektiva huvud; större = överdriven effekt). PAN i mitten = oförändrat ljud. Grafen visar varje öras nivå vid 4 kHz och 500 Hz samt aktuell tidsskillnad (ITD). |
| **DISTANCE** | 0,25–100 m (ratten i STEREO FIELD, standard 1 m = oförändrat). **Avståndslagen**: −6 dB per fördubblat avstånd. **Luftens dämpning** enligt ISO 9613-1 (20 °C, 50 % luftfuktighet): diskanten försvinner med avståndet (t.ex. ca −1 dB vid 8 kHz på 10 m, −10 dB på 100 m). Med HEAD även **närfält**: nära huvudet växer nivåskillnaden mellan öronen även i basen. Punkten i STEREO FIELD blir mindre ju längre bort ljudet är. |
| **MODE** | **STEREO** (standard): båda kanalerna följer med när du panorerar – stereobilden smalnar av mot sidan, inget försvinner. **BALANCE**: som en vanlig balansratt – den bortre kanalen sänks. För monoljud låter de två lika. |
| **WIDTH** | 0 % (mono) … 100 % (oförändrat) … 200 % (extra brett). |
| **LOW MONO** | Off eller 40–300 Hz. Basen under frekvensen blir mono och **stannar i mitten** även när du panorerar (bra med TrueSub). 24 dB/oktav Linkwitz-Riley, platt nivå. |
| **AUTO-PAN** | Tre lägen: **PAN** (ljudet åker mellan vänster och höger), **TREM** (tydligt tremolo: nivån svajar i takt, DEPTH 50 % = −9 dB i dalarna) och **LESLIE** (roterande högtalare i ett rum, snurrar mellan vänster och höger med väggreflexer, dopplereffekt och riktverkan: dovare klang när rotorn pekar bort). Modell väljs i rutan bredvid DEPTH: **MOD 122** (även 147), **MOD 145** eller **VIBRATONE** (Fender/Leslie 16). SPEED: **STOP**, **CHORALE** och **TREMOLO** med originalets fart (122: horn 48/400 varv/min, trumma 40/342 varv/min, horn ca 1 s och trumma ca 5 s att varva om), därefter tempolägen 1/2 … 1/32 (ett varv per notlängd i Lives tempo). DEPTH = hur mycket. För PAN/TREM: RATE (4 takter … 1/32, även trioler) och vågform (klicka på rutan: SINE / TRIANGLE / SQUARE / PARADIDDLE = R-L-R-R-L-R-L-L). Följer Lives takt när Live spelar (”SYNC” visas). |
| **OUTPUT** | −24 … +12 dB. ”Over 0 dB” (uppe till höger) lyser rött om ljudet går över 0 dBFS. Väljaren under ratten: **STEREO** (normalt), **MONO**, **MID** (bara mitten) eller **SIDES** (bara sidorna). MONO och MID låter likadant – MID finns som tydligt namn för mid/side-lyssning. Valet sparas i projektet. |
| **STEREO FIELD** | Visar var ljudet är just nu (ringen = PAN-ratten, punkten = verklig position med AUTO-PAN) och nivåmätare för L och R med skala −30, −24, −18, −12, −6, −3 och 0 dB. |
| **PRESET** | 15 fabrikspresets (syns även i Lives presetlista), sedan dina egna. **SAVE** sparar en egen preset (.tpp) i mappen *Dokument\TruePan Presets*, som skapas automatiskt. **LOAD** öppnar en preset. Pilarna bläddrar genom fabrikspresets och dina egna. |
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
