# Changelog

## 0.27

- **Loops moved along the scale**: SHIFT + OCT+ / OCT− moves every loop of every layer one step of the chosen
  scale up or down (C to D in C major), the notes sounding moving at once; up to two octaves of the scale either
  way, shown on the screen. Another scale or root chosen meanwhile is walked instead. An overdub recorded while
  the loops are moved plays back as played. Sessions keep it; sessions saved before load as they were. The
  screen says TRANSP +2, TRANSP 0.
- **Drones stay whole in loops**: a note still held when REC closes the loop is tied over the loop's end, sounding
  on as it goes round instead of being let go and struck again each time (which was heard as a stutter, ANIMA and
  CALIGULA above all).
- **No click when ANIMA reuses a voice**: a voice taken over for a new note (loops going round, many notes at once)
  kept its comb resonator, phaser and sample hold from the note before, heard again at full level as the new note
  began. They now start empty.
- **The overload guard waits longer**: a release tail goes only when the audio is past 90 % (was 85 %) twice in a
  row; a held note only when it is actually late, past 100 %. SHIFT shows the load now, as one percentage.

## 0.26

- **RUSSOLO**, a fourth engine: noise, logic and broken buffers, re-making the techniques of Robert Heel's
  Noisferatu. Seventeen kinds — HOLD, DUST, GATES, CRACKLE (noise); NOR, XOR, RING, CASCADE, CLANG (oscillators
  combined bit by bit); CHUNKS, JUMP, WANDER, BEND, QUAD (generated buffers read with their addresses bent);
  BLIPS, WALK, CHORD (chance-driven tones) — at a third of the sample rate in 10 bits, crushed and sample-held
  further as each sound says. A key gates it and sets its pitch; one organism a layer, as CALIGULA. Its macros:
  the kind's two parameters, CRUSH and CHAOS. The paths lean it: AMBIENT to chords, dust, crackle, blips and
  walks, clean and slow; EXPERIMENTAL to logic and bent buffers, crushed and abrupt. Presets: 64 for it too.
- **Sessions are a fuller snapshot**: they now also keep each layer's granulator character and its latched
  notes (which sound again on load). The master reverb was already kept. Sessions and presets saved before load
  as they were.
## 0.25

- **Voices from a shared pool**: twelve voices for all the layers instead of two fixed per layer. A layer takes as
  many as its notes need — a twelve-note chord on one layer, single notes on others. With all twelve busy, a new
  note takes the oldest release (any layer), else its own layer's oldest held note, else the oldest held anywhere.

## 0.24

- Sessions are written to and read from the flash a sector at a time: about 50 KB of RAM freed (more than half of
  it), for more voices and a fourth engine to come. Nothing changes in use; every session saved before loads.
- The start-up screen's name fits the screen.

## 0.23

- **Overdubs in and out**: hold PLAY and the chosen layer's loops appear on the white keys (the first loop on
  the leftmost, then each overdub), lit while they play and shown on the screen. A key takes its loop out or
  brings it back in — while they play, at once and from its start; while stopped, choosing what PLAY will
  start. PLAY pressed and let go still starts and stops the loops. Sessions keep which loops are out.

## 0.22

- **Macros**: the black key before SHIFT turns **EDIT** on (lit) and off. While it is on, KNOB 1–4 are four
  macros that reshape the chosen layer's sound live, chosen for each engine and kind of sound — FM: BRIGHT,
  DETUNE or SKEW, GRIT, MOTION; ANIMA: TONE, GRAINS / AIR / FM / VOWEL / DRIVE, SPACE, MOTION; CALIGULA: EDGE,
  DEPTH, CHAOS or ECHO, HAND or PULSE. The middle is the sound as drawn. Sessions and presets keep them; HOME
  starts a new sound in the middle.
- **FM in tune**: a slip in the FM generator put nearly every operator ratio near an even multiple, so patches
  played about two octaves above the key and lost most of their designed ratios (bells, near-harmonic beating).
  New sounds now have the ratios they were designed with, at the key's pitch. Sounds saved before (sessions,
  presets) come back exactly as they were.

## 0.21

- **Presets**: SHIFT + SAVE opens the chosen layer's engine's presets, 64 an engine (P1–P64), four a page.
  SAVE held saves the layer's sound into the slot chosen (asking OVERWRITE? over a full one), SAVE loads it.
- **Richer, smarter generators** (generation 1):
  - FM: eight new families — CHOIR, BOWED, HOLLOW, BLOOM, TIDE, SHARDS, STATIC, WIND; every patch shaped
    with care (no screaming sidebands, brightness scaled down the keyboard), gentler mutations, and several
    patches drawn and measured, keeping one whose brightness suits the path.
  - ANIMA: six new kinds — TIDE, CHOIR, SHARDS, BOWL, DUST, MACHINE; the filter follows the keyboard; the
    second oscillator's interval suits the path; AMBIENT sounds get a space to ring in.
  - CALIGULA: four new species — LITANY, UNDERTOW, WIRE, MOTH; four new tunings (fourths, a minor seventh,
    open fifths, a quarter-tone cluster); the hand plays phrases, often on a pulse; a bed that the gates
    never close.
- Sounds saved before (in sessions) come back exactly as they were: the older generators are kept as they
  were, and a test checks it.

## 0.20 — Tonitor

- The firmware is now called **Tonitor** (it was developed as ACOUSMA). The FM-1 shows the new name at
  start-up and over USB ("FM-1 Tonitor"); sessions saved before keep working.
- A **browser installer**: install from Chrome, Edge or Opera over Web MIDI, no tools needed.
- The website, the logo, the manual online.

## 0.19

- **SHIFT + PLAY** with no loop playing starts every loop there is, all together from their starts.

## 0.18

- On the SESSIONS page, **SAVE held** saves into the slot shown: an empty one asks SAVE NEW, a full one asks
  **OVERWRITE?** (on NO until turned to YES).
- Slots holding a session show their number in red; empty slots in grey with EMPTY.

## 0.17

- **ANIMA EXPERIMENTAL** made much wilder and more textured: crushed rates, folding, dragged noise,
  scattered grains, fast tremolo, a ringing comb.
- **SHIFT + PLAY** stops every loop on every layer, from any layer.
- The level meter runs across the bottom of the screen, above the pan; the level is shown in %.
- Long messages (EXPERIMENTAL) fit the screen.

## 0.16

- **Generative paths**: AMBIENT and EXPERIMENTAL on the two leftmost black keys, per layer, for every
  engine.
- CALIGULA's slow swells and long releases now play as designed.
- Under heavy load the note just played is never the one dropped (ANIMA notes no longer go silent beside
  CALIGULA); SHIFT shows the audio load.

## 0.15

- Latching: hold keys, then press SHIFT. On the SCALE page SELECT chooses the root, ALGORITHM the scale.

## 0.14

- **SHIFT** (the rightmost black key) and **latched keys**.

## 0.13

- CALIGULA played from the keys like the other engines, a simulated hand on its plates.

## 0.12

- **CALIGULA**, a third engine.

## 0.1 – 0.11

- FM and ANIMA engines; six layers with two voices each; a free looper per layer, then up to ten overdubs;
  layer level, pan and reverb send; the master reverb; 64 session slots; scales on the white keys; the DJ
  filter, the granulator and the crush per layer.
