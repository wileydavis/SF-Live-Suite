# SF SHOW.als: the show set, built

The Live set of the manual's chapter 7, ready to open: every track, group and return, every SF plugin
in place with its starting settings, the routing, and every address of the E16 scene SF SHOW
already mapped. Open it, finish it in five steps (section 2), save it as your template.

| FILE | WHAT |
|---|---|
| `SF SHOW.als` | The set. Live 12.1 format: opens in Live 12.1 and later (VST3 song recall needs 12.2). |
| `make_set.py` | Builds it from the track list in the script, `controllers/e16/layout.py` and the plugins' own code. |
| `check_set.py` | Checks any Live set's mappings against the E16 scene: what is missing, on the wrong target or the wrong range, and which keys PANIC and 92 PA are on. Reads the set, never writes it. |

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
| 92 PA | SF ROOM EQ | Resampling, Monitor In, sends disabled | Ext. Out 1/2 |
| 93 REC | none | Resampling, Monitor Off, sends disabled | Sends Only |
| A DUB (return) | SF DELAY, DRY off | | 90 LIVE BUS |
| B SPACE (return) | SF REVERB, DRY off | | 90 LIVE BUS |
| Main | SF LIMIT | | Ext. Out 5/6 |

- Every other plugin setting is the plugin's default. Every SF plugin's parameter panel is unfolded,
  so every mapped parameter is in view.
- **E16:** every address in `controllers/e16/sf-show-map.csv` is mapped, the presses and knob 1's
  second destination (SLOW ALL on SF CONTROL) included; the E16 scene carries them all. Every volume has
  Max 0 dB; DUB FEEDBACK is mapped 0-70 %.
- Every MIDI track starts on MIDI From No Input, so nothing plays until section 2 points it at its
  part. Cue Out 3/4. Tempo 120, 4/4, eight empty scenes. Colours: groups white, instruments light
  grey, the rest grey; red is 92 PA, dark red is 93 REC.

## 1. BEFORE OPENING

- Install the suite and turn VST3 on in Live (Settings > Plug-Ins > Use VST3 Plug-In System Folders).
  The set uses the VST3s: SF ALLOC lists its sources by track name only in VST3.
- Live's MIDI ports as `controllers/e16/README.md`, step 3 (E16 Port 2 Remote, input and output;
  Ports 1 and 3 off; Takeover Mode Value Scaling), and the ONE's ports as chapter 7, CLOCK, MIDI AND
  LATENCY.

## 2. OPEN IT, THEN FINISH IT

Live names MIDI ports and audio outputs after your devices, so the first two belong to your rig:

1. **MIDI From** on twelve tracks. Each is No Input until you set it: an instrument plays nothing
   until then, and never All Ins (every instrument would play every part).

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

2. **Outputs.** 92 PA to the pair that feeds the PA (the set has 1/2). Cue Out, on the Main track,
   to the headphones (3/4; Live cues only when it differs from Main's). Main to a pair that goes
   nowhere (5/6: on an interface without it, Main stays silent). Check all three before any sound
   reaches a PA: the PA pair must carry 92 PA and nothing else.
3. **Cmd-K**, two computer keys, never E16 presses: **PANIC** (SF CONTROL on 00 CONTROL; press,
   every SF plugin silent; again, back) and **92 PA**'s activator (press, the speakers off and REC
   still recording; again, on).
4. **Options:** Delay Compensation on; Reduced Latency When Monitoring off.
5. **Save:** File > Save Live Set As Template, or Save Live Set As into your show folder. The set
   came from a script; from this save on it is one Live wrote.

For DECK's headphone cue, add an audio track: Audio From 41 DECK, CUE; out to the headphones.

## 3. CHECKS

1. Live opens the set without a message and lists no SF plugin as missing. A missing plugin: the
   suite isn't installed, or VST3 is off.
2. Cmd-M: the MIDI Mappings list shows the E16 mappings (108). Turn a few knobs on different pages:
   the parameter named in `sf-show-map.csv` moves, and a volume stops at 0 dB.
   `python3 controllers/live/check_set.py "SF SHOW.als"` says the same from the file: 108 of 108.
3. Play the ONE: each instrument plays only its own part; the LIVE BUS, Main and 92 PA meters move;
   93 REC, armed, records the set. Cue a track: it reaches the headphones and not the PA.
4. Then the manual's chapter 7, BEFORE THE FIRST SHOW.

## A SET OF YOUR OWN

A set you saved before a mapping existed (SLOW ALL, the presses), or one mapped by hand, keeps the
mappings it was saved with: rebuilding SF SHOW.als doesn't reach it. Check it:

```
python3 controllers/live/check_set.py "My Show.als"
```

It lists every E16 address that is missing or on the wrong target, with where it sits on the E16, and
the keys on PANIC and 92 PA. Map what it lists (Cmd-M: click the target, turn or press the knob), save,
run it again. It finds targets by the template's track names and plugin names, VST3 or AU.

## SF TERROIR

The set ships with SF REVERB on B SPACE. To use your captured places there instead, swap in SF
TERROIR (DRY off) and map six controls: `controllers/e16/README.md`, SF TERROIR ON B SPACE.

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
  plugin's parameters. A rebuild is a new set: section 2 again.
- Before writing, the script checks what Live enforces without saying: unique ids, all below
  NextPointeeId; one clip slot per scene; every group and every routed-to track present; no address
  mapped twice. The same input always gives the same file.
