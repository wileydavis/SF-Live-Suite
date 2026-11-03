SF Live Suite — Spec v2
Nov 3, 2026 · Wiley
Source of truth. Decisions are dated inline. Builds are tested as whole suites only.

# Purpose

A suite of Ableton Live plugins (AU + VST3, macOS, Apple Silicon) for performing hypnotic / dub techno live: very simple note content (often one note, fixed patterns), with all variation played on knobs. Designed from the start for a fixed live rig: one Live set that never changes, songs switched by preset, the OXI One MkII sequencing, the OXI E16 knob array (absolute 0-127 CC with value feedback) playing.

The releases are live sets, not tracks [Nov 3]: sets are recorded as played and released, from a bedroom session to a festival main stage. So the bar is release-grade sound from every plugin at 44.1 / 48 / 96 kHz and any buffer size, and nothing a performer does with a knob may produce a click, an alias, an inter-sample over, a level jump or a dropout. See Sound quality.

# Standards

## Visual
Black, white and greys. Colour only carries information:
- RED: clip / fault / missing file
- AMBER: conflict / warning / armed-but-waiting states that need attention
- GREEN: active / locked to the grid / source connected
- CYAN: the value of the control being touched

Monospace type, 1 px rules, no decoration, no gradients, no shadows.

## Naming
Boring, practical, uppercase. Plugins appear in Live as `SF <ROLE>`. Parameters are plain words (`DENSITY`, `GATE`, `CUTOFF`, `DEPTH`). Models are descriptive (`KICK 909-TYPE`, `ACID 303-TYPE`, `STAB DUB`). Trademarked machine names appear only as "-TYPE" descriptions. Presets are `GROUP/NAME` in the same plain words (`DUB/CHORD 1`, `ACID/SQUELCH`); no artist names, no trademarks.

