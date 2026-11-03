SF Live Suite — Spec v1
Nov 2, 2026 · Wiley
Source of truth. Decisions are dated inline. Builds are tested as whole suites only.

# Purpose

A suite of Ableton Live plugins (AU + VST3, macOS, Apple Silicon) for performing hypnotic / dub techno live: very simple note content (often one note, fixed patterns), with all variation played on knobs. Designed from the start for a fixed live rig: one Live set that never changes, songs switched by preset, the OXI One MkII sequencing, the OXI E16 knob array (absolute 0-127 CC with value feedback) playing.

# Standards

## Visual
Black, white and greys. Colour only carries information:
- RED: clip / fault / missing file
- AMBER: conflict / warning / armed-but-waiting states that need attention
- GREEN: active / locked to the grid / source connected
- CYAN: the value of the control being touched

Monospace type, 1 px rules, no decoration, no gradients, no shadows.

## Naming
Boring, practical, uppercase. Plugins appear in Live as `SF <ROLE>`. Parameters are plain words (`DENSITY`, `GATE`, `CUTOFF`, `DEPTH`). Models are descriptive (`KICK 909-TYPE`, `ACID 303-TYPE`, `STAB DUB`). Trademarked machine names appear only as "-TYPE" descriptions.

## Controls
- The first 8 parameters of every plugin are its performance row (Live's device view, one E16 row). Everything else is set-and-forget.
- Every knob is smoothed (30 ms) so 7-bit CC never steps audibly; ranges and curves are tuned so the whole travel is usable.
- Standard trio wherever it applies: DENSITY (how many events), GATE (how long each lasts), FILTER (bipolar DJ filter: left low-pass, centre open, right high-pass).
- System controls on every plugin (appended, never in the first 8):
  - SLOW: OFF / 1-64 BAR. When on, every performance-row knob travels to where it is set over exactly that many bars, however far it moved (with SLOW at 32 BAR a quarter turn becomes a 32-bar rise). Song recalls glide the same way. MACRO moves stay immediate. The knob shows "GLIDE now>target" in green while travelling. [decided Nov 2: fixed time, performance row only]
  - MACRO SRC (OFF, MACRO 1-6) and MACRO AMT (-100..+100 %): the plugin follows one of SF CONTROL's six macros. Each module decides which knobs respond and in which direction.
- Nothing can hurt the PA: resonance is level-compensated, feedback above 100 % saturates instead of exploding, every output is clamped at +12 dBFS and NaN/Inf are replaced with silence (the module resets).

## Live / controller workflow (decided Nov 2)
- E16 is mapped with Live's MIDI Map (Cmd-M) with Remote on for its output, so knob rings follow song and value changes. The plugins report every recalled value back to Live.
- Song change: Program Change on MIDI channel 16 into SF CONTROL (channel selectable). Recall lands on the next bar line while Live plays, at once when stopped. VST3 has no Program Change event: SF CONTROL exposes 16 programs (SONG 01-16) and Live's program change selects the song; the AU also reads Program Change from MIDI. [decided Nov 2, needs Live test]
- Stems from Live's own stem separation go on their own tracks; SF TAP on each puts them into Pocket and ALLOC.

# Shared architecture

## SF BUS
One block of POSIX shared memory (`/sf_live_suite_bus_v2`) mapped by every SF plugin, across plugin binaries and processes. Lock-free atomics only. A segment of the wrong size or version (another build) is never mapped; that plugin falls back to a local bus. Verified Nov 2: two separately built plugin binaries (SF KICK, SF DUCK) loaded in a VST3 host share the bus and Pocket works between them. Carries: macros, song select / store requests, kick reports, ALLOC source energies and cuts, conflict-slot bindings. If shared memory is unavailable the bus falls back to process-local (plugins in one process still talk); SF CONTROL shows BUS SHARED / LOCAL ONLY.

## Clock
Live's transport when playing. When stopped, a wall clock at the last tempo stands in, so tempo-synced motion keeps running and every SF plugin agrees on the beat.

## Songs
Every plugin stores 16 song slots of its own parameter values (with the Live set). SF CONTROL selects the song (SONG knob/menu or Program Change); each plugin recalls its slot on the next bar. STORE ALL on SF CONTROL stores every plugin's current values into the selected song; STORE on any plugin stores just that plugin. Empty slots leave the plugin as it is. Arrangement automation wins over recalled values (as on any device). SLOW applies to recalls, so a song change can glide.

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
- One user order of sources (drag in SF ALLOC's list; top = most important), saved with the set by name, changed only by dragging. Names: Live's track name (VST3 passes it; AU does not), else the module/role name; duplicates get " 2", " 3".
- Per band, a priority curve over the order: PRIORITY = position of the peak (0 top .. 100 % bottom), DEPTH = how hard sources away from the peak are cut (x MAX CUT; 0 = untouched), Q = width (Gaussian, sigma 3 rows at 0 .. 0.3 rows at 100 %). cut_i = MAX CUT x DEPTH x (1 - weight_i).
- MODE DYNAMIC (default): cut_i is scaled by how loudly the most active more-favoured source is playing in that band (above the SENSITIVITY floor, full at +18 dB). STATIC: always cut.
- Display: list left; per band a column with one lane per source (white = what plays, amber = what is cut), the curve (grey; cyan while its knob is touched), the owning source's name, and PRIORITY / DEPTH / Q knobs drawn in the column. The 30 band knobs are hidden from the generic grid but automatable.
- Replaces the conflict-slot design (Nov 2), which was unpredictable: hidden role priority, load-order ties, and knobs that rebound to whichever conflict was worst.

# Modules

## Instruments (MIDI tracks)
Mono models (BASS, ACID, MONO, MS20, HOOVER, SYNC): DENSITY echoes never interrupt a note that is still held; they fill the gaps between notes.

Every instrument has a MODEL menu (header), a SAMPLE model (drop a file or LOAD; embedded in the set as FLAC), DENSITY echoes, GATE, bipolar FILTER, ALLOC participation (ALLOC toggle), and a LEVEL.

DENSITY (instruments): fills a sparse pattern with ghost notes. 0-50 %: one echo one ECHO SPACE later fades in (1/8 hats become 1/16 hats with ECHO SPACE 1/16). 50-100 %: two more ghosts at half spacing (1/32 rolls). Each generation is quieter, darker and shorter. ECHO SWING, ECHO PROB where useful.

GATE (instruments): 0 = clipped staccato, 50 % = as played, 100 % = held long (release up to 8x).

### SF KICK
Models: KICK 909-TYPE, KICK 808-TYPE, KICK SINE, KICK INDUSTRIAL, KICK MODULAR, KICK DUB, SAMPLE. Row: TUNE, DECAY, PUNCH (pitch-sweep depth), CLICK, DRIVE, FILTER, DENSITY, LEVEL. Also: LAYER MODEL + LAYER (blend a second model, e.g. 909 click on an 808 body), PITCH TIME, VEL SENS, ECHO SPACE/SWING, KEY TRACK (off: any note plays the tuned kick), FILTER RES. Reports every hit (including echoes) to the bus. A sampled kick reports its analysed pitch and decay. On SAMPLE, DECAY is a gate: 10 ms at 0, the whole sample from 90 % up, exponential between, 8 ms fade. [decided Nov 2]

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
On its own MIDI track receiving the OXI One. MACRO 1-6 (published to every SF plugin), SONG (01-16), STORE ALL, PC CHANNEL (default 16), PC ENABLE. Display: macro levels, song, bus state, kick reporting, ALLOC running, connected sources. Makes no sound.

## Effects (audio tracks / groups / returns)
- SF STRIP [added Nov 2]: the performance row for any track (Live instruments, samplers, drum racks, hardware). Audio only (Live sends no MIDI to effects). Chain GATE > DENSITY > DRIVE > FILTER > SPACE > POCKET > ALLOC > LEVEL. Row: DENSITY (audio ghost echoes as SF DENSITY), GATE (GATE MODE HITS: each detected hit stays open 1/64 beat to 2 beats, 97 %+ never closes; PATTERN: rhythmic gate, GATE = step length), FILTER, DRIVE, POCKET, SPACE (send to its own dub echo or reverb), THROW, LEVEL. Setup: ROLE, ALLOC, ECHO SPACE, ECHO PROB, DARKEN, GATE MODE, PATTERN, RATE, CHARACTER, FILTER RES, POCKET SHAPE, SPACE TYPE, SPACE SIZE, SENSITIVITY. SF TAP stays as the no-sound option.
- SF FILTER: FILTER (bipolar, 24 dB/oct), RES, DRIVE, ENV AMT (+/- envelope follow), ENV SPEED, THROW (hold: ramps to the extreme over THROW TIME, returns in a quarter of that; THROW TO HP/LP), LEVEL.
- SF GATE: DEPTH, LENGTH, SHAPE (hard to soft), SWING, PATTERN A, PATTERN B, MORPH (blend A to B), RATE (1/8, 1/16, 1/32, 1/16T). 16 patterns.
- SF DENSITY: the send-delay hat trick as a plugin. DENSITY, SPACE (1/8 ... 1/8D), DARKEN, SWING, PROB (per incoming hit, whether its echoes play), MIX, LEVEL, SHORTEN (echoes carry only the start of each hit).
- SF DELAY: tape-style dub delay. TIME (1 BAR ... 1/32; changing it slews the read head = pitch bend, SLEW sets how long), FEEDBACK (to 115 %, saturating), TONE (bipolar, in the repeats), DRIVE, THROW (hold to send), FREEZE, MIX, WIDTH. Also MODE (STEREO / PING-PONG), INPUT (ALWAYS / THROW ONLY), DUCK + DUCK SHAPE (Pocket on the repeats), WOW, LO CUT.
- SF REVERB: SIZE, DECAY, DARK, PREDELAY (synced), DUCK (Pocket + dry-follow: tails bloom in the gaps), FREEZE, THROW, MIX. Also MOD, WIDTH, GATED + GATE TIME, LO CUT, DUCK SHAPE, INPUT.
- SF DRIVE: DRIVE, CHARACTER (one knob TAPE > TUBE > FUZZ > INDUSTRIAL), TONE (tilt), LOW KEEP (lows below this stay clean), MIX, LEVEL, AUTO GAIN. 4x oversampled.
- SF RADIO: SIGNAL (one knob from lost to clear), BAND, WIDTH, NOISE, DROPOUT, SQUELCH (burst after dropouts), RING, MIX, CRUSH, RATE.
- SF REPEAT: ROLL (hold), LENGTH, DECAY, PITCH (drop over repeats), GATE, FILTER (darkens over repeats), MIX, QUANT (start on 1/16, 1/8, 1/4 or at once).
- SF DUCK: DEPTH, SHAPE, SOURCE (KICK BUS / SIDECHAIN / GRID), RATE + CURVE (GRID volume shape), MIX.
- SF ENERGY: group energy (replaces the DJ filter on groups). ENERGY (bipolar, centre neutral). Down: low-pass + space (internal reverb) + softer. Up: high-pass with resonant edge + drive + 1/16 echo + chop. PROFILE (DRUMS / BASS / MUSIC / VOICE / MASTER) sets how far each side travels. RES, DRIVE AMT, SPACE AMT, ECHO AMT, GATE AMT, LEVEL.

## Master / mix
- SF ISOLATOR: LOW, MID, HIGH (0 = kill, 75 % = 0 dB, 100 % = +6 dB), LOW/MID/HIGH KILL, XOVER LO, XOVER HI. Linkwitz-Riley, phase-aligned.
- SF BUILD: BUILD (one knob: high-pass sweep to HP MAX, noise riser, reverb wash, width), DROP (hold: the build resets on the next bar line; the build stays off until BUILD is turned below 5 %), NOISE, SPACE, WIDTH, HP MAX, LEVEL.
- SF GLUE: PUSH (one knob: threshold + makeup), ATTACK, RELEASE (incl. AUTO dual-stage), RATIO, SC HPF, MIX, LEVEL. Gain-reduction trace.
- SF LOWEND: MONO FREQ (mono below), SUB CLIP, RUMBLE CUT (24 dB/oct), WIDTH (above), LOW TRIM, LEVEL.
- SF LIMIT: TARGET (LUFS), CEILING (dBTP), CLIP, INPUT, RELEASE, AUTO (moves input <= 1 dB/s so short-term loudness sits at TARGET; +/-12 dB), RESET. 1.5 ms lookahead, inter-sample peak estimate. Display: short-term, integrated, true peak, GR, auto input.
- SF ROOM EQ: six cut-only bands (CUT 1-6, FREQ 1-6, shared Q), VENUE (1-16) recalls stored venues, STORE VENUE. Not part of songs.
- SF ALLOC: ENABLE, MODE, MAX CUT, SENSITIVITY, and PRIORITY / DEPTH / Q for each of the 10 bands (in the window). See ALLOC above.
- SF TAP: ROLE, ALLOC, POCKET DEPTH, POCKET SHAPE, LEVEL. Makes no sound of its own; puts any track into Pocket and ALLOC.

# Suggested Live layout
MIDI tracks: SF CONTROL, SF KICK, SF PERC x2-3, SF RUMBLE, SF BASS, SF LEAD x1-2, SF VOICE, SF PAD, SF DECK. Groups with SF ENERGY. Returns: SF DELAY (INPUT THROW ONLY or as a send), SF REVERB, SF DENSITY. Master: SF ISOLATOR > SF BUILD > SF GLUE > SF LOWEND > SF ROOM EQ > SF LIMIT, with SF ALLOC anywhere on the master. Stems / hardware tracks: SF TAP.

# Open questions (need Live)
1. Shared memory between plugin binaries in Live 12 on macOS (expected to work: plugins share one process unless sandboxed; also works across processes). SF CONTROL's BUS line shows SHARED.
2. Pocket timing with Live's delay compensation: kick and duckers should line up with 512-sample lookahead at buffer sizes up to 512. If pockets land early/late, trim POCKET OFFSET on SF CONTROL.
3. Live listing VST3 instruments' extra output (DECK CUE) in Audio From.
4. Sound of every model by ear; levels across models.
