# SF Live Suite

Ableton Live plugins for performing hypnotic / dub techno live. Private. `SPEC.md` is the source of truth.

31 plugins, one shared core: 10 instruments (KICK, PERC, RUMBLE, BASS, LEAD, PAD, VOICE, STRIKE, DECK, CONTROL), 12 effects (STRIP, FILTER, GATE, DENSITY, DELAY, REVERB, TERROIR, DRIVE, RADIO, REPEAT, DUCK, ENERGY), 9 master/mix tools (ISOLATOR, BUILD, GLUE, TONE, LOWEND, LIMIT, ROOM EQ, ALLOC, TAP).

The reference manual is `docs/SF-Live-Suite-Manual.pdf`.

## Build (Mac)

```
./build.sh
```

The first run installs CMake via Homebrew if needed and clones JUCE 8.0.4 into `./JUCE`. It builds every plugin as AU and VST3 and copies them into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`. Then rescan in Live (Settings > Plug-Ins > Rescan). Use AU or VST3, not both. They show up as `SF KICK`, `SF BASS` and so on.

One plugin only: `./build.sh KICK` (any module id: KICK PERC RUMBLE BASS LEAD PAD VOICE STRIKE DECK CONTROL FILTER GATE DENSITY DELAY REVERB TERROIR DRIVE RADIO REPEAT DUCK ENERGY ISOLATOR BUILD GLUE TONE LOWEND LIMIT ROOMEQ ALLOC TAP STRIP).

The full build compiles 31 plugins and takes a while the first time (each plugin carries the whole core). Always rebuild and install the whole suite together: plugins from different builds don't share the SF BUS (Pocket, songs, macros, KEY, PANIC).

Offline tests (no host needed):

```
cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target SFTests SFSnapshot
./build-test/SFTests_artefacts/Release/SFTests          # all checks (about a minute)
./build-test/SFTests_artefacts/Release/SFTests --quality   # sound-quality measurements only
./build-test/SFTests_artefacts/Release/SFTests --cpu       # CPU table
./build-test/SFTests_artefacts/Release/SFTests --render RADIO 10 out.wav signal=0.3 squelch=0.8   # listen to one plugin
./build-test/SFSnapshot_artefacts/Release/SFSnapshot    # editor images -> docs/snapshots
```

Manual: `SFTests --dump > docs/manual/specs.json`, then `python3 docs/manual/build_manual.py` (needs reportlab). The show's operator's manual (the template, the E16 scene page by page, the OXI One MkII, performance): `python3 docs/manual/build_show_manual.py` > `docs/SF-SHOW-Operators-Manual.pdf`. Both share `docs/manual/manual_style.py`.

## Setting up the live set

The manual's chapter 7, **The show template**, is a complete show set built on this: every track and its routing, the OXI ONE's sequencers and channels, clock and latency settings, the E16's permanent top half and its pages, a macro plan, recording and a build order. The E16 scene for it is ready to load: **`controllers/e16/SF SHOW.oxie16`**, with an address map and the loading and mapping steps (`controllers/e16/README.md`). The set itself is built too: **`controllers/live/SF SHOW.als`** holds every track, group, return and SF plugin of chapter 7, the routing and every E16 mapping; its README lists the steps left to you (MIDI From, outputs, two computer keys, two options).

1. **SF CONTROL** on its own MIDI track, input from the OXI One. Program Change on channel 16 selects the song (01-16); as VST3 this needs Live 12.2 or later (the AU reads it in any version). Set each song's **KEY** here; map **PANIC** to a computer key (Key Map) or a dedicated button you can find in the dark, never to a knob's press. Everything stays silent while PANIC is on.
2. **Instruments**, one per MIDI track: SF KICK, SF PERC (2-3), SF RUMBLE, SF BASS, SF LEAD, SF VOICE, SF PAD, SF STRIKE, SF DECK. Pick models in the header menu, or a starting point from **PRESET**.
3. **Groups** get SF ENERGY (instead of the DJ filter). PROFILE: DRUMS / BASS / MUSIC / VOICE.
4. **Returns:** SF DELAY, SF REVERB, SF DENSITY (for the send-delay hat fills). Turn **DRY** off on a return, so it gives only the effect.
5. **Master:** SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND > SF LIMIT, plus SF ALLOC anywhere on the master. **SF ROOM EQ goes after the recording point, not on the master** (below).
6. **Live instruments, drum racks, hardware:** SF STRIP last on the track gives it the suite's performance row and puts it in Pocket and ALLOC. **Stems:** SF TAP (no sound of its own) or SF STRIP.
7. **E16:** load the SF SHOW scene (`controllers/e16`). SF SHOW.als comes mapped; in a set of your own, map with Live's MIDI Map (Cmd-M). In Live's MIDI preferences turn **Remote** on for E16 Port 2, input *and output*, so rings follow song recalls and ALLOC slot changes; Ports 1 and 3 off. The first 8 controls of each plugin are its performance row.
8. **Songs:** set every plugin for a song, press **STORE ALL** on SF CONTROL: it goes into the song in force. To store into another song, choose it in **STORE TO** first (it isn't recalled, so nothing you hear changes). Program Change (or the SONG knob) recalls a song on the next bar.

**Recording and the room.** The recording is the set; room correction belongs to the room. Set the Main track's output (the master; Live 12 calls it Main) to an output pair you don't use. Make an audio track **PA**: *Audio From* = Resampling (the Main output), Monitor **In**, SF ROOM EQ on it, *Audio To* = the PA outputs. Record the set on an audio track **REC**: *Audio From* = Resampling, Monitor **Off**, armed. Live records what comes into a track, before its devices, so the recording never carries the venue's correction. (Or do the venue EQ in the audio interface's own mixer, after Live.)

**DECK:** drop tracks on the DECK window (they're analysed in the background: tempo, beat grid, loudness). Click **A** or **B** on a row to load it. Turn **PLAY A** on and it starts on the next QUANT boundary, synced to Live. Hot cues: C1-D#1 (deck A), C2-D#2 (deck B). For headphone cueing, make an audio track with *Audio From* = the DECK track, choose **CUE**, and route it to your headphone outputs.

**Slow mode:** turn **SLOW ALL** on SF CONTROL (1-64 bars) and every performance knob glides to where you set it over that many bars; OFF and your moves land at once. A quarter turn becomes a long, even build. A move keeps the length it was made with, so you can set OFF for a direct move and a length for a glide without stopping a knob that is already travelling. Song recalls follow it too. A plugin's own **SLOW** (CONTROL by default) can fix its own length or OFF instead. On the E16 (the SF SHOW scene), a travelling knob's ring turns green and walks with the sound until it arrives.

**Motion:** every plugin has two modulators (M1, M2: SOURCE, RATE, TARGET, DEPTH in the MOTION row). Waves, random and drift locked to the song position, ramps over up to 64 bars, or the kick, notes or input level. Odd rates (3/16, 5/16, 7/16, dotted, triplet, 3/5/7 BAR) run against the bar for polyrhythm and line up with it again every few bars. They are saved with songs and presets, and random sources replay the same way every time the set is played.

**Places (SF TERROIR):** a convolution reverb for spaces you capture yourself. Clap in a room (or play the plugin's sweep file from any speaker) and record it with one mic or a stereo pair (a recorder's poly file as it comes: its mix tracks are skipped), drop the recording on the plugin, and the place is in one of eight PLACE slots, stored inside the Live set and recalled with songs. SIZE, DECAY, DARK, LENGTH (gated to a note), REVERSE and START reshape it; FREEZE holds its sound. Its first eight controls are SF REVERB's, so it swaps in on B SPACE with the E16 as it is (map six controls once).

**Presets:** the PRESET button in every header. 191 factory presets; SAVE USER PRESET writes to `~/Music/SF Live Suite/Presets/<MODULE>/`.

## Sound quality (measured offline, every build)

| | |
|---|---|
| Oversampling (IIR halfbands, all nonlinear stages) | 118 dB rejection at 44.1 kHz, 112 dB at 48 kHz, 123 dB at 96 kHz; flat to 20 kHz within 0.0001 dB |
| Oversampling (linear-phase FIR, LIMIT clipper) | 99.5 dB rejection; whole 4x path flat to 20 kHz within 0.01 dB at 44.1, 48 and 96 kHz |
| Synth alias floor (worst case, 3-octave sweep) | ACID 94 dB, FM 97 dB, STRING MACHINE 105 dB, SUB OSC 94 dB, MS20 77 dB, SYNC 79 dB |
| LIMIT output true peak (8x, independent check) | never above CEILING: dense programme +12 dB into -1.0 gives -1.12 dBTP; noise +12 dB gives -1.02 dBTP |
| Loudness meter (BS.1770-4 / EBU R128) | -20.00 LUFS on the 1 kHz reference; EBU Tech 3341 cases 1-5 and 9 (level, both gates, short-term) within 0.02 LU |
| REVERB RT60 of the lows at DECAY 1 / 3 / 8 s | 0.99 / 3.04 / 8.00 s |
| Buffer-size independence | 31 of 31 plugins bit-identical at buffer sizes 1 to 2048 |
| Sample rates | all 30 clean at 44.1, 48 and 96 kHz |
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
4. **Songs.** Store two songs and switch with Program Change from the OXI. The E16 rings should follow. With the VST3, Live 12.2 and later deliver Program Change as a program change (SF CONTROL lists SONG 01-16); with an older Live, or if it doesn't arrive, use the AU.
5. **DECK.** Load a track, PLAY A: it should start on the bar, locked to Live's tempo.
6. **KEY and PANIC.** Set KEY on SF CONTROL: BASS, LEAD and PAD transpose (notes already held follow), KICK and RUMBLE retune to the root. Push SF DELAY into feedback and hold PANIC: everything goes quiet within 5 ms and stays quiet until you let go.
7. **Levels.** Default LEVELs are calibrated per role (KICK -15 LUFS ... PAD -22). Faders at 0 dB should give a balanced rough mix with headroom into GLUE and LIMIT.
8. **Song recall across tracks.** Put SF FILTER on tracks above and below SF CONTROL, store two songs with different FREQ, and switch: every track should change on the same bar. Turn a recalled knob on the E16 right after a recall: it should take over at once.

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
- **Song recall** lands on the same bar on every track; a recall sent within 30 ms after a bar line counts for that bar.
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
