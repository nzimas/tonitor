<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="docs/assets/logo-dark.svg">
    <img src="docs/assets/logo-light.svg" alt="Tonitor" width="520">
  </picture>
</p>

<p align="center"><b>An alternative firmware for the M-VAVE FM-1, for electroacoustic experimentation, live ambient music and sound exploration.</b></p>

<p align="center">
  <a href="https://nzimas.github.io/tonitor/install.html"><b>Install in your browser</b></a> ·
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
are six layers of *sound objects*: each layer has its own engine, its own sound, two voices and its own free
loop recorder, and every sound is drawn by algorithms that aim for surprise rather than for a preset that
sounds nice. You play textures, drones, gestures and their slow transformations, layer them, loop them
freely out of time with each other, and shape them while they play.

The screen is made for **low vision**: very few words, in very large type, with high contrast, and each
layer in its own colour.

### Highlights

- **Three sound engines**, chosen per layer:
  - **FM** — six-operator, DX7-style FM, generating sound objects: bells and gongs, spectral drones,
    noise, impacts, iterations, glissandi, beating spectra, sub-bass, creaks.
  - **ANIMA** — a synthesizer played by a simulated hand (oscillators, FM, grains, noise, filters,
    folding, a phaser and a resonator), all moved by the hand's gestures.
  - **CALIGULA** — a restless eight-oscillator organism after the architecture of the LYRA-8, with
    cross-modulation, a hyper-LFO, bursts and surges, and a simulated hand on its plates.
- **Generative paths** — lean each layer's new sounds towards **AMBIENT** (ethereal, melodic, dark) or
  **EXPERIMENTAL** (edgier, gnarlier, more textured), or leave it **OPEN**.
- **Six free loopers**, one per layer, with up to ten overdubs each, every loop with its own length.
- **Latching**: hold notes, press SHIFT, and they sustain as drones while you play on top.
- Per layer: **DJ resonant filter**, **granulator** (dark ↔ scatter), **bit/rate crush**, level, pan and
  a send to a lush **master reverb** (Dattorro plate).
- **Scales** on the white keys (major, minors, dorian, pentatonic, whole tone, enigmatic, Japanese *in*),
  any root.
- **64 session slots** in the FM-1's flash: every layer, sound, loop and setting.

## Installing

