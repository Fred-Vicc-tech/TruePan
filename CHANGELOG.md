# Ändringslogg

## v2.2 – DISTANCE med 1 m i mitten (2026-11-26)
- DISTANCE-ratten har 1 m (standard, oförändrat ljud) klockan 12. Vänster halva: 0,25–1 m (närmare,
  orange båge), höger halva: 1–100 m (längre bort, blå båge). Båda halvorna logaritmiska.
- Projekt och presets från tidigare versioner behåller sitt avstånd.

## v2.1.1 – PAN i Ableton-stil (2026-11-26)
- PAN visas som i Ableton Live: 50L … C … 50R (förut L 100 … R 100), även i Lives parametervisning,
  STEREO FIELD och grafen.
- Procentvärdet under WIDTH och DEPTH 2 px högre upp.
- LEFT EAR / RIGHT EAR i HEAD-grafen flyttade åt höger.

## v2.1 – Ny layout enligt användarens bild (2026-11-26)
- AUTO-PAN: vågform (PAN/TREM) eller modell (LESLIE) har en egen väljarrad med pilar under PAN/TREM/LESLIE,
  samma typ som PRESET. Vågformens symbol och namn (större text) i rutan.
- WIDTH/DEPTH och rattarna längre ner i panelerna; värdet under PAN och OUTPUT 3 px längre ner.
- Over 0 dB längre åt höger i OUTPUT-rubriken. DISTANCE-ratten 2 px längre ner.
- Grafen 14 px högre (mer luft under rubriken), HEAD-knappen och huvudstorleken i mitten av rubrikraden,
  förklaringen (LEFT/RIGHT/LOUDNESS) längre åt höger. ITD-visningen till höger om huvudstorleken.
- Fönstret är 701 px högt.

## v2.0.1 – Inget hack när Lives loop börjar om (2026-11-26)
- När Live spelar med loop hoppade AUTO-PAN-mönstret (PARADIDDLE m.fl.) till rätt läge först i nästa
  ljudblock (upp till ca 12 ms för sent). Det gav ett hack vid varje nytt varv. Nu läses loopens start
  och slut från Live och mönstret börjar om på exakt rätt sampel. Nytt test jämför mot en sampelexakt
  referens (avvikelse förut 0,2, nu under 0,003).

## v2.0 – Rudiment (2026-11-26)
- Fem nya rudiment i AUTO-PAN: DOUBLE STROKE (R R L L), PARADIDDLE-DIDDLE (R L R R L L),
  INV. PARADIDDLE (R L L R L R R L), SIX STROKE ROLL (>R l l r r >L, betonade ändslag) och
  FLAM (lR rL: svagt förslag på motsatt sida 12 % av perioden före huvudslaget, 30 ms vid 1/8 och 120 bpm).
- Separata slag som i PARADIDDLE (en RATE-period per slag). I TREM: betonade slag fulla, obetonade svagare.
- Vågformsrutan: klick på vänster/höger halva bläddrar bakåt/framåt; långa namn krymps så att de får plats.
- Presets: Double Stroke 1/16 och Flam 1/8. Projekt och presets från tidigare versioner behåller sin form.

## v1.9.1 – Högre paneler (2026-11-26)
- De fyra översta panelerna är 30 px högre, med mer luft mellan knappar, rattar och rutor.
  Fönstret är 686 px högt (tidigare 656).

## v1.9 – PARADIDDLE med separata slag, PARAVARIANT (2026-11-26)
- PARADIDDLE: varje slag är en ren SQUARE-puls på en RATE-period: första halvan ute på sidan, andra halvan
  tillbaka i mitten. R-R och L-L hörs som två separata slag. RATE 1/16 = sextondelsslag; hela mönstret
  tar 8 RATE-perioder och följer Lives takt. I TREM: varje slag på, tyst mellan slagen.
- v1.8:s PARADIDDLE finns kvar som PARAVARIANT (ljudet stannar på sidan, halv RATE-period per slag).
- Presets: Paradiddle 1/16 och Paravariant 1/8. Projekt och presets från v1.8 öppnas med PARAVARIANT.

