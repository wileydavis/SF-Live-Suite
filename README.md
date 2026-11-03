# SF Live Suite

Ableton Live plugins for performing hypnotic / dub techno live. Private. `SPEC.md` is the source of truth.

30 plugins, one shared core: 10 instruments (KICK, PERC, RUMBLE, BASS, LEAD, PAD, VOICE, STRIKE, DECK, CONTROL), 11 effects (STRIP, FILTER, GATE, DENSITY, DELAY, REVERB, DRIVE, RADIO, REPEAT, DUCK, ENERGY), 9 master/mix tools (ISOLATOR, BUILD, GLUE, TONE, LOWEND, LIMIT, ROOM EQ, ALLOC, TAP).

The reference manual is `docs/SF-Live-Suite-Manual.pdf`.

## Build (Mac)

```
./build.sh
```

The first run installs CMake via Homebrew if needed and clones JUCE 8.0.4 into `./JUCE`. It builds every plugin as AU and VST3 and copies them into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`. Then rescan in Live (Settings > Plug-Ins > Rescan). Use AU or VST3, not both. They show up as `SF KICK`, `SF BASS` and so on.

One plugin only: `./build.sh KICK` (any module id: KICK PERC RUMBLE BASS LEAD PAD VOICE STRIKE DECK CONTROL FILTER GATE DENSITY DELAY REVERB DRIVE RADIO REPEAT DUCK ENERGY ISOLATOR BUILD GLUE TONE LOWEND LIMIT ROOMEQ ALLOC TAP STRIP).

The full build compiles 30 plugins and takes a while the first time (each plugin carries the whole core). Always rebuild and install the whole suite together: plugins from different builds don't share the SF BUS (Pocket, songs, macros, KEY, PANIC).

Offline tests (no host needed):

```
cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target SFTests SFSnapshot
./build-test/SFTests_artefacts/Release/SFTests          # all checks (about a minute)
./build-test/SFTests_artefacts/Release/SFTests --quality   # sound-quality measurements only
./build-test/SFTests_artefacts/Release/SFTests --cpu       # CPU table
./build-test/SFSnapshot_artefacts/Release/SFSnapshot    # editor images -> docs/snapshots
```

Manual: `SFTests --dump > docs/manual/specs.json`, then `python3 docs/manual/build_manual.py` (needs reportlab).

## Setting up the live set

1. **SF CONTROL** on its own MIDI track, input from the OXI One. Program Change on channel 16 selects the song (01-16). Set each song's **KEY** here; map **PANIC** to a button you can find in the dark.
2. **Instruments**, one per MIDI track: SF KICK, SF PERC (2-3), SF RUMBLE, SF BASS, SF LEAD, SF VOICE, SF PAD, SF STRIKE, SF DECK. Pick models in the header menu, or a starting point from **PRESET**.
3. **Groups** get SF ENERGY (instead of the DJ filter). PROFILE: DRUMS / BASS / MUSIC / VOICE.
4. **Returns:** SF DELAY, SF REVERB, SF DENSITY (for the send-delay hat fills).
5. **Master:** SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND > SF ROOM EQ > SF LIMIT, plus SF ALLOC anywhere on the master.
6. **Live instruments, drum racks, hardware:** SF STRIP last on the track gives it the suite's performance row and puts it in Pocket and ALLOC. **Stems:** SF TAP (no sound of its own) or SF STRIP.
7. **E16:** map with Live's MIDI Map (Cmd-M). In Live's MIDI preferences turn **Remote** on for the E16 *output*, so rings follow song recalls and ALLOC slot changes. The first 8 controls of each plugin are its performance row.
8. **Songs:** set every plugin for a song, choose the song number on SF CONTROL, press **STORE ALL**. Program Change (or the SONG knob) recalls it on the next bar.

**DECK:** drop tracks on the DECK window (they're analysed in the background: tempo, beat grid, loudness). Click **A** or **B** on a row to load it. Turn **PLAY A** on and it starts on the next QUANT boundary, synced to Live. Hot cues: C1-D#1 (deck A), C2-D#2 (deck B). For headphone cueing, make an audio track with *Audio From* = the DECK track, choose **CUE**, and route it to your headphone outputs.

**Slow mode:** set **SLOW** on any plugin (1-64 bars). Every performance knob then glides to where you set it over that many bars. A quarter turn becomes a long, even build.

**Motion:** every plugin has two modulators (M1, M2: SOURCE, RATE, TARGET, DEPTH in the MOTION row). Waves, random and drift locked to the song position, ramps over up to 64 bars, or the kick, notes or input level. They are saved with songs and presets, and random sources replay the same way every time the set is played.

**Presets:** the PRESET button in every header. 181 factory presets; SAVE USER PRESET writes to `~/Music/SF Live Suite/Presets/<MODULE>/`.

## Sound quality (measured offline, every build)

| | |
|---|---|
| Oversampling (IIR halfbands, all nonlinear stages) | 118 dB rejection at 44.1 kHz, 112 dB at 48 kHz, 123 dB at 96 kHz; flat to 20 kHz within 0.0001 dB |
| Oversampling (linear-phase FIR, LIMIT clipper) | 99.5 dB rejection; whole 4x path flat to 20 kHz within 0.01 dB at 44.1, 48 and 96 kHz |
| Synth alias floor (worst case, 3-octave sweep) | ACID 94 dB, FM 97 dB, STRING MACHINE 105 dB, SUB OSC 94 dB, MS20 77 dB, SYNC 79 dB |
| LIMIT output true peak (8x, independent check) | never above CEILING: dense programme +12 dB into -1.0 gives -1.12 dBTP; noise +12 dB gives -1.02 dBTP |
| Loudness meter (BS.1770-4 / EBU R128) | -20.00 LUFS on the 1 kHz reference |
| REVERB RT60 of the lows at DECAY 1 / 3 / 8 s | 0.99 / 3.04 / 8.00 s |
| Buffer-size independence | 30 of 30 plugins bit-identical at buffer sizes 1 to 2048 |
| Sample rates | all 30 clean at 44.1, 48 and 96 kHz |
| Gain staging | every model of every instrument at its role's target within 0.1 dB (VOICE phrase models within 0.6 dB): KICK -15, PERC -22, RUMBLE -20, BASS -18, LEAD -20, PAD -22, VOICE -20, STRIKE -20 LUFS |
| PANIC | feedback delay, 20 s reverb and a held pad silenced to -140 dB |
| Song recall | on the bar line at every buffer size and song position tested (20 of 20; was 9 of 20) |
| LIMIT AUTO | settles on TARGET with no overshoot (was +0.7 dB and a long settle) |

## CPU

Offline, one instance at default settings, 48 kHz, buffer 256, measured on the 2-core Linux build machine (an M4 Max core is faster). Percent of one core; Live spreads tracks across cores. Double these at 96 kHz.

| Plugin | % of one core | Plugin | % of one core | Plugin | % of one core |
|---|---:|---|---:|---|---:|
| KICK | 1.0 | FILTER | 0.6 | ISOLATOR | 0.6 |
| PERC | 1.2 | GATE | 0.3 | BUILD | 2.2 |
| RUMBLE | 2.2 | DENSITY | 0.4 | GLUE | 0.4 |
| BASS | 4.9 | DELAY | 1.2 | TONE | 0.5 |
| LEAD | 2.0 | REVERB | 2.0 | LOWEND | 0.6 |
| PAD | 11.1 | DRIVE | 1.0 | LIMIT | 4.3 |
| VOICE | 1.3 | RADIO | 0.5 | ROOM EQ | 0.2 |
| STRIKE | 1.5 | REPEAT | 0.3 | ALLOC | 0.9 |
| DECK | 0.3 | DUCK | 0.4 | TAP | 0.5 |
| CONTROL | 0.1 | ENERGY | 0.7 | STRIP | 1.4 |

A full set (CONTROL, KICK, 3 PERC, RUMBLE, BASS, 2 LEAD, VOICE, PAD, STRIKE, DECK, 4 ENERGY, DELAY, REVERB, DENSITY and the whole master chain) adds up to about half of one core here.

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

### First things to check in Live

1. **The bus.** SF CONTROL's display should say **BUS SHARED**. Put SF KICK on a track and play it: CONTROL should show **KICK REPORTING**.
2. **Pocket timing.** SF KICK + SF BASS (POCKET DEPTH up, SHAPE right for an audible pump). The pump should land exactly on the kick. Live's buffer size must be 512 or less. If it's consistently early or late, trim POCKET OFFSET on SF CONTROL.
3. **ALLOC.** Put SF ALLOC on the master. Every SF source should appear in its list (by track name with VST3). Drag the order, raise DEPTH on a band where two overlap, and watch the amber.
4. **Songs.** Store two songs and switch with Program Change from the OXI. The E16 rings should follow. With the VST3, Live delivers Program Change as a program change (SF CONTROL lists SONG 01-16); if it doesn't arrive, try the AU.
5. **DECK.** Load a track, PLAY A: it should start on the bar, locked to Live's tempo.
6. **KEY and PANIC.** Set KEY on SF CONTROL: BASS, LEAD and PAD transpose, KICK and RUMBLE retune to the root. Push SF DELAY into feedback and hold PANIC: everything goes quiet within 5 ms.
7. **Levels.** Default LEVELs are calibrated per role (KICK -15 LUFS ... PAD -22). Faders at 0 dB should give a balanced rough mix with headroom into GLUE and LIMIT.

### Changes in the Nov 3 update that affect existing sets

- **Rebuild and install everything.** The SF BUS moved to v3; old and new builds don't talk to each other.
- **Levels changed.** Models are now calibrated to role targets. At the same LEVEL, KICK is about 3-6 dB quieter, some PERC and PAD models change by several dB, and the VOICE vocoder models are much quieter than before. Re-balance faders once.
- **KICK LAYER works now.** It used to cut the main voice after 3 ms, so a layered kick was only the layer. Main and layer now sum to the level of one kick.
- **DRIVE knobs start clean.** FILTER, ENERGY and STRIP drive (and synth DRIVE at its low end) fade in from exactly clean instead of jumping +3-4 dB at the first notch.
- **KEY needs SF CONTROL running.** Without it (deleted, or another set), instruments ignore any key left over.
- **SF LIMIT latency** is now 176 samples at 48 kHz (was 72); Live compensates it. **SF DRIVE** now uses the suite's own 4x oversampler (112 dB instead of about 70 dB alias rejection); its latency is 5 samples.
- **Editors are taller:** a MOTION row and a SYSTEM row sit under each plugin's controls.
- New: PRESET menus, KEY and PANIC on SF CONTROL, KEY FOLLOW on instruments, SF STRIKE, SF TONE, FAULT indicator in every header.

## Licences

Signalsmith Stretch and Signalsmith Linear (third_party/) are MIT licensed. JUCE is used under its own licence.