## Controls
- The first 8 parameters of every plugin are its performance row (Live's device view, one E16 row). Everything else is set-and-forget.
- Every knob is smoothed (30 ms) so 7-bit CC never steps audibly; ranges and curves are tuned so the whole travel is usable.
- Standard trio wherever it applies: DENSITY (how many events), GATE (how long each lasts), FILTER (bipolar DJ filter: left low-pass, centre open, right high-pass).
- System controls on every plugin (appended, never in the first 8):
  - SLOW: OFF / 1-64 BAR. When on, every performance-row knob travels to where it is set over exactly that many bars, however far it moved (with SLOW at 32 BAR a quarter turn becomes a 32-bar rise). Song recalls glide the same way. MACRO moves stay immediate. The knob shows "GLIDE now>target" in green while travelling. [decided Nov 2: fixed time, performance row only]
  - MACRO SRC (OFF, MACRO 1-6) and MACRO AMT (-100..+100 %): the plugin follows one of SF CONTROL's six macros. Each module decides which knobs respond and in which direction.
  - MOTION [added Nov 3]: two modulators, M1 and M2, each SOURCE / RATE / TARGET / DEPTH. See MOTION below.
- PRESET menu in every header [added Nov 3]: factory presets (181 across 25 plugins) and user presets. See Presets below.
- Nothing can hurt the PA: resonance is level-compensated, feedback above 100 % saturates instead of exploding, every output is clamped at +12 dBFS. NaN/Inf, or 250 ms held at the clamp (a runaway), replaces the output with silence and resets the module; the header's meter label shows FAULT in red. PANIC on SF CONTROL silences every SF plugin (see Engine).

## Live / controller workflow (decided Nov 2)
- E16 is mapped with Live's MIDI Map (Cmd-M) with Remote on for its output, so knob rings follow song and value changes. The plugins report every recalled value back to Live.
- Song change: Program Change on MIDI channel 16 into SF CONTROL (channel selectable). Recall lands on the next bar line while Live plays, at once when stopped. VST3 has no Program Change event: SF CONTROL exposes 16 programs (SONG 01-16) and Live's program change selects the song; the AU also reads Program Change from MIDI. [decided Nov 2, needs Live test]
- Stems from Live's own stem separation go on their own tracks; SF TAP on each puts them into Pocket and ALLOC.

# Sound quality [decided Nov 3]
Nothing audible may come from the implementation. Measured by SFTests (`--quality`), every build.
- Oversampling: every nonlinearity runs oversampled.
  - Polyphase IIR halfbands (elliptic, HIIR-style design, cascaded for 4x): flat to 20 kHz within 0.0001 dB, image and alias rejection 118 dB at 44.1 kHz, 112 dB at 48 kHz, 123 dB at 96 kHz. Near-zero latency, so it is used inside voices and feedback loops.
  - Linear-phase FIR (79- and 23-tap halfbands, then a Kaiser low-pass decimator, 100 dB) where phase matters: SF LIMIT's clipper. 99.5 dB rejection.
  - Rates: synth voices 2x (ladder, MS20-TYPE and SYNC models 4x); KICK, PERC 2x; DRIVE and STRIP drive 4x (DRIVE moved from JUCE's oversampler, about 70 dB, to the suite's own); FILTER, ENERGY, VOICE drive 2x; DELAY loop saturation 2x; RUMBLE grit 2x; LOWEND sub clip 2x; STRIKE fracture 2x; TONE 2x; LIMIT clip 4x linear phase.
  - Oversampled stages are never switched in and out (that would move their delay and click): they run all the time, and each drive fades in from exactly clean at 0 (FILTER, ENERGY, STRIP across the whole knob; synth DRIVE over the first 10 %; VOICE DRIVE and RUMBLE GRIT over the first 2 %, so their calibrated defaults are unchanged). [review, Nov 3]
  - LIMIT's FIR stages are sized from the sample rate (2x stage 151 taps below 46 kHz, 79 above; 4x stage 31 taps): the whole path is flat to 20 kHz within 0.01 dB at 44.1, 48 and 96 kHz.
- Oscillators: band-limited wavetables (one table per octave, harmonics kept below 0.75 x the oversampled Nyquist, 4-point Hermite reads), hard sync with BLEP correction, polyBLEP carriers in VOICE, first-order anti-derivative anti-aliasing (ADAA) on tanh stages. Alias floor, worst case across a 3-octave sweep: ACID 303-TYPE 94 dB, FM 97 dB, STRING MACHINE 105 dB, SUB OSC 94 dB, MS20-TYPE 77 dB, SYNC 79 dB.
- Samples: files are converted to the session rate with a windowed-sinc resampler on load; playback interpolates with 4-point Hermite.
- Engine: audio runs in 64-sample chunks on a fixed grid of the stream (see Engine), so the output does not depend on Live's buffer size.
- SF LIMIT: see Master / mix. True peak at the output never exceeds CEILING (8x detection, measured on dense clipped material, sine sweeps and square waves).
- Loudness metering (LIMIT, DECK analysis): ITU-R BS.1770-4 / EBU R128. K-weighting designed exactly at any sample rate; momentary 400 ms, short-term 3 s, integrated with absolute (-70 LUFS) and relative (-10 LU) gating on a 0.1 dB histogram (constant memory over a whole set). Reads -20.00 LUFS on the 1 kHz reference sine.
- REVERB: feedback delay network with input diffusion (four all-passes per channel) and two-band absorption in every line (shelving: DARK shortens the highs while the lows keep DECAY). RT60 of the lows at DECAY 1 / 3 / 8 s: 0.99 / 3.04 / 8.00 s. Line lengths, loop gains, absorption and modulation depth ramp across each chunk and glide (30 ms), so SIZE / DECAY / DARK / MOD under a knob, automation, MACRO or MOTION never zipper. Reverbs inside ENERGY and STRIP ring out after their send closes instead of being cut.
- DELAY: TONE and saturation sit inside the feedback loop, so each repeat is darker (or thinner) and dirtier than the last. FREEZE crossfades (10 ms) into a lossless loop; the read head never moves.
- GLUE: 6 dB soft knee.

## Gain staging [decided Nov 3]
Every instrument model is trimmed so that at default settings it plays at its role's loudness target (integrated LUFS, role pattern): KICK -15, PERC -22, RUMBLE -20, BASS -18, LEAD -20, PAD -22, VOICE -20, STRIKE -20. Switching MODEL never jumps the level, and a rig at default LEVELs is balanced with headroom on the master before GLUE and LIMIT. Trims are in `Source/modules/Calibration.h`, applied after any drive, and verified by SFTests (`--calibration`). Sample models are not trimmed (the file's own level counts).

# Shared architecture

## SF BUS
One block of POSIX shared memory (`/sf_live_suite_bus_v3`) mapped by every SF plugin, across plugin binaries and processes. Lock-free atomics only. A segment of the wrong size or version (another build) is never mapped; that plugin falls back to a local bus, so all plugins in a set must come from the same build. Verified Nov 2: two separately built plugin binaries (SF KICK, SF DUCK) loaded in a VST3 host share the bus and Pocket works between them. Carries: macros, song select / store requests, PANIC, KEY, kick reports, ALLOC source energies and cuts. v3 [Nov 3] added PANIC and KEY. If shared memory is unavailable the bus falls back to process-local (plugins in one process still talk); SF CONTROL shows BUS SHARED / LOCAL ONLY.

