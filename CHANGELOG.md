# Changelog

## Build 66

Changed
- The splash screen shows the Tonitor logo: the ring mark in the middle, the lettering across the width under it,
  the build number below. It is drawn from the logo's shapes (docs/assets/logo-dark.svg) at start, with smoothed
  edges, rather than kept as an image.

## Build 65

Added
- SHIFT + black key 1 draws a new scale and root at random, as at start. The SCALE page shows them for 1.5 s,
  then the page that was up returns; the loops follow the new scale. Black key 1 alone still sets the AMBIENT path.

## Build 64

Fixed
- A sound chosen by family (SHIFT + PRESETS, HOME's SAME FAMILY) on FM, ANIMA, CALIGULA, RUSSOLO, DRUID, BAYAREA
  or HIVE came back as another sound, often of another family, when kept as its seed: from a preset, from a
  session for the engines the track was not playing, and when measured on first being played. Each engine now
  draws the family from the seed whether one is given or not (as OBRADOR does), so the seed and the family make
  the same sound again. Sounds drawn without a family are unchanged.
- Sessions (format 26) keep the family with the seed of every engine's sound, not only OBRADOR's; presets use
  the family they already kept. Earlier sessions load as before. A preset saved by an earlier build from a sound
  chosen by family now loads in that family, but not as the same sound.

## Build 63

Added
- OBRADOR, an eighth engine: an analog polysynth after the Oberheim SEM and the two-voice Oberheims. Two
  band-limited oscillators a voice (saw, pulse; detune, intervals, hard sync, cross-modulation), a second voice card detuned against the first in the lush kinds, the SEM's 12 dB
  filter morphing low-pass, notch, high-pass (24 dB and band-pass too), filter and amplifier envelopes on analog
  curves, a delayed LFO, each card's own offsets and drift. Ten kinds: STRINGS, BRASS, PWM PAD, BASS, SYNC, NOTCH,
  METAL, DRONE, KEYS, SWELL. Macros: CUTOFF, MORPH, RESONANCE, DETUNE.
- Presets are named by their family and numbered within it, in the order of the slots (BRASS 1, BRASS 2,
  STRINGS 1). The PRESETS page shows the engine's name alone over the slots ("CALIGULA PRESETS" was wider than the
  screen), each name in the largest size it fits whole.
- Sessions (format 25) keep each track's OBRADOR sound; earlier sessions load with a new one. A sound kept as its
  seed keeps its family too, so one chosen with SHIFT + PRESETS comes back the same from a preset or a session.

## Build 62

Added
- GLOBAL SETUP has an AUDIO section: USB, STEREO (the master, as before) or TRACKS (tracks 1-6, each on its own
  mono channel, after its level, before its pan and the master delay and reverb), to record the tracks apart.
  Changed, the FM-1 leaves the USB a moment and comes back as the new device. Settings kept by earlier builds load
  as they were, USB at STEREO.

Changed
- USB audio: the next packet is queued within 0.1 ms of the last (the endpoint looked to at 10 kHz, 2 kHz before).
  Six channels take about 0.55 ms on the cable: looked to every 0.5 ms, a frame now and then went without a packet
  and macOS reset the device's clock, dropping audio every 1.25 s. The ring is kept fuller (3.6 .. 4.7 ms ahead at
  44.1 kHz), and the 48 kHz resampler and its taps run from RAM.

## Build 61

