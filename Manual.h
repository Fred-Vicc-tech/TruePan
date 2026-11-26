// Manual.h - texten på TruePans baksida (klicka på loggan).
// Engelska, som resten av gränssnittet. Varje rad är ett stycke:
//   kManHead = rubrik, kManBody = brödtext, kManNote = källa/notis (mindre, grå).
#pragma once

enum { kManHead = 0, kManBody = 1, kManNote = 2 };

struct ManualPara
{
    int kind;
    const wchar_t* text;
};

static const ManualPara kManual[] = {
    { kManHead, L"WHAT TRUEPAN DOES" },
    { kManBody, L"TruePan places a sound in the stereo field the way your ears and brain expect it. Besides level panning it can model "
                L"a human head (time and level differences between the ears), distance (air absorption and the inverse distance law), "
                L"and rhythmic movement (auto-pan, tremolo, rotary speaker and drum rudiments). With PAN in the center and the default "
                L"settings, TruePan passes the sound through unchanged." },
    { kManBody, L"All knobs: drag up/down, hold Shift for fine adjustment, use the mouse wheel for small steps and double-click to "
                L"return to the default value. Every setting can be automated and is saved with your project." },

    { kManHead, L"PAN AND PAN LAW" },
    { kManBody, L"PAN moves the sound from 50L through C to 50R, the same scale as Ableton Live. PAN LAW sets how much louder a sound "
                L"gets at the sides: 0, -3, -4.5 or -6 dB. The center is always 0 dB, so inserting TruePan never changes the level. "
                L"-3 dB (default) keeps a mono sound equally loud in every position (constant power)." },

    { kManHead, L"MODE, WIDTH AND LOW MONO" },
    { kManBody, L"STEREO (default) keeps both channels when you pan: the stereo image narrows toward the side and nothing is lost. "
                L"BALANCE works like a classic balance control and turns down the far channel. For mono sources both modes sound the same." },
    { kManBody, L"WIDTH goes from 0 % (mono) through 100 % (unchanged) to 200 % (extra wide), using mid/side processing." },
    { kManBody, L"LOW MONO (40-300 Hz) makes the bass below the chosen frequency mono and keeps it in the center, even when you pan. "
                L"It uses a 24 dB/octave Linkwitz-Riley crossover, so the summed level stays flat." },

    { kManHead, L"AUTO-PAN: PAN, TREM AND SHAPES" },
    { kManBody, L"PAN moves the sound between left and right. TREM (tremolo) moves the level instead. DEPTH sets the amount, RATE the "
                L"speed (4 bars to 1/32, including triplets). When Live is playing the movement locks to the song position and to loops; "
                L"SYNC is shown under RATE." },
    { kManBody, L"Shapes: SINE, TRIANGLE and SQUARE are classic waveforms. PARADIDDLE (R L R R L R L L) and the rudiments DOUBLE STROKE "
                L"(R R L L), PARADIDDLE-DIDDLE (R L R R L L), INV. PARADIDDLE (R L L R L R R L), SIX STROKE ROLL (accented R l l r r L) and "
                L"FLAM (a soft grace note on the opposite side just before each stroke) play every stroke as a separate pulse; RATE is the "
                L"length of one stroke. DIDDLETIE plays the paradiddle without returning to the center between strokes." },
    { kManBody, L"In TREM the sound stays in the center, so the two hands are told apart by their tone instead: left-hand strokes "
                L"sound slightly darker and right-hand strokes slightly brighter, and the first stroke of each group is accented "
                L"(unaccented strokes are 4 dB softer). Both follow DEPTH." },
    { kManNote, L"Tip: patterns with 6 strokes fit triplet rates (1/8T, 1/16T) and 6/8 time." },

    { kManHead, L"AUTO-PAN: LESLIE" },
    { kManBody, L"LESLIE simulates a rotating speaker in a room. The sound is split at 800 Hz into a treble horn and a bass drum that spin "
                L"at different speeds. Two microphones on opposite sides hear the level change, the Doppler effect (pitch wobble) and "
                L"the reflection from the wall behind. The horn beams the treble like a flashlight, so the sound gets darker when it "
                L"points away from a microphone." },
    { kManBody, L"Models: MOD 122 (also 147) is the classic Hammond Leslie, MOD 145 a smaller cabinet with a shorter room sound, and "
                L"VIBRATONE (Fender / Leslie 16) a single rotating drum for guitar. SPEED: STOP, CHORALE and TREMOLO run at the original "
                L"speeds, followed by tempo-synced steps (one horn revolution per note value). Speed changes ramp like the real motors: "
                L"the horn takes about 1 s, the heavy drum about 5 s." },
    { kManNote, L"Measured Leslie 122/147 speeds: horn 48 / 400 rpm, drum 40 / 342 rpm; ramp up about 0.9 s (horn) and 5.5 s (drum)." },

    { kManHead, L"OUTPUT AND MONITOR" },
    { kManBody, L"OUTPUT sets the level from -24 to +12 dB. Over 0 dB lights red when the signal exceeds 0 dBFS. The selector below "
                L"chooses what you hear: STEREO (normal), MONO, MID (the center only) or SIDES (the difference between left and right). "
                L"MONO is the quickest way to check mono compatibility." },

    { kManHead, L"STEREO FIELD AND DISTANCE" },
    { kManBody, L"The ring shows the PAN knob, the dot shows where the sound actually is right now (including auto-pan). Click or drag "
                L"sideways on the track to move PAN (it snaps to the steps 50L, 36L, 22L, 10L, C, 10R, 22R, 36R and 50R; double-click returns to C). The arrows beside the PAN value, and the left and right arrow keys (while the mouse is over the plugin), move PAN one step at a time. The orange band "
                L"is the auto-pan range. The dot gets smaller the farther away the sound is. The meters show peak level per channel." },
    { kManBody, L"DISTANCE goes from 0.25 to 100 m, with 1 m (unchanged sound) at 9 o'clock. Farther away the sound follows the "
                L"inverse distance law (-6 dB per doubling) and the air absorbs the treble. Closer than 1 m the level rises; TruePan "
                L"lowers OUTPUT by the same amount, so you see the change without the volume jumping. With HEAD on, a close sound also "
                L"gets a larger level difference between the ears, even in the bass, which makes it feel right next to your ear." },
    { kManNote, L"Air absorption follows ISO 9613-1 (20 C, 50 % humidity): about 1 dB at 8 kHz over 10 m and 10 dB over 100 m." },

    { kManHead, L"HEAD: THE PSYCHOACOUSTIC HEAD MODEL" },
    { kManBody, L"With HEAD on, panning works like a real head instead of a volume control. A sound from the side reaches the far ear a "
                L"little later (interaural time difference, up to about 0.66 ms) and with less treble, because the head is in the way "
                L"(interaural level difference: about 16 dB at 8 kHz but almost nothing in the bass). The near ear gets slightly more "
                L"treble. Your brain uses the time difference mainly below 1.5 kHz and the level difference above it, which is why "
                L"this gives a depth that plain panning cannot." },
    { kManBody, L"Head size (12-30 cm) sets the effective head diameter. 17.5 cm is the research standard (effective radius 8.75 cm, "
                L"matching measured human time differences). Larger values exaggerate the effect. The graph then shows each ear's level "
                L"at 4 kHz (solid) and 500 Hz (dashed), and ITD shows the current time difference." },
    { kManNote, L"Time difference: Woodworth's formula (a/c)(az + sin az). Head shadow: Brown & Duda (1998), spherical head model. "
                L"Check your mix in MONO when using HEAD: time differences can sound hollow when summed to mono." },

    { kManHead, L"PRESETS" },
    { kManBody, L"The arrows browse the factory presets first, then your own. SAVE stores a preset (.tpp) in the folder "
                L"Documents\\TruePan Presets, which is created automatically. LOAD opens a preset file. Factory presets also appear in "
                L"Live's preset list." },

    { kManHead, L"SCIENCE AND REFERENCES" },
    { kManNote, L"Woodworth, R. S. (1938). Experimental Psychology. Interaural time difference of a spherical head." },
    { kManNote, L"Brown, C. P. & Duda, R. O. (1998). A structural model for binaural sound synthesis. IEEE Trans. Speech and Audio Processing." },
    { kManNote, L"ISO 9613-1 (1993). Acoustics - Attenuation of sound during propagation outdoors - Part 1: Calculation of the "
                L"absorption of sound by the atmosphere." },
    { kManNote, L"Linkwitz, S. H. (1976). Active crossover networks for noncoincident drivers. J. Audio Eng. Soc." },
    { kManNote, L"Leslie rotor speeds and ramp times: measurements of Leslie 122/147 cabinets reported by organ technicians." },
};

static const int kManualCount = (int)(sizeof(kManual) / sizeof(kManual[0]));