## Clock
Live's transport when playing. When stopped, a wall clock at the last tempo stands in, so tempo-synced motion keeps running and every SF plugin agrees on the beat.

## Engine [decided Nov 3]
- Every plugin processes in 64-sample chunks on a fixed grid of the audio stream, whatever Live's buffer size. Parameters (30 ms smoothing, SLOW glides), MACRO and MOTION update once per chunk at the same stream positions every time, and envelopes, modulators and Pocket run on that grid, so the output is the same at any buffer size: bit-identical at buffer sizes 1 to 2048 for every module (SFTests). MIDI events keep their exact sample offsets inside the chunk.
- Song recall lands on the exact sample of the bar line (the chunk is cut there), not on the next buffer. Positions are compared with a 0.001-sample tolerance, so rounding of the song position (which grows over a long set) can never push a recall to the following bar.
- Safety: after every chunk, NaN/Inf become silence, the output is clamped at +12 dBFS, and NaN or 250 ms held at the clamp resets the module (FAULT, red, in the header).
- PANIC (SF CONTROL): every SF plugin fades out over 5 ms, then clears every voice, tail, delay line, envelope and filter state. Use it for a runaway, a stuck note or a wrong song recall in front of a crowd. Nothing restarts until new notes or audio arrive. A second PANIC during the fade doesn't restart it.
- No allocation, locks or file access on the audio thread. Samples are swapped through an atomic pointer; replaced samples are freed on the message thread after 2 s.

## Songs
Every plugin stores 16 song slots of its own parameter values (with the Live set). SF CONTROL selects the song (SONG knob/menu or Program Change); each plugin recalls its slot on the next bar. STORE ALL on SF CONTROL stores every plugin's current values into the selected song; STORE on any plugin stores just that plugin. Empty slots leave the plugin as it is. Arrangement automation wins over recalled values (as on any device). SLOW applies to recalls, so a song change can glide. MOTION settings and SF CONTROL's KEY are part of songs.

## Presets [added Nov 3]
- The PRESET button in every header opens the menu: factory presets in GROUP submenus, user presets, SAVE USER PRESET, SHOW PRESET FOLDER. The button shows the loaded preset.
- A preset sets every sound parameter of the plugin (unnamed ones go to their defaults) and reports each change to Live, so the E16 rings follow. It never touches SLOW, MACRO SRC / AMT, ALLOC participation, ROLE, KICK BUS / MIDI sources, or venue data. Presets may set MOTION.
- 181 factory presets across 25 plugins (instruments, effects, master tools; none for DECK, CONTROL, ROOM EQ, ALLOC, TAP). Every one is rendered by SFTests: finite, in range, no runaway.
- User presets: one XML file per preset in `~/Music/SF Live Suite/Presets/<MODULE>/`. Copy the folder to move them between machines.

## MOTION [added Nov 3]
Two modulators in every plugin (M1, M2), stored with songs and presets.
- SOURCE: SINE, TRIANGLE, SQUARE, RANDOM (stepped), DRIFT (smooth random) are bipolar around the knob; RAMP UP / RAMP DOWN are one-way ramps that restart every RATE; KICK follows SF KICK's hits (RATE sets the release); NOTES (instruments: note velocity) or INPUT (effects: input level), RATE sets the release.
- RATE: 1/16 to 64 BAR, locked to Live's song position, so every plugin's modulators move together and move the same way every time the set is played. RANDOM and DRIFT are seeded per plugin instance; the seed is saved with the set.
- TARGET: any continuous parameter of the plugin. DEPTH: -100 to +100 % of the target's whole range, applied in the knob's own scale (a sweep of CUTOFF moves evenly in pitch, not in Hz). The modulation adds to the knob, MACRO and SLOW; the knob shows a bar for the modulated value.
- Steps (SQUARE, RANDOM, ramp resets, loop jumps) are slewed over 3 ms: no clicks.

## KEY [added Nov 3]
SF CONTROL's KEY (OFF, C ... B; stored per song) transposes the suite while SF CONTROL is running (the bus outlives sets, so a key left by a deleted CONTROL or another set is ignored). Parts are written in C; with KEY on, every pitched SF instrument plays them in the chosen key (nearest transposition, -6 to +5 semitones). KICK, RUMBLE and STRIKE snap their own pitch (the kick's fundamental, the rumble's thump, the object's main partial) to the nearest note of the key root, so the low end is always in tune with the track. KEY FOLLOW on each instrument opts out. OFF = no change anywhere.