Fixed
- A loop's first note could be heard only in the first round. A note held from the loop's first moment to its last
  (still down when REC ended the loop, under the notes played after it) was tied over the end as a drone: never
  struck again. A plucked sound (DRUID's PLUCK) died away and stayed silent. Now a tied note that has died away
  (more than 18 dB under its loudest) is let go at the end and struck again at the start; one still sounding goes
  on as one note. The same note struck at the start and again, held, at the end is no longer taken for one note.

Changed
- At start, and in an empty session, the tracks are on six engines: FM, ANIMA, CALIGULA, HIVE, DRUID, BAYAREA
  (as on the SMK-37 Pro port); until now FM and ANIMA in turn.
- The DELAY and REVERB pages: every row's name and value in one size (it changed from row to row). The delay's
  TIME in seconds, without the unit (0.02 .. 0.99, then 1.0 .. 2.0), no longer in milliseconds against its name; FEEDBACK is
  named REPEATS, which fits that size with its value.

## Build 60

Added
- PLAY ALONG, SEQ's fourth page (after the SMK-37 Pro port): other tracks play along with the chosen one, the guide.
  REACT improvises from the guide's phrases, SHADOW follows it from behind at its own pace, ARPEGGIO breaks its
  chords into arpeggios. PRESETS chooses the mode, PLAY turns it on or off, the track buttons take tracks in or out
  (the guide lit, the tracks in a glow), KNOB 1-4 (ARPEGGIO: SELECT and ALGORITHM too) set its values.

## Build 59

Changed
- At start, and in an empty session, the scale and root are drawn at random (until now C major). The SCALE page
  (ARP) shows which.

## Build 58

Fixed
- An empty session took seconds to start (issue #5), and so did the FM-1 itself: every track drew and measured a
  sound for each of the seven engines, 42 in all, each measured by rendering seconds of it. Now only the sound each
  track plays is measured; another engine's is measured the first time the track is moved to it (a moment, then).

Changed
- The loop keys (black keys 4–8: direction and speed) work only while the chosen track's loops play. With no
  loops, or the loops stopped, they are dark and do nothing, and show nothing on the screen.

## Build 57

Added
- The keyboard shows where the scale's root is: the white keys that play the root (the one chosen on the SCALE
  page, in every octave) glow dimly, and move with the loops' transposition (SHIFT + OCT). PLAY held shows the
  loops instead.

Changed
- Versions are numbered as builds: this one is build 57, after 0.56. The changelog names the earlier versions
  the same way (0.56 is build 56).

## Build 56

Added
- FM: five families learned from a collection of DX7 voices, as on the SMK-37 port: SHAPER (a carrier at 1-19 Hz
  shaping its modulators), LADDER (carriers on a ladder of ratios, entering or dying one by one), REVERSE (a swell
  while the key is held), CREEP (the pitch leaving slowly), CLANG (bells as the DX7's programmers made them).
  Generation 6 of the generators.

Changed
- FM: GLISSE and RUBBER are no longer drawn as families of their own; WARP has their glides and bounces, now and
  then (generation 5). Sounds saved on earlier generations come back as they were (checked by the golden tests).
- USB audio offers 48 kHz first, and starts at 48 kHz, the rate most current computers and programs expect;
  44.1 kHz stays available. The FM-1 renders at 44.1 kHz: at 48 kHz it resamples (SLOOP's polyphase filter,
  uac_fir.h), which costs some CPU while a program records.
- The audio's hot code runs from RAM (after the SMK-37 port's build 31): the engines' per-sample code (FM, ANIMA,
  CALIGULA, BAYAREA, HIVE), their helpers, the delay, the reverb and the sine table. From the flash, through its
  small cache, several engines at once kept evicting each other's code. Measured on the FM-1 (a diagnostic build
  that plays notes by itself and reports the audio's load):

| | build 55 | build 56 |
|---|---|---|
| idle | 33 % | 11 % |
| six tracks, FM, one note each | 59 % | 54 % |
| six tracks, six engines, one note each | 133 % (peak 162 %) | 82 % (peak 92 %) |
| six engines, two notes each | 170 % (peak 200 %) | 118 % (peak 132 %) |

  (Over 100 % the audio misses its deadline: the overload guard lets voices go.)
- The reverb runs at 22.05 kHz, with its times and decay kept (Dattorro designed the plate at 29.8 kHz): a
  slightly darker top, half the work and half the memory. The delay's line moved out of the code RAM into the
  pool.
- The delay and reverb are skipped when nothing is sent to them and their tails have died away.
- A voice let go to make room (the budget guard, a steal) is the quietest tail, not the oldest, faded over 6 ms
  (2 ms before): fewer clicks.
- HIVE skips silent oscillators; with three HIVE voices or more sounding, also those 12 dB under the loudest.

Fixed
- Every sound ends once let go: a voice is faded out (0.19 s) when it has been 66 dB under its own peak for 0.2 s,
  when it has not fallen 1.5 dB in 2 s (DRUID's resonators could ring on, very faintly, for ever, and keep costing
  CPU), or 30 s after release.
- Latched notes are never let go by the overload guard or taken by a new note.

## Build 55

Fixed
- USB audio on macOS 12 to 15: the FM-1 now lists its audio streaming interface before its MIDI one in the audio
  control's collection (as SLOOP 2.7.2, after Felucca 1.5.1). macOS up to 15 drops the MIDI interface from that list
  as it reads it, and with MIDI listed first it stopped before the audio interface: no audio input appeared (MIDI
  still worked). macOS 26 was not affected. The device's version number changes too (bcdDevice), so a computer
  reads the description again.

Changed
- USB audio is 44.1 kHz only again (build 54's 48 kHz did not help; 44.1 kHz is the rate the FM-1 renders at, with no
  resampling).

## Build 54

Fixed
- USB audio was not offered on macOS 12 (Monterey): the FM-1 did not appear as an input there, while other FM-1
  firmwares do. The FM-1 now offers 44.1 kHz and 48 kHz, as SLOOP does; at 48 kHz it resamples (SLOOP's polyphase
  filter, uac_fir.h), which costs some CPU only while a program records at 48 kHz. 44.1 kHz is still the rate
  the FM-1 renders at, and the better one to record at.

## Build 53

Fixed
- Presets are saved where their page is opened: on the PRESETS page (SHIFT + ARP), ARP held saves the track's sound
  into the slot chosen and ARP pressed loads it (or answers OVERWRITE?). SAVE still does the same there.

