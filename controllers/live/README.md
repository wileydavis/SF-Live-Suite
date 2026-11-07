# SF SHOW.als: the show set, built

The Live set of the manual's chapter 7, ready to play: every track, group and return, every SF plugin
in place with its starting settings, the routing, every instrument's MIDI From on its OXI One port and
channel, and every address of the E16 scene SF SHOW already mapped. Live's own settings (section 1)
are set once per computer; then open it and play.

| FILE | WHAT |
|---|---|
| `SF SHOW.als` | The set. Live 12.1 format: opens in Live 12.1 and later (VST3 song recall needs 12.2). |
| `make_set.py` | Builds it from the track list in the script, `controllers/e16/layout.py` and the plugins' own code. |
| `check_set.py` | Checks any Live set's mappings against the E16 scene: what is missing, on the wrong target or the wrong range, and any computer keys mapped. Reads the set, never writes it. |

## IN THE SET

| TRACK | DEVICES | IN / MONITOR | OUT |
|---|---|---|---|
| 00 CONTROL | SF CONTROL | MIDI, Monitor In | Main |
| 01 KICK | SF KICK | MIDI, Monitor In | 90 LIVE BUS |
| 10 LOW (group) | SF ENERGY, PROFILE BASS | | 90 LIVE BUS |
| 11 BASS | SF BASS | MIDI, Monitor In | LOW |
| 12 RUMBLE | SF RUMBLE, SOURCE KICK BUS | MIDI No Input (stays), Monitor In | LOW |
| 20 DRUMS (group) | SF ENERGY, PROFILE DRUMS; SF REPEAT | | 90 LIVE BUS |
| 21 CLOSED | SF PERC, HAT 909-TYPE | MIDI, Monitor In | DRUMS |
| 22 OPEN | SF PERC, HAT 909-TYPE, DECAY 0.8 | MIDI, Monitor In | DRUMS |
| 23 CLAP | SF PERC, CLAP | MIDI, Monitor In | DRUMS |
| 24 STRIKE | SF STRIKE | MIDI, Monitor In | DRUMS |
| 30 MUSIC (group) | SF ENERGY, PROFILE MUSIC | | 90 LIVE BUS |
| 31 ACID | SF LEAD, ACID 303-TYPE | MIDI, Monitor In | MUSIC |
| 32 STAB | SF LEAD, STAB DUB | MIDI, Monitor In | MUSIC |
| 33 TEXTURE | SF PAD, GRANULAR | MIDI, Monitor In | MUSIC |
| 34 VOICE | SF VOICE | MIDI, Monitor In | MUSIC |
| 40 BRIDGE | your audio clips | No Input, Monitor Off | Main |
| 41 DECK | SF DECK | MIDI, Monitor In | Main |
| 90 LIVE BUS | SF ISOLATOR > BUILD > GLUE > TONE > LOWEND; SF ALLOC, ENABLE off | No Input, Monitor In, sends disabled | Main |
| 92 PA | SF ROOM EQ > SF LIMIT (safety: CEILING -0.5 dB, CLIP 0 %, AUTO off) | Main, Post Mixer; Monitor In, sends disabled | Ext. Out 1/2 |
| 93 REC | none | Main, Post Mixer; Monitor Off, sends disabled | Sends Only |
| 94 ROOM | SF CAPTURE | Ext. In 1/2 (the room mics, a stereo pair), Monitor In, sends disabled | Ext. Out 3/4 |
| A DUB (return) | SF DELAY, DRY off | | 90 LIVE BUS |
| B SPACE (return) | SF REVERB, DRY off | | 90 LIVE BUS |
| Main | SF LIMIT | | Ext. Out 5/6 |

- Every other plugin setting is the plugin's default. Every SF plugin's parameter panel is unfolded,
  so every mapped parameter is in view.
- **E16:** every address in `controllers/e16/sf-show-map.csv` is mapped, the presses and knob 1's
  second destination (SLOW ALL on SF CONTROL) included; the E16 scene carries them all. Every volume has
  Max 0 dB; DUB FEEDBACK is mapped 0-70 %.