## v1.8 – PARADIDDLE (2026-11-26)
- Ny form i AUTO-PAN: PARADIDDLE (R-L-R-R-L-R-L-L) med egen symbol. Varje slag är lika långt som ett
  steg i SQUARE (en halv RATE-period); hela mönstret tar 4 RATE-perioder och följer Lives takt
  (vid RATE 1/4: åttondelsslag, mönstret börjar om varje takt). Fungerar även i TREM.
- Ny preset: Paradiddle 1/8.
- Projekt och presets från tidigare versioner behåller sin form (3 -> 4 former räknas om).

## v1.7 – DISTANCE (2026-11-26)
- Ny ratt DISTANCE 0,25–100 m i STEREO FIELD (standard 1 m = oförändrat ljud).
- Avståndslagen 1/r: −6 dB per fördubblat avstånd (+12 dB vid 0,25 m, −40 dB vid 100 m).
- Luftens dämpning enligt ISO 9613-1 (20 °C, 50 % RH, 101,325 kPa). Filtret anpassas per avstånd och
  samplingsfrekvens; avvikelse från standardens kurva högst ca 0,5 dB vid 44,1/48 kHz och 1,4 dB vid 96 kHz.
- Närfält med HEAD: avståndslagen per öra. 200 Hz nivåskillnad rakt från sidan: 6,8 dB på 0,25 m, 1,9 dB på 1 m.
- Avståndet glider mjukt vid automation (ca 30 ms).

## v1.6 – HEAD: psykoakustisk huvudmodell (2026-11-26)
- Ny knapp HEAD (i grafens rubrikrad) med huvudstorlek 12–30 cm (standard 17,5 cm = effektiv huvudradie 8,75 cm).
- Tidsskillnad mellan öronen enligt Brown & Duda / Woodworth: ITD = (a/c)(az + sin az), ca 0,66 ms rakt från sidan.
- Huvudskugga enligt Brown & Duda (1998): första ordningens filter per öra, alpha(theta) med amin 0,1 och thetamin 150 grader.
  Ljud från höger: 8 kHz +8 dB i höger öra och -8 dB i vänster; 200 Hz mindre än 0,5 dB skillnad.
- Allt normerat mot läget rakt fram: PAN i mitten är helt oförändrat. Mjuk av/på, fungerar med AUTO-PAN.
- Grafen visar i HEAD-läget varje öras nivå vid 4 kHz och 500 Hz och aktuell ITD.
- HEAD är av som standard, så äldre projekt låter som förut.

## v1.5 – Leslie: riktverkan (dovare bakåt) (2026-11-26)
- Hornet strålar diskanten som en ficklampa: brytfrekvensen följer riktningen på log-skala,
  ca 16 kHz rakt mot mikrofonen, 7 kHz åt sidan och 2,2 kHz rakt bort (12 dB/oktav).
  Bastrumman har en mild variant (4 kHz / 1,5 kHz / 450 Hz), VIBRATONE en mellanvariant.
- Nivåsvajet minskat något (horn 0,85 -> 0,7), eftersom diskantförlusten i sig sänker upplevd styrka;
  total nivå kompenserad så att Leslien fortfarande upplevs lika stark som PAN.
- Nytt test: diskanten (8 kHz) svajar minst 6 dB mer än mellanregistret (1,2 kHz).

## v1.4.2 – Mätarskala från -30 dB (2026-11-26)
- Nivåmätarna börjar vid -30 dB (skala -30, -24, -18, -12, -6, -3, 0 dB; topp +6 dB).
- Modellnamnet (t.ex. VIBRATONE) 1 px längre ner, centrerat i rutan.

## v1.4.1 – Fönsterjusteringar (2026-11-26)
- SPEED visar STOP, CHORALE och TREMOLO i samma stil som 1/4 osv.
- Modell-/vågformsrutan bredvid DEPTH är bredare, så att VIBRATONE får plats.

