# Ändringslogg

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