- Every instrument listens to one OXI One port and channel (section 2's table), never All Ins. 12
  RUMBLE has no MIDI: it follows the kick. Cue Out 3/4. Tempo 120, 4/4, eight empty scenes. Colours:
  groups white, instruments light grey, the rest grey; red is 92 PA, dark red 93 REC and 94 ROOM.

### WHERE THE SOUND GOES

Every part reaches **90 LIVE BUS** (the kick and the returns directly, the rest through their group),
the bus goes to the **Main**, and so do 40 BRIDGE and 41 DECK. The Main is the set: SF LIMIT holds it
at -1 dBTP, and its own output goes nowhere (5/6). Two tracks take the Main after its fader:

- **92 PA** adds the venue's correction (SF ROOM EQ) and a safety limiter that never touches the set
  (its peaks are already at -1 dBTP; it only catches what the EQ's phase pushes past -0.5 dB), and
  feeds the speakers on 1/2. Keep its fader at 0 dB: the PA's level, and the way to silence the room
  in an emergency, is the interface's output knob. The recording carries on regardless.
- **93 REC** records the set without the venue's correction.

Don't send 90 LIVE BUS to 92 PA instead: BRIDGE, DECK and SF LIMIT would be left out of the PA, and
93 REC would record only BRIDGE and DECK.

**94 ROOM** is the room mics. Armed, it records them with the set, for the mix. Its SF CAPTURE never
passes them (the set never hears the mics); for SF TERROIR it plays the sweep to a speaker on the
headphone out and records the place (the operator's manual, chapter 8, CAPTURING THE ROOM).

## 1. BEFORE OPENING

- Install the suite and turn VST3 on in Live (Settings > Plug-Ins > Use VST3 Plug-In System Folders).
  The set uses the VST3s: SF ALLOC lists its sources by track name only in VST3.
- Live's MIDI ports (Settings > Link, Tempo & MIDI), once per computer:

  | PORT | TRACK | SYNC | REMOTE |
  |---|---|---|---|
  | OXI ONE MKII (Port 1), input | **on** | off | off |
  | OXI ONE MKII (Port 2), input | **on** | off | off |
  | OXI ONE MKII (Port 1), output | off | **on** | off |
  | OXI E16 (Port 1), input and output | off | off | **on** |
  | every other port | off | off | off |

  Takeover Mode: **Value Scaling**. Audio: 48 kHz, buffer 256.
- Live's preferences live outside any set: nothing in this file can set them.
- The OXI One's own setup (sync, its config, the startup project): `controllers/oxi-one`. Live sends
  its clock only while it plays: start and stop from Live.

## 2. OPEN IT

1. **MIDI From** is set on twelve tracks, under the names Live gives the OXI One MkII's USB ports on a
   Mac: **OXI ONE MKII (Port 1)** is the ONE's port A, **(Port 2)** its port B. If your Live lists the
   ONE under another name, its tracks show the port as missing: set each by this table. Never All Ins
   (every instrument would play every part).

   | TRACK | MIDI FROM |
   |---|---|
   | 00 CONTROL | ONE port B, ch 16 |
   | 01 KICK | ONE port A, ch 1 |
   | 11 BASS | ONE port B, ch 1 |
   | 21 CLOSED | ONE port A, ch 2 |
   | 22 OPEN | ONE port A, ch 3 |
   | 23 CLAP | ONE port A, ch 4 |
   | 24 STRIKE | ONE port B, ch 6 |
   | 31 ACID | ONE port B, ch 2 |
   | 32 STAB | ONE port B, ch 3 |
   | 33 TEXTURE | ONE port B, ch 4 |
   | 34 VOICE | ONE port B, ch 5 |
   | 41 DECK | ONE port B, ch 7 (hot cues; optional) |

2. **Inputs and outputs**, set for an interface with the PA on 1/2 and headphones on 3/4 (a Babyface:
   its main outs and its phones). 92 PA to the pair that feeds the PA (the set has 1/2). Cue Out, on the
   Main track, to the headphones (3/4; Live cues only when it differs from Main's), and 94 ROOM there
   too. Main to a pair that goes nowhere (5/6: on an interface without it, Main stays silent). Check
   all three before any sound reaches a PA: the PA pair must carry 92 PA and nothing else. The room
   mics on inputs 1 and 2 (1: the mic on your left as you face the speaker); in Settings > Audio >
   Input Config, 1/2 as a stereo pair, or 94 ROOM has no input.
3. **Options:** Delay Compensation on; Reduced Latency When Monitoring off.
4. **Save:** File > Save Live Set As Template, or Save Live Set As into your show folder. The set
   came from a script; from this save on it is one Live wrote.

For DECK's headphone cue, add an audio track: Audio From 41 DECK, CUE; out to the headphones.

## 3. CHECKS

1. Live opens the set without a message and lists no SF plugin as missing. A missing plugin: the
   suite isn't installed, or VST3 is off.
2. Cmd-M: the MIDI Mappings list shows the E16 mappings (108). Turn a few knobs on different pages:
   the parameter named in `sf-show-map.csv` moves, and a volume stops at 0 dB.
   `python3 controllers/live/check_set.py "SF SHOW.als"` says the same from the file: 108 of 108.
3. Play the ONE: each instrument plays only its own part; the LIVE BUS, Main and 92 PA meters move;
   93 REC, armed, records the set. Cue a track: it reaches the headphones and not the PA. 94 ROOM's
   SF CAPTURE shows the room on IN L and IN R; armed, ROOM records it.
4. Then the manual's chapter 7, BEFORE THE FIRST SHOW.

## A SET OF YOUR OWN

A set you saved before a mapping existed (SLOW ALL, the presses), or one mapped by hand, keeps the
mappings it was saved with: rebuilding SF SHOW.als doesn't reach it. Check it:

```
python3 controllers/live/check_set.py "My Show.als"
```

It lists every E16 address that is missing or on the wrong target, with where it sits on the E16, and
any computer keys mapped. Map what it lists (Cmd-M: click the target, turn or press the knob), save,
run it again. It finds targets by the template's track names and plugin names, VST3 or AU.

## SF TERROIR

The set ships with SF REVERB on B SPACE. To use your captured places there instead, swap in SF
TERROIR (DRY off) and map six controls: `controllers/e16/README.md`, SF TERROIR ON B SPACE. With
TERROIR in the set, 94 ROOM captures the room you play in straight into a PLACE slot: transport
stopped, a speaker on the headphone out, CAPTURE on TERROIR's display, then the slot (46 s).

## REBUILDING

```
python3 controllers/live/make_set.py --base EMPTY.als [--out PATH]
```

- `EMPTY.als`: a set saved from your Live (File > New Live Set, then Save Live Set As). The result
  is in your Live's own format. The base's tracks are used as patterns only; its tracks, devices,
  clips and tempo automation are not kept.
- It needs SFTests (`cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF && cmake --build
  build-test`): each plugin's starting state is written by the plugin's own code and loaded back
  before it goes in the set.
- Rebuild after a change to the track list (SHOW in the script), `controllers/e16/layout.py` or a
  plugin's parameters. A rebuild is a new set: songs stored in your own copy stay there.
- Before writing, the script checks what Live enforces without saying: unique ids, all below
  NextPointeeId; one clip slot per scene; every group and every routed-to track present; no address
  mapped twice. The same input always gives the same file.