## POCKET (kick-aware ducking)
SF KICK reports every hit to the bus: time (in beats), velocity, pitch sweep start/end and time constant, amplitude decay, and its FILTER corners. Any module with Pocket carves a kick-shaped hole in itself:
- DEPTH: how deep. SHAPE: one knob from transparent to pumping. Left: a narrow cut that follows the kick's pitch sweep down to its fundamental (felt, not heard). Middle: the low band (below ~3x the kick fundamental) ducks for the kick's length. Right: full-band pump, release tied to the tempo.
- Adapts to the kick: velocity scales each pocket (ghost kicks get shallow pockets), decay sets the length, the pitch sweep sets the frequencies, a high-passed kick closes the low pocket (the bass gets the sub), rolls merge into one dip on transparent settings and pump per hit on pumping settings.
- Safety: when kick tails overlap so much the pocket would be permanent, the pocket shrinks to the attack.
- Kick returns after a gap: pockets fade back in over a beat (breakdowns open up the bass automatically).
- Sidechain fallback (SF DUCK, SOURCE = SIDECHAIN, or any time no SF KICK is reporting): low-band envelope of the sidechain input.
- Transport jumps [decided Nov 2]: kick reports carry a wall-clock stamp; a report is used only if the wall time since it fits the musical time since it, so reports from before a loop wrap / restart / relocate are ignored. RUMBLE re-arms on a backward jump. POCKET OFFSET on SF CONTROL (+/-20 ms) calibrates Pocket and RUMBLE timing for the host if needed.
- Timing: Pocket runs 512 samples of lookahead (reported to Live as latency) so a kick report from the same audio cycle arrives before it is needed. Requires Live's buffer size <= 512. [decided Nov 2, needs Live test]

