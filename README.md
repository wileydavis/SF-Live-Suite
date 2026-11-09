# SF Live Suite

Ableton Live plugins for performing hypnotic / dub techno live. Private. `SPEC.md` is the source of truth.

32 plugins, one shared core: 10 instruments (KICK, PERC, RUMBLE, BASS, LEAD, PAD, VOICE, STRIKE, DECK, CONTROL), 13 effects (STRIP, FILTER, GATE, DENSITY, DELAY, REVERB, TERROIR, CAPTURE, DRIVE, RADIO, REPEAT, DUCK, ENERGY), 9 master/mix tools (ISOLATOR, BUILD, GLUE, TONE, LOWEND, LIMIT, ROOM EQ, ALLOC, TAP).

The reference manual is `docs/SF-Live-Suite-Manual.pdf`. New to the suite: `docs/course/`, nine lessons that build three songs and a recorded set, read on a phone with a spoken guide (`docs/course/README.md`).

## Build (Mac)

```
./build.sh
```

The first run installs CMake via Homebrew if needed and clones JUCE 8.0.4 into `./JUCE`. It builds every plugin as AU and VST3 and copies them into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`. Then rescan in Live (Settings > Plug-Ins > Rescan). Use AU or VST3, not both. They show up as `SF KICK`, `SF BASS` and so on.

One plugin only: `./build.sh KICK` (any module id: KICK PERC RUMBLE BASS LEAD PAD VOICE STRIKE DECK CONTROL FILTER GATE DENSITY DELAY REVERB TERROIR CAPTURE DRIVE RADIO REPEAT DUCK ENERGY ISOLATOR BUILD GLUE TONE LOWEND LIMIT ROOMEQ ALLOC TAP STRIP).

The full build compiles 32 plugins and takes a while the first time (each plugin carries the whole core). Always rebuild and install the whole suite together: plugins from different builds don't share the SF BUS (Pocket, songs, macros, KEY, PANIC).

Offline tests (no host needed):

```
cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target SFTests SFSnapshot
./build-test/SFTests_artefacts/Release/SFTests          # all checks (about a minute)
./build-test/SFTests_artefacts/Release/SFTests --quality   # sound-quality measurements only
./build-test/SFTests_artefacts/Release/SFTests --cpu       # CPU table
./build-test/SFTests_artefacts/Release/SFTests --render RADIO 10 out.wav signal=0.3 squelch=0.8   # listen to one plugin
./build-test/SFTests_artefacts/Release/SFTests --capture take.wav place.wav   # a TERROIR recording through the plugin's import: what it found, balance, bands
./build-test/SFTests_artefacts/Release/SFTests --sweep sweep.wav   # TERROIR's sweep file
./build-test/SFSnapshot_artefacts/Release/SFSnapshot    # editor images -> docs/snapshots
```

Manual: `SFTests --dump > docs/manual/specs.json`, then `python3 docs/manual/build_manual.py` (needs reportlab). The show's operator's manual (the template, the E16 scenes page by page, the OXI One MkII, performance): `python3 docs/manual/build_show_manual.py` > `docs/SF-SHOW-Operators-Manual.pdf`. Both share `docs/manual/manual_style.py`.

## Setting up the live set

The manual's chapter 7, **The show template**, is a complete show set built on this: every track and its routing, the OXI ONE's sequencers and channels, clock and latency settings, the E16's pages, a macro plan, recording and a build order. One number per track: a track's number is its MIDI channel on the ONE and the E16, and its E16 page. The two E16 scenes for it are ready to load: **`controllers/e16/SF SHOW.oxie16`** (a page per track, and PERFORM: every ENERGY, the song call, the build) and **`SF MIX.oxie16`** (levels, returns, macros), with an address map and the loading and mapping steps (`controllers/e16/README.md`). The set itself is built too: **`controllers/live/SF SHOW.als`** holds every track, group, return and SF plugin of chapter 7, the routing, every instrument's MIDI From on its OXI One port and channel, and every E16 mapping. Its README lists Live's own settings, made once per computer; then open it and play. The ONE's side is in **`controllers/oxi-one`**: `SFSTART.oxipro`, the startup project every song is copied from (four scenes of simple patterns), and a checker for any song's project.

1. **SF CONTROL** on its own MIDI track. The song (01-16) is called from the E16: encoder 15 of any SF SHOW page picks NEXT SONG, its press (GO) calls it on the next bar line. Live sends a VST3 no Program Change; an AU of SF CONTROL also takes one on channel 16 (from the OXI One, its track's MIDI From). Set each song's **KEY** here. **PANIC**, held in its window, silences every SF plugin; in a show the emergency stop is the audio interface's output knob.
2. **Instruments**, one per MIDI track: SF KICK, SF PERC (2-3), SF RUMBLE, SF BASS, SF LEAD, SF VOICE, SF PAD, SF STRIKE, SF DECK. Pick a **MODEL** (at the top of DETAIL in the plugin's window), or a starting point from **PRESET**.
3. **Groups and every instrument track** get SF ENERGY (instead of the DJ filter), last on the track. PROFILE: DRUMS / BASS / MUSIC / VOICE. The E16's ENERGY knobs (SF SHOW's PERFORM page) are theirs.
4. **Returns:** SF DELAY, SF REVERB, SF DENSITY (for the send-delay hat fills). Turn **DRY** off on a return, so it gives only the effect.
5. **Master:** SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND > SF LIMIT, plus SF ALLOC anywhere on the master. **SF ROOM EQ goes after the recording point, not on the master** (below).
6. **Live instruments, drum racks, hardware:** SF STRIP last on the track gives it the suite's performance row and puts it in Pocket and ALLOC. **Stems:** SF TAP (no sound of its own) or SF STRIP.
7. **E16:** load the SF SHOW and SF MIX scenes (`controllers/e16`). SF SHOW.als comes mapped; in a set of your own, map with Live's MIDI Map (Cmd-M). In Live's MIDI preferences turn **Remote** on for E16 Port 1, input *and output*, so rings follow song recalls and ALLOC slot changes; Ports 2 and 3 off. The first 8 controls of each plugin are its performance row.
8. **Songs:** set every plugin for a song, press **STORE ALL** on SF CONTROL: it goes into the song in force. To store into another song, choose it in **STORE TO** first (it isn't recalled, so nothing you hear changes). NEXT SONG and GO (the E16's encoder 15 on any SF SHOW page), the SONG knob, or a Program Change to an AU recall a song on the next bar.

**Recording and the room.** The recording is the set; room correction belongs to the room. Set the Main track's output (the master; Live 12 calls it Main) to an output pair you don't use. Make an audio track **PA**: *Audio From* = Main, Post Mixer (the Main output), Monitor **In**, SF ROOM EQ on it and a safety SF LIMIT after (CEILING -0.5 dB, AUTO off), *Audio To* = the PA outputs, fader at 0 dB. Record the set on an audio track **REC**: *Audio From* = Main, Post Mixer, Monitor **Off**, armed. Live records what comes into a track, before its devices, so the recording never carries the venue's correction. (Or do the venue EQ in the audio interface's own mixer, after Live.)

**DECK:** drop tracks on the DECK window (they're analysed in the background: tempo, beat grid, loudness). Click **A** or **B** on a row to load it. Turn **PLAY A** on and it starts on the next QUANT boundary, synced to Live. Hot cues: C1-D#1 (deck A), C2-D#2 (deck B). For headphone cueing, make an audio track with *Audio From* = the DECK track, choose **CUE**, and route it to your headphone outputs.

**Slow mode:** turn **SLOW ALL** on SF CONTROL (1-128 bars) and every performance knob glides to where you set it over that many bars; OFF and your moves land at once. A quarter turn becomes a long, even build. A move keeps the length it was made with, so you can set OFF for a direct move and a length for a glide without stopping a knob that is already travelling. Song recalls follow it too. A plugin's own **SLOW** (CONTROL by default) can fix its own length or OFF instead. On the E16 (the SF SHOW and SF MIX scenes), a travelling knob's ring turns green and walks with the sound until it arrives.

**Motion:** every plugin has two modulators (M1, M2: SOURCE, RATE, TARGET, DEPTH, in the MOTION group at the bottom of the window). Waves, random and drift locked to the song position, ramps over up to 64 bars, or the kick, notes or input level. Odd rates (3/16, 5/16, 7/16, dotted, triplet, 3/5/7 BAR) run against the bar for polyrhythm and line up with it again every few bars. They are saved with songs and presets, and random sources replay the same way every time the set is played.

**Places (SF TERROIR):** a convolution reverb for spaces you capture yourself. Clap in a room (or play the plugin's sweep file from any speaker) and record it with one mic or a stereo pair (a recorder's poly file as it comes: its mix tracks are skipped), drop the recording on the plugin, and the place is in one of eight PLACE slots, stored inside the Live set and recalled with songs. A finished IR file comes in as it is; IMPORT says what a file is when AUTO can't tell. SIZE, DECAY, DARK, LENGTH (gated to a note), REVERSE and START reshape it; FREEZE holds its sound. Its first eight controls are SF REVERB's, so it swaps in on B SPACE with the E16 as it is (map six controls once). **In the set (SF CAPTURE):** with the room mics on their own track (ROOM in SF SHOW) holding SF CAPTURE, TERROIR's CAPTURE button and a slot click play the sweep to a speaker on the headphone out and record the mics, 46 s with the transport stopped; the take lands in the slot as if dropped. SF CAPTURE never passes the mics.

**Presets:** the PRESET button in every header. 191 factory presets; SAVE USER PRESET writes to `~/Music/SF Live Suite/Presets/<MODULE>/`.

**Samples:** drop a file on KICK, PERC, RUMBLE, BASS, LEAD or PAD (MODEL: SAMPLE, or PAD's GRANULAR) or a phrase on VOICE, or use LOAD; it is stored inside the Live set. Set the part that plays as you would an Ableton clip's: drag the START and END lines on the display (or turn them in the SAMPLE group), and FADE IN / FADE OUT shape its ends. The instrument's own envelope (DECAY, ATTACK, GATE) still shapes each note on top. VOICE slices that part; PAD's GRANULAR takes its grains from it, and follows START and END while a chord holds.

**The plugin windows** are narrow strips (296 pixels wide), so several sit side by side. From the top: the header; the plugin's page of the E16 scenes as the E16 shows it (its own encoders lit where they sit on the E16, in the scenes' ring colours, the others dark; presses under their knobs; the scene's name beside the page's); PERFORM (the rest of the performance row); the display; DETAIL (MODEL first); and, folded, SAMPLE (the samplers' START, END and fades), MOTION, SYSTEM (SLOW, MACRO) and SETUP (ALLOC, KEY FOLLOW, DRY ...), each with its settings on its title line. Click a title line to open or fold a group; the plugin keeps it with the set. The window is never taller than your screen: when the open groups don't fit, the strip scrolls under the header (the thin bar at the right, or the wheel, which then scrolls and turns no knob). A plugin the scenes have on several tracks (LEAD, PERC, ENERGY) finds its page by its track's name, so use the VST3 for those.

## Sound quality (measured offline, every build)

| | |
|---|---|
| Oversampling (IIR halfbands, all nonlinear stages) | 118 dB rejection at 44.1 kHz, 112 dB at 48 kHz, 123 dB at 96 kHz; flat to 20 kHz within 0.0001 dB |
| Oversampling (linear-phase FIR, LIMIT clipper) | 99.5 dB rejection; whole 4x path flat to 20 kHz within 0.01 dB at 44.1, 48 and 96 kHz |
| Synth alias floor (worst case, 3-octave sweep) | ACID 94 dB, FM 97 dB, STRING MACHINE 105 dB, SUB OSC 94 dB, MS20 77 dB, SYNC 79 dB |
| LIMIT output true peak (8x, independent check) | never above CEILING: dense programme +12 dB into -1.0 gives -1.12 dBTP; noise +12 dB gives -1.02 dBTP |
| Loudness meter (BS.1770-4 / EBU R128) | -20.00 LUFS on the 1 kHz reference; EBU Tech 3341 cases 1-5 and 9 (level, both gates, short-term) within 0.02 LU |
| REVERB RT60 of the lows at DECAY 1 / 3 / 8 s | 0.99 / 3.04 / 8.00 s |
| Buffer-size independence | 32 of 32 plugins bit-identical at buffer sizes 1 to 2048 |
| Sample rates | all 32 clean at 44.1, 48 and 96 kHz |
| Gain staging | every model of every instrument at its role's target within 0.1 dB (VOICE phrase models within 0.6 dB): KICK -15, PERC -22, RUMBLE -20, BASS -18, LEAD -20, PAD -22, VOICE -20, STRIKE -20 LUFS |
| PANIC | feedback delay, 20 s reverb and a held pad silenced to -140 dB; held: -140 dB while held, back in 5 ms on release with no step, also through effects with latency |
| Song recall | on the bar line at every buffer size and song position tested, whichever order Live processes the tracks in (20 of 20, no plugin a bar late); its KEY switches on that bar too (it used to switch up to a buffer early or late); inside an Arrangement loop that never reaches the bar, on the next bar line after the jump back |
| LIMIT AUTO | settles on TARGET with no overshoot; holds through breakdowns (the drop comes back within 0.5 dB of TARGET; a 12 dB breakdown is not lifted) |
| Sample playback above the original pitch | a 17 kHz sample an octave up folds back at -107 dB (it used to come back at 14 kHz); eight octaves up costs 1.3 x what a small transposition does (it used to cost 130 x) |
| DRIVE MIX | MIX 0 is the input (0.00 dB); 50 % blend flat within 0.01 dB (dry path phase-aligned) |
| DENSITY quiet end | at the knob's second step (MIDI 2 of 127) the first ghost is 30 dB under the 50 % ghost |
| REVERB / DELAY FREEZE | a frozen loop holds its level and brightness within 0.15 dB over 14 s; freezing a playing signal adds no click (REVERB brightness unchanged, was +11 dB); TIME turned while frozen bends cleanly (was +13 dB of zipper) |
| FILTER resonance guard | hot input at full resonance: 3rd harmonic -62 dB (smooth gain, no clipper) |

## CPU

Offline, one instance at default settings, 48 kHz, buffer 256, measured on the 2-core Linux build machine (an M4 Max core is faster). Percent of one core; Live spreads tracks across cores. Double these at 96 kHz.

| Plugin | % of one core | Plugin | % of one core | Plugin | % of one core |
|---|---:|---|---:|---|---:|
| KICK | 0.8 | FILTER | 0.5 | ISOLATOR | 0.5 |
| PERC | 1.0 | GATE | 0.2 | BUILD | 1.3 |
| RUMBLE | 2.0 | DENSITY | 0.2 | GLUE | 0.3 |
| BASS | 3.4 | DELAY | 1.0 | TONE | 0.5 |
| LEAD | 1.7 | REVERB | 1.4 | LOWEND | 0.5 |
| PAD | 7.4 | DRIVE | 1.0 | LIMIT | 3.5 |
| VOICE | 1.2 | RADIO | 0.4 | ROOM EQ | 0.2 |
| STRIKE | 1.5 | REPEAT | 0.2 | ALLOC | 0.7 |
| DECK | 0.4 | DUCK | 0.4 | TAP | 0.5 |
| CONTROL | 0.1 | ENERGY | 0.6 | STRIP | 1.3 |
| | | TERROIR | 1.5 | | |
| | | CAPTURE | 0.3 | | |

A full set (CONTROL, KICK, 3 PERC, RUMBLE, BASS, 2 LEAD, VOICE, PAD, STRIKE, DECK, 4 ENERGY, DELAY, REVERB, DENSITY and the whole master chain) adds up to about 36 % of one core here (best of three runs; timings on this shared machine vary by 10-20 % from run to run).

## Status

| Milestone | State |
|---|---|
| 0 Toolchain | Written. Plugin targets build on Linux (VST3) and load in a VST3 host; **awaiting first Mac build** |
| 1 Core: bus, songs, slow mode, macros, editor | Written and passing offline tests. **Awaiting test in Live** |
| 2 Pocket (kick-aware ducking) | Written and passing offline tests. **Awaiting timing check in Live** |
| 3 ALLOC (mix assigner) + TAP | Written and passing offline tests. **Awaiting test in Live** |
| 4 Effects: FILTER GATE DENSITY DELAY REVERB DRIVE RADIO REPEAT DUCK ENERGY | Written and passing offline tests. **Awaiting ear test** |
| 5 Master: ISOLATOR BUILD GLUE LOWEND LIMIT ROOM EQ | Written and passing offline tests. **Awaiting ear test** |
| 6 Instruments: KICK PERC RUMBLE BASS LEAD PAD VOICE (all models + samplers) | Written and passing offline tests. **Awaiting ear test** |
| 7 DECK (tracklist, analysis, sync, key lock, cues, roll) | Written and passing offline tests (sample-accurate sync 128 -> 124 BPM). **Awaiting test in Live** |
| 8 Sound and reliability pass (Nov 3): oversampling everywhere, band-limited oscillators, true-peak LIMIT, BS.1770 meters, REVERB / DELAY / GLUE upgrades, gain staging, KEY, MOTION, PANIC, presets, SF STRIKE, SF TONE | Written and passing offline tests and measurements. **Awaiting ear test and test in Live** |
| 9 Second audit (Nov 3): recall across tracks, knob takeover, held PANIC, DRY on returns, sample reader, DENSITY ghosts, REPEAT, FREEZE, stuck notes, ALLOC stereo and source identity, LIMIT AUTO breakdowns, record / PA routing; then an independent review of those fixes: PANIC release and latching, KEY on the bar, recall in loops, FREEZE edges, sample reader CPU | Fixed, each with a regression test. **Awaiting test in Live** |

### First things to check in Live

1. **The bus.** SF CONTROL's display should say **BUS SHARED**. Put SF KICK on a track and play it: CONTROL should show **KICK REPORTING**.
2. **Pocket timing.** SF KICK + SF BASS (POCKET DEPTH up, SHAPE right for an audible pump). The pump should land exactly on the kick. Live's buffer size must be 512 or less. If it's consistently early or late, trim POCKET OFFSET on SF CONTROL.
3. **ALLOC.** Put SF ALLOC on the master. Every SF source should appear in its list (by track name with VST3). Drag the order, raise DEPTH on a band where two overlap, and watch the amber.
4. **Songs.** Store two songs and switch from the E16: encoder 15 of any SF SHOW page picks NEXT SONG (SF CONTROL shows it in amber, PRESS GO), its press calls it on the next bar line. The E16 rings should follow. (Live sends a VST3 no Program Change; an AU of SF CONTROL also takes one from the OXI.)
5. **DECK.** Load a track, PLAY A: it should start on the bar, locked to Live's tempo.
6. **KEY and PANIC.** Set KEY on SF CONTROL: BASS, LEAD and PAD transpose (notes already held follow), KICK and RUMBLE retune to the root. Push SF DELAY into feedback and hold PANIC: everything goes quiet within 5 ms and stays quiet until you let go.
7. **Levels.** Default LEVELs are calibrated per role (KICK -15 LUFS ... PAD -22). Faders at 0 dB should give a balanced rough mix with headroom into GLUE and LIMIT.
8. **Song recall across tracks.** Put SF FILTER on tracks above and below SF CONTROL, store two songs with different FREQ, and switch: every track should change on the same bar. Turn a recalled knob on the E16 right after a recall: it should take over at once.
9. **A sample's part.** Drop a one-shot on SF KICK (MODEL: SAMPLE): its waveform appears under OUTPUT. Drag END in and turn FADE OUT up (the SAMPLE group): the kick gets shorter and ends without a click. On SF PAD's GRANULAR with a field recording, hold a chord and drag START: the texture wanders through the file.

### Changes in the second Nov 8 update

Rebuild the whole suite, load both new E16 scenes (`controllers/e16/SF SHOW.oxie16` and `SF MIX.oxie16`), open the new `controllers/live/SF SHOW.als`, and load the new `controllers/oxi-one/SFSTART.oxipro`: every E16 address moved, the ONE's channels moved, and the set needs SF CONTROL's two new controls.

- **One number per track.** A track's number is its MIDI channel, on the OXI One (port A) and on the E16, and its page in SF SHOW, in the set's order from the top: 1 KICK, then DRUMS (2 CLOSED HAT, 3 OPEN HAT, 4 CLAP, 5 PERC: SF STRIKE), LOW (6 BASS, 7 RUMBLE), MUSIC (8 LEAD, was ACID; 9 CHORD, was STAB; 10 PAD, was TEXTURE; 11 VOICE). Tracks are named by their role: a track keeps its name, channel and page when another instrument goes on it. The bus tracks lost their numbers (LIVE BUS, PA, REC, ROOM), so a number is always a channel; CONTROL, BRIDGE and DECK have none.
- **The ONE plays each part on its track's number, port A:** the multitrack (bottom row up) KICK 1A, PERC 5A, CLAP 4A, OPEN HAT 3A, CLOSED HAT 2A; BASS 6A, LEAD 8A, CHORD 9A, PAD 10A, VOICE 11A. Spare sequencer 7 is on 7B (DECK's hot cues), 8 on 16A (SF CONTROL, for an AU). SF START has them; a project of your own needs its channels set (`controllers/oxi-one`, the card).
- **SF SHOW, a page per track:** pages 1-11 are the tracks, titled with their names. Rows 1-2 are the instrument's own eight knobs; row 3 is GATE (DECAY where the instrument has none: every drum), DENSITY, FILTER, LEVEL; row 4 the track's SEND A and SEND B, NEXT SONG (press: GO) and BUILD (press: DROP). Knob 1's press swaps it to SLOW ALL on every page. Page 12, **PERFORM**: an ENERGY knob for every track (knob n is track n's), 12-14 the groups DRUMS, LOW, MUSIC, each press back to the centre, then NEXT SONG and BUILD.
- **SF MIX:** LEVELS (every volume in PERFORM's places, 15 and 16 the returns), SPACE (both returns, THROW and FREEZE on their presses), MACROS (MACRO 1-6, SLOW ALL, the drum roll).
- **An ENERGY knob for every track.** Every instrument track ends in SF ENERGY, with its part's profile.
- **Songs are called from the E16** (the set stays VST3: Live sends a VST3 no Program Change). Encoder 15 turns SF CONTROL's **NEXT SONG** (new): it picks the song and changes nothing else, so a knob knocked in passing never changes the song; SF CONTROL shows it in amber, PRESS GO. Its press, **GO** (new), calls it on the next bar line (Live stopped: at once); a press with nothing new picked does nothing. NEXT SONG follows the song whenever it changes another way (SONG, STORE TO, a Program Change to an AU). A ONE project and a song are now independent: any song with any project's patterns. SFTests --songgo.
- **One address rule**, so a track's page can be mapped to another instrument on that track: channel = track; in SF SHOW a knob sends its place + 11 (a track's page: CC 12-23), a press its place + 27; in SF MIX + 43 and + 59; a track's ENERGY CC 28 (the groups on 14: 28-30), its volume CC 102, sends 103 and 104. `controllers/e16/README.md`, ADDRESSES, has the table.
- **SLOW moves every knob on a track's page:** the detail knobs placed there (KICK PITCH TIME, BASS GLIDE, VOICE ECHO FDBK ...) now glide with SLOW and SLOW ALL like the rest.
- **In a set of your own:** its mappings are the old addresses and the old track names. Start from the new SF SHOW.als (built on your Live 12.4 set's format); songs you stored in your own set stay in it.

### Changes in the Nov 8 update

- **The effects that cut or start on the beat follow the groove.** Every part comes from the ONE, and the ONE plays 30-45 ms after Live's beat (more at faster tempos; MIDI Clock Sync Delay doesn't move it, and Live can't follow the ONE's clock: with EXT on it turns delay compensation off). GATE's steps, STRIP's PATTERN, ENERGY's chop, DUCK's GRID pump, REPEAT's roll start and DECK's starts, hot-cue jumps and trance gate went by Live's own grid, so they cut and started ahead of the notes. They now move their grid by where SF KICK's kicks land: the last eight kicks, once most of them agree within 4 ms (as SF BUILD's LAND ON KICK). Until then, and with no SF KICK in the set, Live's grid as before; through a breakdown, the last value. Offline, with the kicks 40 ms late: all six 40 ms later than Live's grid, and on it with the kicks on the beat (SFTests --groove).
- **Nothing in a set changes**: no new parameter, and the plugins need SF KICK on the bus (any track).

### Changes in the third Nov 7 update

- **A song change from the ONE lands on the bar the ONE changed pattern on.** SF CONTROL took a Program Change as "on the bar" only up to 30 ms after Live's bar line, and the ONE's arrive later than that (on Nov 7 its kick came 40 ms after Live's beat on the recordings; MIDI recorded from it lands 1/64 after the bar at 120 BPM, 31 ms, and 1.5/64 at 132, 43 ms): every song change from sequencer 8 would have landed a bar late. A Program Change from MIDI now counts for the bar it follows up to a sixteenth after it (125 ms at 120 BPM); a turn of SONG (knob, menu, automation) keeps 30 ms, so a hand a little late on the one still waits for the next bar. SFTests --songwindow.

### Changes in the second Nov 7 update

- **SF BUILD lands the drop on the kick it hears** (LAND ON, new, default KICK). On the show set's recordings the kick reached BUILD 80 ms after Live's beat, so the gap ended, and the bass, the pads and the last kick's tail came back, 80 ms before the kick: the drop hit like any other beat (the 80 ms before it at the groove's own level). Two halves: the ONE plays about 40 ms after Live's beat (its notes come back through USB and Live's buffers), and Live gives a plugin on the LIVE BUS a song position about 40 ms ahead of its audio. BUILD now listens to what reaches it, finds where each kick lands against Live's beat (from the last eight kicks, once most of them agree within 4 ms; with SF KICK playing only the beats it played a kick on count, with none the beats outside a build; onsets that don't keep to the beat never move it) and moves the gap and the drop by that much: the silence breaks 10 ms before the kick. Its display shows it under the bar, KICK +80 MS; KICK -- (and Live's bar line) until four kicks agree. It keeps the last value through a breakdown with no kick. LAND ON BAR LINE is the old timing.
- **The ONE's MIDI Clock Sync Delay: back to 0.** -70 ms moved the ONE's kick not at all on the recordings (37 ms after Live's beat before, 40 ms after), and nothing needs the ONE on Live's beat now: SF BUILD follows the kick, SF CONTROL takes its Program Change up to a sixteenth late as on the bar (third Nov 7 update), and Pocket follows SF KICK.
- Offline: a groove from 20 ms early to 120 ms late, the gap ends 10.8-11.1 ms before the kick, and the kick comes through whole; SF KICK 40 ms late with 42 ms more to BUILD, building all the way, 11.3 ms (SFTests --drop, 7 and 8). The groove of SFTests --dropweight 80 ms late: the 70 ms before the drop's kick at the Main went from -12.2 dB (the groove's own level, as recorded) to silence.
- **In a set of your own:** LAND ON is appended (BUILD's twelfth parameter, no E16 address), so a saved BUILD gets KICK. SF SHOW.als is rebuilt.

### Changes in the Nov 7 update

- **SF BUILD's drop hits with the kick's full weight.** DROP DUCK is off by default (it dipped the bass, rumble and pads around the drop's kicks: measured offline, 1.6 dB off the first kick's peak at the Main), and the build lets go 10 ms before the bar line, the gap's silence with it: a kick that reaches Live a little early used to lose its attack to the gap (5 ms early: -1.8 dB over its first 20 ms), or go through the build's high-pass with no gap.
- **In a set of your own:** BUILD keeps the DROP DUCK it was saved with (6 dB before today), and songs store it. Set it to 0 and STORE ALL in each song, or keep it where you like it. SF SHOW.als is rebuilt with it off.

### Changes in the third Nov 6 update

- **START, END, FADE IN and FADE OUT on every sampler** (KICK, PERC, RUMBLE, BASS, LEAD, PAD, VOICE), in a new SAMPLE group (folded) and on the display: with a sample loaded its waveform appears under the display (VOICE: on the phrase), the part that plays bright, the fades as ramps, START and END as lines you drag. The fades run in the sample's own time (an octave up, twice as fast), and a cut inside the waveform always fades for at least 1 ms, so it never clicks. DECAY's gate on KICK and PERC measures the part, and KICK tells the pockets its shorter length. VOICE slices the part (AUTO from START; EQUAL 8 / 16 divide it). PAD's GRANULAR takes its grains from it, live.
- **Nothing in a set changes:** at their defaults (0 %, 100 %, 0 ms, 0 ms) every sample plays as before, to the bit. The four are appended to each sampler's parameters, so the E16 addresses stay where they are; songs keep them; presets leave them alone (a preset has no sample of its own).
- **A drag that starts on a plugin's display stays with it** when the pointer leaves the display (ALLOC's knobs too).

### Changes in the second Nov 6 update

Fixes from an independent audit of the first Nov 6 build (revision 93d84a4). Nothing in a set changes, and the SF BUS is still v11; as always, install the whole suite from one build.

- **PANIC ends a capture asked for before it**, whatever it had reached. A take still waiting for SF CAPTURE to pick it up could start its sweep the moment PANIC was let go; a quick tap could let a take ready to start play a few ms. Live's audio restarting (a new sample rate or buffer) ends one the same way. Ask again with CAPTURE.
- **A take being written is not kept** when PANIC, a restart or STOP comes: no file, nothing taken into the slot (it used to be written and taken in). A take already written stays usable, as before.
- **A preset loaded right after a song recall keeps its own values.** Loaded (Live's browser, undo) before Live had taken the recall in, a preset whose value was the knob's from before the recall lost to the recall: it played 6 dB off, and the set saved it so. A song change still waiting for its bar line, or a STORE not yet made, in that plugin when the preset comes in is dropped there (the preset is newer); a change asked after it lands as ever. The same for a preset from the PRESET menu.
- **SF TERROIR's LENGTH is right from the first note** at Live's tempo and meter, also with a preset saved at another tempo, a set reopened in another meter, or the tempo changed while the plugin was off (the first notes kept the old length until the new place was built).
- **The plugin windows fit your screen.** With MOTION, SYSTEM and SETUP open some strips are over 1000 pixels tall (PAD 1070); the window now stops at your screen's height and the strip scrolls under the header. While it scrolls the wheel scrolls and turns no knob; a group you open at the bottom scrolls into view; a set saved with every group open reopens fitted to the screen, at the top.

### Changes in the Nov 6 update

- **New plugin windows: a narrow strip** (296 pixels wide instead of 928), ordered from what you play to what you set once. At the top, the plugin's E16 page as a 4 x 4 grid of encoders drawn like the E16's (knurled caps, 16-LED rings, the scene's colours): this plugin's encoders lit where they sit on the E16, the rest dark; THROW, FREEZE, DROP and ROLL under their knobs. Then PERFORM, the display, DETAIL (MODEL moved here from the header), and MOTION, SYSTEM and SETUP folded with their settings on their title lines. Every display was redrawn for the width (ALLOC: the order on top, the bands side by side, then the band knobs in three rows).
- **A gliding knob looks like its E16 ring:** green LEDs walking with the sound, the target LED ringed in white. The suite's GREEN is now the E16's own mint everywhere (it was a darker green).
- **Nothing in a set changes:** the same parameters, songs and presets. A window you open or fold is saved with the set; a set saved with this build opens in an older one as before.
- **SF CONTROL's display** shows the suite's state on its own (the macros are in the E16 grid above it).
- **SF TERROIR's CAPTURE** sits under the place slots, left of SWEEP FILE.

### Changes in the second Nov 5 update that affect existing sets

Fixes from an independent audit of the first Nov 5 build (revision 352df3c).

- **Rebuild and install everything.** The SF BUS moved to v11 (the capture's messages, SLOW ALL's 128 BAR); old and new builds don't talk to each other.
- **SLOW and SLOW ALL go to 128 BAR**, about four minutes at 132 BPM, for long sets. In a plugin's own SLOW menu it comes after OFF, so the settings saved in your sets stay as they are. Automation recorded on SLOW or SLOW ALL is different: Live keeps it as a position along the menu, so above 4 BAR it now lands on the next entry (SLOW ALL 64 BAR becomes 128; a plugin's own 64 BAR becomes OFF, and OFF 128 BAR). Check any you have.
- **A set saved right after a song recall** keeps the recalled sound. Saved in the moment before Live had taken the recall in, it kept the values from before it (a set could reopen 6 dB off).
- **A knob moved off a recalled value and back** before Live had taken the recall in stays where you left it (+3 then 0: it stays at 0; the recalled value used to come back).
- **SLOW ALL in a loop:** going back over the spot where SLOW ALL was changed (a loop, a jump back) uses SLOW ALL as it is; a move there used to take the setting from before the change.
- **A MODEL change with a full chord held** plays the whole chord on the new model (with every voice busy it came back as one note), and a mono model then plays the key you pressed last (it could take an older one).
- **SF TERROIR: IMPORT** (new, after DRY; in no song or preset) says what a dropped or loaded file is: AUTO (decides; the status line says what it took the file for), IR FILE (a finished IR kept as it is: every arrival, however strong), CLAP (a room recording, cut before a second clap, and the status line says where) or SWEEP (a recording of the sweep file; one sweep through a small speaker, which AUTO can take for a clap). A finished IR with a strong reflection is no longer cut short, and a status line says IR FILE or CLAP where it said IMPULSE. Places already in a set keep their audio; load an IR file again to take it in the new way.
- **A capture take is always the sweep.** If it didn't come back (the speaker, the mics, the routing) the take fails, THE SWEEP DIDN'T COME BACK, and the slot stays as it was; it used to come in as a clap with a long, wrong tail. A mic that heard nothing is left out: the place is mono, and the status line says so.
- **CAPTURE always ends:** STOP before SF CAPTURE had started the sweep stops it (it could play anyway), and a take whose SF CAPTURE is removed or stops answering ends (CAPTURE STOPPED) so the next one can start (the capture could stay busy for good, refusing every take).
- **SF TERROIR in real time** plays the knobs' place from the first note after a set opens, a preset loads or Live's audio restarts (a new sample rate or buffer: knobs turned meanwhile count); the first notes could go through the place as it was before, for a moment; a preset loaded while it rings crossfades to the new place. It stores the tempo with its places, so LENGTH's notes are right from the first note.

### Changes in the Nov 5 update that affect existing sets

Fixes from an independent audit of the Nov 4 build (revision 0fbbd7f): one gesture, one meaning; the newest intent wins.

- **Rebuild and install everything.** The SF BUS moved to v9; old and new builds don't talk to each other. **Load SF SHOW.oxie16 into the E16 again** (its script changed).
- **New: SF CAPTURE** (32 plugins), for capturing the room you play in as an SF TERROIR place without leaving Live: on the room mics' track, it plays TERROIR's sweep to a speaker and records the mics; TERROIR's new CAPTURE button asks for it (manual: SF CAPTURE; operator's manual, chapter 8, CAPTURING THE ROOM). It never passes the mics. A new plugin: nothing in an existing set changes.
- **SF SHOW.als has a new track, 94 ROOM:** the room mics (Ext. In 1/2, a stereo pair: Live's Settings > Audio > Input Config), Monitor In, SF CAPTURE, out to the headphones (3/4). Armed, it records the room with the set. Take the new set (its section 2 again: the input and the outputs are yours to check), or add the track to your own.
- **SF BUILD: one drop per press.** Holding DROP through the drop (and turning BUILD back down while still holding) used to plan a second drop, with a second GAP, on the next bar. Press again for another drop.
- **A recalled knob turned back** to where it was before the recall now sounds at once (it used to stay on the recalled value for 3 s). **STORE after a recall** stores what you hear: your newer move if you made one (STORE TO too); it used to store the recalled value.
- **A Program Change for the song already playing** cancels one still waiting for its bar.
- **SLOW ALL changed in the same instant as a move** (one audio buffer): the move takes the length from before the change, the same in every plugin (it used to depend on the order Live runs the tracks in).
- **SLOW reaches the instruments' COLOR, DETUNE, ATTACK and SPACE** (TEXTURE COLOR, DETUNE, ATTACK and STAB SPACE on the E16): with SLOW or SLOW ALL set, a move or a song recall on them now travels instead of landing in 30 ms. Menus, switches and MODEL still change at once; the operator's manual lists what glides (GLIDES).
- **A MODEL change with keys held** plays them again on the new model (a held chord goes on); it used to stop until the next note.
- **E16 knob 1's ring** no longer shows LOW ENERGY travelling: the knob swaps to SLOW ALL and the script can't tell which side it is on, so it always shows the knob's own value.
- **SF TERROIR's level is the same at every sample rate.** At 96 kHz the wet was 6 dB louder than at 48 kHz (88.2 kHz: 5.3 dB), at 44.1 kHz 0.7 dB quieter; re-balance the return if you run at those rates. A place recorded at 96 kHz is now as loud as the same room recorded at 48 kHz: 3 dB louder than before (a 44.1 kHz recording 0.4 dB quieter). Places recorded at 48 kHz, played at 48 kHz: no change.
- **SF TERROIR plays the knobs' place from the first note**, after a render, a new sample rate, a reload or with the state loaded either side of prepare (after a render it could keep an older answer for good: a short room turned long).
- **SF TERROIR imports a finished IR as it is:** from 1 ms before its first arrival (a direct sound before a stronger reflection is kept; a click at the very first sample too), later arrivals after silence kept, files of a few ms accepted; a sweep take with one channel inverted is found; a click in a long silence is a click (it was taken for a sweep). Places already in a set keep their audio; load an IR file again to take it in the new way.
- **check_set.py** now fails (exit 1) on an address mapped twice, a mapping range wrong at either end, or a relative mapping; it used to print the first two and pass.

### Changes in the Nov 4 update that affect existing sets

- **New: SF TERROIR**, a convolution reverb for places you capture (manual: SF TERROIR). It is a new plugin: nothing in an existing set changes. Rebuild and install the whole suite (31 plugins).
- **SF TERROIR's sweep file changed the same day:** its lowest octaves now come in softer, so a small speaker isn't pushed below what it can play. Use the file this build writes (SWEEP FILE on the display); a take recorded with the earlier file comes out with too much sub-bass, so record it again. Places already stored in a set are not affected.

- **SF RADIO: CRUSH and RATE follow SIGNAL.** They are the damage at SIGNAL 0; at 100 % the sound is 16-bit and full rate. A RADIO set to a constant crunch now cleans up as SIGNAL rises: lower SIGNAL (or CRUSH) for the old sound. At their defaults (16 bit, 48 kHz) nothing changes.
- **SF RADIO: SQUELCH is a squeal**, not a burst of noise: a whistle sliding down as each dropout ends, with smooth edges. Its peaks are lower (the old burst reached -2 dBFS at SQUELCH 100 %); dropouts fade over ~10 ms instead of 2 ms.
- **Longer menus: MOTION RATE** (M1 / M2 on every plugin) gains 3/16, 5/16, 7/16, 1/4D, 1/2D, 1/4T, 1/2T, 3 BAR, 5 BAR, 7 BAR; **SF DELAY TIME** gains 5/16 and 7/16. They are added at the end, so saved settings, songs and presets keep their values; an E16 knob or MIDI mapping now sweeps the longer list (DUB TIME's last two steps are the new ones). Live automation already recorded on these menus lands on different items.

### Changes in the Nov 3 update that affect existing sets

- **Rebuild and install everything.** The SF BUS moved to v4; old and new builds don't talk to each other.
- **Returns: turn DRY off.** SF DELAY, SF REVERB and SF DENSITY have a DRY switch (on by default, as before). On a return track switch it off, or the send comes back dry as well as wet.
- **PANIC is held.** While PANIC is held every SF plugin stays silent (it used to come back after 5 ms) and shows PANIC in red in its header; releasing it fades back in. A set never reopens with PANIC (or any momentary button) pressed.
- **LIMIT AUTO turns down only** (new AUTO MODE, default DOWN ONLY) and holds through breakdowns: it keeps the loudest moments of the last minute at TARGET. UP AND DOWN also lifts a quieter set, at 0.5 dB/s, but never a breakdown.
- **DENSITY ghosts are quieter at the low end.** They now rise from silence; at the same DENSITY a fill sounds softer below about 30 %. Quiet ghosts no longer cut the ringing kick or CHOKE the previous hit.
- **SF REPEAT** keeps the captured slice for the whole hold (a long roll used to pick up later input) and keeps playing the loop through the release crossfade.
- **Song recall** lands on the same bar on every track; a Program Change up to a sixteenth after a bar line, or a turn of SONG up to 30 ms after it, counts for that bar.
- **SF ALLOC's order follows each plugin, not its name.** Reopening the set or renaming a track keeps every source's place (two PERCs used to be able to swap). A duplicated track joins at the bottom.
- **Levels changed.** Models are now calibrated to role targets. At the same LEVEL, KICK is about 3-6 dB quieter, some PERC and PAD models change by several dB, and the VOICE vocoder models are much quieter than before. Re-balance faders once.
- **KICK LAYER works now.** It used to cut the main voice after 3 ms, so a layered kick was only the layer. Main and layer now sum to the level of one kick.
- **DRIVE knobs start clean.** FILTER, ENERGY and STRIP drive (and synth DRIVE at its low end) fade in from exactly clean instead of jumping +3-4 dB at the first notch.
- **KEY needs SF CONTROL running** (and notes already held follow KEY and TUNE). Without it (deleted, or another set), instruments ignore any key left over.
- **SF LIMIT latency** is now 176 samples at 48 kHz (was 72); Live compensates it. **SF DRIVE** now uses the suite's own 4x oversampler (112 dB instead of about 70 dB alias rejection); its latency is 5 samples.
- **Editors are taller:** a MOTION row and a SYSTEM row sit under each plugin's controls.
- New: PRESET menus, KEY and PANIC on SF CONTROL, KEY FOLLOW on instruments, SF STRIKE, SF TONE, FAULT indicator in every header.

## Licences

Signalsmith Stretch and Signalsmith Linear (third_party/) are MIT licensed. JUCE is used under its own licence.