The easiest way is the **[browser installer](https://nzimas.github.io/tonitor/install.html)**: connect
the FM-1 with a USB data cable, open the page in a desktop **Chrome, Edge or Opera** (they support Web
MIDI with SysEx), and follow it. It finds the FM-1, checks what it runs, writes Tonitor and checks the
result. Nothing is uploaded anywhere: the page talks to the FM-1 directly.

It installs over whatever the FM-1 runs now — M-VAVE's own firmware, SLOOP, Felucca or an earlier Tonitor.
Do not unplug the FM-1 while it writes.

To build and install from source instead, see [Building from source](#building-from-source).

**If something goes wrong:** hold **OCT−** while switching on for the USB rescue mode (the screen says
RESCUE), then install again. Two failed starts in a row also enter it. Holding **OCT− and OCT+** for five
seconds enters the chip's own updater (UBOOT). If the FM-1 no longer starts at all, recovery needs
[FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).

**Going back:** the installer and `./install.sh` install any FM-1 package, for example a SLOOP release; to
return to M-VAVE's firmware, use M-VAVE's own updater. Apart from the update itself, Tonitor writes to the
FM-1's flash only when you save a session — into the area SLOOP keeps its projects in, so **saving Tonitor
sessions overwrites SLOOP projects** stored there.

# Manual

## Playing it

| Control | What it does |
| --- | --- |
| **FX SEL ENV LFO EDIT GLO** (top row) | choose layer **1 2 3 4 5 6**. The chosen layer's button is lit; the others glow while they sound. |
| **PRESETS** knob | the chosen layer's engine: **FM**, **ANIMA** or **CALIGULA**, left to right. Each layer keeps a sound of each engine, so turning back finds the sound it had. |
| **HOME** | a **new sound** for the chosen layer, on its engine (never the same family twice in a row). HOME lights for a moment. |
| **OCT− / OCT+** | the chosen layer's octave, −3 to +3. OCT− is lit below the middle, OCT+ above. |
| the white keys | play the chosen layer **in the scale chosen** (ARP): the first white key is the root, each white key the next note of the scale, up across all 16. Two voices per layer: **a new note never cuts the release of the one before it** — a third note takes the voice that has rung longest. A key lifted ends its note on the layer it was played on, so **a held note keeps sounding while you choose and play another layer**. The keys you hold on the chosen layer light up. |
| the rightmost black key | **SHIFT** (lit while held; each press shows the audio's load in % of the time it has, now / at most since the last press, e.g. `LOAD 62/91`). Hold white keys and press SHIFT: they **latch** — they keep sounding after you let go, and their keys stay lit. Hold SHIFT and press a lit key to let go of it. |
| the two leftmost black keys | the chosen layer's **generative path**: the first **AMBIENT**, the second **EXPERIMENTAL** (lit while chosen; the screen says which). Pressing the lit one again goes back to **OPEN**. See below. |
| the other black keys | play no notes (kept for other functions to come). |
| **ARP** | the **SCALE page** (ARP lit): **SELECT** chooses the root (C to B), **ALGORITHM** the scale. The root is shown very large, the scale under it; what you just turned flashes black on white. ARP again, or a layer button, goes back. |
| **REC** | the chosen layer's looper. Press to **arm** a new loop (REC flashes, the band shows ARMED and its number); the **first note** you play starts it (REC lights; REC and the number). Press REC again to **end** it: the loop is exactly as long as from that first note to this press, and it starts playing at once, round and round. **Press REC again while it plays to overdub**: the same again, a new loop over the others — up to **10 loops a layer**. |
| **REC held** (1 s) | **empty** the chosen layer's looper, all its loops: the whole screen flashes LOOP n / EMPTY. |
| **PLAY** | stop the chosen layer's loops, or play them again, each from its start. PLAY is lit while they play. **SHIFT and PLAY**, from any layer: if any loop plays, every loop on every layer stops (LOOPS OFF; the loops are kept); if none plays, every loop there is starts, all together from their starts (LOOPS ON). PLAY alone still starts or stops the chosen layer's own. |
| **SELECT** knob | the chosen layer's **level**: 1.5 dB a click, from OFF up to +4.5 dB. The message band says it as the knob's share of its turn, OFF and 4 % to 100 % (VOL 74 %); a layer starts at 87 % (0 dB). |
| **KNOB 1** | the chosen layer's **DJ filter**: from the middle (off), left is a resonant **low-pass** coming down from 20 kHz to ~35 Hz, right a resonant **high-pass** going up from 16 Hz to ~12 kHz; the further from the middle, the more resonant. While you turn it, and for a second after, the **whole screen is its dial** (in the layer's colour, the mode and cutoff under it: LP 1.2 KHZ). Saved with the session. |
| **KNOB 2** | the chosen layer's **granulator**: the middle is off (as it starts). **Left** towards dark ambient: long, overlapping, slowly drifting grains, pitched down now and then, darkened, fed back on themselves, sometimes frozen. **Right** towards scattering: short, fast grains, their timing, place, pitch and level jittered more and more, reversed, stuttered, full of holes, decimated at the far end. The further from the middle, the more grains and the less of the layer. While you turn it, the **whole screen is its dial** (DARK 40, SCATTER 25, OFF). Saved with the session. |
| **KNOB 3** | the chosen layer's **crush**: the middle is off (as it starts). **Left** takes the bit depth down from 16 to 1.5 bits (the sound coarsens, then breaks up, quiet tails gating out); **right** takes the sample rate down from 44.1 kHz to ~300 Hz (metallic aliasing, then a stepped buzz). Both along curves that keep the first half of the way playable; the rate glides, it does not jump. While you turn it, the **whole screen is its dial** (BITS 6.5, RATE 2.6K). After the granulator, before the layer's limiter. Saved with the session. |
| **KNOB 4** | the chosen layer's **reverb send**, 0–100 (40 at the start). While you turn it, the **whole screen is its dial** (SEND 40), as for the filter and the granulator. |
| **SAVE** | the **SESSIONS page**: **PRESETS** turns through the **64 slots** (the number shown very large: **red** when the slot holds a session, faint **grey** with EMPTY under it when it does not; the session loaded says LOADED). **SAVE** on a saved slot **loads** it; on an empty slot it **chooses** it (the sound stays as it is) so the next save goes there. Either way back to playing. A layer button goes back without either. |
| **SHIFT + SAVE** | the chosen layer's engine's **PRESETS page** (see [Presets](#presets)); SHIFT + SAVE again, or a layer button, goes back. |
| **SAVE held** (0.8 s) | **save** into the session loaded or chosen, at once; the whole screen flashes SAVED and the number. With none loaded or chosen yet (just after switching on), the first empty slot is offered: **PRESETS** chooses **NO** / **YES**, **SAVE** answers. **On the SESSIONS page**, SAVE held saves into the slot shown, whatever is loaded: an empty one asks SAVE NEW; one holding a session asks **OVERWRITE?** in red, on **NO** until PRESETS turns it to YES; SAVE answers. |
| **SEQ** | the **REVERB page** (SEQ lit). There **KNOB 1–4** are **SIZE**, **DECAY**, **DAMP**, **MIX**. SEQ again, or a layer button, goes back. |
| **ALGORITHM** knob | the chosen layer's **pan**, left to right in 41 steps. The message band says it (PAN L 50, PAN C). Layers start spread out: 1 and 2 half left and right, 3 and 4 a quarter, 5 and 6 near the middle. |
| USB MIDI in | notes play the chosen layer, as the keys do. |
| **MASTER** | the level. |

### Generative paths

Each layer has a path that leans what **HOME** draws, on every engine. A path changes nothing already
playing: it shapes the next sounds drawn on that layer. The path's key is lit while that layer is chosen.

| Path | Key | Leans towards |
| --- | --- | --- |
| **AMBIENT** | 1st black key | sounds for ambient, dark ambient above all: ethereal, melodic, often melancholic. FM draws drones, beating spectra (SPECTRE), the depths (ABYSS), bells, voices (CHOIR), bowed, hollow, blooming and tidal tones, never BRUIT, IMPACT, FRICTION, SHARDS or STATIC; then softer attacks, long tails, darker modulators, slow LFOs, no sirens, narrower glides, often an octave down. ANIMA draws mostly HAZE, GOLDEN, VOYAGE, TIDE, CHOIR and BOWL, never GRIT, SHARDS or MACHINE, with a darker tone, more space, a slower hand, long releases, no crusher. CALIGULA draws SPORE, VEIL and LITANY most, UNDERTOW, CARRION, MOTH, sometimes RUST; tuned on root, fifths, octaves, depths and unisons; slow swells, softer edges, fewer bursts and surges, a slower hand |
| **EXPERIMENTAL** | 2nd black key | sounds for electroacoustic and acousmatic music: edgier, gnarlier. FM draws bruits, frictions, shards, static, impacts and iterations, brighter, bent more often (sometimes twice), never SPECTRE, CHOIR, BOWED or TIDE. ANIMA draws mostly GRIT, SWARM, SHARDS and MACHINE, never HAZE, TIDE or CHOIR, and makes them wild: the sliders near their tops (FM, grain, texture, density, a quick hand), the resonance often screaming, and on most designs a crushed sample rate, folding drive, a band of noise and scattered grains, all dragged about by the hand, often a fast tremolo tearing it apart, the comb ringing hard. CALIGULA draws WIRE, THORN, HIVE and RUST most, never VEIL or LITANY; semitone clusters and tritones; sharper, more driven, more bursts and surges, a quicker hand |
| **OPEN** | neither lit | every family, kind and species alike, as before the paths. Every layer starts here |

Sessions keep each layer's path, and the path each of its sounds was drawn on.

### Latched keys

Hold one or more white keys and press **SHIFT** (the rightmost black key): they latch, sounding on after
you let go, and they stay lit. A key pressed while SHIFT is already held plays as usual (unless it is lit). Latches belong to the layer they were made on: the keys are lit while that
layer is chosen, and on another layer the same keys play as usual, so a drone can be left on one layer
while you play another. Pressing a latched key without SHIFT does nothing (it sounds already); **SHIFT and
the lit key** lets go of it, and its note ends as a lifted key's would. Loading a session lets go of every
latch. A layer has two voices, so a third note takes one of them, even a latched one's.

### The loopers

Every layer has its own looper of up to **ten loops**, and they are all **free**: no tempo, no grid, no
quantising — not even between a layer's own loops. Each overdub is a loop of its own, as long as you made
it (from its first note to REC), going round in its own length over the others, so they drift in and out of
phase. Each plays back exactly what was played, to 0.73 ms. The keys go on playing the layer over its loops,
and the loops' notes and the keys' never cut each other's notes off — but all of them share the layer's two
voices, so many loops at once take voices from each other. A layer's loops hold about 1000 notes between
them; nothing is lost when the FM-1 is switched off if the session is saved.

### The screen

From top to bottom: the **layer number** (very large, in the layer's colour), its **engine** (FM or
ANIMA, in white), the **family of its sound** (in the layer's colour). When something changes (NEW,
OCT +1, LOOP), a black-on-white message replaces the family for a moment. While the chosen layer's
looper is armed the band below the engine flashes **ARMED** in red; while it records it shows **REC**,
white on red. A message too long for the band at its usual size (EXPERIMENTAL) is drawn a size smaller.
Under it, across the screen, the layer's **level meter**: a bar filling from the left with the layer's colour
as it sounds (-48 to +6 dB), and a white mark at the level the SELECT knob sets (red at the left when the
layer is off). At the very bottom, the layer's **pan**: a block in its colour on a track from left to right,
with a tick at the middle.

KNOB 1 to 4 (filter, granulator, crush, reverb send) each show, while they turn and for a second after, the
same full-screen dial: the knob's name and the layer, an arc from 7 to 5 o'clock lit in the layer's colour,
a white pointer, and in large type what it does.

The **REVERB page** shows the title and its four values in rows, each with a bar; the row you just
turned is drawn black on white for a moment.

### Scales

One scale and root for the whole keyboard (and saved with the session); C major at the start. The root
sits on the first white key, in the octave from C3 to B3 (each layer's OCT moves it); the scale climbs
from there across the 16 white keys, so a pentatonic scale spans more than three octaves.

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
| JAPANESE IN | 0 1 5 7 8 — the *in* (miyako-bushi) scale of the koto and shakuhachi, known for its sorrow |

USB MIDI notes are not quantised.

### Presets

Each engine has **64 presets**, P1 to P64, for the sounds you want to keep and bring back on any layer.
Hold **SHIFT** and press **SAVE**: the PRESETS page of the chosen layer's engine opens, four slots a page.
**PRESETS** turns through them; a slot holding a sound shows its number in red and the sound's name, an
empty one EMPTY in grey.

- **SAVE held** saves the layer's sound into the slot chosen. Over a sound already there it asks
  **OVERWRITE?** first, on **NO** until PRESETS turns it to YES; SAVE answers.
- **SAVE** loads the slot chosen into the layer (which crossfades to it, as to a new sound) and goes back to
  playing.

A preset keeps what draws the sound again, exactly — its seed, its path and the generation of the
generators that drew it — and its level: all 192 take one sector of flash, beside the sessions. Saving
erases flash, which silences the audio for a moment, as a session save does.

### Sessions

The FM-1 always starts with no session loaded. There are 64 slots, numbered 1–64.

A session stores in full the sound of the engine each layer is on, and the other two as the seed that
made them and their level (the same sounds, drawn again at once), so a session usually takes one 4 KB
sector of flash. A session is the whole machine: every layer's engine and its three sounds, its path, octave, level, pan,
reverb send, filter, granulator, crush and loops (playing or stopped); the scale and root, the reverb, and
the layer chosen. They are kept in the
FM-1's flash, in the area SLOOP uses for its projects (**saving overwrites any SLOOP projects, presets and
samples left on the device**): dozens of typical sessions, at least ten even with six full loops.
A session also keeps the seeds its sounds were made from, with the path and the *generation* of the
generators that drew them: every generation is kept exactly as it was (a test checks thousands of sounds of
each), so sessions and presets saved under an older Tonitor load with the same sounds after an update that
brings new generators. Erasing flash silences the audio for a moment: a save
uses blank flash when there is some, and the FM-1 blanks free flash by itself while nothing sounds.

### The granulator

Each layer keeps the last half second of its own sound (at 22 kHz, 8-bit mu-law: a little grit is part of
it) and plays grains out of it, after its filter. Each layer's granulator has a **character of its own**,
drawn at random when the FM-1 starts and every time a session is loaded: the intervals its grains are
pitched to, how often they reverse, stutter, drop out or freeze, the shape of its scattered grains, how far
its feedback and darkening go, how it drifts. The knob's position is saved with the session; the character
is drawn anew. When the layer's notes end, its grains ring on for about two seconds.

### The reverb

A plate after Jon Dattorro (1997): predelay, input diffusion, and a figure-of-eight tank whose two
allpasses sweep slowly out of step, so the tail stays in motion and never rings. Each layer sends to it
after its level; the reverb's return is added to the mix before the master.

| | | at the start |
| --- | --- | --- |
| **SIZE** | the room: every delay 0.5–1.25 times Dattorro's (turning it bends the tail, gently) | 70 |
| **DECAY** | the time to fall 60 dB, 0.4 s to about 25 s, whatever the size | 65 (about 3.5 s) |
| **DAMP** | how fast the highs die in the tail | 40 |
| **MIX** | the reverb's return level, ×0 to ×4 | 50 (×1) |

Layer colours: 1 yellow, 2 cyan, 3 pink, 4 green, 5 orange, 6 lilac.

## The engines

### FM — six operators, DX7 style

The classic six-operator FM (msfa, the core of Dexed: 32 algorithms, rate/level envelopes, ratio or
fixed frequencies, feedback, LFO, pitch envelope). HOME draws a patch from one of seventeen families of
sound objects:

| Family | After |
| --- | --- |
| **METAL** | bells and gongs: many carriers on inharmonic ratios, the high partials dying first |
| **DRONE** | spectral drones: near-harmonic pairs detuned to beat, an index wandering over minutes |
| **BRUIT** | noises: feedback and high ratios scattered by a sample-and-hold LFO |
| **IMPACT** | struck bodies, the pitch falling, the modulators dying before the carriers |
| **ITERATIF** | iterative objects: a sound pulsed by its own LFO, square or random |
| **GLISSE** | glissandi: a pitch envelope travelling octaves, slowly |
| **SPECTRE** | a spectral arpeggio: rich tones a few cents apart whose beating lifts one harmonic after another |
| **ABYSS** | sub-bass rumble folded by feedback, breathing slowly |
| **FRICTION** | creaking: sub-audio fixed-frequency modulators and random pitch |
| **CHOIR** | vowels: carriers on the harmonics nearest two formants, a late vibrato |
| **BOWED** | a bowed string: a saw from feedback, the bow's pressure rising with the swell |
| **HOLLOW** | odd harmonics only: a dark pipe |
| **BLOOM** | a tone that opens over seconds and half closes again |
| **TIDE** | three stacks crossfading, one fading as the next swells, under a slow tide of level |
| **SHARDS** | a tone broken by random steps of pitch and chopped by a square |
| **STATIC** | ring-modulated bands from modulators nailed to audio frequencies |
| **WIND** | breath and gusts over a soft drone |

Each patch is then shaped with care: modulators on high ratios turned down (their sidebands alias and
scream), the brightness scaled down the keyboard so high notes are not shrill nor low ones muddy, rate
scaling on the carriers. HOME draws several patches, measures each, and keeps one whose brightness suits
the layer's path (darker on AMBIENT, brighter on EXPERIMENTAL) and that is not of the family the layer had.

About half the patches are then bent once more, gently (an operator detuned to beat, an envelope turned
over to grow as it holds, the LFO waiting and then moving the timbre, an octave down, more feedback, a
modulator's ratio moved to a neighbour, a partial nailed to a frequency).
Every patch is rendered offline as it is made and given the gain that brings it to one loudness.

### ANIMA — a synthesizer played by a simulated hand

A fixed-point port of the *anima* opcode from Vascular, rebuilt for the FM-1. While a key is held, a
simulated finger rests on a virtual surface and drags one figure after another — orbits, figures of
eight, scribbles, slow glides, now and then a flick — pressing in slow swells, and now and then lifting
to land elsewhere. A ghost finger comes and goes on an irregular clock. Where the finger is, how hard it
presses, how fast it moves and how long it has been down move the timbre, never the pitch.

The voice: two wavetable oscillators (24 band-limited single cycles), an FM pair, a stack of copies, a
grain cloud, a filtered noise, a resonant filter, a wavetable tremolo; then the hand's chain: a
downsampler, a waveshaper, a phaser, a second filter and a comb resonator tuned to the note. Each new
sound is a *design*: which of these are in, on which waveforms, and for every parameter which gesture
moves it, over what range, along what curve, plus a mod matrix of three to seven routes and Anima's
eight sliders, drawn — often far from their middle. Fourteen kinds: **HAZE**, **VOYAGE**, **GLASS**,
**BELLS**, **GOLDEN** (Anima's own) and **BREATH**, **GRIT**, **SWARM**, **TIDE** (waves of noise over a low
drone), **CHOIR** (vowels, a formant the hand moves), **SHARDS** (glass grains scattered and crushed),
**BOWL** (a beating inharmonic hum in a ringing comb), **DUST** (sparse crackle over a dark drone),
**MACHINE** (a pulsing, folded mechanism). The filter follows the keyboard, so low notes are not muffled
nor high ones shrill; the second oscillator's interval suits the path (consonant on AMBIENT, anything on
EXPERIMENTAL); and an AMBIENT sound nearly always has a resonant space to ring in.

In Vascular a design plays up to six fingers, each a voice; here a layer has one voice, so the hand has
one finger on the sound.

### CALIGULA — an organism

The third engine, after the architecture of SOMA's LYRA-8 as Mike Moreno's LIRA-8 emulates it (BSD
licence), rebuilt for the FM-1 and bent towards dark, restless textures. Eight oscillators in four pairs
on two sides; each a triangle that its pair's SHARP pushes towards a square; each with a **sensor** and an
envelope, slow (a long swell, a longer fall) or fast; each pair frequency-modulated at audio rate by its
partner, the other pair of its side, a pair of the other side or the **total feedback** (the whole
organism, saturated, fed back in). A **hyper LFO** of two LFOs (alone, AND, OR, XOR) opens and closes the
modulation and gates chosen pairs into irregular rhythms; a short modulated delay (two lines, ~35 ms)
choruses, flanges or warps it; a drive.

A CALIGULA layer is **played like the others**: the white keys play the scale's notes, and it is
**silent until a key is down**. A key wakes the organism at that note; a second key on top moves the
whole organism to the new note at once (a snap, not a glide), and letting go of it goes back to the key
still held; letting go of the last key lifts the hand and the organism rings out. It is **rooted**:
tuned around the note played, in intervals of it (octaves, fifths, unisons and semitone clusters that
beat, tritones, the depths). The hyper LFO moves timbre, never pitch: no sirens.

Nobody touches the eight plates: **a simulated hand** does, while a key is held. Each species has its own
hand: how often it touches, how long a finger stays, how many fingers at most, its favourite plates,
quick taps against long holds. The first finger stays down until the key is let go, so the sound never
drops out under a held key. Around the hand the organism has a life of its own: the modulation depths
and the sharpness wander, oscillators fire in **bursts** at irregular times, and now and then the
feedback and the drive **surge** for a moment. A pair's envelope may be slow: a swell of seconds, a release of up to
nine seconds after the key is let go.

The hand often has a **phrase**: two to four plates it keeps coming back to, varied now and then, often on
a pulse of its own (long, short, short, longer) — so it plays like someone rather than like chance. A pair
the hyper LFO's gates never close stays under it all. **HOME** draws a new organism from ten species, tuned
on one of ten sets of intervals (pillars of octaves and fifths, semitone clusters, the depths, the
harmonics, unisons, tritones, fourths, a minor seventh, open fifths, a quarter-tone cluster):

| Species | |
| --- | --- |
| **SPORE** | slow swells, held drones, breathing; low |
| **THORN** | sharp, deep FM, driven, bursting often |
| **HIVE** | the hyper LFO quick and gating: irregular rhythms, bursts |
| **CARRION** | the depths, heavy total feedback, the delay warped by the voices, surges |
| **VEIL** | mostly triangles, light modulation: the calmest |
| **RUST** | driven, cross-modulated, surging, sometimes rhythmic |
| **LITANY** | a slow choir: unisons and octaves beating, nothing sharp, a hand that stays |
| **UNDERTOW** | deep: heavy feedback and a warped delay pulling it under, slow surges |
| **WIRE** | high and sharp, clusters and quarter tones, a quick hand tapping, bursts |
| **MOTH** | soft triangles fluttering through the gates, on a pulse |


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

- `build/sounds/` — WAVs of every engine, family and path, and `mix.wav`, the six layers entering one after
  another;
- `build/screens/` — the screen in its states, as PNGs.

## How it is built

`firmware/src/tonitor.c` is the single compilation unit: `fm.c` (FM, around msfa's `fm6_core.c`),
`anima.c` (ANIMA), `cal.c` (CALIGULA), `gran.c` (the granulator), `reverb.c`, `layers.c` (the six layers,
the loopers, the mix), `ui.c` (panel and screen), `sessions.c`, `gfx.c`, `dsp.h` (fixed-point helpers),
`main.c` (boot, interrupts). From SLOOP / Felucca, unchanged or nearly: `firmware/hal/`, `firmware/loader/`,
`usb.c`, `ota.c`, `recovery.c`, `lcd.c`, `libc.c`, the build checks and the installer. The website and the
browser installer are in `docs/` (served by GitHub Pages); `tools/make_site.py` builds its pages from `site/` (home, install), this README (the manual) and `CHANGELOG.md` and publishes `build/tonitor.fwsc` for the installer.

# Licence

- **The firmware** is free software under the **GNU General Public License, version 3 only**
  (`GPL-3.0-only`, [`LICENSE`](LICENSE)): it is a work based on Felucca and SLOOP, which are GPL-3.0. The
  browser installer (`docs/js/fm1ota.js`), a port of their installer, is GPL-3.0 too.
- **The website, the logo and the documentation** written for Tonitor (`docs/` except `docs/js/fm1ota.js`
  and the firmware package, and this README's text) are under the **MIT licence**
  ([`LICENSES/MIT.txt`](LICENSES/MIT.txt)).
- Third-party parts keep their own licences: msfa (Apache-2.0), the LIRA-8 architecture (BSD 3-clause), the
  fonts (SIL OFL 1.1), the JieLi SDK files placed in the package (Apache-2.0).

[`LICENSING.md`](LICENSING.md) lists every file and its licence.

# Credits

Tonitor is by Nuno Zimas. It stands on the work of others:

- [**Felucca**](https://github.com/hugelton/Felucca) by Leo Kuroshita / Hügelton Instruments and
  [**SLOOP**](https://github.com/isod89/sloop-fm1) by isod89 — the hardware layer, USB stack, update loader,
  rescue mode and installer that made an FM-1 firmware possible at all.
- **msfa** by Google Inc. and Pascal Gauthier (Apache-2.0), from Dexed, in Felucca's integer port — the FM.
- **ANIMA** is ported from the anima opcode of Vascular.
- **CALIGULA** follows the architecture of [**LIRA-8**](https://github.com/MikeMorenoDSP/LIRA-8) by Miguel
  Moreno (BSD), an emulation of SOMA Laboratory's LYRA-8.
- **Atkinson Hyperlegible** by the Braille Institute (SIL OFL 1.1) and **Terminus** (SIL OFL 1.1).

M-VAVE and FM-1 are trademarks of their owner, used here only to say which device Tonitor runs on.

# Disclaimer

Tonitor is an **unofficial** firmware. It is not made, endorsed, approved or supported by M-VAVE or by the
maker of the FM-1, and it is not affiliated with them.

**Installing it replaces the device's original firmware and may void any warranty that applies to your
FM-1**, if any applies at all; check the terms of your warranty before installing. Installing any firmware
carries a risk: an interrupted or failed update can leave the device unable to start until it is recovered
(see [Installing](#installing)), and saving sessions overwrites data that other firmwares (SLOOP) keep in the
same flash area.

The software is provided **"as is", without warranty of any kind**, express or implied, including but not
limited to the warranties of merchantability, fitness for a particular purpose and non-infringement. In no
event shall the authors or contributors be liable for any claim, damages or other liability — including
damage to your device, loss of data, or loss of the use of the device — arising from, out of or in
connection with the software, its installation or its use. You install and use it entirely at your own
risk. See also sections 15 and 16 of the [GPL](LICENSE).