## ALLOC (mix assigner) [redesigned Nov 3]
Ten fixed bands (20-60, 60-100, 100-150, 150-220, 220-320, 320-480, 480-750, 750-1500, 1.5k-4k, 4k-20k Hz). Every instrument, SF STRIP, SF TAP and SF DECK measures its energy per band (40 ms attack, 80 ms release) and publishes it; SF ALLOC decides cuts; each source applies its own cuts (bell EQ per band, gliding at 60 dB/s). The audio never leaves its track.
- A global order of sources (drag in SF ALLOC's list; top = most important), saved with the set by name, changed only by dragging. Any band can have its own order instead [decided Nov 3: different frequencies want different priorities]: click the band, drag (band marked OWN); LINK TO ALL returns it to the global order. Newcomers join the end of every order. Names: Live's track name (VST3 passes it; AU does not), else the module/role name; duplicates get " 2", " 3".
- Per band, a priority curve over the order: PRIORITY = position of the peak (0 top .. 100 % bottom), DEPTH = how hard sources away from the peak are cut (x MAX CUT; 0 = untouched), Q = width (Gaussian, sigma 3 rows at 0 .. 0.3 rows at 100 %). cut_i = MAX CUT x DEPTH x (1 - weight_i).
- MODE DYNAMIC (default): cut_i is scaled by how loudly the most active more-favoured source is playing in that band (above the SENSITIVITY floor, full at +18 dB). STATIC: always cut.
- Display: list left; per band a column with one lane per source (white = what plays, amber = what is cut), the curve (grey; cyan while its knob is touched), the owning source's name, and PRIORITY / DEPTH / Q knobs drawn in the column. The 30 band knobs are hidden from the generic grid but automatable.
- Replaces the conflict-slot design (Nov 2), which was unpredictable: hidden role priority, load-order ties, and knobs that rebound to whichever conflict was worst.

# Modules

## Instruments (MIDI tracks)
Mono models (BASS, ACID, MONO, MS20, HOOVER, SYNC): DENSITY echoes never interrupt a note that is still held; they fill the gaps between notes.

Every instrument has a MODEL menu (header), a SAMPLE model (drop a file or LOAD; embedded in the set as FLAC), DENSITY echoes, GATE, bipolar FILTER, ALLOC participation (ALLOC toggle), a LEVEL, and KEY FOLLOW (follows SF CONTROL's KEY; on by default) [Nov 3]. Every model is calibrated to the role's loudness target (Gain staging).

DENSITY (instruments): fills a sparse pattern with ghost notes. 0-50 %: one echo one ECHO SPACE later fades in (1/8 hats become 1/16 hats with ECHO SPACE 1/16). 50-100 %: two more ghosts at half spacing (1/32 rolls). Each generation is quieter, darker and shorter. ECHO SWING, ECHO PROB where useful.

GATE (instruments): 0 = clipped staccato, 50 % = as played, 100 % = held long (release up to 8x).

### SF KICK
Models: KICK 909-TYPE, KICK 808-TYPE, KICK SINE, KICK INDUSTRIAL, KICK MODULAR, KICK DUB, SAMPLE. Row: TUNE, DECAY, PUNCH (pitch-sweep depth), CLICK, DRIVE, FILTER, DENSITY, LEVEL. Also: LAYER MODEL + LAYER (blend a second model, e.g. 909 click on an 808 body), PITCH TIME, VEL SENS, ECHO SPACE/SWING, KEY TRACK (off: any note plays the tuned kick), FILTER RES. Reports every hit (including echoes) to the bus. A sampled kick reports its analysed pitch and decay. On SAMPLE, DECAY is a gate: 10 ms at 0, the whole sample from 90 % up, exponential between, 8 ms fade. [decided Nov 2] Synthesis runs at 2x. With KEY on, the model's fundamental lands on the nearest key root and TUNE moves from there (TUNE 0 = on the root). [Nov 3]

### SF PERC
Models: HAT 909-TYPE, HAT 808-TYPE, HAT 606-TYPE, RIDE 909-TYPE, CRASH, CLAP, RIM, COWBELL, CLAVE, SHAKER, METAL FM, MODAL, SAMPLE. All synthesised (909 hats were samples in the original; these are approximations — load your own into SAMPLE for the real thing). Row: TUNE, DECAY, TONE (model brightness / ratios), DENSITY, FILTER, ACCENT (velocity depth), DRIVE, LEVEL. Also ECHO SPACE/SWING/PROB, CHOKE, SPREAD (echoes alternate left/right). On SAMPLE, DECAY gates the sample exactly as on SF KICK.

### SF RUMBLE
Kick-driven low-end bed. Listens to SF KICK on the bus (SOURCE: KICK BUS / MIDI / BOTH), excites a thump using the kick's own pitch sweep, through a model: REVERB, DELAY, SUB DRONE, DISTORTED, DUB BOOM, SAMPLE. Row: SIZE, DARK, GRIT, PUMP (pocket depth), GATE (tail cut-off after each kick), DENSITY (extra thumps between kicks), FILTER, LEVEL. Ducks around the kick with Pocket.

### SF BASS / SF LEAD / SF PAD
One synth engine, role-specific models and defaults. Row: CUTOFF, RES, ENV AMT, GATE, DENSITY, FILTER, DRIVE, LEVEL. Also DECAY, ATTACK, ACCENT, GLIDE (portamento), TUNE, DETUNE, COLOR (model-specific: sub level, osc 2 shape, drawbars, FM index, wavetable position, grain size, saw/square for acid), LAYER MODEL + LAYER, CHORD (MODEL default or MIN7 / MIN9 / SUS4 / MAJ7 / MIN / 5TH / OCT / NONE), SPACE (ensemble), POCKET DEPTH / SHAPE, ECHO SPACE / SWING.
- BASS models: BASS LADDER, MONO 101-TYPE, SUB OSC, ACID 303-TYPE, FM, SINE SUB, OFFBEAT, SAMPLE.
- LEAD models: ACID 303-TYPE, MONO 101-TYPE, MS20-TYPE, STAB DUB, STAB DETROIT, ORGAN M1-TYPE, HOOVER, FM BELL, SYNC, SAMPLE.
- PAD models: STRING MACHINE, POLY PAD, WAVETABLE, GRANULAR, DUB WASH, TAPE LOOP, NOISE BED, SAMPLE.
- ACID 303-TYPE: mono, 4-pole resonant ladder, accent from velocity > 108 (sharper, louder, shorter filter envelope; ACCENT sets how much), slide when notes overlap (legato, no retrigger). COLOR < 50 % saw, >= 50 % square.
- Mono models keep a note stack: releasing a note slides back to the one still held.
- SAMPLE: chromatic from the analysed root note. GRANULAR uses the loaded sample (or a built-in chord bed).
- Changing MODEL releases all voices.

### SF VOICE
Spoken-word / vocal phrases. A loaded phrase is sliced (SLICE MODE: AUTO onsets / EQUAL 8 / EQUAL 16 / WHOLE); C3 plays slice 1, C#3 slice 2 ...; keys below C3 play the whole phrase. Models: SAMPLE, RADIO (CB-style band-limit and dirt), VOCODER (16 bands, saw carrier at TONE pitch), ROBOT (pulse carrier, 8 kHz / 8-bit), CHOIR (vocoder with a minor-chord carrier), STUTTER (loops the start of the slice; TONE = loop length). Row: SIGNAL (clarity / sibilance), TONE, GATE, DENSITY, FILTER, ECHO (built-in dub echo send, ECHO TIME / ECHO FDBK), DRIVE, LEVEL. With no phrase loaded it plays a formant choir (TONE morphs A-E-I-O-U). drosaka Voice stays separate.

### SF STRIKE [added Nov 3]
Blast-excited resonant objects: industrial impacts, bells, sheet metal, distant detonations. Four voices ring at once: a fifth strike fades the quietest out over 5 ms (never cut); four more slots hold the fades. A pressure wave (Friedlander waveform: sharp overpressure, then a negative phase) from a charge of POWER at DISTANCE hits an OBJECT modelled as 6-16 resonant modes. The blast scales like the real thing: its duration grows with the cube root of the charge, its level falls with distance, and distance takes the highs away (air absorption). A short, close blast excites the high modes; a long, distant push the low ones. Big hits sound sharp for a moment and settle (TENSION). Above ~70 % POWER the object FRACTURES: modes split and detune, debris rattles, and the output saturates (2x oversampled; the amount follows the fractured voice's own envelope, smoothed, and fades in from clean, so nothing else ringing changes level).
- Objects: BELL, PLATE, BAR, TUBE, MEMBRANE, GONG (measured mode ratios).
- Row: POWER, DISTANCE, SIZE (pitch: bigger = lower), MATERIAL (wood to glass: how long the high modes ring), DECAY, DENSITY, FILTER, LEVEL. Also OBJECT, POSITION (strike point: mutes modes near a node), SPREAD (modes across the stereo field), TENSION, TUNE, KEY TRACK, FILTER RES, ECHO SPACE / SWING, VEL SENS (velocity scales the charge), ALLOC, KEY FOLLOW (the main partial lands on the nearest key root, TUNE moves from there; SIZE then picks the octave).
- Display: the blast's pressure-time curve for the current POWER / DISTANCE, and the object's modes.

### SF DECK
Two decks inside the set. Tracklist (drop files; persisted by path), background analysis on add/load: tempo + beat grid (onset regression, sample-accurate), integrated LUFS (BS.1770). Row: XFADE, FILTER A, FILTER B, LOW A, LOW B, ROLL, GATE (1/16 trance gate), LEVEL. Also PLAY A/B (start on the next QUANT boundary: 1 BEAT / 1 BAR / 4 / 8 / 16 BAR), MID/HIGH A/B (isolator: 0 = kill, 75 % = 0 dB, 100 % = +6 dB), KEY LOCK (Signalsmith Stretch, MIT licence; off = varispeed), CUE A/B (to the CUE output, pre-crossfader, for headphones), TRIM TARGET (every track normalised to this LUFS), FILTER RES.
- Decks always follow Live's tempo. A plugin cannot change Live's tempo; use Live's tempo automation for tempo transitions. [decided Nov 2]
- Hot cues from MIDI: C1-D#1 deck A cues 1-4, C2-D#2 deck B (jump on the next beat, short fade in; ignored while the deck is stopped). Default cues at beats 0 / 32 / 64 / 96.
- ROLL: slip loop roll (the track keeps running underneath; releasing returns to where it would have been). The rolled audio is read at tempo ratio, so with a large tempo difference it is slightly pitched (124 vs 128 BPM = about half a semitone).
- Loading a deck uses its own queue (never waits for list analysis) and reuses the list's analysis. Tracks are freed on the message thread, never on the audio thread.
- Transport jumps: playing decks keep running and are re-anchored by whole beats (stay on Live's grid). Beat grids are cached in seconds, so a sample-rate change keeps them valid. [decided Nov 2]
- Track log: every deck start is appended to Documents/SF Live Suite/tracklog.txt.
- Stems: use Live's stem separation, stems on their own tracks with SF TAP. [decided Nov 2]

### SF CONTROL
On its own MIDI track receiving the OXI One. MACRO 1-6 (published to every SF plugin), SONG (01-16), STORE ALL, PC CHANNEL (default 16), PC ENABLE, POCKET OFFSET, KEY (OFF, C ... B; per song) and PANIC (hold: every SF plugin fades out and clears) [Nov 3]. Display: macro levels, song, key, bus state, kick reporting, ALLOC running, connected sources. Makes no sound.

## Effects (audio tracks / groups / returns)
- SF STRIP [added Nov 2]: the performance row for any track (Live instruments, samplers, drum racks, hardware). Audio only (Live sends no MIDI to effects). Chain GATE > DENSITY > DRIVE > FILTER > SPACE > POCKET > ALLOC > LEVEL. Row: DENSITY (audio ghost echoes as SF DENSITY), GATE (GATE MODE HITS: each detected hit stays open 1/64 beat to 2 beats, 97 %+ never closes; PATTERN: rhythmic gate, GATE = step length), FILTER, DRIVE, POCKET, SPACE (send to its own dub echo or reverb), THROW, LEVEL. Setup: ROLE, ALLOC, ECHO SPACE, ECHO PROB, DARKEN, GATE MODE, PATTERN, RATE, CHARACTER, FILTER RES, POCKET SHAPE, SPACE TYPE, SPACE SIZE, SENSITIVITY. SF TAP stays as the no-sound option.
- SF FILTER: FILTER (bipolar, 24 dB/oct), RES, DRIVE, ENV AMT (+/- envelope follow), ENV SPEED, THROW (hold: ramps to the extreme over THROW TIME, returns in a quarter of that; THROW TO HP/LP), LEVEL.
- SF GATE: DEPTH, LENGTH, SHAPE (hard to soft), SWING, PATTERN A, PATTERN B, MORPH (blend A to B), RATE (1/8, 1/16, 1/32, 1/16T). 16 patterns.
- SF DENSITY: the send-delay hat trick as a plugin. DENSITY, SPACE (1/8 ... 1/8D), DARKEN, SWING, PROB (per incoming hit, whether its echoes play), MIX, LEVEL, SHORTEN (echoes carry only the start of each hit).
- SF DELAY: tape-style dub delay. TIME (1 BAR ... 1/32; changing it slews the read head = pitch bend, SLEW sets how long), FEEDBACK (to 115 %, saturating), TONE (bipolar, inside the feedback loop: every repeat darker or thinner than the last) [Nov 3], DRIVE (inside the loop, 2x oversampled), THROW (hold to send), FREEZE, MIX, WIDTH. Also MODE (STEREO / PING-PONG), INPUT (ALWAYS / THROW ONLY), DUCK + DUCK SHAPE (Pocket on the repeats), WOW, LO CUT.
- SF REVERB: SIZE, DECAY, DARK, PREDELAY (synced), DUCK (Pocket + dry-follow: tails bloom in the gaps), FREEZE, THROW, MIX. Also MOD, WIDTH, GATED + GATE TIME, LO CUT, DUCK SHAPE, INPUT. 8-line feedback delay network with input diffusion and two-band absorption per line: DECAY sets the low-frequency RT60 exactly, DARK shortens the highs only [Nov 3].
- SF DRIVE: DRIVE, CHARACTER (one knob TAPE > TUBE > FUZZ > INDUSTRIAL), TONE (tilt), LOW KEEP (lows below this stay clean), MIX, LEVEL, AUTO GAIN (per-sample loudness match). 4x oversampled.
- SF RADIO: SIGNAL (one knob from lost to clear), BAND, WIDTH, NOISE, DROPOUT, SQUELCH (burst after dropouts), RING, MIX, CRUSH, RATE.
- SF REPEAT: ROLL (hold), LENGTH, DECAY, PITCH (drop over repeats), GATE, FILTER (darkens over repeats), MIX, QUANT (start on 1/16, 1/8, 1/4 or at once).
- SF DUCK: DEPTH, SHAPE, SOURCE (KICK BUS / SIDECHAIN / GRID), RATE + CURVE (GRID volume shape), MIX.
- SF ENERGY: group energy (replaces the DJ filter on groups). ENERGY (bipolar, centre neutral). Down: low-pass + space (internal reverb) + softer. Up: high-pass with resonant edge + drive + 1/16 echo + chop. PROFILE (DRUMS / BASS / MUSIC / VOICE / MASTER) sets how far each side travels. RES, DRIVE AMT, SPACE AMT, ECHO AMT, GATE AMT, LEVEL.

## Master / mix
- SF ISOLATOR: LOW, MID, HIGH (0 = kill, 75 % = 0 dB, 100 % = +6 dB), LOW/MID/HIGH KILL, XOVER LO, XOVER HI. Linkwitz-Riley, phase-aligned.
- SF BUILD: BUILD (one knob: high-pass sweep to HP MAX, noise riser, reverb wash, width), DROP (hold: the build resets on the next bar line; the build stays off until BUILD is turned below 5 %), NOISE, SPACE, WIDTH, HP MAX, LEVEL.
- SF GLUE: PUSH (one knob: threshold + makeup), ATTACK, RELEASE (incl. AUTO dual-stage), RATIO, SC HPF, MIX, LEVEL. 6 dB soft knee [Nov 3]. Gain-reduction trace.
- SF TONE [added Nov 3]: master voicing EQ, five broad moves on one row: LOW (shelf at 70 Hz, +/-6 dB), MUD (cut at 280 Hz, 0-6 dB), PRESENCE (3.2 kHz, +/-4 dB), AIR (shelf at 12 kHz, +/-6 dB), TILT (+/-3 dB around 900 Hz), LEVEL. Runs at 2x so the air shelf keeps its analog shape up to 20 kHz. Gentle by design: the ranges stop before damage.
- SF LOWEND: MONO FREQ (mono below), SUB CLIP (2x oversampled), RUMBLE CUT (24 dB/oct), WIDTH (above), LOW TRIM, LEVEL.
- SF LIMIT [rebuilt Nov 3]: TARGET (LUFS), CEILING (dBTP), CLIP, INPUT, RELEASE, AUTO (moves input <= 1 dB/s so short-term loudness sits at TARGET; +/-12 dB), RESET. Chain: INPUT (+ AUTO) > soft-knee CLIP at 4x through linear-phase FIR oversampling > true-peak lookahead limiter > CEILING. Detection: 8x true peak (32-tap windowed sinc) of the clipped signal. Gain: minimum over a 2 ms lookahead, program-dependent release (fast at RELEASE, held by a slow envelope under sustained limiting: no pumping on dense material, quick recovery after a lone peak), then a 2 ms moving average that always reaches each peak's gain before the peak leaves the delay line. Output true peak never exceeds CEILING (measured). Latency 176 samples at 48 kHz (reported to Live). AUTO measures the raw input, so the trim it needs is known directly and followed at <= 1 dB/s: no loop around the 3 s meter, no overshoot. Display: momentary, short-term, integrated (BS.1770-4 gated), true peak, GR, auto input.
- SF ROOM EQ: six cut-only bands (CUT 1-6, FREQ 1-6, shared Q), VENUE (1-16) recalls stored venues, STORE VENUE. Not part of songs.
- SF ALLOC: ENABLE, MODE, MAX CUT, SENSITIVITY, and PRIORITY / DEPTH / Q for each of the 10 bands (in the window). See ALLOC above.
- SF TAP: ROLE, ALLOC, POCKET DEPTH, POCKET SHAPE, LEVEL. Makes no sound of its own; puts any track into Pocket and ALLOC.

# Suggested Live layout
MIDI tracks: SF CONTROL, SF KICK, SF PERC x2-3, SF RUMBLE, SF BASS, SF LEAD x1-2, SF VOICE, SF PAD, SF STRIKE, SF DECK. Groups with SF ENERGY. Returns: SF DELAY (INPUT THROW ONLY or as a send), SF REVERB, SF DENSITY. Master: SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND > SF ROOM EQ > SF LIMIT, with SF ALLOC anywhere on the master. Stems / hardware tracks: SF TAP or SF STRIP.

Recording a set for release: record the master (post-LIMIT) in Live at the session rate. LIMIT's integrated meter covers the whole set; RESET it at the start.

# Open questions (need Live)
1. Shared memory between plugin binaries in Live 12 on macOS (expected to work: plugins share one process unless sandboxed; also works across processes). SF CONTROL's BUS line shows SHARED.
2. Pocket timing with Live's delay compensation: kick and duckers should line up with 512-sample lookahead at buffer sizes up to 512. If pockets land early/late, trim POCKET OFFSET on SF CONTROL.
3. Live listing VST3 instruments' extra output (DECK CUE) in Audio From.
4. Sound of every model by ear. Levels across models are calibrated offline (Gain staging); confirm by ear.
5. CPU of a full set at 96 kHz on the M4 Max (offline figures in README).
6. Live's delay compensation with SF LIMIT's 176-sample latency on the master (expected: compensated automatically).
