# Changelog

## 0.31

Added
- Loop direction and speed per track, on the 4th–7th black keys (lit while on): REVERSE, PENDULUM (forwards
  and backwards alternately), HALF SPEED, DOUBLE SPEED. REVERSE/PENDULUM and HALF/DOUBLE are exclusive pairs;
  direction and speed combine. Applies to all overdubs of the track, each within its own length. A direction
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
