# SF Live Suite

Ableton Live plugins for performing hypnotic / dub techno live. Private. `SPEC.md` is the source of truth.

28 plugins, one shared core: 9 instruments (KICK, PERC, RUMBLE, BASS, LEAD, PAD, VOICE, DECK, CONTROL), 11 effects (STRIP, FILTER, GATE, DENSITY, DELAY, REVERB, DRIVE, RADIO, REPEAT, DUCK, ENERGY), 8 master/mix tools (ISOLATOR, BUILD, GLUE, LOWEND, LIMIT, ROOM EQ, ALLOC, TAP).

## Build (Mac)

```
./build.sh
```

The first run installs CMake via Homebrew if needed and clones JUCE 8.0.4 into `./JUCE`. It builds every plugin as AU and VST3 and copies them into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`. Then rescan in Live (Settings > Plug-Ins > Rescan). Use AU or VST3, not both. They show up as `SF KICK`, `SF BASS` and so on.

One plugin only: `./build.sh KICK` (any module id: KICK PERC RUMBLE BASS LEAD PAD VOICE DECK CONTROL FILTER GATE DENSITY DELAY REVERB DRIVE RADIO REPEAT DUCK ENERGY ISOLATOR BUILD GLUE LOWEND LIMIT ROOMEQ ALLOC TAP STRIP).

The full build compiles 27 plugins and takes a while the first time (each plugin carries the whole core).

Offline tests (no host needed):

```
cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-test --target SFTests SFSnapshot
./build-test/SFTests_artefacts/Release/SFTests          # all checks
./build-test/SFSnapshot_artefacts/Release/SFSnapshot    # editor images -> docs/snapshots
```

## Setting up the live set

1. **SF CONTROL** on its own MIDI track, input from the OXI One. Program Change on channel 16 selects the song (01-16).
2. **Instruments**, one per MIDI track: SF KICK, SF PERC (2-3), SF RUMBLE, SF BASS, SF LEAD, SF VOICE, SF PAD, SF DECK. Pick models in the header menu.
3. **Groups** get SF ENERGY (instead of the DJ filter). PROFILE: DRUMS / BASS / MUSIC / VOICE.
4. **Returns:** SF DELAY, SF REVERB, SF DENSITY (for the send-delay hat fills).
5. **Master:** SF ISOLATOR > SF BUILD > SF GLUE > SF LOWEND > SF ROOM EQ > SF LIMIT, plus SF ALLOC anywhere on the master.
6. **Live instruments, drum racks, hardware:** SF STRIP last on the track gives it the suite's performance row and puts it in Pocket and ALLOC. **Stems:** SF TAP (no sound of its own) or SF STRIP.
7. **E16:** map with Live's MIDI Map (Cmd-M). In Live's MIDI preferences turn **Remote** on for the E16 *output*, so rings follow song recalls and ALLOC slot changes. The first 8 controls of each plugin are its performance row.
8. **Songs:** set every plugin for a song, choose the song number on SF CONTROL, press **STORE ALL**. Program Change (or the SONG knob) recalls it on the next bar.

**DECK:** drop tracks on the DECK window (they're analysed in the background: tempo, beat grid, loudness). Click **A** or **B** on a row to load it. Turn **PLAY A** on and it starts on the next QUANT boundary, synced to Live. Hot cues: C1-D#1 (deck A), C2-D#2 (deck B). For headphone cueing, make an audio track with *Audio From* = the DECK track, choose **CUE**, and route it to your headphone outputs.

**Slow mode:** set **SLOW** on any plugin (1-64 bars). Every performance knob then glides to where you set it over that many bars. A quarter turn becomes a long, even build.

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

### First things to check in Live

1. **The bus.** SF CONTROL's display should say **BUS SHARED**. Put SF KICK on a track and play it: CONTROL should show **KICK REPORTING**.
2. **Pocket timing.** SF KICK + SF BASS (POCKET DEPTH up, SHAPE right for an audible pump). The pump should land exactly on the kick. Live's buffer size must be 512 or less. If it's consistently early or late, trim POCKET OFFSET on SF CONTROL.
3. **ALLOC.** Put SF ALLOC on the master. Every SF source should appear in its list (by track name with VST3). Drag the order, raise DEPTH on a band where two overlap, and watch the amber.
4. **Songs.** Store two songs and switch with Program Change from the OXI. The E16 rings should follow. With the VST3, Live delivers Program Change as a program change (SF CONTROL lists SONG 01-16); if it doesn't arrive, try the AU.
5. **DECK.** Load a track, PLAY A: it should start on the bar, locked to Live's tempo.

## Licences

Signalsmith Stretch and Signalsmith Linear (third_party/) are MIT licensed. JUCE is used under its own licence.