## v1.4 – Leslie-modeller med originalfart (2026-11-26)
- Modellväljare i LESLIE-läget (rutan bredvid DEPTH, klicka vänster/höger halva): MOD 122 (även 147),
  MOD 145 och VIBRATONE (Fender/Leslie 16, en enda roterande trumma, gitarrområdet).
- SPEED: STOP, CHORALE och TREMOLO med originalets fart, oberoende av tempot, följt av tempolägena 1/2 … 1/32.
  122/147: horn 48/400 varv/min, trumma 40/342 varv/min (uppmätta värden). Uppvarvning ca 0,9 s (horn)
  och 5,5 s (trumma), inbromsning ca 1,0 s / 4,8 s. VIBRATONE: 40/340 varv/min.
- I tempolägena snurrar trumman i samma förhållande till hornet som på originalet.
- Nya presets: Leslie 122 Chorale, Leslie 122 Tremolo, Leslie 145 Tremolo, Vibratone Fast.
- Projekt och presets från v1.1: SLOW blir CHORALE, FAST blir TREMOLO. Från v1.2-1.3: tempolägena behålls.

## v1.3 – Nivåer och mätarskala (2026-11-26)
- TREMOLO en aning mildare: DEPTH 50 % = −9 dB i dalarna (v1.2: −12 dB).
- LESLIE ca 2,4 dB starkare (ca +1,5 dB över torrt i effekt), så att den upplevs lika stark som PAN.
- Nivåmätarna i STEREO FIELD har skalstreck och siffror: −36, −24, −18, −12, −6, −3, 0 dB.

## v1.2 – SPEED-ratt, bredare Leslie, tydligare tremolo (2026-11-26)
- LESLIE: SPEED är en ratt i 8 steg (1/2, 1/4, 1/6, 1/8, 1/12, 1/16, 1/24, 1/32) = ett varv per
  notlängd i Lives tempo. Farten ändras fortfarande mjukt som på en riktig Leslie.
- LESLIE i stereo: mikrofonerna står på var sin sida, och rummets väggar ger reflexer
  (olika långt till vänster och höger). Ljudet snurrar tydligt mellan höger och vänster.
- TREMOLO: kvadrerad kurva, djupare dalar (DEPTH 50 % = −12 dB i stället för −6 dB).
  Presetet Tremolo 1/8 har DEPTH 85 %.
- Projekt och presets från v1.1: SLOW blir 1/2, FAST blir 1/16.

## v1.1 – Tremolo, Leslie, SAVE/LOAD, MONITOR (2026-11-26)
- AUTO-PAN har tre lägen: PAN, TREM (tremolo) och LESLIE (roterande högtalare, SLOW/FAST).
- Vågformen väljs genom att klicka på rutan bredvid DEPTH.
- OUTPUT: väljare STEREO / MONO / MID / SIDES under ratten (ersätter MONO-knappen). Over 0 dB flyttad upp.
- PRESET: SAVE och LOAD av egna presets i Dokument\TruePan Presets (ersätter RESET/MONO).
- LAW-rutan lika bred som PRESET-rutan.
- Ny text: "Psychoacoustic Stereo Panner".
- 3 nya fabrikspresets: Tremolo 1/8, Leslie Slow, Leslie Fast.
- Projekt sparade med v1.0 öppnas som förut (nya inställningar får standardvärden).

## v1.0 – Första versionen (2026-11-25)
- PAN med PAN LAW 0 / −3 / −4,5 / −6 dB (mitten alltid 0 dB).
- MODE: STEREO (båda kanalerna följer med) och BALANCE.
- WIDTH 0–200 %, LOW MONO 40–300 Hz (basen mono och i mitten).
- AUTO-PAN: DEPTH, RATE (4 takter … 1/32, trioler), SINE / TRIANGLE / SQUARE, följer Lives takt.
- OUTPUT −24…+12 dB med "Over 0 dB"-indikator.
- Fönster i TrueSub-stil: kuggrattar, STEREO FIELD med nivåmätare, PAN LAW-graf.
- 10 fabrikspresets, RESET och MONO-kontroll.
- Automatiska DSP- och värdtester i bygget, kontroll av Windows 7-kompatibilitet.