Added
- A modulation matrix per track: two LFOs (SHAPE: sine, triangle, saw up, saw down, square, steps, drift; RATE
  0.01-20 Hz; MODE free, note, once; FADE in up to 10 s) and four slots, each FROM a source (LFO 1, LFO 2,
  velocity, key, a random value per note) TO a destination (the sound's four macros, filter, grains, crush,
  drive, reverb send, delay send, level, pan, either LFO's rate) by an AMOUNT (-100 to +100). It moves the
  track's own values once a block. On the MOD page, SEQ's third. Sessions and copies keep it (session format 24).
- HOME draws a modulation with some sounds: often with the slow, sustained families of FM, HIVE, BAYAREA and
  DRUID, now and then a velocity on the brightness of struck and plucked ones, never with ANIMA or CALIGULA. A
  preset loaded draws it again.

Changed
- SEQ goes through three pages, press after press: DELAY, REVERB, MOD, then back to playing. SHIFT + SEQ no
  longer opens the delay: it throws the chosen track's modulation anew (MOD RANDOM) -- both LFOs, and two to four
  slots from any source to any destination, by amounts that may go far.

## Build 52

Added
- USB audio (as BIGORNA's build 49): the FM-1 is a class-compliant USB audio input, "FM-1 Tonitor", carrying its
  master output, stereo, 16-bit, 44.1 kHz -- as it is rendered, with no resampling. It is Felucca's and SLOOP's USB
  audio, switched on, fed by the render a block at a time and served at every 1 ms USB frame, also while a half
  buffer renders. The stream is the master before the MASTER knob: the knob sets the headphone jack's level only.
  MIDI goes on as before over the same cable. The update loader is unchanged.

## Build 51

Added
- GLOBAL SETUP on SHIFT + SAVE (after BIGORNA): the FM-1's own settings, kept apart from the sessions. One section,
  MIDI, with IN CHAN (each track's channel, and AUTO for the chosen track), OUT CHAN (each track's), RECEIVE and
  SEND (notes, CC), and PORTS (USB IN, TRS IN). PRESETS moves, SAVE enters and sets, SHIFT + SAVE goes back; BACK
  heads each level. The settings are kept in flash, and the Backup page saves and restores them.
- MIDI IN on the TRS jack (after BIGORNA and MELODEE 1.4.5), beside USB. Notes go to the tracks by channel, at
  their velocity; CC 7 level, 10 pan, 16-19 the four macros, 74 the filter, 91 the reverb send, 92 the delay
  send.
- MIDI out over USB: each track's notes (from the keys, the loops, BALLSY, BRICKLAYER and MIDI) on its OUT
  channel with SEND NOTES; the CCs above when their values change, with SEND CC.
- SHIFT + PRESETS in the track's view: a new sound of the family before or after, on the track's engine (after
  BIGORNA). At the first or last family, a new sound of that family again.
- A battery meter at the top right (after BIGORNA): three bars green, two green, one amber, one red when low; a
  white bolt on a computer's USB power.
- The track's octave at the top right, left of the battery (+1, -2; nothing at 0).

Changed
- The PRESETS page moves to SHIFT + ARP (SHIFT + SAVE is GLOBAL SETUP's).
- MIDI notes no longer all play the chosen track: each track listens on its own channel (1 to 6 at the start),
  and channel 16 plays the chosen track.
- VAR moves under the octave and the battery.
- The screen's type is stored in runs (about 116 KB of flash freed, for this and what comes); the screen is the same,
  pixel for pixel.

## Build 50

Changed
- The FM-1's second core (CPU1) now renders about half the work of each audio block: the sounding voices and the
  CALIGULA and RUSSOLO organisms are dealt to the two cores by their cost, the dearest first, and CPU1 also runs
  the master delay and reverb. Each core renders into buffers of its own; CPU0 then applies each track's
  envelope, fades, filter, granulator, crush, distortion, level and sends as before. The start, hold, fault
  handling, job mailbox and supply setup come from BIGORNA, after Melodee 1.4.5 (keremimo/melodee; LICENSING.md).
- The delay and reverb run one block behind, on the sends of the block before: their returns come 0.73 ms later.
  The sound is otherwise the same with one core or two (checked by a test).
- At start-up, the stock firmware's supply setup (core rail 1.38 V). CPU1 starts only if that held and the last
  boot did not fail.
- A fault, stall or misread on CPU1 stops it until the FM-1 is switched off: CPU0 renders everything from then
  on and the sound goes on. 1 CORE shows on the screen when the FM-1 starts on one core or falls back to one.
- The overload guard counts a new voice at half its cost while CPU1 is on.
- ANIMA keeps 13 wavetables in RAM (15 before), to make room for CPU1's stacks and the two cores' buffers; a
  table missed is copied in after both cores have finished the block.

## Build 49

Changed
- BALLSY and BRICKLAYER wait for PLAY when turned on. On their page PLAY starts and stops them (PLAY or STOP on
  the screen) and blinks with their key while they play; on every other page PLAY is the looper's, lit
  steadily while the loops play. BRICKLAYER starts from its first step; BALLSY's balls stop where they are and
  move on from there. The looper works beside either, and records only notes played on the keys: BALLSY's and
  BRICKLAYER's notes are no longer recorded.
- SHIFT + PLAY starts or stops every loop and every sequencer that is on, together (ALL PLAYING, ALL STOPPED).
- Session format 23: whether each sequencer plays. Older sessions load with their sequencers playing.

## Build 48

Added
- HIVE, a seventh engine, ported from the SWARMER machine of DigiRust and extended: per voice, seven
  oscillators, each sound drawing for each one its place (the note, an octave or fifth, a cluster step, or an
  inharmonic partial), its share of the detune spread, its wave (sine, triangle, saw, square), its level and the
  LFO that moves it; slow noise on the pitches; noise; ring modulation; a 4-pole low-, band- or high-pass filter
  with its own envelope and an LFO; an ADSR; a drive. Twelve kinds: SUB, LEAD, STAB, PAD, DRONE, WASH (after
  DigiRust's), and CLUSTER, METAL, SPECTRA, GROUND, AIR, TIDES (inharmonic and textural, for non-melodic use).
  Macros: DETUNE, MOVEMENT, MIX (NOISE on GROUND and AIR), CUTOFF. It is the last position of the PRESETS knob,
  after BAYAREA, and has its own preset table.
- 14 scales, after the nine there were (23 in all): phrygian, lydian, mixolydian, locrian, major pentatonic,
  blues, Hungarian minor, double harmonic, phrygian dominant, hirajoshi, iwato, pelog (selisir, equal
  temperament), octatonic (half–whole, eight notes) and Prometheus. Sessions keep their scale. Long names are
  split over the screen's two lines.

Changed
- Session format 22 (a seventh sound per track). Older sessions load; their tracks get a new HIVE sound.

## Build 47

Changed
- Generator generation 4 for new sounds. FM IMPACT: six kinds (thud, wooden knock, metal hit, drum skin, debris,
  falling pitch); the falling pitch, previously on every IMPACT sound, now about one in ten; carrier ratios and
  the octave vary more. FM GLITCH: five kinds (level chopped by a square or random LFO, feedback crunch with
  fixed-frequency modulators, pitch steps from the pitch envelope, slow wobble, sample-and-hold pitch); the
  sample-and-hold pitch stepping, previously on every GLITCH sound, now about one in seven and shallower.
  The other families and engines are unchanged.

Added
- Backup page on the website (backup.html): saves every session and both preset tables from the FM-1 to a JSON
  file and restores them, over Web MIDI. The firmware answers new SysEx commands in the installer's framing
  (0x50 list, 0x51 read, 0x52/0x53/0x54 write and commit, 256-byte pieces). Restored records are written whole,
  with a new sequence number, after their head and body CRCs are checked; a record that does not arrive intact
  is not written. The screen shows RESTORED and the number.

Compatibility
- Sounds stored as seeds (sessions, presets) regenerate identically for generations 0–3 (checked by a test).

## Build 46

Changed
- One sequencer per track: BALLSY and BRICKLAYER are mutually exclusive. Starting one while the other runs on
  the same track shows NOT ALLOWED and changes nothing. A session that has both on for a track loads with
  BALLSY on and BRICKLAYER off. The looper runs beside either.

Fixed
- The FM-1 showed up as "Felucca" in MIDI applications on computers that had seen Felucca before: they kept the
  name stored for the same USB IDs (1209:0001). Tonitor now uses its own USB product ID (1209:0003), so it shows
  up as "FM-1 Tonitor". The update loader is unchanged (1209:0002).

## Build 45

Added
- BRICKLAYER, a step sequencer per track on SHIFT + the envelope key (the key blinks while it runs; holding the
  key 0.6 s shows or hides its page; a short press is still the ENVELOPE page, now acting on release). On its
  page REC is the note entry (lit while on): white keys add notes at the end; SHIFT + a white key removes that
  note; SHIFT + REC removes the last; REC held 1 s empties the sequence. Notes play in order, spread evenly over
  the length (Euclidean, first note on the first step) and are stored as scale steps, following the loop
  transposition, scale and root. KNOB 1: length (number of notes to 32 steps). KNOB 2: rate (0.5–16 steps a
  second). KNOB 3: harmony (diads and triads in the scale). KNOB 4: variation (each note's level, down to
  −12 dB, and length, ×¼ to ×2, spread at random; pace from ×0.5 to ×2, stops, rushes, stutters, beat repeats,
  echoes, ghost notes). On its page the loop direction and speed keys (REVERSE, PENDULUM, RANDOM, HALF SPEED,
  DOUBLE SPEED) are its own, lit while on. Notes last 70 % of a step. The screen shows the steps as cells, eight
  to a row, the step playing lit. Stored in sessions (format 20) and copied with the track.

Removed
- SHIFT + the envelope key no longer resets the track's envelope to AS DRAWN; HOME and loading a preset still do.

## Build 44

Fixed
- VARY switched on while a loop's notes were sounding (long notes, especially ones held over the loop's end) left
  those notes sounding indefinitely: under VARY the recorded note-offs were ignored. A note struck as recorded
  before VARY came on is now released at its recorded end; notes struck by VARY keep their own random lengths.

## Build 43

Added
- VARY also modulates the loops' pace: a slow sine undulation (0.07–0.35 Hz, depth ±15–40 %, redrawn every few
  seconds), and on average every ~6 s one event: halt (0.15–1 s, abrupt stop and resume), burst (2–3× for
  0.2–0.8 s), grind (slowing to a stop over 0.4–1.5 s, held up to 0.4 s, then back at once) or swoop (rising to
  2–2.5× over 0.8–2 s, then 0.4× for 0.3 s). Note lengths stay in real time. Not applied while REC records.

## Build 42

Added
- A sixth engine, BAYAREA: a West Coast voice, ported to fixed point from the bayarea opcode of Vascular. Per
  voice: an oscillator morphing sine → triangle → square → saw with two linear (through-zero) FM modulators
  (ratios 0.66 and 3.3 by default, each wandering slowly), a wavefolder, a 12 kHz Butterworth smoothing filter,
  then a low-pass gate (resonant low-pass and amplifier struck by one-shot envelopes) or a sustaining envelope
  over two resonant low-passes, a saturating output stage, a bank of three resonators, and an LFO on eight
  destinations. Six kinds: BONGO, PLUCK, CHIME, FOLDED (gate); SWELL, DRONE (sustain). Macros: SHAPE, FOLDS, FM,
  DECAY. Square and saw band-limited with polyBLEP (the opcode sums 44 partials); the opcode's reverb is left to
  the track's sends, its resonator bank kept. Polyphonic from the shared voice pool; each voice uses the memory
  of an ANIMA voice. Sixth position on the PRESETS knob.

- VARY (SHIFT + REC) also changes or adds notes, each at fixed odds per loop note: drop (5 %), 5–40 ms late
  (25 %), octave up or down (8 %), a note not recorded — ±1–2 scale steps or a fifth, replacing or added (8 %),
  a grace note a step away 50–90 ms before (4 %), ratchet of 2–4 hits 50–110 ms apart (4 %), a third or fifth
  above added (10 %), 1–4 echoes 120–400 ms apart at 60 % level each, sometimes climbing or falling a scale step
  each (15 %), chord notes strummed 20–60 ms apart (12 %). All notes stay in the scale; up to 32 notes queued
  per track.

Changed
- Sessions: format 19 (a sixth sound per track; macros and envelopes for six engines). Older sessions load
  with a new BAYAREA sound drawn for each track.
- Presets: BAYAREA's 64 slots are kept in a second preset record; the first is unchanged.

## Build 41

Added
- BALLSY, a physics sequencer per track: SHIFT + EDIT turns it on (its page shown) or off; EDIT blinks while it
  runs. Holding EDIT (0.6 s) while it runs shows or hides its page; with the page hidden the balls keep playing
  and the knobs, screen and other pages work as usual. A short press of EDIT is still EDIT (now acting on
  release). One to eight balls bounce in a box; each wall hit plays a note on the track as a key would (scale,
  loop transposition, chord mode, recorded by the looper), with a random length (50 ms – 1.5 s) and a random
  level (−10 dB to full; FM, ANIMA and DRUID voices; looped notes replay at full level). Each bounce changes the
  angle at random by up to 12°, kept 15°–75° from the walls. Each wall holds the scale's steps, one segment a
  step; pitch rises clockwise, one octave per wall: bottom −1 (right to left), left 0 (upwards), top +1 (left to
  right), right +2 (downwards), relative to the keyboard's octave. KNOB 1: number of balls (1–8). KNOB 2: speed
  (0.08–2.4 box widths per second). KNOB 3: the four walls' octaves, one octave per click (bottom wall from −3
  to +2). KNOB 4: instability (0–100): per ball, two smoothed random values scale the speed (up to ×8 or ÷8)
  and turn the direction (up to half a turn a second), plus occasional gusts of up to 60°. Physics runs in the
  main loop on elapsed time in steps of at most 4 ms. Stored in sessions (format 18) and copied with the track.
- VARY per track on SHIFT + REC (VAR shown at the top right of the screen): the track's loops play each note at
  a random level (−10 dB to full, FM, ANIMA and DRUID voices) for a random length (50 ms – 1.5 s) instead of the
  recorded length; notes tied over the loop's end are struck again each time round. All overdubs of the track.
  Stored in sessions (in the loop state) and copied with the track; reset when the track's loops are emptied.
  SHIFT + REC previously did the same as REC.
- Note level: EV_ON events carry a level; pool voices (FM, ANIMA, DRUID) are scaled by it. Keys and loops play
  at full level.

## Build 40

Added
- A fifth engine, DRUID, after the resonators of Mutable Instruments Rings (Émilie Gillet, MIT licence),
  ported to fixed point. Three resonators: a Karplus-Strong string (Hermite-interpolated delay, all-pass
  dispersion, delay noise, a buzzing bridge, FIR and state-variable damping with the decay set as a time to
  −60 dB, a pickup comb at the position), sympathetic strings (a string and three more tuned to intervals of
  it, driven by its sound), and a modal resonator (up to 16 two-pole resonators at stretched partials). Four
  internal exciters: pluck (Rings' plucker), strike, bow (noise while the key is held) and dust (random impacts
  while the key is held). Ten kinds: PLUCK, BOWED, STRUCK, SITAR, RATTLE, HARP, DRONE, BELL, BOWL, PLATE. EDIT's
  macros are Rings' controls: STRUCTURE, BRIGHTNESS, DAMPING, POSITION. Releasing a key damps the sound by an
  amount set per sound. Voices come from the shared pool; each uses the memory of an ANIMA voice (no new
  memory). Fifth position on the PRESETS knob.

Changed
- Sessions: format 17 (a fifth sound per track; macros and envelopes for five engines). Older sessions load
  with a new DRUID sound drawn for each track.
- Presets: format 4 (12 bytes a slot, 64 slots for each of five engines in one 4 KB sector). Presets saved
  before still load.

## Build 39

Changed
- Track 1 starts with its pan in the middle (was half left), at power-on and in an empty session. The other
  tracks' starting pans are unchanged. Saved sessions keep their pans.

Fixed
- With SHIFT held, a KNOB 3 or KNOB 4 click arriving while the panel was being read could go to the knob's
  own function: the crush or the reverb send changed by one step and its dial showed briefly. While SHIFT is
  held, KNOB 3 and 4 now go only to the distortion and the delay send.

## Build 38

Added
- Per-track distortion on SHIFT + KNOB 3 (0–100, 0 off), after the track's limiter and before its level, pan
  and the delay and reverb sends. Two gain stages: symmetric, then asymmetric; drive ×0.5 to ×32 (+36 dB). A
  bias proportional to the input's peak level is added before the first stage, so even harmonics remain at
  high drive. Highs +6 dB before the stages and −6 dB after; the stages run at 2× the sample rate (linear
  interpolation up, [¼ ½ ¼] half-band down); a DC blocker; a one-pole low-pass from 16 kHz down to 6.5 kHz as
  the amount increases. The output is matched to the dry signal's RMS level (about 0.2 s), so the loudness
  does not change with the amount. Stored in sessions (format 16) and copied with the track.

## Build 37

Added
- ENVELOPE page on the 3rd black key from the right (lit while up): the chosen track's amplitude envelope as
  attack, decay, sustain and release, drawn and with values. Until the knobs are turned it shows the sound's
  own envelope (AS DRAWN; FM: the loudest carrier; CALIGULA: its pairs' times averaged; CALIGULA and
  RUSSOLO: no decay or sustain stage). KNOB 1–4 set ATTACK (1 ms – 10 s), DECAY (5 ms – 10 s), SUSTAIN
  (0–100 %) and RELEASE (5 ms – 20 s), logarithmic. The first turn starts from the values shown and replaces
  the sound's amplitude envelope: FM carrier envelopes and the ANIMA envelope are held at full, CALIGULA and
  RUSSOLO are held through the release and then let go. Applies from the next note; afterwards knob changes
  also affect sounding notes. Linear attack, exponential decay and release (release ends at −60 dB).
- SHIFT + the envelope key, HOME and preset loading return to the sound's own envelope. One envelope per
  track and engine; stored in sessions (format 15) and copied with the track; not stored in presets.

Changed
- The XS font has a space character.

## Build 36

Added
- SHIFT + HOME generates a new sound of the same family (FM), kind (ANIMA, RUSSOLO) or species (CALIGULA) as
  the selected track's current sound, on its engine and path. HOME alone still changes the family.

## Build 35

Added
- Chord mode per track on the 3rd black key: held 0.6 s toggles it (CHO at the top left of the screen, key lit),
  pressed steps the form: TRIAD (0 2 4), INV 1 (2 4 7), INV 2 (−3 0 2), SUS2 (0 1 4), SUS4 (0 3 4), OPEN (0 4 9),
  FOURTHS (0 3 6), in scale steps from the key's note. One chord per track at a time. Chords latch, follow the
  loop transpositions, are recorded by the loopers, and are stored in sessions (format 14) and copied with the
  track.

Changed
- Latched notes and chords move with the loop transposition: SHIFT + track button + OCT moves that track's, SHIFT +
  OCT moves every track's, by scale steps. The lit latched keys move by the same number of white keys.
- The chord form is shown under CHO as a three-letter abbreviation (TRI, IN1, IN2, SU2, SU4, OPN, 4TH) in a new
  smaller font (XS, 19 px).

## Build 34

Changed
- The loop direction and speed keys (REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED) are lit only while the
  selected track's loops play (PLAY lit): dimly when off, fully when on. With the loops stopped they are unlit;
  their settings are kept and the keys still respond.

## Build 33

Changed
- The black keys with a function (AMBIENT, EXPERIMENTAL, REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED,
  EDIT) are dimly lit while off and fully lit while on. SHIFT and the black keys without a function stay unlit.

## Build 32

Added
- Master delay, mono, before the reverb. Per-track send on SHIFT + KNOB 4 (default 0). DELAY page on
  SHIFT + SEQ with TIME (20 ms – 2 s, logarithmic), FEEDBACK (0–0.95), DAMP (one-pole low-pass in the feedback
  path) and MIX. Delay line: 2 s at 22.05 kHz, 8-bit μ-law (44 100 bytes, split between the buffer pool and
  unused code RAM). Echoes go to both master channels and, at half level, to the reverb input. TIME changes glide
  over about 0.1 s. Stored in sessions (format 13); the send is copied with the track.

Changed
- The screen is drawn in bands of at most 56 rows (was 112), freeing 27 KB of buffer memory; the display is
  unchanged.

## Build 31

Added
- Loop direction and speed per track, on the 4th–8th black keys (lit while on): REVERSE, PENDULUM (forwards
  and backwards alternately), RANDOM, HALF SPEED, DOUBLE SPEED. REVERSE/PENDULUM/RANDOM are exclusive, as are
  HALF/DOUBLE; direction and speed combine. RANDOM plays forwards and, each time the next note-on is due and at
  the loop end, releases the sounding notes and jumps to a randomly chosen note-on (with any note-ons at the
  same time); note lengths and the interval to the following note-on are those recorded. Applies to all overdubs of the track, each within its own length. A direction
  change continues from the current position (sounding notes are not cut or retriggered). The loop clock runs
  at the selected speed, so overdubs recorded at half or double speed keep their played timing; overdubs
  recorded in reverse are stored mirrored and play back as played. Stored in sessions (format 12), copied with
  the track, reset when the track's loops are cleared.

## Build 30

Added
- Per-track loop transposition: hold SHIFT and a track button, then press OCT+ or OCT− to move that track's
  loops (all overdubs) by one scale step. The screen shows the value (e.g. TRANSP T3 +2). It is added to the
  global loop transposition (SHIFT + OCT). Stored in sessions (format 11), copied with the track, reset when the
  track's loops are cleared.
- The white keys follow the transposition of the selected track (global + per-track), so new notes and
  overdubs are in the same transposed scale. Overdubs store the transposition at which they were recorded and
  play back as recorded. USB MIDI notes are not transposed.

Changed
- FM macros: BRIGHT renamed BRIGHTNESS, range on modulator output levels increased from ±26 to ±36 steps
  (about ±27 dB); turning it up also raises feedback. DETUNE now spreads carriers ±40 cents (was ±30) and
  modulators ±25 cents (was ±12), so single-carrier patches also produce beating.
- All macros on all engines use a square-root response: a quarter turn gives half of the full change.
- Macro names: FM SKEW → INHARMONIC, GRIT → SATURATION; ANIMA AIR → NOISE, VOWEL → FORMANT, SPACE →
  RESONATOR; CALIGULA EDGE → WAVEFORM, DEPTH → MOD DEPTH, CHAOS → FEEDBACK, ECHO → DELAY, HAND → TOUCHES,
  PULSE → GATE RATE; RUSSOLO CHAOS → RANDOMNESS. Names too wide for the dial title use the smaller font.
- SHIFT no longer displays the audio load.

## Build 29

Added
- Generator generation 3. ANIMA: eight kinds (PLUCK, REED, FOG, ORGAN, BITS, DRIFT, MURMUR, HOWL). FM: five
  families (PLUCK, MALLET, WARP, GLITCH, RUBBER). All kinds and families draw from wider ranges: any of the 24
  wavetables, more intervals, FM ratios and stack steps, detuning, optional percussive or slow-attack envelopes,
  other hand-motion modes, square-wave tremolo, sample-rate reduction, high filter resonance (ANIMA); optional
  percussive envelopes, modulator ratio changes and operator detuning (FM).

Changed
- ANIMA renders each voice at 22.05 kHz (16 samples per 32-sample block), upsampled by linear interpolation;
  bandwidth limited to 11 kHz. Oscillator increments, delay lengths, grain lengths and filter coefficients are
  scaled to keep pitch and timing. CPU cost per voice reduced from about 15 % to about 9 % of a block.
- ANIMA wavetables in use are copied to a 15-slot RAM cache (one copy per audio block at most); a read from
  flash costs 6–33 CPU ticks, from RAM 1.6.
- Loop capacity reduced from 2048 to 1024 note events per track (about 500 notes) to free memory for the
  cache. Sessions with longer loops load the first 1024 events per track.
- ANIMA macros: TONE ±4 octaves on all filters (was ±2.5) and FM index; the kind macro (GRAINS, AIR, FM, VOWEL,
  DRIVE) ranges from zero to several times the design value, DRIVE below centre removes the second
  oscillator, FM, noise and grains; SPACE from 0 to 0.85 resonator mix; MOTION from ¼ to 4× hand speed, adding
  a tremolo above centre. At centre the sound is unchanged.

Compatibility
- Sounds stored as seeds (sessions, presets) regenerate identically for generations 0–2. ANIMA sounds play at
  the new sample rate.

## Build 28

Added
- Track copy/paste: holding a track button for 0.8 s copies the track (screen: COPIED; the button blinks while
  the clipboard holds it). Holding another track button pastes it (screen: PASTED) and empties the clipboard.
  Copied: engine, the sound of each of the four engines with macros, filter, granulator, crush, reverb send,
  octave, generative path, all loops with their mute states. Not copied: level, pan. The destination's latched
  notes are released.

Changed
- Documentation uses "track" instead of "layer".

## Build 27

Added
- Global loop transposition: SHIFT + OCT+/OCT− moves all loops by one step of the current scale, up to two
  octaves of the scale either way. Sounding notes are moved immediately. Changing scale or root re-maps the
  loops to the new scale. Stored in sessions (format 10).
- Selecting an empty session slot resets the machine to its power-on state: loops and latches cleared, new
  sounds on all tracks, all track settings, scale, root, reverb and transposition at defaults.

Fixed
- A note held while REC closes a loop is no longer released at the loop end and retriggered at the start; it
  sustains across the loop boundary.
- ANIMA voice reuse: the comb resonator, phaser and sample-and-hold states of a reassigned voice are now
  cleared; previously the previous note's state was audible as a click.
- Audio dropouts when starting a loop under high load: the render cost of each voice and organism is now
  measured per block, and before new voices start the oldest release tails are faded out (2 ms) until the
  predicted cost is under 82 % of the block. Held notes are only dropped if a buffer is actually late.

## Build 26

Added
- RUSSOLO engine, based on the techniques of Noisferatu by Robert Heel: 17 kinds of noise, logic-gate
  oscillators, buffer manipulation and random tones, rendered at FS/3 with 10-bit values plus per-sound bit and
  sample-hold reduction. One instance per track; keys gate it and set its pitch. Macros: two per kind, CRUSH,
  CHAOS. 64 presets.
- Sessions also store each track's granulator character and latched notes (format 8).

## Build 25

Changed
- Voice allocation: a pool of 12 voices shared by all tracks replaces two fixed voices per track. Stealing
  order: oldest release tail, then the oldest held note of the same track, then the oldest held note.

## Build 24

Changed
- Sessions are read and written one 4 KB flash sector at a time; about 54 KB of RAM freed. Session format
  unchanged.

Fixed
- The name on the start-up screen fits.

## Build 23

Added
- Overdub muting: holding PLAY shows the selected track's loops on the white keys; pressing a key mutes or
  unmutes that loop. Mute states are stored in sessions (format 7).

## Build 22

Added
- EDIT key (the black key left of SHIFT): KNOB 1–4 control four macros of the selected track's sound, defined
  per engine and kind. Stored in sessions and presets.

Fixed
- FM generator: operator ratios were computed with the fine value 100 times too large, placing patches about
  two octaves above the played note. Corrected for new sounds (generation 2); generations 0 and 1 are kept
  unchanged so stored sounds regenerate identically.

## Build 21

Added
- Presets: SHIFT + SAVE opens 64 preset slots per engine.
- Generator generation 1. FM: eight families (CHOIR, BOWED, HOLLOW, BLOOM, TIDE, SHARDS, STATIC, WIND),
  modulator levels reduced on high ratios, keyboard level scaling, several candidates generated and the one
  whose brightness fits the path kept. ANIMA: six kinds (TIDE, CHOIR, SHARDS, BOWL, DUST, MACHINE), filter
  keyboard tracking, path-dependent oscillator intervals, resonator on most AMBIENT sounds. CALIGULA: four
  species (LITANY, UNDERTOW, WIRE, MOTH), four tunings, repeated touch patterns.

Compatibility
- Generation 0 is kept; sounds stored as seeds regenerate identically (checked by a test).

## Build 20

Changed
- Firmware renamed Tonitor (previously ACOUSMA). USB device name "FM-1 Tonitor". Existing sessions load.

Added
- Browser installer using Web MIDI (Chrome, Edge, Opera).
- Website and online manual.

## Build 19

Added
- SHIFT + PLAY with no loop playing starts all loops from their beginnings.

## Build 18

Added
- Sessions page: holding SAVE saves to the displayed slot, with an OVERWRITE? confirmation for occupied slots.
  Occupied slots shown in red, empty slots in grey.

## Build 17

Changed
- ANIMA EXPERIMENTAL path: sample-rate reduction, wavefolding, band-pass noise, scattered grains, fast
  tremolo and high resonator feedback on most sounds.
- Level meter moved to the bottom of the screen; level shown in percent.

Added
- SHIFT + PLAY stops all loops on all tracks.

Fixed
- Long messages fit the screen.

## Build 16

Added
- Generative paths AMBIENT and EXPERIMENTAL on the two leftmost black keys, per track.

Fixed
- CALIGULA slow attacks and long releases.
- Overload handling no longer drops the most recently played note.

## Build 15

Changed
- Latching: hold keys, then press SHIFT. SCALE page: SELECT sets the root, ALGORITHM the scale.

## Build 14

Added
- SHIFT key and latched notes.

## Build 13

Changed
- CALIGULA played from the keyboard.

## Build 12

Added
- CALIGULA engine.

## Builds 1 – 11

- FM and ANIMA engines; six tracks with two voices each; one looper per track with up to ten overdubs; track
  level, pan and reverb send; master reverb; 64 session slots; scales on the white keys; per-track filter,
  granulator and crush.
