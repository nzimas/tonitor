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

- **Seven sound engines**, chosen per track:
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
- **Generative paths** — lean each track's new sounds towards **AMBIENT** (ethereal, melodic, dark) or
  **EXPERIMENTAL** (edgier, gnarlier, more textured), or leave it **OPEN**.
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
| **FX SEL ENV LFO EDIT GLO** (top row) | choose track **1 2 3 4 5 6**. The chosen track's button is lit; the others glow while they sound. **Held** (0.8 s): **copy** the track, or **paste** the copy onto it — see [Copying a track](#copying-a-track). |
| **PRESETS** knob | the chosen track's engine: **FM**, **ANIMA**, **CALIGULA**, **RUSSOLO**, **DRUID**, **BAYAREA** or **HIVE**, left to right. Each track keeps a sound of each engine, so turning back finds the sound it had. |
| **SHIFT + PRESETS** | in the track's view, a **new sound of the family before or after** on the track's engine (the family's name shows under the engine). At the first or last family, a new sound of that family again. |
| **HOME** | a **new sound** for the chosen track, on its engine (never the same family twice in a row). HOME lights for a moment. |
| **SHIFT + HOME** | a **new sound of the same family** (kind, species) as the chosen track's sound — to explore a family you like. The screen says SAME FAMILY. |
| **OCT− / OCT+** | the chosen track's octave, −3 to +3, shown at the top right of the screen (+1, −2; nothing at 0). OCT− is lit below the middle, OCT+ above. |
| **SHIFT + OCT− / OCT+** | **every loop** of every track down or up **one step of the scale** (see [the loopers](#the-loopers)). |
| **SHIFT + a track's button held + OCT− / OCT+** | **that track's loops** only, down or up one step of the scale (TRANSP T3 +2). |
| the white keys | play the chosen track **in the scale chosen** (ARP): the first white key is the root, each white key the next note of the scale, up across all 16. **Voices come from one pool of twelve shared by all the tracks**: a note takes a free voice, so a track has as many as its notes need — a chord on one, a single line on another — and **a new note never cuts the release of the one before it** while voices are free. With the twelve all busy, a new note takes the release that has rung longest (on any track), else the oldest note held on its own track, else the oldest held anywhere. (CALIGULA and RUSSOLO are one organism a track, outside the pool.) A key lifted ends its note on the track it was played on, so **a held note keeps sounding while you choose and play another track**. The keys you hold on the chosen track light up. |
| the rightmost black key | **SHIFT** (lit while held). Hold white keys and press SHIFT: they **latch** — they keep sounding after you let go, and their keys stay lit. Hold SHIFT and press a lit key to let go of it. |
| the two leftmost black keys | the chosen track's **generative path**: the first **AMBIENT**, the second **EXPERIMENTAL** (lit while chosen; the screen says which). Pressing the lit one again goes back to **OPEN**. See below. |
| the 3rd black key from the right | the **ENVELOPE** page, up and down (lit while up; it acts when let go): KNOB 1–4 set the chosen track's attack, decay, sustain and release. See [Envelope](#envelope). With SHIFT: the chosen track's step sequencer, **BRICKLAYER**, on and off (the key blinks while it is on; PLAY starts it); **held** (0.6 s) while it is on, its page up or down. See [BRICKLAYER](#bricklayer). |
| the black key before SHIFT | **EDIT**, on and off (lit while on): KNOB 1–4 become four **macros** that shape the chosen track's sound. See [Macros](#macros). With SHIFT: the chosen track's physics sequencer, **BALLSY**, on and off (EDIT blinks while it is on; PLAY starts it); **held** (0.6 s) while it is on, its page up or down. A short press is always EDIT. See [BALLSY](#ballsy). |
| the 3rd black key | **chord mode** for the chosen track: **held** (0.6 s), on or off — **CHO** at the top left of the screen while on, the key lit; **pressed**, the next chord form (its name on the screen). See [Chords](#chords). |
| the 4th, 5th and 6th black keys | the chosen track's loops **REVERSE** (played backwards), **PENDULUM** (forwards and backwards in turn) or **RANDOM** (the notes in an order drawn at random); pressed again, forwards. Lit while on. See [Direction and speed](#direction-and-speed). |
| the 7th and 8th black keys | the chosen track's loops at **HALF SPEED** or **DOUBLE SPEED**; pressed again, normal speed. Lit while on. |
| all black keys | play no notes. They glow dimly while off and are fully lit while on — the loop direction and speed keys only while the chosen track's loops play; SHIFT is lit only while held. |
| **SHIFT + ARP** | the chosen track's engine's **PRESETS page** (see [Presets](#presets)): there **ARP held** saves the track's sound into the slot chosen, **ARP** pressed loads it. SHIFT + ARP again, or a track button, goes back. (SHIFT + SAVE before 0.51.) |
| **ARP** | the **SCALE page** (ARP lit): **SELECT** chooses the root (C to B), **ALGORITHM** the scale. The root is shown very large, the scale under it; what you just turned flashes black on white. ARP again, or a track button, goes back. |
| **REC** | the chosen track's looper. Press to **arm** a new loop (REC flashes, the band shows ARMED and its number); the **first note** you play starts it (REC lights; REC and the number). Press REC again to **end** it: the loop is exactly as long as from that first note to this press, and it starts playing at once, round and round. **Press REC again while it plays to overdub**: the same again, a new loop over the others — up to **10 loops a track**. |
| **SHIFT + REC** | the chosen track's loops **VARY**, on and off (VAR at the top right of the screen, under the octave and the battery, while on): each note they play gets a random level (from −10 dB to full) and a random length (50 ms to 1.5 s) instead of the recorded one, and now and then, at odds of its own, it is changed or joined: dropped this time round (5 %), late by 5–40 ms (25 %), an octave up or down (8 %), replaced or joined by a note that was not recorded — a step or two of the scale away, or a fifth (8 %), preceded by a grace note a step away (4 %), struck 2–4 times quickly (4 %), joined by a third or fifth above (10 %), echoed 1–4 times 120–400 ms apart, each quieter, sometimes climbing or falling a step each time (15 %); the notes of a chord are sometimes strummed 20–60 ms apart (12 %). Every note stays in the scale. The loops' **pace** is no longer steady either: it rises and falls slowly, and every few seconds on average it halts for a moment and resumes at once, bursts to 2–3× for a moment, grinds to a stop and snaps back, or climbs to 2–2.5× and drops suddenly (not while REC records). Applies to all its overdubs. Stored in sessions; ends when the track's loops are emptied. |
| **REC held** (1 s) | **empty** the chosen track's looper, all its loops: the whole screen flashes LOOP n / EMPTY. |
| **PLAY** | on BALLSY's or BRICKLAYER's page: starts or stops that sequencer (PLAY or STOP on the screen; PLAY blinks with the sequencer's key while it plays). On every other page, pressed and let go: stop the chosen track's loops, or play them again, each from its start. **Held**: the chosen track's loops on the white keys, to bring each in or take it out (see [The loopers](#the-loopers)). PLAY is lit, steadily, while they play. **SHIFT and PLAY**, from any track: if any loop or sequencer plays, every loop and sequencer on every track stops (ALL STOPPED; the loops are kept); if none plays, every loop and every sequencer that is on starts, all together from their starts (ALL PLAYING). PLAY alone still starts or stops the chosen track's own. |
| **SELECT** knob | the chosen track's **level**: 1.5 dB a click, from OFF up to +4.5 dB. The message band says it as the knob's share of its turn, OFF and 4 % to 100 % (VOL 74 %); a track starts at 87 % (0 dB). |
| **KNOB 1** | the chosen track's **DJ filter**: from the middle (off), left is a resonant **low-pass** coming down from 20 kHz to ~35 Hz, right a resonant **high-pass** going up from 16 Hz to ~12 kHz; the further from the middle, the more resonant. While you turn it, and for a second after, the **whole screen is its dial** (in the track's colour, the mode and cutoff under it: LP 1.2 KHZ). Saved with the session. |
| **KNOB 2** | the chosen track's **granulator**: the middle is off (as it starts). **Left** towards dark ambient: long, overlapping, slowly drifting grains, pitched down now and then, darkened, fed back on themselves, sometimes frozen. **Right** towards scattering: short, fast grains, their timing, place, pitch and level jittered more and more, reversed, stuttered, full of holes, decimated at the far end. The further from the middle, the more grains and the less of the track. While you turn it, the **whole screen is its dial** (DARK 40, SCATTER 25, OFF). Saved with the session. |
| **KNOB 3** | the chosen track's **crush**: the middle is off (as it starts). **Left** takes the bit depth down from 16 to 1.5 bits (the sound coarsens, then breaks up, quiet tails gating out); **right** takes the sample rate down from 44.1 kHz to ~300 Hz (metallic aliasing, then a stepped buzz). Both along curves that keep the first half of the way playable; the rate glides, it does not jump. While you turn it, the **whole screen is its dial** (BITS 6.5, RATE 2.6K). After the granulator, before the track's limiter. Saved with the session. |
| **KNOB 4** | the chosen track's **reverb send**, 0–100 (40 at the start). While you turn it, the **whole screen is its dial** (SEND 40), as for the filter and the granulator. |
| **SAVE** | the **SESSIONS page**: **PRESETS** turns through the **64 slots** (the number shown very large: **red** when the slot holds a session, faint **grey** with EMPTY under it when it does not; the session loaded says LOADED). **SAVE** on a saved slot **loads** it; on an empty slot it starts a **new session** there: a clean slate, as when the FM-1 is switched on — the loops and latches gone, every track on its first engine with new sounds and its settings at their defaults, the scale, reverb and transposition too (the session you were in stays saved as it was) — and the next save goes there. Either way back to playing. A track button goes back without either. |
| **SHIFT + SAVE** | **GLOBAL SETUP**: the FM-1's own settings, MIDI for now (see [Global setup and MIDI](#global-setup-and-midi)). SAVE is lit while it is up; a track button goes back. |
| **SAVE held** (0.8 s) | **save** into the session loaded or chosen, at once; the whole screen flashes SAVED and the number. With none loaded or chosen yet (just after switching on), the first empty slot is offered: **PRESETS** chooses **NO** / **YES**, **SAVE** answers. **On the SESSIONS page**, SAVE held saves into the slot shown, whatever is loaded: an empty one asks SAVE NEW; one holding a session asks **OVERWRITE?** in red, on **NO** until PRESETS turns it to YES; SAVE answers. |
| **SEQ** | pressed again and again, three pages in turn, then back to playing (SEQ lit while one is up; a track button also goes back): the **DELAY page** — **KNOB 1–4** are **TIME** (20 ms – 2 s), **FEEDBACK**, **DAMP**, **MIX** (see [The delay](#the-delay)); the **REVERB page** — **SIZE**, **DECAY**, **DAMP**, **MIX**; the **MOD page** — the chosen track's modulation (see [Modulation](#modulation)). |
| **SHIFT + SEQ** | the chosen track's **modulation thrown anew**: both LFOs and two to four slots, at random (MOD RANDOM). |
| **SHIFT + KNOB 3** | the chosen track's **distortion**, 0–100 (0, off, at the start); its dial reads DISTORTION n and the amount. See [The distortion](#the-distortion). |
| **SHIFT + KNOB 4** | the chosen track's **delay send**, 0–100 (0 at the start); its dial reads DELAY n / SEND. |
| **ALGORITHM** knob | the chosen track's **pan**, left to right in 41 steps. The message band says it (PAN L 50, PAN C). Tracks start spread out: 1 in the middle, 2 half right, 3 and 4 a quarter left and right, 5 and 6 near the middle. |
| MIDI in (USB, and the TRS jack) | notes play the tracks by channel, CCs set their level, pan, macros, filter and sends (see [Global setup and MIDI](#global-setup-and-midi)). |
| **MASTER** | the level. |

### Generative paths

Each track has a path that leans what **HOME** draws, on every engine. A path changes nothing already
playing: it shapes the next sounds drawn on that track. The path's key is lit while that track is chosen.

| Path | Key | Leans towards |
| --- | --- | --- |
| **AMBIENT** | 1st black key | sounds for ambient, dark ambient above all: ethereal, melodic, often melancholic. FM draws drones, beating spectra (SPECTRE), the depths (ABYSS), bells, voices (CHOIR), bowed, hollow, blooming and tidal tones, never BRUIT, IMPACT, FRICTION, SHARDS or STATIC — and plucks and mallets, never WARP, GLITCH or RUBBER; then softer attacks (a pluck or a mallet may keep its strike), long tails, darker modulators, slow LFOs, no sirens, narrower glides, often an octave down. ANIMA draws mostly HAZE, GOLDEN, VOYAGE, TIDE, CHOIR, BOWL, FOG and DRIFT, never GRIT, SHARDS, MACHINE, BITS or HOWL, with a darker tone, more space, a slower hand, long releases, no crusher. CALIGULA draws SPORE, VEIL and LITANY most, UNDERTOW, CARRION, MOTH, sometimes RUST; tuned on root, fifths, octaves, depths and unisons; slow swells, softer edges, fewer bursts and surges, a slower hand. RUSSOLO draws CHORD most, then DUST, CRACKLE, BLIPS, WALK and WANDER, never the logic kinds (NOR, XOR, RING, CASCADE, CLANG), JUMP, BEND or QUAD; clean (9–10 bits), slow attacks, long releases, calmer chance, no major chords |
| **EXPERIMENTAL** | 2nd black key | sounds for electroacoustic and acousmatic music: edgier, gnarlier. FM draws bruits, frictions, shards, static, impacts, iterations, warps, glitches and rubber, brighter, bent more often (sometimes twice), never SPECTRE, CHOIR, BOWED or TIDE. ANIMA draws mostly GRIT, SWARM, SHARDS, MACHINE, BITS and HOWL, never HAZE, TIDE, CHOIR, FOG or DRIFT, and makes them wild: the sliders near their tops (FM, grain, texture, density, a quick hand), the resonance often screaming, and on most designs a crushed sample rate, folding drive, a band of noise and scattered grains, all dragged about by the hand, often a fast tremolo tearing it apart, the comb ringing hard. CALIGULA draws WIRE, THORN, HIVE and RUST most, never VEIL or LITANY; semitone clusters and tritones; sharper, more driven, more bursts and surges, a quicker hand. RUSSOLO draws NOR, XOR, CLANG, BEND and QUAD most, never WALK or CHORD; crushed to 4–9 bits, often sample-held, abrupt attacks and short releases, chance driving it hard |
| **OPEN** | neither lit | every family, kind and species alike, as before the paths. Every track starts here |

Sessions keep each track's path, and the path each of its sounds was drawn on.

### Macros

Press **EDIT** (the black key just left of SHIFT; lit while on) and the four knobs across the top stop being
the filter, the granulator, the crush and the send: they become **four macros** that reshape the chosen
track's sound, while it plays, under a held note. Every macro answers early in the turn (a quarter of the way, half the change). Each is turned from the middle, which is the sound as HOME
drew it; either way transforms it. While a knob turns, the whole screen is its dial, with the macro's name
for this sound and how far it is turned (+40 %, -25 %, AS DRAWN). Press EDIT again to give the knobs back.

The macros are chosen for each engine, and for some by the kind of sound:

| | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **FM** | **BRIGHTNESS**: the modulation index about 27 dB either way (down: towards pure sines; up: the feedback too), the carriers tilted (the higher partials up) | **DETUNE**: the carriers pulled apart ±40 cents and the modulators ±25 cents off them — beating, chorus, the partials off their places — or **INHARMONIC** for metallic and noisy families (every partial off its ratio: clangour) | **SATURATION**: feedback and saturation turning up; a low-pass, smoother, turning down | **MOTION**: the LFO's depths; turning up, a tremolo, quicker and deeper |
| **ANIMA** | **TONE**: every filter four octaves up or down (sharper when up), the FM brighter or darker | what the kind is about, from nothing to far more: **GRAINS**, **NOISE**, **FM** (the index), **FORMANT** (sharper when up) or **DRIVE** (folded hard; turned down, a pure tone: the second voice, FM, noise and grains drawn out) | **RESONATOR**: much wetter and ringing longer (brought in if the sound had none), or dry | **MOTION**: the hand four times quicker, a tremolo pulsing — or nearly still, no tremolo |
| **CALIGULA** | **WAVEFORM**: triangles to squares | **MOD DEPTH**: how deeply the pairs modulate each other | **FEEDBACK** (total feedback and drive) — or **DELAY** for SPORE, VEIL, LITANY and MOTH | **TOUCHES**: how busy the hand is, its touches and bursts — or **GATE RATE** (the gates' speed) for HIVE, MOTH and WIRE |
| **DRUID** | **STRUCTURE**: on a string, its dispersion (below a quarter, a buzzing bridge; above, the partials stretched, then noise in the period); on sympathetic strings, the intervals they are tuned to; on a body, how its partials are spaced, from squeezed through harmonic to bell-like | **BRIGHTNESS**: the exciter's low-pass and the string's or body's high-frequency loss | **DAMPING**: how long it rings, from ~70 ms to ~18 s to -60 dB, and above 0.95 on without decay | **POSITION**: where it is plucked or struck and where it is picked up — which partials sound |
| **BAYAREA** | **SHAPE**: the oscillator from sine through triangle and square to saw | **FOLDS**: the wavefolder's drive | **FM**: both modulators' depth | **DECAY**: the gate's decay, or the sustain's release, from an eighth to eight times the sound's own |
| **HIVE** | **DETUNE**: the swarm's spread around its places, from none to twice the sound's own | **MOVEMENT**: the LFOs' depth on the pitches, the levels and the filter, from none to twice | **MIX**: the swarm against the main oscillator — or **NOISE** on GROUND and AIR | **CUTOFF**: the filter's cutoff |
| **RUSSOLO** | the kind's first parameter (RATE, TONE, WIDTH, HISS, RATIO, SPREAD, DENSITY, SILENCE, WINDOW, STEP or DEPTH) | the kind's second parameter (CHANCE, DENSITY, WANDER, DRIFT, SUB, CLOCK, CHANGE, JUMPS or RATE) | **CRUSH**: fewer bits and longer sample holds, or cleaner | **RANDOMNESS**: how much chance drives it; above the middle, stutters too |

Each track keeps the macros of each of its seven sounds. **HOME** draws a new sound as it comes (its macros
in the middle). Sessions and presets keep the macros with the sound.

### Envelope

The 3rd black key from the right opens the **ENVELOPE** page (the key is lit while it is up; pressed again, or a
track button, closes it). It shows the amplitude envelope of the chosen track's sound as attack, decay,
sustain and release, drawn and with their values. As HOME drew the sound, the page reads its own envelope and
says **AS DRAWN** (in grey): for FM, the envelope of the loudest carrier; for CALIGULA, the average of its
pairs' attacks and releases; CALIGULA and RUSSOLO have no decay or sustain stage (shown as 5 ms and 100 %).

While the page is up, the four knobs across the top set **ATTACK** (KNOB 1, 1 ms – 10 s), **DECAY** (KNOB 2,
5 ms – 10 s), **SUSTAIN** (KNOB 3, 0–100 %) and **RELEASE** (KNOB 4, 5 ms – 20 s), on logarithmic scales. The
first turn starts from the values shown and replaces the sound's own amplitude envelope with this one: the
attack is linear, the decay and release are exponential, the release ending 60 dB down. FM's carrier envelopes
and ANIMA's envelope are then held at full; CALIGULA and RUSSOLO are kept playing through the release and
then let go. The change applies from the next note; once the track's envelope is in use, turning the knobs
also changes the notes that are sounding.

**HOME** (a new sound) or loading a preset gives the sound's own envelope back. Each track
keeps an envelope for each of its seven engines. Sessions store them, copying a track copies them, presets do
not.

### PLAY, the sequencers and the looper

A track has one sequencer at a time: while its BALLSY is on, SHIFT + the envelope key shows NOT ALLOWED and
changes nothing, and while its BRICKLAYER is on, SHIFT + EDIT does the same. The looper works beside either.

A sequencer turned on waits for **PLAY**. On the sequencer's page, PLAY starts and stops it, and blinks with the
sequencer's key while it plays. On every other page PLAY is the looper's, as ever, lit steadily while the
track's loops play: so with a sequencer playing you can put its page down (or choose the track), record loops
over it from the keys (the looper records only the keys, never the sequencer's notes), and start and stop them
with PLAY; bring the sequencer's page up again to start or stop the sequencer.

### BALLSY

**SHIFT + EDIT** turns on BALLSY, a physics sequencer of bouncing balls, for the chosen track and shows its page; EDIT blinks
while it is on, and SHIFT + EDIT again turns it off. The balls wait in the middle until **PLAY** (on their page) starts
them; PLAY again holds them still where they are (their notes let go), and PLAY once more moves them on from there.
Each track has its own, and they run at the same time.
**Holding EDIT** (0.6 s) while the chosen track's balls are on puts their page down or up: with the page down the
balls keep playing and the knobs, the screen and the other pages work as usual; a track button or another page
also puts it down. A short press of EDIT still turns EDIT on or off.

One to eight balls move in a box on the screen. Each time a ball hits a wall, it plays a note on the track, as
a key would: in the scale, with the loops' transposition and chord mode. The looper does not record them: it
records only what is played on the keys. Each note's length (50 ms to 1.5 s) and level (from −10 dB to full) are drawn at random; the level applies to
FM, ANIMA, DRUID, BAYAREA and HIVE (CALIGULA and RUSSOLO play at their own level). Each bounce changes the ball's angle at random by up to 12°, kept between 15° and 75° from the walls, so
the paths change and every wall is reached. Each wall holds the notes of the scale, one segment per step, and the
segment that is hit lights up. The pitch rises clockwise all the way round, one octave per wall:

| Wall | Octave (at the start) | Direction of rising pitch |
| --- | --- | --- |
| bottom | −1 (the octave below the keyboard's) | right to left |
| left | 0 (the keyboard's) | upwards |
| top | +1 | left to right |
| right | +2 | downwards |

While their page is up, the four knobs control them:

| Knob | |
| --- | --- |
| **KNOB 1** | the number of balls, 1–8; new ones start from the middle in a random direction |
| **KNOB 2** | speed, 0–100 (from 0.08 to 2.4 box widths per second) |
| **KNOB 3** | the octaves of the four walls, moved together by one octave per click, from −3…0 to +2…+5 |
| **KNOB 4** | instability, 0–100 (0 at the start): each ball gets its own wind, two values that wander at random (a new aim about twice a second, followed over ~0.3 s). One scales its speed, up to 8× faster or slower at 100, so a ball can nearly stop and then rush on; the other turns it, up to half a turn per second. Occasional gusts change its direction by up to 60°. At 0 the pace is steady and the paths straight |

The screen shows the box, the walls' segments in the track's colour and the balls; the line above shows the
number of balls, or the value just turned. Sessions store each track's ball settings and whether they are on;
copying a track copies them.

### BRICKLAYER

**SHIFT + the envelope key** (the 3rd black key from the right) turns on BRICKLAYER, a step sequencer built a
note at a time, for the chosen track, and shows its page; the key blinks while it is on, and SHIFT + the key again
turns it off. It waits for **PLAY** (on its page), which starts it from its first step and, pressed again, stops
it (its notes let go); notes can be entered whether it plays or not. **Holding the envelope key** (0.6 s) while it is on puts its page down or up: with the page down it
keeps playing and the knobs, the screen and the other pages work as usual (a track button or another page also
puts it down). A short press of the key is still the ENVELOPE page. Each track has its own.

On its page, **REC** is the entry, not the looper:

| Gesture | |
| --- | --- |
| **REC** | entry on or off (REC stays lit while it is on) |
| a white key, entry on | its note is added at the end of the sequence (and sounds) |
| **SHIFT** + a white key, entry on | that note is taken out, wherever it is |
| **SHIFT + REC** | the last note added is taken out |
| **REC** held 1 s | the sequence is emptied (BRICKS / EMPTY) |

The notes play in the order they were added, spread over the sequence's length as evenly as possible (a
Euclidean rhythm, the first note on the first step: 3 notes in 8 steps fall on steps 1, 4 and 7). Notes are kept
as steps of the scale, so they follow the loops' transposition (SHIFT + OCT, and SHIFT + the track's button + OCT),
the scale and the root, as latched notes do; chord mode applies to them as to keys.

| Knob | |
| --- | --- |
| **KNOB 1** | **LENGTH**: steps, from the number of notes up to 32 |
| **KNOB 2** | **RATE**: 0.5–16 steps a second |
| **KNOB 3** | **HARMONY**: how often a note becomes a diad (a third or a fifth above) or a triad |
| **KNOB 4** | **VARIATION**: each note's level (down to −12 dB) and length (from a quarter to twice the usual) are spread at random; the pace wanders (from half to twice the rate), and now and then stops for a moment or rushes; as likely as the knob says, a note stutters (struck 2–4 times within its step), the last 2–4 steps are played again (beat repeat), a note echoes 1–3 times, quieter, or an empty step plays a quiet ghost note from the sequence. At 0: a steady level and length, a steady pace |

On its page the loops' direction and speed keys (the 4th to 8th black keys) are BRICKLAYER's own, lit while on and
glowing while off: **REVERSE**, **PENDULUM** (forwards and backwards in turn), **RANDOM** (a step drawn at random
each time), **HALF SPEED** and **DOUBLE SPEED**, combined as for the loops. The step lit on the screen follows them.
With the page down, the keys are the looper's again.

The screen shows the steps as cells, eight to a row (up to four rows): a note's cell filled in the track's
colour, an empty one as an outline, the step being played in white. The line above shows the notes and the
steps (3 IN 8), REC and the number of notes while entering, or the value just turned. Each note lasts 70 % of
its step (VARIATION spreads it). Sessions store each track's sequence and settings; copying a track copies them.

### Latched keys

Hold one or more white keys and press **SHIFT** (the rightmost black key): they latch, sounding on after
you let go, and they stay lit. A key pressed while SHIFT is already held plays as usual (unless it is lit). Latches belong to the track they were made on: the keys are lit while that
track is chosen, and on another track the same keys play as usual, so a drone can be left on one track
while you play another. Pressing a latched key without SHIFT does nothing (it sounds already); **SHIFT and
the lit key** lets go of it, and its note ends as a lifted key's would. Loading a session lets go of every
latch. Latched notes hold voices of the shared pool: with all twelve busy, a new note can take one, even a latched one's.

### Chords

With chord mode on for a track (the 3rd black key held 0.6 s; CHO at the top left of the screen, the form's
abbreviation under it in grey: TRI, IN1, IN2, SU2, SU4, OPN, 4TH), each white
key plays three notes: its own note and two more of the same scale, so every chord stays in the scale and its
quality (major, minor, diminished) is the scale's at that degree. Pressing the 3rd black key steps through the
forms (in steps of the scale from the key's note):

| Form | Steps | |
| --- | --- | --- |
| **TRIAD** | 0, 2, 4 | root, third, fifth (the start) |
| **INV 1** | 2, 4, 7 | first inversion: the third at the bottom, the root an octave up |
| **INV 2** | −3, 0, 2 | second inversion: the fifth below the root |
| **SUS2** | 0, 1, 4 | the third replaced by the second |
| **SUS4** | 0, 3, 4 | the third replaced by the fourth |
| **OPEN** | 0, 4, 9 | root, fifth, and the third an octave up |
| **FOURTHS** | 0, 3, 6 | stacked fourths |

One chord sounds at a time on a track: a new key lets the one before go. Chords follow the loops' moves along
the scale like single notes, latch with SHIFT (all three notes), are recorded by the loopers as three notes,
and are kept in sessions (chord mode, its form, the chords latched); copying a track copies its chord mode.

### Direction and speed

The 4th to 8th black keys set how the chosen track's loops play, all its overdubs together, each within its own
length:

| Key | Setting |
| --- | --- |
| 4th black key | **REVERSE**: the loops play backwards — each note starts where it ended and ends where it started |
| 5th black key | **PENDULUM**: forwards to the end of each loop, backwards to its start, and so on |
| 6th black key | **RANDOM**: the loops play forwards, but each time the next note is due (and at the loop's end) they jump to a note of the loop drawn at random and go on from there — each note keeps its length and the time to the note that followed it when recorded; notes struck together stay together; the notes sounding are let go at each jump |
| 7th black key | **HALF SPEED**: the loops take twice as long (the pitch does not change) |
| 8th black key | **DOUBLE SPEED**: the loops take half as long |

REVERSE, PENDULUM and RANDOM exclude each other, as do HALF and DOUBLE; pressing the lit key again returns to forwards
or to normal speed. Direction and speed combine (backwards at half speed, pendulum at double, and so on).
Changing the direction while a loop plays continues from where it is: a note sounding at that moment carries on
and ends where its start was. An overdub recorded at half or double speed, or while the loops play backwards, is
stored so that it plays back as it was played. Sessions keep the settings; copying a track copies them;
emptying a track's loops (REC held) returns them to forwards and normal speed.

### Copying a track

Hold a track's button until the screen flashes **COPIED / TRACK n**: the track is on the clipboard, and its
button blinks. Choose another track and hold its button until **PASTED / TRACK n**: it now has the copied
track's tone and loops. The clipboard is then empty: the next hold copies again. Holding the copied track's own
button again copies it again (as it is now), pasting nothing.

What is copied: the **tone** — the engine, the sounds of all seven engines with their macros and envelopes, the filter, the
granulator, the crush, the reverb send, the octave and the generative path — and **all the loops**, every
overdub, those taken out still out, playing or stopped as they were (pasted, they start from their beginnings).
The copy is taken when you hold the button: what the track does afterwards is not pasted. The track pasted
onto keeps its **level** and **pan** (its place in the mix); whatever it had before is replaced, and its
latched notes are let go.

### The loopers

Every track has its own looper of up to **ten loops**, and they are all **free**: no tempo, no grid, no
quantising — not even between a track's own loops. Each overdub is a loop of its own, as long as you made
it (from its first note to REC), going round in its own length over the others, so they drift in and out of
phase. Each plays back exactly what was played, to 0.73 ms. A note still held when REC closes the loop is
**tied over its end**: it sounds on unbroken as the loop goes round, so a drone stays one long note rather than
being struck again each time. The keys go on playing the track over its loops,
and the loops' notes and the keys' never cut each other's notes off — but all of them share the pool's twelve
voices with the other tracks, so many loops and tracks at once take voices from each other. A track's loops hold about 500 notes between
them; nothing is lost when the FM-1 is switched off if the session is saved.

**Bringing loops in and out.** Hold **PLAY** (it does not start or stop anything held): the chosen track's
loops appear on the white keys, the first loop — the foundation — on the leftmost, each overdub on the next,
up to ten, lit while they play; the screen shows them too (in the track's colour: in; a grey frame: out).
Press a key to take its loop out — its notes stop, the others go on — or to bring it back in: it comes in
**at once, from its start**. With the loops stopped it chooses which will play: take out all but one, let go
of PLAY, press PLAY, and only that one starts; then bring the others in, one by one, as the piece unfolds.
Every loop is in when recorded. Sessions keep which loops are out.

**Moving the loops along the scale.** Hold **SHIFT** and press **OCT+** or **OCT−**: every loop of every track
moves one step of the chosen scale up or down — from C to D in C major, from C to D♯ in C minor pentatonic —
so each loop keeps its shape within the scale. The notes sounding move at once; the screen shows how far the
loops are moved (TRANSP +2, TRANSP -1, TRANSP 0), up to two octaves of the scale either way. Choosing another scale
or root while they are moved walks the new one. A note outside the scale keeps its distance from the scale note
below it. **The white keys move with them**: on a track, they play the scale moved as far as that track's loops are
(all the loops' move and its own), so what you add fits what is there — and **an overdub recorded while the
loops are moved plays back as you played it**, the others staying where they are. (USB MIDI notes are not
moved.) Sessions keep how far the loops are moved.

**One track's loops along the scale.** Hold **SHIFT**, hold a track's button, and press **OCT+** or **OCT−**:
only that track's loops move (every overdub with them), a step of the scale at a time; the screen says
TRANSP T3 +2. It adds to the move of all the loops, and that track's white keys and **latched notes** (and chords)
move with it; SHIFT + OCT alone moves every track's latched notes with all the loops. The lit latched keys move
along the keyboard by as many white keys, in the same direction (a latch that would leave the keyboard stays on
its key, its note still moved). Holding the track's button with SHIFT down never copies
it. Emptying a track's loops (REC held) takes its own move away; copying a track takes it along; sessions keep
it.

### The screen

From top to bottom: the **track number** (very large, in the track's colour), with the **battery** at the top
right (three bars green, two green, one amber, one red when low; a white bolt on a computer's USB power, when
the reading is the supply's, not the battery's -- coarse levels, not a percentage) and the track's **octave**
to its left (+1, −2; nothing at 0), VAR under them while the loops VARY; its **engine** (in white), the **family of its sound** (in the track's colour). When something changes (NEW,
OCT +1, LOOP), a black-on-white message replaces the family for a moment. While the chosen track's
looper is armed the band below the engine flashes **ARMED** in red; while it records it shows **REC**,
white on red. A message too long for the band at its usual size (EXPERIMENTAL) is drawn a size smaller.
Under it, across the screen, the track's **level meter**: a bar filling from the left with the track's colour
as it sounds (-48 to +6 dB), and a white mark at the level the SELECT knob sets (red at the left when the
track is off). At the very bottom, the track's **pan**: a block in its colour on a track from left to right,
with a tick at the middle.

KNOB 1 to 4 (filter, granulator, crush, reverb send) each show, while they turn and for a second after, the
same full-screen dial: the knob's name and the track, an arc from 7 to 5 o'clock lit in the track's colour,
a white pointer, and in large type what it does.

The **REVERB page** shows the title and its four values in rows, each with a bar; the row you just
turned is drawn black on white for a moment.

### Scales

One scale and root for the whole keyboard (and saved with the session); C major at the start. The root
sits on the first white key, in the octave from C3 to B3 (each track's OCT moves it); the scale climbs
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
| PHRYGIAN | 0 1 3 5 7 8 10 |
| LYDIAN | 0 2 4 6 7 9 11 |
| MIXOLYDIAN | 0 2 4 5 7 9 10 |
| LOCRIAN | 0 1 3 5 6 8 10 |
| MAJOR PENTA | 0 2 4 7 9 |
| BLUES | 0 3 5 6 7 10 |
| HUNGARIAN MINOR | 0 2 3 6 7 8 11 |
| DOUBLE HARMONIC | 0 1 4 5 7 8 11 (also called Byzantine) |
| PHRYGIAN DOMINANT | 0 1 4 5 7 8 10 — the fifth mode of the harmonic minor |
| JAPANESE HIRAJOSHI | 0 2 3 7 8 |
| JAPANESE IWATO | 0 1 5 6 10 |
| PELOG | 0 1 3 7 8 — Balinese *selisir*, in equal temperament |
| OCTATONIC | 0 1 3 4 6 7 9 10 — half step, whole step, alternating (eight notes: the 16 white keys span two octaves) |
| PROMETHEUS | 0 2 4 6 9 10 — Scriabin's |

The screen splits the longer names over its two lines (MIXO- LYDIAN, HUNGAR- IAN MINOR, OCTA- TONIC, PROME- THEUS)
rather than squeezing the letters together.

MIDI notes are not quantised.

### Global setup and MIDI

**SHIFT + SAVE** opens **GLOBAL SETUP**, the FM-1's own settings, kept apart from the sessions (a track
button, or SHIFT + SAVE at its top level, goes back; what changed is kept when you leave). It has one section,
**MIDI**, for now. **PRESETS** moves through the entries and **SAVE** goes into one; on a setting SAVE puts
the focus on its value (a white field), PRESETS changes it and SAVE sets it, while **SHIFT + SAVE** leaves it
as it was, or goes back a level. Each level below the top starts with **BACK**.

| Part | Settings | At the start |
| --- | --- | --- |
| **IN CHAN** | TRACK 1 to 6: the channel each track listens to (OFF, 1–16); AUTO: the channel that plays the chosen track | 1 to 6; AUTO 16 |
| **OUT CHAN** | TRACK 1 to 6: the channel each track sends on (OFF, 1–16) | 1 to 6 |
| **RECEIVE** | NOTES, CC (ON / OFF) | ON, ON |
| **SEND** | NOTES, CC (ON / OFF) | OFF, OFF |
| **PORTS** | USB IN, TRS IN (the 3.5 mm MIDI IN jack, Type A) | ON, ON |

MIDI comes in over USB and on the TRS jack. A note plays every track whose IN channel it is on (and the chosen
track on the AUTO channel), at its velocity. The CCs a track answers to, on the same channels, and sends when
SEND CC is on:

| CC | | CC | |
| --- | --- | --- | --- |
| 7 | level | 74 | filter (64 off; below, low-pass; above, high-pass) |
| 10 | pan | 91 | reverb send |
| 16–19 | the four macros of the track's sound | 92 | delay send |

With SEND NOTES on, each track sends the notes it plays — from the keys, the loops, BALLSY, BRICKLAYER and
MIDI — on its OUT channel, over USB. With SEND CC on, the values above go out when they change, whatever
changed them. There is no MIDI clock, transport or program change: Tonitor has no tempo or patterns. The
Backup page keeps the setup with the sessions and presets.

### Modulation

Each track has a small **modulation matrix**: two LFOs and four slots that move the track's own values while it
plays. The third press of **SEQ** opens the **MOD page**, in the track's colour. **PRESETS** goes through its six
entries — **LFO 1**, **LFO 2**, then **MOD A** to **MOD D** — and **KNOB 1–4** set the entry's four rows.

| Entry | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
| --- | --- | --- | --- | --- |
| **LFO 1**, **LFO 2** | **SHAPE**: SINE, TRI, SAW UP, SAW DN, SQUARE, STEPS (a new random value each cycle), DRIFT (a smooth random wander) | **RATE**: 0.01 Hz (a cycle in 100 s) to 20 Hz | **MODE**: FREE (never restarted), NOTE (from the start at each note), ONCE (one cycle from each note, then held: an envelope) | **FADE**: off, or in over up to 10 s from each note |
| **MOD A** – **MOD D** | **FROM**: OFF, LFO 1, LFO 2, VELOCITY (softer notes less), KEY (the note, around C4), RANDOM (a new value each note) | **TO**: the sound's four macros (by their names), FILTER, GRAINS, CRUSH, DRIVE, REVERB, DELAY, LEVEL, PAN, LFO 1 RATE, LFO 2 RATE | **AMOUNT**: −100 to +100 of the destination's whole swing | — (the row **NOW** shows what the slot does at the moment) |

The modulation moves the values the track already has — what the knobs set, or the macros — around where they
are, and the whole track follows (every voice of it). It moves a value once a block, so it costs next to nothing.
Sessions keep each track's modulation, and copying a track copies it.

**SHIFT + SEQ** throws the chosen track's modulation anew (MOD RANDOM): both LFOs (any shape, rate, mode and fade)
and two to four slots, from any source to any destination, by amounts that may go far. It takes risks on
purpose; press it again for another, or set what you like on the MOD page.

**HOME** draws a modulation with some sounds, by engine and family: often with the slow, sustained families of
FM, HIVE, BAYAREA and DRUID (drones, pads, washes, bowed and swelling sounds) — a slow LFO on the tone, sometimes a
fade in, sometimes a second, slower one wandering in the stereo field or the reverb; now and then a struck or
plucked sound gets its brightness on the velocity; never ANIMA or CALIGULA, whose sounds already move by
themselves. A new sound with none clears the track's modulation; a preset loaded draws the same modulation again.

### Presets

Each engine has **64 presets**, P1 to P64, for the sounds you want to keep and bring back on any track.
Hold **SHIFT** and press **ARP**: the PRESETS page of the chosen track's engine opens, four slots a page.
**PRESETS** turns through them; a slot holding a sound shows its number in red and the sound's name, an
empty one EMPTY in grey.

- **ARP held** (or SAVE held) saves the track's sound into the slot chosen. Over a sound already there it asks
  **OVERWRITE?** first, on **NO** until PRESETS turns it to YES; ARP (or SAVE) answers.
- **ARP** (or SAVE) pressed loads the slot chosen into the track (which crossfades to it, as to a new sound) and
  goes back to playing.

A preset keeps what draws the sound again, exactly — its seed, its path and the generation of the
generators that drew it — and its level: all 256 take one sector of flash, beside the sessions. Saving
erases flash, which silences the audio for a moment, as a session save does.

### Sessions

The FM-1 always starts with no session loaded. There are 64 slots, numbered 1–64.

A session stores in full the sound of the engine each track is on, and the others as the seed that
made them and their level (the same sounds, drawn again at once), so a session usually takes one 4 KB
sector of flash. A session is the whole machine: every track's engine and its seven sounds, its path, octave, level, pan,
reverb send, filter, granulator (and the character it was drawn with), crush, distortion, envelopes, latched notes and loops (playing or stopped, each overdub in or out); the scale and root, the reverb, and
the track chosen. They are kept in the
FM-1's flash, in its user data area (**saving overwrites any projects, presets or samples another firmware
left there**): dozens of typical sessions, at least ten even with six full loops.
A session also keeps the seeds its sounds were made from, with the path and the *generation* of the
generators that drew them: every generation is kept exactly as it was (a test checks thousands of sounds of
each), so sessions and presets saved under an older Tonitor load with the same sounds after an update that
brings new generators. Erasing flash silences the audio for a moment: a save
uses blank flash when there is some, and the FM-1 blanks free flash by itself while nothing sounds.


**Backing up.** The [backup page](https://nzimas.github.io/tonitor/backup.html) (desktop Chrome, Edge or Opera,
over USB MIDI; Tonitor 0.47 or later) copies every session and both preset tables from the FM-1 to a file on the
computer, and writes them back from that file — before trying another firmware, which may erase or overwrite
them, and after reinstalling Tonitor. Each record is copied whole and checked (its head and body CRCs) before it
replaces the one with the same number; the others on the FM-1 are kept. The FM-1 shows RESTORED and the number as
each one arrives.
### The granulator

Each track keeps the last half second of its own sound (at 22 kHz, 8-bit mu-law: a little grit is part of
it) and plays grains out of it, after its filter. Each track's granulator has a **character of its own**,
drawn at random when the FM-1 starts and every time a session is loaded: the intervals its grains are
pitched to, how often they reverse, stutter, drop out or freeze, the shape of its scattered grains, how far
its feedback and darkening go, how it drifts. The knob's position is saved with the session; the character
is drawn anew. When the track's notes end, its grains ring on for about two seconds.

### The distortion

Each track has a distortion, set with **SHIFT + KNOB 3** (0–100; 0 is off). It comes after the track's filter,
granulator, crush and limiter, and before its level, pan and sends, so the delay and the reverb receive the
distorted sound.

- Two gain stages in series: the first symmetric (odd harmonics), the second asymmetric (even harmonics).
  The amount sets the drive into the first stage (×0.5 to ×32, up to +36 dB) and the second stage's gain and
  bias.
- A bias in proportion to the input's own level is added before the first stage, so the clipped wave stays
  asymmetric and keeps its even harmonics at any drive. At low amounts the second harmonic dominates; higher
  amounts add more harmonics (on a sine, up to 14 of the first 15 above −60 dB).
- The highs are raised 6 dB before the stages and lowered 6 dB after, so the mids and highs distort more than
  the bass. The stages run at twice the sample rate to reduce aliasing. A low-pass after them goes from 16 kHz
  down to 6.5 kHz as the amount goes up.
- The output level is matched to the dry signal's RMS level (measured over about 0.2 s), so turning the
  amount up changes the drive and the harmonics, not the loudness.

Stored in sessions and copied with the track.

### The delay

A master delay, mono, placed before the reverb: each track's delay send (SHIFT + KNOB 4) feeds it, its echoes
go to both sides of the master, and half of their level also goes into the reverb, so the echoes are
reverberated. Its line holds 2 seconds at 22.05 kHz in 8 bits (μ-law, as the granulator's), so the repeats are
slightly dark and grainy.

| Parameter | |
| --- | --- |
| **TIME** | 20 ms to 2 s, on a logarithmic scale (shown in ms). Turning it glides the delay over about 0.1 s, so the repeats bend in pitch rather than click |
| **FEEDBACK** | how much of each repeat comes back: 0 (one repeat) to 0.95 |
| **DAMP** | a low-pass in the feedback path: each repeat darker than the one before |
| **MIX** | the level of the echoes |

The delay's parameters and every track's send are kept in sessions; copying a track copies its send.

### The reverb

A plate after Jon Dattorro (1997): predelay, input diffusion, and a figure-of-eight tank whose two
allpasses sweep slowly out of step, so the tail stays in motion and never rings. Each track sends to it
after its level; the reverb's return is added to the mix before the master.

| | | at the start |
| --- | --- | --- |
| **SIZE** | the room: every delay 0.5–1.25 times Dattorro's (turning it bends the tail, gently) | 70 |
| **DECAY** | the time to fall 60 dB, 0.4 s to about 25 s, whatever the size | 65 (about 3.5 s) |
| **DAMP** | how fast the highs die in the tail | 40 |
| **MIX** | the reverb's return level, ×0 to ×4 | 50 (×1) |

Track colours: 1 yellow, 2 cyan, 3 pink, 4 green, 5 orange, 6 lilac.

### USB audio

Connected to a computer (or a phone or tablet that takes USB audio), the FM-1 is also a **USB audio input**:
choose "FM-1 Tonitor" as the input in the recording program. It carries the master output, stereo, 16-bit, at
**48 kHz** by default (resampled from the 44.1 kHz the FM-1 renders at) or **44.1 kHz** (as rendered, a little
less CPU), as the recording program or the computer's audio settings choose. It is a class-compliant device
(macOS, Windows, Linux, and phones and tablets that take USB audio). The stream is taken before the **MASTER** knob,
so the knob sets the headphones' level and not the recording's. No driver is needed; MIDI goes on over the same
cable.

## The engines

### FM — six operators, DX7 style

The classic six-operator FM (msfa, the core of Dexed: 32 algorithms, rate/level envelopes, ratio or
fixed frequencies, feedback, LFO, pitch envelope). HOME draws a patch from one of twenty-five families of
sound objects (since 0.22 with their operators on the ratios they were designed with — before, a slip in the
generator put nearly every ratio near an even multiple and the patches two octaves above the key; sounds
saved then come back as they were):

| Family | After |
| --- | --- |
| **METAL** | bells and gongs: many carriers on inharmonic ratios, the high partials dying first |
| **DRONE** | spectral drones: near-harmonic pairs detuned to beat, an index wandering over minutes |
| **BRUIT** | noises: feedback and high ratios scattered by a sample-and-hold LFO |
| **IMPACT** | struck bodies of several kinds: thuds, wooden knocks, metal hits, drum skins (membrane-like partials), debris (heavy feedback, fixed-frequency modulators); the modulators dying before the carriers; the octave sometimes moved; the pitch falling to the note on about one sound in ten |
| **ITERATIF** | iterative objects: a sound pulsed by its own LFO, square or random |
| **GLISSE** | glissandi: a pitch envelope travelling octaves, slowly (from 0.56 drawn as part of WARP) |
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
| **PLUCK** | plucked strings, bright or dark: the modulators die at once (the attack's brightness), the carriers die away, higher strings quicker |
| **MALLET** | struck bars — marimba, vibraphone, kalimba: a high modulator's knock, gone at once, the bar ringing on |
| **WARP** | anything: ratios anywhere, partials nailed to frequencies, the pitch bent about by its envelope; from 0.56 now and then GLISSE's long glide (sometimes with a siren) or RUBBER's bounce, or both, and RUBBER's plain ratios |
| **GLITCH** | broken, mechanical sounds of several kinds: the level chopped by a square or random LFO, a crunch of heavy feedback with fixed-frequency modulators, the pitch thrown between steps by its envelope, a slow uneven wobble, and now and then a gentle sample-and-hold on the pitch |
| **RUBBER** | low ratios and the pitch bouncing in at every note: up past it, back under, onto it (from 0.56 drawn as part of WARP) |
| **SHAPER** | a carrier nailed to 1–19 Hz under modulators that follow the key: it works as a wave shaper, the tone rolling between odd and even harmonics a few times a second, at the same pace up and down the keyboard |
| **LADDER** | four to six carriers on a ladder of ratios (near unison, an octave stack, uneven steps), their envelopes staggered: the partials enter, or die, one by one |
| **REVERSE** | a sound that swells for as long as the key is held and stops when it is let go: tape run backwards |
| **CREEP** | a held note leaving its pitch slowly, over seconds to tens of seconds |
| **CLANG** | bells as the DX7's programmers made them: carriers turned by modulators on ratios off their harmonics, the index dying with the note |

From 0.56 (generation 6) GLISSE and RUBBER are no longer drawn as families of their own: WARP has their glides
and bounces (SHIFT + PRESETS steps over them). SHAPER, LADDER, REVERSE, CREEP and CLANG were learned from a
collection of 9,606 DX7 voices (the idioms their programmers used that the other families do not make; each
patch is a new one of that kind, never one of the collection's voices), after the SMK-37 port of Tonitor.

Each patch is then shaped with care: modulators on high ratios turned down (their sidebands alias and
scream), the brightness scaled down the keyboard so high notes are not shrill nor low ones muddy, rate
scaling on the carriers. HOME draws several patches, measures each, and keeps one whose brightness suits
the track's path (darker on AMBIENT, brighter on EXPERIMENTAL) and that is not of the family the track had.

Since 0.29 (generation 3) a patch may also be made plucked whatever its family, have a modulator moved to
another ratio, or an operator detuned against the rest; the last five families are drawn only from then.
About half the patches are then bent once more, gently (an operator detuned to beat, an envelope turned
over to grow as it holds, the LFO waiting and then moving the timbre, an octave down, more feedback, a
modulator's ratio moved to a neighbour, a partial nailed to a frequency).
Every patch is rendered offline as it is made and given the gain that brings it to one loudness.

### ANIMA — a lo-fi synthesizer played by a simulated hand

ANIMA is the lo-fi engine: each voice runs at half the sample rate (22.05 kHz, so nothing above 11 kHz),
its samples drawn out to the full rate by straight lines between them. Its pitches, rates and filters are
scaled to sound where they would at the full rate; what goes is the top octave of air — and half of the
chip's time, so more voices sound at once.

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
eight sliders, drawn — often far from their middle. Twenty-two kinds: **HAZE**, **VOYAGE**, **GLASS**,
**BELLS**, **GOLDEN** (Anima's own) and **BREATH**, **GRIT**, **SWARM**, **TIDE** (waves of noise over a low
drone), **CHOIR** (vowels, a formant the hand moves), **SHARDS** (glass grains scattered and crushed),
**BOWL** (a beating inharmonic hum in a ringing comb), **DUST** (sparse crackle over a dark drone),
**MACHINE** (a pulsing, folded mechanism), and since 0.29 **PLUCK** (a plucked string: a burst through the
comb tuned to the note, dying away), **REED** (nasal, through a formant the hand moves, a breath in it),
**FOG** (a stack under fog, a slow phaser drifting through it), **ORGAN** (drawbars, a rotary tremolo, a
little drive), **BITS** (pulses in few bits: the rate crushed and dragged about, folded), **DRIFT** (two saws
drifting against each other, a chorus the hand moves), **MURMUR** (a dark cloud of noisy grains) and
**HOWL** (noise through a resonance so sharp it whistles, swept by the hand). Every kind draws wider since
0.29 (generation 3): any of the 24 waveforms, wider intervals and ratios, a detuning, now and then a plucked
envelope or a slow swell, another way for the hand to move, a pulsing tremolo, a crushed rate or a sharp
resonance. The filter follows the keyboard, so low notes are not muffled
nor high ones shrill; the second oscillator's interval suits the path (consonant on AMBIENT, anything on
EXPERIMENTAL); and an AMBIENT sound nearly always has a resonant space to ring in.

In Vascular a design plays up to six fingers, each a voice; here a track has one voice, so the hand has
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

A CALIGULA track is **played like the others**: the white keys play the scale's notes, and it is
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


### RUSSOLO — noise, logic and broken buffers

The fourth engine re-makes the techniques of Robert Heel's Noisferatu (GPL-3.0) in fixed point, for the
keys. It runs at a third of the sample rate (about 14.7 kHz) in 10-bit values, each held until the next —
the grit of a small DAC — then reduces its bits and holds its samples further as the sound says.

A RUSSOLO track is **played like the others** and is **silent until a key is down**: the key opens an
envelope and sets the pitch (the oscillators' frequency, the speed a buffer is read at, the rate noise is
sampled at); a second key on top moves it there at once; letting go of the last key lets it fade. Like
CALIGULA it is one organism a track, outside the pool of voices. **HOME** draws one of seventeen kinds:

| Kind | |
| --- | --- |
| **HOLD** | sample-and-hold noise, its rate from the key |
| **DUST** | random clicks through a low-pass that follows the key |
| **GATES** | noise opened and closed by two triangles, one wandering off the other |
| **CRACKLE** | sparse clicks ringing on short pitched decays, over hiss |
| **NOR**, **XOR** | two oscillators combined bit by bit |
| **RING** | two oscillators multiplied |
| **CASCADE** | squares modulating squares, a sub-octave below |
| **CLANG** | two triangles at an inharmonic ratio, XOR'd; the ratio redrawn when two clocks meet |
| **CHUNKS** | a generated buffer of noise, saw and silent chunks, read at the key's speed and rebuilt in parts |
| **JUMP** | the buffer read with jumps at random |
| **WANDER** | a window wandering through the buffer |
| **BEND** | an address bit of the buffer forced to 0 or 1, flipped or held; which and how change on a clock |
| **QUAD** | four such bendings at once, each switched on and off by chance |
| **BLIPS** | decaying triangles fired by chance on the key's harmonics |
| **WALK** | a random walk over a scale's steps from the key |
| **CHORD** | triangles on the key (minor, major, open fifths or suspended), slowly beating and swelling |

Each kind has two parameters of its own, the first two macros; **CRUSH** and **CHAOS** are the other two.
Its level is measured when it is drawn, and never raised so far that its loudest click could clip.

### DRUID — strings and resonant bodies

The fifth engine is a fixed-point port of the resonators of **Mutable Instruments Rings** by Émilie Gillet
(MIT licence). It plays from the shared pool of voices, like FM and ANIMA. Each kind is one of Rings'
resonators and an exciter:

| Resonator | |
| --- | --- |
| **String** | a Karplus-Strong string: a delay line read with Hermite interpolation, damped by a short FIR filter (brightness) and a state-variable low-pass, its decay set as a time to −60 dB. Dispersion (STRUCTURE) stretches the partials with an all-pass stage, adds noise to the period near the top, or below a quarter of the range bends the bridge so the string buzzes. The output is picked up at the position along the string |
| **Sympathetic strings** | a string and three more tuned to intervals of its note (octaves, fifths, a twelfth, chosen by STRUCTURE), driven only by the first string's sound; their pickup positions drift slowly |
| **Modal** | up to 16 two-pole resonators at the partials of a bar or plate, stretched or squeezed by STRUCTURE; each partial's level depends on the position |

| Exciter | |
| --- | --- |
| **Pluck** | Rings' plucker: a burst of noise as long as the position's share of the period, through a comb filter and a low-pass |
| **Strike** | a short pulse, longer and softer as the brightness goes down |
| **Bow** | noise for as long as the key is held, swelling in and dying away when released |
| **Dust** | random impacts for as long as the key is held |

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

AMBIENT favours BOWED, HARP, DRONE, BELL and BOWL, with long decays that carry on after the key is released;
EXPERIMENTAL favours RATTLE and PLATE, with shorter decays, faster dust, extreme positions and a stronger damping
when the key is released. Releasing a key damps the sound by an amount set per sound, from ringing on to stopping
short. Each voice's delay lines are 16-bit and use the memory an ANIMA voice would use: a string reaches
down to about 29 Hz, and sympathetic strings that would not fit their shorter lines are tuned an octave up.

### BAYAREA — a West Coast voice

The sixth engine is a fixed-point port of the bayarea opcode of Vascular, contributed by its author. It plays
from the shared pool of voices, like FM, ANIMA and DRUID. Each voice has:

- an oscillator that morphs from sine through triangle and square to saw (SHAPE), its frequency modulated
  linearly, through zero, by two sines at ratios of it (0.66 and 3.3 by default, other ratios on some sounds),
  each ratio wandering slightly and slowly;
- a wavefolder (FOLDS), reflecting the wave back into range;
- a 12 kHz smoothing low-pass and a trace of noise;
- either a **low-pass gate**, where a resonant low-pass and an amplifier are struck together by one-shot
  envelopes (an attack of 2–3 ms and a decay), or a **sustaining envelope** over two resonant low-passes (an
  attack while the key is held, a release when it is let go). The filters sweep from 300 Hz to the sound's peak
  frequency, higher for louder notes;
- a saturating output stage (tanh, then a soft clip) with its own drive;
- a bank of three resonators (508, 270 and 1153 Hz, ringing for 1–1.4 s), mixed in by the sound's amount;
- an LFO (triangle, ramp, square or random steps) that moves the shape, the folds, the two FM depths, the
  attack, the peak, the decay and the resonator mix, by amounts drawn with the sound.

Changed from the opcode: the square and saw are band-limited with polyBLEP instead of being summed from 44
partials (too costly for this chip), and the opcode's reverb is replaced by the track's sends to the master
delay and reverb, keeping only its resonator bank.

| Kind | Envelope | |
| --- | --- | --- |
| **BONGO** | gate | low peak, short decay, little folding |
| **PLUCK** | gate | brighter, square to saw, short to medium decay |
| **CHIME** | gate | the upper modulator deep, long decay, resonators |
| **FOLDED** | gate | heavy folding |
| **SWELL** | sustain | slow attack (0.3–3 s), long release |
| **DRONE** | sustain | slower attack (0.5–4 s), the lower modulator deep, long release |

AMBIENT favours BONGO, CHIME, SWELL and DRONE; EXPERIMENTAL favours PLUCK and FOLDED, with more drive and folding
and other FM ratios more often.

### HIVE — a swarm of oscillators

The seventh engine is a port of the SWARMER machine of DigiRust, contributed by its author, extended here with
inharmonic and textural kinds. It plays from the shared pool of voices. Each voice has seven oscillators, and each
sound draws, for every one of them:

- its place relative to the note: the note itself, an octave or a fifth, a step of a cluster (quarter-tones,
  seconds), or an inharmonic partial (the modes of a free bar, a bell's partials, a stretched harmonic series, or
  ratios at random);
- its share of the spread that DETUNE sets (symmetric, clustered, to one side, at random, or in pairs);
- its wave, a position along sine, triangle, saw and square (neighbouring waves crossfaded);
- its level, and which of three LFOs moves its pitch and its level, and at what phase. The LFOs' rates are drawn
  too, from about 0.02 Hz to a few Hz depending on the kind; MOVEMENT scales their depth.

The seventh oscillator is the main one, set against the other six by MIX. Each voice also has a slow noise on
every oscillator's pitch; how closely it follows the key (fully for the melodic kinds, partly or hardly at all for
the textural ones); noise; a ring modulation of the first oscillator by the main one; a 4-pole filter (two
state-variable filters, low-pass, band-pass or high-pass) whose cutoff follows half the key's movement and is swept
by its own envelope and by the first LFO; an ADSR envelope on the level; and a drive (tanh).

Changed from DigiRust: the waves are computed as they play (the sine from its table, the triangle directly, the
saw and square band-limited with polyBLEP) instead of read from band-limited tables, which would not fit in RAM
here; the filter, envelopes and drive are Tonitor's own.

| Kind | |
| --- | --- |
| **SUB** | the main oscillator one or two octaves down, short attack; sometimes an octave up in the swarm |
| **LEAD** | brighter, short attack, resonant |
| **STAB** | short attack and decay, little or no sustain, a strong filter sweep; sometimes a fifth and an octave in the swarm |
| **PAD** | slow attack (0.2–3 s), wide detune, long release; sometimes octaves in the swarm |
| **DRONE** | slower attack, full sustain, long release (1.5–6 s); sometimes fifths and octaves stacked |
| **WASH** | the slowest attack (0.8–5 s), the longest release (3–9 s), often noise and a moving filter |
| **CLUSTER** | the oscillators at quarter-tone and semitone steps around the note, slowly beating |
| **METAL** | inharmonic partials (bar, bell, stretched or random), struck: short attack, a long decay, often a ring modulation |
| **SPECTRA** | inharmonic partials whose levels and pitches turn slowly (or shimmer), the key hardly followed: a spectrum evolving over seconds |
| **GROUND** | two octaves down, a dense inharmonic cluster with noise, a slowly moving filter, driven: rumble |
| **AIR** | filtered noise and one to seven high, drifting whistles, band-pass or high-pass, the filter swept: wind |
| **TIDES** | the oscillators gliding apart and back by up to five semitones on slow LFOs, sometimes on inharmonic partials |

CLUSTER, METAL, SPECTRA, GROUND, AIR and TIDES are meant for non-melodic, textural use. On GROUND and AIR the third
macro is **NOISE** (the noise's level) instead of MIX.

AMBIENT favours PAD, DRONE, WASH, SPECTRA and TIDES, and makes the LFOs slower and the sound darker, the attacks
longer; EXPERIMENTAL favours STAB, CLUSTER, METAL, GROUND and AIR, and adds more noise to the pitches, more drive
and, sometimes, a ring modulation.

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
