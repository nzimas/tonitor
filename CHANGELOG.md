# Changelog

## 0.49

Changed
- BALLSY and BRICKLAYER wait for PLAY when turned on. On their page PLAY starts and stops them (PLAY or STOP on
  the screen) and blinks with their key while they play; on every other page PLAY is the looper's, lit
  steadily while the loops play. BRICKLAYER starts from its first step; BALLSY's balls stop where they are and
  move on from there. The looper works beside either, and records only notes played on the keys: BALLSY's and
  BRICKLAYER's notes are no longer recorded.
- SHIFT + PLAY starts or stops every loop and every sequencer that is on, together (ALL PLAYING, ALL STOPPED).
- Session format 23: whether each sequencer plays. Older sessions load with their sequencers playing.

## 0.48

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

## 0.47

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

## 0.46

Changed
- One sequencer per track: BALLSY and BRICKLAYER are mutually exclusive. Starting one while the other runs on
  the same track shows NOT ALLOWED and changes nothing. A session that has both on for a track loads with
  BALLSY on and BRICKLAYER off. The looper runs beside either.

Fixed
- The FM-1 showed up as "Felucca" in MIDI applications on computers that had seen Felucca before: they kept the
  name stored for the same USB IDs (1209:0001). Tonitor now uses its own USB product ID (1209:0003), so it shows
  up as "FM-1 Tonitor". The update loader is unchanged (1209:0002).

## 0.45

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

## 0.44

Fixed
- VARY switched on while a loop's notes were sounding (long notes, especially ones held over the loop's end) left
  those notes sounding indefinitely: under VARY the recorded note-offs were ignored. A note struck as recorded
  before VARY came on is now released at its recorded end; notes struck by VARY keep their own random lengths.

## 0.43

Added
- VARY also modulates the loops' pace: a slow sine undulation (0.07–0.35 Hz, depth ±15–40 %, redrawn every few
  seconds), and on average every ~6 s one event: halt (0.15–1 s, abrupt stop and resume), burst (2–3× for
  0.2–0.8 s), grind (slowing to a stop over 0.4–1.5 s, held up to 0.4 s, then back at once) or swoop (rising to
  2–2.5× over 0.8–2 s, then 0.4× for 0.3 s). Note lengths stay in real time. Not applied while REC records.

## 0.42

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

## 0.41

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

## 0.40

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

## 0.39

Changed
- Track 1 starts with its pan in the middle (was half left), at power-on and in an empty session. The other
  tracks' starting pans are unchanged. Saved sessions keep their pans.

Fixed
- With SHIFT held, a KNOB 3 or KNOB 4 click arriving while the panel was being read could go to the knob's
  own function: the crush or the reverb send changed by one step and its dial showed briefly. While SHIFT is
  held, KNOB 3 and 4 now go only to the distortion and the delay send.

## 0.38

Added
- Per-track distortion on SHIFT + KNOB 3 (0–100, 0 off), after the track's limiter and before its level, pan
  and the delay and reverb sends. Two gain stages: symmetric, then asymmetric; drive ×0.5 to ×32 (+36 dB). A
  bias proportional to the input's peak level is added before the first stage, so even harmonics remain at
  high drive. Highs +6 dB before the stages and −6 dB after; the stages run at 2× the sample rate (linear
  interpolation up, [¼ ½ ¼] half-band down); a DC blocker; a one-pole low-pass from 16 kHz down to 6.5 kHz as
  the amount increases. The output is matched to the dry signal's RMS level (about 0.2 s), so the loudness
  does not change with the amount. Stored in sessions (format 16) and copied with the track.

## 0.37

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

## 0.36

Added
- SHIFT + HOME generates a new sound of the same family (FM), kind (ANIMA, RUSSOLO) or species (CALIGULA) as
  the selected track's current sound, on its engine and path. HOME alone still changes the family.

## 0.35

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

## 0.34

Changed
- The loop direction and speed keys (REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED) are lit only while the
  selected track's loops play (PLAY lit): dimly when off, fully when on. With the loops stopped they are unlit;
  their settings are kept and the keys still respond.

## 0.33

Changed
- The black keys with a function (AMBIENT, EXPERIMENTAL, REVERSE, PENDULUM, RANDOM, HALF SPEED, DOUBLE SPEED,
  EDIT) are dimly lit while off and fully lit while on. SHIFT and the black keys without a function stay unlit.

## 0.32

Added
- Master delay, mono, before the reverb. Per-track send on SHIFT + KNOB 4 (default 0). DELAY page on
  SHIFT + SEQ with TIME (20 ms – 2 s, logarithmic), FEEDBACK (0–0.95), DAMP (one-pole low-pass in the feedback
  path) and MIX. Delay line: 2 s at 22.05 kHz, 8-bit μ-law (44 100 bytes, split between the buffer pool and
  unused code RAM). Echoes go to both master channels and, at half level, to the reverb input. TIME changes glide
  over about 0.1 s. Stored in sessions (format 13); the send is copied with the track.

