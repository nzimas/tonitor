<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
    <img src="docs/assets/logo-light.svg" alt="Tonitor" width="520">
  </picture>
</p>

<p align="center"><b>An alternative firmware for the M-VAVE FM-1, for electroacoustic experimentation, live ambient music and sound exploration.</b></p>

<p align="center">
  <a href="https://nzimas.github.io/tonitor/install.html"><b>Install via browser</b></a> ·
  <a href="https://nzimas.github.io/tonitor/manual.html">Manual</a> ·
  <a href="https://nzimas.github.io/tonitor/">Website</a> ·
  <a href="CHANGELOG.md">Changelog</a>
</p>

---

> [!WARNING]
> Installing Tonitor replaces the firmware M-VAVE ships with the FM-1. It is unofficial, is not made,
> endorsed or supported by M-VAVE, and installing it **may void any warranty that applies to your device**.
> You install it at your own risk; read the [disclaimer](#disclaimer) first.

## What it is

Tonitor turns the FM-1, a small FM groovebox, into an instrument for **acousmatic and electroacoustic
music** — sound heard for itself, its texture, its grain and its movement — and for **live ambient music**,
dark ambient above all.

**It is not a groovebox.** There is no tempo, no step sequencer, no pattern, no drum kit. Instead there
are six tracks of *sound objects*: each track has its own engine, its own sound, voices drawn from a shared pool as it needs them, and its own free
loop recorder, and every sound is drawn by algorithms that aim for surprise rather than for a preset that
sounds nice. You play textures, drones, gestures and their slow transformations, layer them, loop them
freely out of time with each other, and shape them while they play.

The screen is made for **low vision**: very few words, in very large type, with high contrast, and each
track in its own colour.

### Highlights

- **Eight sound engines**, chosen per track:
  - **FM** — six-operator, DX7-style FM, generating sound objects: bells and gongs, spectral drones,
    noise, impacts, iterations, glissandi, beating spectra, sub-bass, creaks.
  - **ANIMA** — a lo-fi synthesizer played by a simulated hand (oscillators, FM, grains, noise, filters,
    folding, a phaser and a resonator), all moved by the hand's gestures.
  - **CALIGULA** — a restless eight-oscillator organism after the architecture of the LYRA-8, with
    cross-modulation, a hyper-LFO, bursts and surges, and a simulated hand on its plates.
  - **RUSSOLO** — noise, logic and broken buffers: sample-and-hold and dusty noise, oscillators combined
    bit by bit, generated buffers read with their address bits bent, chance-driven tones; crushed and
    sample-held like a small DAC.
  - **DRUID** — strings and resonant bodies after Mutable Instruments Rings: a Karplus-Strong string with
    dispersion, sympathetic strings, and a modal resonator, plucked, struck, bowed or rattled.
  - **BAYAREA** — a West Coast voice: a morphing oscillator with two FM modulators, a wavefolder, and a low-pass
    gate or a sustaining envelope over resonant low-passes.
  - **HIVE** — a swarm of seven oscillators per voice, detuned, stacked or on inharmonic partials, moved by
    slow LFOs and noise, through a 4-pole filter with its own envelope and a drive; melodic kinds and textural ones.
  - **OBRADOR** — an analog polysynth after the Oberheim SEM: two oscillators and a second voice card, the SEM's
    filter morphing from low-pass through notch to high-pass, analog envelopes and drift.
- **Generative paths** — lean each track's new sounds towards **AMBIENT** (ethereal, melodic, dark) or
  **EXPERIMENTAL** (edgier, gnarlier, more textured), or leave it **OPEN**.
- **Play along**: other tracks improvise with the one you play (REACT), shadow it (SHADOW) or arpeggiate its
  chords (ARPEGGIO).
- **Six free loopers**, one per track, with up to ten overdubs each, every loop with its own length.
- **Envelope**: attack, decay, sustain and release per track, on the ENVELOPE page, drawn on the screen.
- **Latching**: hold notes, press SHIFT, and they sustain as drones while you play on top.
- Per track: **DJ resonant filter**, **granulator** (dark ↔ scatter), **bit/rate crush**, **distortion**, level, pan and
  a send to a lush **master reverb** (Dattorro plate).
- **Scales** on the white keys (23: the seven modes, minors, pentatonics, blues, whole tone, octatonic, Hungarian minor, double harmonic, enigmatic, Prometheus, Japanese and Balinese scales),
  any root.
- **64 session slots** in the FM-1's flash: every track, sound, loop and setting.

## Installing

The easiest way is the **[browser installer](https://nzimas.github.io/tonitor/install.html)**: connect
the FM-1 with a USB data cable, open the page in a desktop **Chrome, Edge or Opera** (they support Web
MIDI with SysEx), and follow it. It finds the FM-1, checks what it runs, writes Tonitor and checks the
result. Nothing is uploaded anywhere: the page talks to the FM-1 directly.

It installs over whatever the FM-1 runs now — M-VAVE's own firmware, another alternative firmware or an earlier Tonitor.
Do not unplug the FM-1 while it writes.

To build and install from source instead, see [Building from source](#building-from-source).

**If something goes wrong:** hold **OCT−** while switching on for the USB rescue mode (the screen says
RESCUE), then install again. Two failed starts in a row also enter it. Holding **OCT− and OCT+** for five
seconds enters the chip's own updater (UBOOT). If the FM-1 no longer starts at all, recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

**Going back:** the installer and `./install.sh` install any FM-1 package; to
return to M-VAVE's firmware, use M-VAVE's own updater. Apart from the update itself, Tonitor writes to the
FM-1's flash only when you save a session — into a user data area that other firmwares may also use, so
**saving Tonitor sessions overwrites any projects another firmware left** there.

# Manual

## Playing it

| Control | What it does |
| --- | --- |
| **FX SEL ENV LFO EDIT GLO** (top row) | choose track **1–6** (lit: chosen; glowing: sounding). From any page, back to playing. **Held** (0.8 s): copy the track, or paste the copy onto it ([Copying a track](#copying-a-track)). |
| **PRESETS** knob | the chosen track's engine: **FM**, **ANIMA**, **CALIGULA**, **RUSSOLO**, **DRUID**, **BAYAREA**, **HIVE**, **OBRADOR**. Each track keeps one sound per engine. At start, and in an empty session, tracks 1–6 are on FM, ANIMA, CALIGULA, HIVE, DRUID and BAYAREA. |
| **SHIFT + PRESETS** | a new sound of the previous or next family. |
| **HOME** | a new sound for the chosen track. |
| **SHIFT + HOME** | a new sound of the same family. |
| **OCT− / OCT+** | the chosen track's octave, −3 to +3. |
| **SHIFT + OCT− / OCT+** | every loop of every track one step of the scale down or up ([Loopers](#loopers)). |
| **SHIFT + a track's button held + OCT− / OCT+** | that track's loops only. |
| the white keys | play the chosen track in the scale; the first white key is the root, and the root's keys glow dimly. A held note keeps sounding when you choose another track. The tracks share twelve voices. |
| the rightmost black key | **SHIFT**. Hold white keys and press SHIFT to latch them; SHIFT + a lit key releases it ([Latched keys](#latched-keys)). |
| black keys 1 and 2 | the chosen track's generative path: **AMBIENT**, **EXPERIMENTAL**; the lit one again: **OPEN** ([Generative paths](#generative-paths)). |
| **SHIFT +** black key 1 | a new scale and root, drawn at random; the SCALE page shows them for 1.5 s ([Scales](#scales)). |
| black key 3 | chord mode: **held** (0.6 s), on or off; **pressed**, the next chord form ([Chords](#chords)). |
| black keys 4, 5, 6 | the chosen track's loops **REVERSE**, **PENDULUM**, **RANDOM**; pressed again, forwards ([Direction and speed](#direction-and-speed)). Only while its loops play. |
| black keys 7, 8 | the loops at **HALF SPEED** or **DOUBLE SPEED**; pressed again, normal speed. Only while the loops play. |
| black key 9 (3rd from the right) | the **ENVELOPE** page ([Envelope](#envelope)). **SHIFT +**: BRICKLAYER on or off; **held** (0.6 s) while it is on: its page ([BRICKLAYER](#bricklayer)). |
| black key 10 (left of SHIFT) | **EDIT**: KNOB 1–4 become four macros ([Macros](#macros)). **SHIFT +**: BALLSY on or off; **held** (0.6 s) while it is on: its page ([BALLSY](#ballsy)). |
| **ARP** | the SCALE page: **SELECT** sets the root, **ALGORITHM** the scale ([Scales](#scales)). |
| **SHIFT + ARP** | the engine's PRESETS page ([Presets](#presets)). |
| **REC** | press to arm; the first note starts the loop; REC again ends it and it plays. REC while it plays: overdub. Up to 10 loops a track. |
| **SHIFT + REC** | **VARY** on or off: the loops play with random changes of level, length, timing, notes and pace, always in the scale. |
| **REC held** (1 s) | empty the chosen track's loops. |
| **PLAY** | stop or restart the chosen track's loops. **Held**: bring loops in or out on the white keys. On BALLSY's or BRICKLAYER's page: start or stop that sequencer. |
| **SHIFT + PLAY** | stop everything, or, if nothing plays, start every loop and sequencer. |
| **SELECT** knob | the chosen track's level, OFF to +4.5 dB. |
| **ALGORITHM** knob | the chosen track's pan. |
| **KNOB 1** | the chosen track's filter: off in the middle, low-pass to the left, high-pass to the right. |
| **KNOB 2** | the chosen track's granulator: off in the middle, dark to the left, scattered to the right ([Granulator](#granulator)). |
| **KNOB 3** | the chosen track's crush: off in the middle, fewer bits to the left, a lower sample rate to the right. |
| **KNOB 4** | the chosen track's reverb send. |
| **SHIFT + KNOB 3** | the chosen track's distortion ([Distortion](#distortion)). |
| **SHIFT + KNOB 4** | the chosen track's delay send. |
| **SAVE** | the SNAPSHOTS page; SAVE again leaves it ([Snapshots](#snapshots)). |
| **SAVE held** (0.8 s) | save the session. With none loaded, it offers the first empty slot; on the SESSIONS page, it saves into the slot shown and asks before overwriting; on the SNAPSHOTS page, it stores the current state in the framed snapshot. |
| **SHIFT + SAVE** | the SESSIONS page: **PRESETS** chooses a slot, **SAVE** loads it; on an empty slot, SAVE starts a new, blank session there. SHIFT + SAVE again leaves it ([Sessions](#sessions)). |
| **SHIFT + SAVE held** (0.8 s) | GLOBAL SETUP: the MIDI and AUDIO settings ([Global setup and MIDI](#global-setup-and-midi)). |
| **SEQ** | the DELAY, REVERB, MOD and PLAY ALONG pages in turn, then back ([Delay](#delay), [Reverb](#reverb), [Modulation](#modulation), [Play along](#play-along)). |
| **SHIFT + SEQ** | a new random modulation for the chosen track. |
| **MASTER** | the output level. |
| MIDI in (USB and TRS) | notes and CCs for each track ([Global setup and MIDI](#global-setup-and-midi)). |

### Generative paths

Each track has a path that steers the sounds **HOME** draws next on it; sounds already playing do not change.
Every track starts on OPEN, and sessions keep each track's path.

| Path | Key | Leans towards |
| --- | --- | --- |
| **AMBIENT** | black key 1 | ambient and dark ambient: soft attacks, long tails, darker, often melodic |
| **EXPERIMENTAL** | black key 2 | electroacoustic and acousmatic: noisier, brighter, more bent, crushed and driven |
| **OPEN** | neither lit | every family alike |

### Macros

Press **EDIT** (black key 10) and KNOB 1–4 become four macros that reshape the chosen track's sound. The middle
is the sound as HOME drew it; turn either way. The screen shows the macro's name and position. Press EDIT again
to get the knobs back. Each sound keeps its macros, and sessions and presets store them.

| | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **FM** | **BRIGHTNESS** | **DETUNE** (**INHARMONIC** on metallic and noisy families) | **SATURATION** | **MOTION** |
| **ANIMA** | **TONE** | **GRAINS**, **NOISE**, **FM**, **FORMANT** or **DRIVE**, by kind | **RESONATOR** | **MOTION** |
| **CALIGULA** | **WAVEFORM** | **MOD DEPTH** | **FEEDBACK** (**DELAY** on SPORE, VEIL, LITANY, MOTH) | **TOUCHES** (**GATE RATE** on HIVE, MOTH, WIRE) |
| **DRUID** | **STRUCTURE** | **BRIGHTNESS** | **DAMPING** | **POSITION** |
| **BAYAREA** | **SHAPE** | **FOLDS** | **FM** | **DECAY** |
| **HIVE** | **DETUNE** | **MOVEMENT** | **MIX** (**NOISE** on GROUND, AIR) | **CUTOFF** |
| **OBRADOR** | **CUTOFF** (four octaves either way) | **MORPH** (the filter: low-pass, notch, high-pass) | **RESONANCE** | **DETUNE** (none to three times as wide) |
| **RUSSOLO** | the kind's first parameter | the kind's second parameter | **CRUSH** | **RANDOMNESS** |

### Envelope

Black key 9 opens the **ENVELOPE** page with the chosen track's amplitude envelope (**AS DRAWN** until you change
it). KNOB 1–4 set **ATTACK** (1 ms – 10 s), **DECAY** (5 ms – 10 s), **SUSTAIN** (0–100 %) and **RELEASE**
(5 ms – 20 s). A change applies from the next note, and after that to the notes sounding too. HOME or loading a
preset brings back the sound's own envelope. Sessions store the envelopes; presets do not.

### PLAY, sequencers and looper

A track runs one sequencer at a time: with BALLSY on, BRICKLAYER shows NOT ALLOWED, and the other way round. A
sequencer waits for **PLAY** on its own page. On every other page PLAY is the looper's, so you can record loops
over a running sequencer. The looper records only the keys, never the sequencer.

### BALLSY

**SHIFT + EDIT** turns on BALLSY, a sequencer of bouncing balls, for the chosen track and shows its page (EDIT
blinks while it is on). **PLAY** starts and pauses the balls. **Hold EDIT** (0.6 s) to hide or show the page; the
balls keep playing.

Each time a ball hits a wall it plays a note in the scale, of random length and level, with chord mode and the
loops' transposition. The pitch rises clockwise, one octave per wall:

| Wall | Octave (at the start) | Pitch rises |
| --- | --- | --- |
| bottom | −1 | right to left |
| left | 0 | upwards |
| top | +1 | left to right |
| right | +2 | downwards |

| Knob | |
| --- | --- |
| **KNOB 1** | the number of balls, 1–8 |
| **KNOB 2** | speed |
| **KNOB 3** | the walls' octaves, moved together |
| **KNOB 4** | instability: the balls speed up, slow down and turn at random; at 0, steady and straight |

### BRICKLAYER

**SHIFT + black key 9** turns on BRICKLAYER, a step sequencer built a note at a time, for the chosen track and
shows its page (the key blinks while it is on). **PLAY** starts and stops it. **Hold black key 9** (0.6 s) to hide
or show the page; it keeps playing.

On its page, **REC** is the entry, not the looper:

| Gesture | |
| --- | --- |
| **REC** | entry on or off |
| a white key, entry on | add its note at the end |
| **SHIFT** + a white key, entry on | take that note out |
| **SHIFT + REC** | take the last note out |
| **REC** held 1 s | empty the sequence |

The notes are spread evenly over the length (3 notes in 8 steps fall on steps 1, 4 and 7) and follow the scale,
the root, the loops' transposition and chord mode.

| Knob | |
| --- | --- |
| **KNOB 1** | **LENGTH**: from the number of notes up to 32 steps |
| **KNOB 2** | **RATE**: 0.5–16 steps a second |
| **KNOB 3** | **HARMONY**: how often a note becomes a two- or three-note chord |
| **KNOB 4** | **VARIATION**: random level, length and pace, stutters, repeats, echoes and ghost notes; at 0, steady |

On its page, black keys 4–8 set BRICKLAYER's own direction and speed.

### Latched keys

Hold white keys and press **SHIFT**: they latch and stay lit. **SHIFT + a lit key** releases it. Latches belong
to the track they were made on; on another track the same keys play as usual, so a drone can sound on one track
while you play another. Loading a session releases every latch.

### Chords

With chord mode on (black key 3 held 0.6 s; **CHO** on the screen), each white key plays three notes of the scale.
Press black key 3 to step through the forms (in steps of the scale from the key's note):

| Form | Steps | |
| --- | --- | --- |
| **TRIAD** | 0, 2, 4 | root, third, fifth |
| **INV 1** | 2, 4, 7 | first inversion |
| **INV 2** | −3, 0, 2 | second inversion |
| **SUS2** | 0, 1, 4 | the second instead of the third |
| **SUS4** | 0, 3, 4 | the fourth instead of the third |
| **OPEN** | 0, 4, 9 | root, fifth, the third an octave up |
| **FOURTHS** | 0, 3, 6 | stacked fourths |

One chord sounds at a time on a track. Chords latch, loop and are saved like single notes.

### Direction and speed

Black keys 4–8 set how the chosen track's loops play. They work only while that track has loops and they are playing
(PLAY lit): then they glow dimly, and the setting in use is lit. With no loops, or the loops stopped, they are dark
and do nothing. They have no effect on the sound itself:

| Key | Setting |
| --- | --- |
| black key 4 | **REVERSE**: backwards |
| black key 5 | **PENDULUM**: forwards, then backwards, in turn |
| black key 6 | **RANDOM**: at each note, a jump to a note of the loop drawn at random |
| black key 7 | **HALF SPEED** (the pitch does not change) |
| black key 8 | **DOUBLE SPEED** |

The three direction keys exclude each other, as do the two speed keys; a direction and a speed combine. Press the
lit key again to go back to forwards or normal speed. Emptying the loops (REC held) resets both.

### Copying a track

Hold a track's button until the screen shows **COPIED / TRACK n** (the button blinks). Choose another track and
hold its button until **PASTED / TRACK n**. The track pasted onto gets everything of the copied one, its sounds,
settings and loops, except its own level and pan; its latched notes are released.

### Loopers

Each track has a looper of up to **ten loops**, and they run **free**: no tempo, no grid. Each overdub is as long
as you played it, so the loops drift against each other. A note held from the loop's first moment to its last
(still down when REC ends it) is tied over its end: while it still sounds (a drone, a pad) it goes on, one unbroken
note; once it has died away (a pluck, a bell) it is struck again at the start of each round. Other notes still
held when REC ends the loop are let go at its end, their release ringing on into the next round. Loops share the twelve voices with the keys and the other tracks.

**Bringing loops in and out.** Hold **PLAY**: the chosen track's loops appear on the white keys, the first loop on
the leftmost, lit while they play. Press a key to take its loop out, or to bring it back in from its start. With
the loops stopped, this chooses which loops the next PLAY starts.

**Moving the loops along the scale.** **SHIFT + OCT+ / OCT−** moves every loop of every track one step of the
scale (TRANSP +1), up to two octaves either way; the white keys and latched notes move with them, and an overdub
recorded meanwhile plays back as you played it. **SHIFT + a track's button held + OCT+ / OCT−** moves that
track's loops only (TRANSP T3 +2). Emptying a track's loops resets its own move.

### Screen

From the top: the track number in the track's colour, with the octave, VAR (while the loops VARY) and the battery
at the top right; the engine; the sound's family, or a short message; **ARMED** or **REC** while the looper
records; the track's level meter, with a mark at its SELECT level; and its pan. KNOB 1–4 show a full-screen dial
while they turn.

Track colours: 1 yellow, 2 cyan, 3 pink, 4 green, 5 orange, 6 lilac.

### Scales

One scale and root for all the tracks, saved with the session. At start, and in an empty session, both are drawn at
random (ARP shows which); **SHIFT +** black key 1 draws them again, and the loops follow the new scale. The root is the first white
key (C3 to B3, moved by each track's OCT), and the scale climbs across the 16 white keys.

| Scale | Steps |
| --- | --- |
| MAJOR | 0 2 4 5 7 9 11 |
| MINOR NATURAL | 0 2 3 5 7 8 10 |
| MINOR HARMONIC | 0 2 3 5 7 8 11 |
| MINOR MELODIC | 0 2 3 5 7 9 11 |
| DORIAN | 0 2 3 5 7 9 10 |
| MINOR PENTA | 0 3 5 7 10 |
| WHOLE TONE | 0 2 4 6 8 10 |
| ENIGMATIC | 0 1 4 6 8 10 11 |
| JAPANESE IN | 0 1 5 7 8 |
| PHRYGIAN | 0 1 3 5 7 8 10 |
| LYDIAN | 0 2 4 6 7 9 11 |
| MIXOLYDIAN | 0 2 4 5 7 9 10 |
| LOCRIAN | 0 1 3 5 6 8 10 |
| MAJOR PENTA | 0 2 4 7 9 |
| BLUES | 0 3 5 6 7 10 |
| HUNGARIAN MINOR | 0 2 3 6 7 8 11 |
| DOUBLE HARMONIC | 0 1 4 5 7 8 11 |
| PHRYGIAN DOMINANT | 0 1 4 5 7 8 10 |
| JAPANESE HIRAJOSHI | 0 2 3 7 8 |
| JAPANESE IWATO | 0 1 5 6 10 |
| PELOG | 0 1 3 7 8 |
| OCTATONIC | 0 1 3 4 6 7 9 10 |
| PROMETHEUS | 0 2 4 6 9 10 |

MIDI notes are not quantised to the scale.

### Global setup and MIDI

**SHIFT + SAVE held** (0.8 s) opens **GLOBAL SETUP**, the FM-1's own settings (MIDI and AUDIO), kept apart from the sessions.
**PRESETS** moves through the entries; **SAVE** opens one, or selects a value and sets it; **SHIFT + SAVE** cancels
or goes back a level. A track button leaves; changes are kept.

**AUDIO** has one setting, **USB**: what the FM-1 sends to a computer as USB audio ([USB audio](#usb-audio)).
**STEREO** (at the start): the master, as the headphones hear it. **TRACKS**: the six tracks, each on its own mono
channel. Changed, the FM-1 leaves the USB for a moment and comes back as the new device; the sound on the
headphones goes on.

MIDI's parts:

| Part | Settings | At the start |
| --- | --- | --- |
| **IN CHAN** | TRACK 1 to 6: the channel each track listens to (OFF, 1–16); AUTO: the channel that plays the chosen track | 1 to 6; AUTO 16 |
| **OUT CHAN** | TRACK 1 to 6: the channel each track sends on (OFF, 1–16) | 1 to 6 |
| **RECEIVE** | NOTES, CC (ON / OFF) | ON, ON |
| **SEND** | NOTES, CC (ON / OFF) | OFF, OFF |
| **PORTS** | USB IN, TRS IN (the 3.5 mm MIDI IN jack, Type A) | ON, ON |

A note plays every track whose IN channel it arrives on (and the chosen track on AUTO). Each track answers to,
and with SEND CC on sends, these CCs on its channel:

| CC | | CC | |
| --- | --- | --- | --- |
| 7 | level | 74 | filter (64 off; below, low-pass; above, high-pass) |
| 10 | pan | 91 | reverb send |
| 16–19 | the four macros | 92 | delay send |

With SEND NOTES on, each track sends the notes it plays (keys, loops, sequencers, MIDI) on its OUT channel over
USB. There is no MIDI clock, transport or program change.

### Modulation

Each track has two LFOs and four modulation slots. Press **SEQ** three times for the **MOD page**: **PRESETS**
chooses the entry, **KNOB 1–4** set its rows.

| Entry | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **LFO 1**, **LFO 2** | **SHAPE**: SINE, TRI, SAW UP, SAW DN, SQUARE, STEPS, DRIFT | **RATE**: 0.01–20 Hz | **MODE**: FREE, NOTE (restarts at each note), ONCE (one cycle from each note) | **FADE**: in over up to 10 s |
| **MOD A** – **MOD D** | **FROM**: OFF, LFO 1, LFO 2, VELOCITY, KEY, RANDOM | **TO**: the four macros, FILTER, GRAINS, CRUSH, DRIVE, REVERB, DELAY, LEVEL, PAN, LFO 1 RATE, LFO 2 RATE | **AMOUNT**: −100 to +100 | **NOW**: what the slot does at the moment |

**SHIFT + SEQ** draws a random modulation for the chosen track; press again for another. HOME also draws one with
some sounds, mostly slow FM, HIVE, BAYAREA and DRUID sounds. Sessions keep each track's modulation.

### Play along

Other tracks play along with the one you play. The chosen track is the **guide**. The tracks you take in listen to
everything the guide sounds (its keys, latches, chords, MIDI in, loops, BALLSY and BRICKLAYER) and play their own
sounds in answer, in the scale. Three modes: **REACT**, **SHADOW** and **ARPEGGIO**. After the SMK-37 Pro port of
Tonitor.

To turn it on:

1. Press the **track button** of the track you will play: it becomes the guide.
2. Press **SEQ** four times: the **PLAY ALONG page**.
3. Turn **PRESETS** to choose the mode: REACT, SHADOW or ARPEGGIO.
4. Press the **track buttons** of the tracks that should play along (2 IN, 3 IN ...). At first no track is in, and
   with none in, nothing plays along.
5. Press **PLAY**: the mode is on, its name in its colour (grey while off).
6. Press **SEQ** to go back to playing, and play the guide.

To turn it off, open the page again and press **PLAY** (REACT OFF ...). While it is on, **PLAY** flashes with
each note the guide plays, and SEQ blinks slowly off the page.

On the page:

- **PRESETS**: the mode. Changing it lets go of the notes the tracks are playing.
- **PLAY**: the mode on or off.
- **The track buttons**: tracks in or out (2 IN, 2 OUT). The guide's button stays lit; a track taken in glows
  dimly; a track out is dark. The guide's own button says GUIDE TRACK (a track cannot follow itself).
- **KNOB 1–4**: the mode's values, 0–100; ARPEGGIO has two more, on **SELECT** and **ALGORITHM**. In REACT and
  SHADOW, SELECT and ALGORITHM stay the guide's level and pan. The page shows the knob turned last, its value large,
  and a bar for each.
- **SEQ**: back to playing; the mode goes on running. Choosing another track there makes it the guide.

The tracks taken in play their notes as a sequencer's: the looper does not record them, and they never play on the
guide. When the guide stops, they stop. Sessions and snapshots keep PLAY ALONG: on or off, the mode, the tracks
taken in and the values.

**REACT** improvises from a memory of everything the guide plays: its last 128 events (notes or chords), with their
levels, lengths and the gaps before them. Each track taken in goes on as the guide once went on, or jumps to
another place where the guide played the same intervals, so its lines are the guide's own, recombined, in the
guide's rhythm. Its notes fit the harmony the guide holds. At each of the guide's phrases the tracks regroup, each
taking a way to play: **FOLLOWS** (with the guide), **CONTRASTS** (slow under a busy guide, moving under long
chords), **ECHOES** (the guide's line a few events behind, a third, sixth or octave away), **ANSWERS** (in the
guide's rests), or **RESTS**. After François Pachet's Continuator, IRCAM's OMax and Somax, and George Lewis's
Voyager.

| Knob | | |
| --- | --- | --- |
| **KNOB 1** | **DENSITY** | how much they play, how long they wait, how many sit out |
| **KNOB 2** | **COMPLEXITY** | low: single notes in plain rhythms; high: harmonised notes, steps split, notes off the beat |
| **KNOB 3** | **CONTINUITY** | low: they jump about the guide's memory; high: they keep to its phrases as played |
| **KNOB 4** | **SPREAD** | how far from the guide's register: low, around its hands; high, from deep bass to high air |

**SHADOW**: each track taken in shadows what the guide sounds, from its own distance behind, at its own pace; now
and then backwards or to and fro, resting and coming in again, moved along the scale.

| Knob | | |
| --- | --- | --- |
| **KNOB 1** | **DELAY** | how far behind: from right behind to 15 s |
| **KNOB 2** | **DRIFT** | how far each one's pace strays from the guide's: a little loose, to half or twice |
| **KNOB 3** | **VARIATION** | how often each changes its way: backwards, to and fro, resting, notes late or left out |
| **KNOB 4** | **TRANSPOSE** | how far it is moved: as played, or a third, fourth, fifth or sixth away |

**ARPEGGIO**: the tracks taken in break the guide's chord (what it holds; a single note over its octaves) into
arpeggios, each at its own pace, its own way and in its own octaves, and follow the chord as it changes, at their
next step.

| Knob | | |
| --- | --- | --- |
| **KNOB 1** | **RATE** | a step every 2 s (0) to every 62 ms (100) |
| **KNOB 2** | **RELATION** | how the tracks' paces relate: all one, or twice, half, three in two ... |
| **KNOB 3** | **MOTION** | which way: all up, or each its own (down, up and down, as played, at random ...) |
| **KNOB 4** | **MUTATE** | how much a pattern changes as it repeats |
| **SELECT** | **RANGE** | how many octaves each spans, 1 to 4 |
| **ALGORITHM** | **CHANCE** | steps left out and struck twice |

### Presets

Each engine has **64 presets**. **SHIFT + ARP** opens the PRESETS page of the chosen track's engine; **PRESETS**
chooses a slot. Each preset is named by its sound's family and numbered within it, in the order of the slots:
BRASS 1, BRASS 2, STRINGS 1.

- **ARP held** (or SAVE held) saves the track's sound there, asking before overwriting.
- **ARP** (or SAVE) loads it and goes back to playing.

Saving silences the audio for a moment.

### Snapshots

A snapshot is the state of the machine at one moment. Each session holds 16 of them.

**SAVE** opens the SNAPSHOTS page: 16 squares, one for each white key. A filled square holds a
snapshot; the one last stored or recalled is white.

| To | Do this |
| --- | --- |
| Recall a snapshot | press and release its white key |
| Store the current state | hold its white key 0.8 s, or choose it with **PRESETS** and hold **SAVE** |
| Empty a snapshot | hold **SHIFT** and hold its white key 0.8 s |
| Set the morph time (0 to 30 s) | **KNOB 1** |
| Leave the page | **SAVE**, or a track button |

On this page the white keys do not play notes. Stored snapshots glow on the keys; the last one recalled is lit.

**What a snapshot holds.** The whole state of the machine. For each track: the engine it plays; the sound on every
one of its eight engines, each with its macros and envelope; level, pan, reverb and delay sends, filter, granulator,
crush, distortion, octave, the track's transposition, its path, chord mode and modulation; its latched notes and
chords; BALLSY's and BRICKLAYER's settings, BRICKLAYER's notes, and whether each plays; and its loops: which loops
there are, where each stood against the others, which are taken out, whether they play, and their direction, speed
and VARY. For the machine: the reverb, the delay, the scale, the root, the loops' transposition and PLAY ALONG (on or
off, its mode, the tracks taken in, the values on its pages). Not stored: the track chosen, EDIT and which page is up.

For example: record a loop and store snapshot 1; record an overdub and store snapshot 2. Snapshot 1 brings back the
loop alone and snapshot 2 brings back both. Loops that are in both snapshots keep playing when you switch; a loop
that comes back is placed where it stood against the others when it was stored.

A loop that a snapshot holds stays in the FM-1's memory even after you empty the looper (hold REC). It shares the
looper's room with what you record: when the looper has no room left, REC says LOOPER FULL. Snapshots that share
sounds, latched notes or loops store them once. If there is no room for a new snapshot, the screen says FULL; empty
snapshots you no longer need (SHIFT + hold the key).

**Morph.** With a morph time of 0 (OFF), a recalled snapshot is applied at once. With a longer time, the machine
moves to it over that time; the snapshot's square and key blink until it is reached:

- Levels, pans, sends, filter, granulator, crush, distortion, the macros and envelope of a sound that stays, the
  modulation amounts, LFO rates and fades, the reverb's DECAY, DAMP and MIX, the delay's REPEATS, DAMP and MIX, and
  the sequencers' speed, rate, harmony and instability change gradually.
- Everything that affects pitch changes at once, halfway through: scale, root, octaves and transpositions. So do
  the delay's TIME and the reverb's SIZE, the loops, the latched notes, chord mode, the sequencers and their notes,
  the modulation routings and LFO shapes, and PLAY ALONG.
- A track whose sound changes fades out from a quarter of the way, switches sound halfway and fades back in by three
  quarters.

Recalling another snapshot during a morph starts a new morph from the current state. The morph runs in the main
loop and adds no load to the audio.

Snapshots are saved in the flash with the session (hold **SAVE** to save). A new session has none.

### Sessions

There are 64 session slots; the FM-1 starts with none loaded. **SHIFT + SAVE** opens the SESSIONS page. A session
stores the whole machine: every track's sounds, settings, latched notes and loops, the scale, the delay, the reverb,
PLAY ALONG and the 16 snapshots with the loops they hold. Sessions saved under an older Tonitor load with the same sounds after an update. Saving
silences the audio for a moment.

Sessions are kept in the FM-1's flash: **saving overwrites any projects, presets or samples another firmware left
there**.

**Backing up.** The [backup page](https://nzimas.github.io/tonitor/backup.html) (desktop Chrome, Edge or Opera,
over USB MIDI; Tonitor build 47 or later) copies every session and preset to a file on the computer and writes
them back. Use it before trying another firmware and after reinstalling Tonitor.

### Granulator

Each track's granulator plays grains from the last half second of the track's sound (KNOB 2). Off in the middle;
to the left, long, overlapping, drifting grains, sometimes frozen; to the right, short grains, scattered,
reversed and stuttered. Each track's granulator has a character of its own, drawn anew at power-on and when a
session is loaded. Grains ring on for about two seconds after the notes end.

### Distortion

**SHIFT + KNOB 3**, 0–100 (0 is off): a two-stage drive after the track's filter, granulator and crush, before its
sends, so the delay and the reverb receive the distorted sound. Its output is matched to the dry level, so the
amount changes the tone, not the loudness.

### Delay

A master delay, before the reverb, fed by each track's delay send (**SHIFT + KNOB 4**); half its echoes also go
into the reverb. The repeats are slightly dark and grainy. **SEQ** once opens its page:

| Parameter | |
| --- | --- |
| **TIME** | 0.02 to 2.0 seconds (shown without the unit); turning it bends the repeats' pitch |
| **REPEATS** | the feedback: from one repeat to nearly endless |
| **DAMP** | each repeat darker than the one before |
| **MIX** | the level of the echoes |

### Reverb

A plate reverb (after Jon Dattorro), fed by each track's reverb send (**KNOB 4**). **SEQ** twice opens its page:

| | | At the start |
| --- | --- | --- |
| **SIZE** | the room | 70 |
| **DECAY** | 0.4 s to about 25 s | 65 (about 3.5 s) |
| **DAMP** | how fast the highs die | 40 |
| **MIX** | the reverb's level | 50 |

### USB audio

Connected to a computer, phone or tablet, the FM-1 is also a class-compliant **USB audio input** ("FM-1
Tonitor"), 16-bit, at 48 or 44.1 kHz (44.1 kHz is the FM-1's own rate: nothing is converted). **GLOBAL SETUP >
AUDIO > USB** chooses what it sends:

| USB | Channels |
| --- | --- |
| **STEREO** | 2: the master output, with the delay and reverb (what the headphones hear) |
| **TRACKS** | 6: tracks 1–6, each mono, after its filter, granulator, crush, distortion and level; before its pan and the master delay and reverb |

Both are taken before **MASTER**, which sets only the headphones' level. No driver is needed, and MIDI works over
the same cable. To hear the FM-1 while a computer records it, use its headphone output: a program's own monitoring
(Audacity's software playthrough) adds a delay and may break up.

## Engines

### FM — six operators, DX7 style

Six-operator FM after the DX7 (msfa, the core of Dexed). HOME draws a patch from one of these families and
levels it to one loudness:

| Family | |
| --- | --- |
| **METAL** | bells and gongs, inharmonic |
| **DRONE** | spectral drones, slowly beating |
| **BRUIT** | noises |
| **IMPACT** | struck bodies: thuds, knocks, metal hits, drum skins, debris |
| **ITERATIF** | a sound pulsed by its own LFO |
| **SPECTRE** | a spectral arpeggio: harmonics lifted one after another by beating |
| **ABYSS** | sub-bass rumble, breathing slowly |
| **FRICTION** | creaking |
| **CHOIR** | vowels |
| **BOWED** | a bowed string |
| **HOLLOW** | odd harmonics only: a dark pipe |
| **BLOOM** | a tone opening over seconds |
| **TIDE** | three stacks crossfading under a slow tide |
| **SHARDS** | a tone broken by random pitch steps and chopped |
| **STATIC** | ring-modulated bands |
| **WIND** | breath and gusts over a soft drone |
| **PLUCK** | plucked strings |
| **MALLET** | struck bars: marimba, vibraphone, kalimba |
| **WARP** | anything: odd ratios, long glides, bouncing pitch |
| **GLITCH** | broken, mechanical sounds |
| **SHAPER** | a tone rolling between odd and even harmonics a few times a second |
| **LADDER** | partials entering or dying one by one |
| **REVERSE** | a sound swelling while the key is held: tape run backwards |
| **CREEP** | a held note slowly leaving its pitch |
| **CLANG** | DX7-style bells |

GLISSE and RUBBER, families of older builds, are now part of WARP; sounds saved with them load as they were.

### ANIMA — a lo-fi synthesizer played by a simulated hand

A port of the *anima* opcode from Vascular, at half the sample rate. While a key is held, a simulated finger drags
figures across a virtual surface; where it is, how hard it presses and how fast it moves change the timbre, never
the pitch. HOME draws one of twenty-two kinds: **HAZE**, **VOYAGE**, **GLASS**, **BELLS**, **GOLDEN**,
**BREATH**, **GRIT**, **SWARM**, **TIDE**, **CHOIR**, **SHARDS**, **BOWL**, **DUST**, **MACHINE**, **PLUCK**,
**REED**, **FOG**, **ORGAN**, **BITS**, **DRIFT**, **MURMUR** and **HOWL**.

### CALIGULA — an organism

After SOMA's LYRA-8, as Mike Moreno's LIRA-8 emulates it (BSD licence): eight cross-modulating oscillators in four
pairs, a total feedback, gating LFOs and a short modulated delay, played by a simulated hand that touches the
plates while a key is held. It is silent until a key is down and is tuned around the note played; a second key
moves it to the new note, and letting go of the last key lets it ring out. It is one organism per track, outside
the twelve voices. HOME draws one of ten species:

| Species | |
| --- | --- |
| **SPORE** | slow swells, held drones, breathing; low |
| **THORN** | sharp, deep FM, driven, bursting often |
| **HIVE** | quick gates: irregular rhythms, bursts |
| **CARRION** | the depths, heavy feedback, a warped delay, surges |
| **VEIL** | mostly triangles, light modulation: the calmest |
| **RUST** | driven, cross-modulated, surging, sometimes rhythmic |
| **LITANY** | a slow choir of unisons and octaves |
| **UNDERTOW** | deep, heavy feedback, slow surges |
| **WIRE** | high and sharp, clusters, a quick hand tapping |
| **MOTH** | soft triangles fluttering through the gates, on a pulse |

### RUSSOLO — noise, logic and broken buffers

The techniques of Robert Heel's Noisferatu (GPL-3.0), at a low sample rate and bit depth. Like CALIGULA it is
silent until a key is down, one organism per track, and a key sets its pitch. HOME draws one of seventeen kinds;
the first two macros are the kind's own parameters, the others **CRUSH** and **RANDOMNESS**.

| Kind | |
| --- | --- |
| **HOLD** | sample-and-hold noise |
| **DUST** | random clicks through a low-pass |
| **GATES** | noise opened and closed by two triangles |
| **CRACKLE** | sparse clicks ringing over hiss |
| **NOR**, **XOR** | two oscillators combined bit by bit |
| **RING** | two oscillators multiplied |
| **CASCADE** | squares modulating squares |
| **CLANG** | two triangles at an inharmonic ratio, XOR'd |
| **CHUNKS** | a buffer of noise, saw and silence, read at the key's speed |
| **JUMP** | the buffer read with random jumps |
| **WANDER** | a window wandering through the buffer |
| **BEND** | the buffer's addresses bent on a clock |
| **QUAD** | four such bendings, switched by chance |
| **BLIPS** | decaying triangles on the key's harmonics |
| **WALK** | a random walk over a scale from the key |
| **CHORD** | triangles on a chord, slowly beating |

### DRUID — strings and resonant bodies

A port of the resonators of Mutable Instruments Rings by Émilie Gillet (MIT licence): strings, sympathetic
strings and modal bodies, plucked, struck, bowed or rattled. Letting go of a key damps the sound by an amount set
per sound.

| Kind | Resonator | Exciter |
| --- | --- | --- |
| **PLUCK** | string | pluck |
| **BOWED** | string | bow |
| **STRUCK** | string, stretched partials | strike |
| **SITAR** | string, buzzing bridge | pluck |
| **RATTLE** | string | dust |
| **HARP** | sympathetic strings | pluck |
| **DRONE** | sympathetic strings | bow |
| **BELL** | modal | strike |
| **BOWL** | modal | bow |
| **PLATE** | modal | dust |

### BAYAREA — a West Coast voice

A port of the bayarea opcode of Vascular: an oscillator from sine to saw, frequency-modulated by two sines, a
wavefolder, then a low-pass gate (struck) or a sustaining envelope over resonant low-passes, a drive and a bank of
resonators, all moved by an LFO.

| Kind | Envelope | |
| --- | --- | --- |
| **BONGO** | gate | low, short decay |
| **PLUCK** | gate | brighter, short to medium decay |
| **CHIME** | gate | long decay, resonators |
| **FOLDED** | gate | heavy folding |
| **SWELL** | sustain | slow attack, long release |
| **DRONE** | sustain | slower attack, long release |

### HIVE — a swarm of oscillators

A port of DigiRust's SWARMER, extended: seven oscillators per voice, each at its own place around the note (an
octave, a fifth, a cluster step or an inharmonic partial), each moved by slow LFOs, through a filter with its own
envelope.

| Kind | |
| --- | --- |
| **SUB** | the main oscillator one or two octaves down |
| **LEAD** | bright, short attack, resonant |
| **STAB** | short, a strong filter sweep |
| **PAD** | slow attack, wide detune, long release |
| **DRONE** | slower attack, full sustain, long release |
| **WASH** | the slowest attack and release, noise, a moving filter |
| **CLUSTER** | quarter-tone and semitone steps, slowly beating |
| **METAL** | inharmonic partials, struck |
| **SPECTRA** | inharmonic partials turning slowly |
| **GROUND** | two octaves down, a dense cluster with noise: rumble |
| **AIR** | filtered noise and high whistles: wind |
| **TIDES** | oscillators gliding apart and back |

CLUSTER, METAL, SPECTRA, GROUND, AIR and TIDES are for textures rather than melodies.

### OBRADOR — an analog polysynth

An analog polysynth voice after the Oberheim SEM and the two-voice Oberheims, written for Tonitor with OberVave
(an Oberheim-style firmware for the FM-1, by zvenson) as its frame of reference. Per voice: two oscillators (saw,
pulse or both, band-limited), the second detuned or an interval away, hard-synced or cross-modulating now and
then; in the lush kinds a second voice card, detuned against the first; a drive into the SEM's 12 dB filter, its
output morphing from low-pass through notch to high-pass, its resonance soft-limited; a second low-pass stage for
24 dB, or the band-pass; filter and amplifier envelopes on analog curves; an LFO that comes in after a delay; and
each card's own small offsets of tune and cutoff with a slow drift.

| Kind | |
| --- | --- |
| **STRINGS** | two cards of saws, a slow attack, a delayed vibrato: the string pad |
| **BRASS** | the envelope opening the 24 dB low-pass |
| **PWM PAD** | two pulses, their widths moving slowly |
| **BASS** | the second oscillator an octave down, the 24 dB low-pass, a plucked envelope |
| **SYNC** | the second oscillator synced, its pitch swept by the envelope |
| **NOTCH** | the filter near its notch, the LFO moving it: a phasing sweep |
| **METAL** | cross-modulation at an odd interval through the band-pass: bells, clangs |
| **DRONE** | two cards a fifth or an octave apart, the cutoff and the morph moving slowly |
| **KEYS** | a struck filter, no sustain |
| **SWELL** | the filter opening slowly under a held chord |

# Building from source

Once:

```bash
./setup.sh
```

It makes a Python environment in `.venv` (Pillow, mido, python-rtmidi), downloads JieLi's Linux toolchain
to `~/.jieli/toolchain`, and fetches the three files of the JieLi AC79 SDK the package needs into
`~/fw-AC79_AIoT_SDK`. On macOS the toolchain runs in Docker, so **Docker Desktop must be running** to
build. `./setup.sh --font` also fetches Atkinson Hyperlegible (the Braille Institute's typeface for low
vision), for smoother large type than the default Terminus.

Then, with the FM-1 connected by USB and switched on:

```bash
./install.sh
```

It builds `build/tonitor.fwsc` and installs it over USB-MIDI. `./install.sh --info` tells what the
connected FM-1 runs; `./build.sh` only builds.

## Testing without the FM-1

```bash
tests/run_tests.sh
```

needs only `clang`, Python and (for the browser installer) Node. It checks the firmware for anything the
chip cannot run (it has no FPU and no compiler runtime), then runs the real DSP, sound generators, mixer,
loopers, sessions and screen code on the computer, and the browser installer against a simulated FM-1:

- `build/sounds/` — WAVs of every engine, family and path, and `mix.wav`, the six tracks entering one after
  another;
- `build/screens/` — the screen in its states, as PNGs.

## How it is built

`firmware/src/tonitor.c` is the single compilation unit: `fm.c` (FM, around msfa's `fm6_core.c`),
`anima.c` (ANIMA), `cal.c` (CALIGULA), `russolo.c` (RUSSOLO), `druid.c` (DRUID), `bayarea.c` (BAYAREA), `hive.c` (HIVE), `gran.c` (the granulator), `reverb.c`, `layers.c` (the six tracks,
the loopers, the mix; each block the voices, the organisms and the master effects are dealt to the FM-1's two cores, `audio_worker.h` and `hal/fm1_multicore.h`), `ui.c` (panel and screen), `sessions.c`, `gfx.c`, `dsp.h` (fixed-point helpers),
`main.c` (boot, interrupts). Based on Felucca and earlier open FM-1 firmware work: `firmware/hal/`, `firmware/loader/`,
`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `libc.c`, the build checks and the installer. The website and the
browser installer are in `docs/` (served by GitHub Pages); `tools/make_site.py` builds its pages from `site/` (home, install), this README (the manual) and `CHANGELOG.md` and publishes `build/tonitor.fwsc` for the installer.

# Licence

- **The firmware** is free software under the **GNU General Public License, version 3 only**
  (`GPL-3.0-only`, [`LICENSE`](LICENSE)): it is a work based on GPL-3.0 code (see [`LICENSING.md`](LICENSING.md)). The
  browser installer (`docs/js/fm1ota.js`), a port of Felucca's installer, is GPL-3.0 too.
- **The website, the logo and the documentation** written for Tonitor (`docs/` except `docs/js/fm1ota.js`
  and the firmware package, and this README's text) are under the **MIT licence**
  ([`LICENSES/MIT.txt`](LICENSES/MIT.txt)).
- Third-party parts keep their own licences: msfa (Apache-2.0), the LIRA-8 architecture (BSD 3-clause), Noisferatu's techniques (GPL-3.0), Rings (MIT), the
  fonts (SIL OFL 1.1), the JieLi SDK files placed in the package (Apache-2.0).

[`LICENSING.md`](LICENSING.md) lists every file and its licence.

# Credits

Tonitor is by Nuno Zimas. It stands on the work of others:

- [**Felucca**](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments — the hardware
  track, USB stack, update loader, rescue mode and installer that made an FM-1 firmware possible at all.
- **msfa** by Google Inc. and Pascal Gauthier (Apache-2.0), from Dexed, in Felucca's integer port — the FM.
- **ANIMA** is ported from the anima opcode of Vascular.
- **CALIGULA** follows the architecture of [**LIRA-8**](https://github.com/MikeMorenoDSP/LIRA-8) by Miguel
  Moreno (BSD), an emulation of SOMA Laboratory's LYRA-8.
- **RUSSOLO** re-makes, in fixed point, the techniques of
  [**Noisferatu**](https://github.com/rob-scape/noisferatu) by Robert Heel (GPL-3.0).
- **BAYAREA** is ported from the bayarea opcode of Vascular.
- **HIVE** is ported from the SWARMER machine of DigiRust.
- **DRUID** is a fixed-point port of the resonators of [**Mutable Instruments Rings**](https://github.com/pichenettes/eurorack)
  by Émilie Gillet (MIT).
- **Atkinson Hyperlegible** by the Braille Institute (SIL OFL 1.1) and **Terminus** (SIL OFL 1.1).

M-VAVE and FM-1 are trademarks of their owner, used here only to say which device Tonitor runs on.

# Disclaimer

Tonitor is an **unofficial** firmware. It is not made, endorsed, approved or supported by M-VAVE or by the
maker of the FM-1, and it is not affiliated with them.

**Installing it replaces the device's original firmware and may void any warranty that applies to your
FM-1**, if any applies at all; check the terms of your warranty before installing. Installing any firmware
carries a risk: an interrupted or failed update can leave the device unable to start until it is recovered
(see [Installing](#installing)), and saving sessions overwrites data that other firmwares may keep in the
same flash area.

The software is provided **"as is", without warranty of any kind**, express or implied, including but not
limited to the warranties of merchantability, fitness for a particular purpose and non-infringement. In no
event shall the authors or contributors be liable for any claim, damages or other liability — including
damage to your device, loss of data, or loss of the use of the device — arising from, out of or in
connection with the software, its installation or its use. You install and use it entirely at your own
risk. See also sections 15 and 16 of the [GPL](LICENSE).