Changed
- The screen is drawn in bands of at most 56 rows (was 112), freeing 27 KB of buffer memory; the display is
  unchanged.

## 0.31

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

## 0.30

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

## 0.29

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

## 0.28

Added
- Track copy/paste: holding a track button for 0.8 s copies the track (screen: COPIED; the button blinks while
  the clipboard holds it). Holding another track button pastes it (screen: PASTED) and empties the clipboard.
  Copied: engine, the sound of each of the four engines with macros, filter, granulator, crush, reverb send,
  octave, generative path, all loops with their mute states. Not copied: level, pan. The destination's latched
  notes are released.

Changed
- Documentation uses "track" instead of "layer".

## 0.27

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

## 0.26

Added
- RUSSOLO engine, based on the techniques of Noisferatu by Robert Heel: 17 kinds of noise, logic-gate
  oscillators, buffer manipulation and random tones, rendered at FS/3 with 10-bit values plus per-sound bit and
  sample-hold reduction. One instance per track; keys gate it and set its pitch. Macros: two per kind, CRUSH,
  CHAOS. 64 presets.
- Sessions also store each track's granulator character and latched notes (format 8).

## 0.25

Changed
- Voice allocation: a pool of 12 voices shared by all tracks replaces two fixed voices per track. Stealing
  order: oldest release tail, then the oldest held note of the same track, then the oldest held note.

## 0.24

Changed
- Sessions are read and written one 4 KB flash sector at a time; about 54 KB of RAM freed. Session format
  unchanged.

Fixed
- The name on the start-up screen fits.

## 0.23

Added
- Overdub muting: holding PLAY shows the selected track's loops on the white keys; pressing a key mutes or
  unmutes that loop. Mute states are stored in sessions (format 7).

## 0.22

Added
- EDIT key (the black key left of SHIFT): KNOB 1–4 control four macros of the selected track's sound, defined
  per engine and kind. Stored in sessions and presets.

Fixed
- FM generator: operator ratios were computed with the fine value 100 times too large, placing patches about
  two octaves above the played note. Corrected for new sounds (generation 2); generations 0 and 1 are kept
  unchanged so stored sounds regenerate identically.

## 0.21

Added
- Presets: SHIFT + SAVE opens 64 preset slots per engine.
- Generator generation 1. FM: eight families (CHOIR, BOWED, HOLLOW, BLOOM, TIDE, SHARDS, STATIC, WIND),
  modulator levels reduced on high ratios, keyboard level scaling, several candidates generated and the one
  whose brightness fits the path kept. ANIMA: six kinds (TIDE, CHOIR, SHARDS, BOWL, DUST, MACHINE), filter
  keyboard tracking, path-dependent oscillator intervals, resonator on most AMBIENT sounds. CALIGULA: four
  species (LITANY, UNDERTOW, WIRE, MOTH), four tunings, repeated touch patterns.

Compatibility
- Generation 0 is kept; sounds stored as seeds regenerate identically (checked by a test).

## 0.20

Changed
- Firmware renamed Tonitor (previously ACOUSMA). USB device name "FM-1 Tonitor". Existing sessions load.

Added
- Browser installer using Web MIDI (Chrome, Edge, Opera).
- Website and online manual.

## 0.19

Added
- SHIFT + PLAY with no loop playing starts all loops from their beginnings.

## 0.18

Added
- Sessions page: holding SAVE saves to the displayed slot, with an OVERWRITE? confirmation for occupied slots.
  Occupied slots shown in red, empty slots in grey.

## 0.17

Changed
- ANIMA EXPERIMENTAL path: sample-rate reduction, wavefolding, band-pass noise, scattered grains, fast
  tremolo and high resonator feedback on most sounds.
- Level meter moved to the bottom of the screen; level shown in percent.

Added
- SHIFT + PLAY stops all loops on all tracks.

Fixed
- Long messages fit the screen.

## 0.16

Added
- Generative paths AMBIENT and EXPERIMENTAL on the two leftmost black keys, per track.

Fixed
- CALIGULA slow attacks and long releases.
- Overload handling no longer drops the most recently played note.

## 0.15

Changed
- Latching: hold keys, then press SHIFT. SCALE page: SELECT sets the root, ALGORITHM the scale.

## 0.14

Added
- SHIFT key and latched notes.

## 0.13

Changed
- CALIGULA played from the keyboard.

## 0.12

Added
- CALIGULA engine.

## 0.1 – 0.11

- FM and ANIMA engines; six tracks with two voices each; one looper per track with up to ten overdubs; track
  level, pan and reverb send; master reverb; 64 session slots; scales on the white keys; per-track filter,
  granulator and crush.
