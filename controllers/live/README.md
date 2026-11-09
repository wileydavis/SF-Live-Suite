# SF SHOW.als: the show set, built

The Live set of the manual's chapter 7, ready to play: every track, group and return, every SF plugin
in place with its starting settings, the routing, every instrument's MIDI From on its OXI One port and
channel, and every address of the E16 scenes SF SHOW and SF MIX already mapped. Live's own settings
(section 1) are set once per computer; then open it and play.

| FILE | WHAT |
|---|---|
| `SF SHOW.als` | The set. Live 12.4 format (built on a set Live 12.4.16 saved): opens in Live 12.4 and later. |
| `make_set.py` | Builds it from the track list in the script, `controllers/e16/layout.py` and the plugins' own code. |
| `check_set.py` | Checks any Live set's mappings against the E16 scenes: what is missing, on the wrong target or the wrong range, and any computer keys mapped. Reads the set, never writes it. |

## IN THE SET

One number per track [Nov 8]: an instrument track's number is its MIDI channel, on the OXI One (port
A) and on the E16, and its page in the E16 scene SF SHOW. Tracks are named by their role, not their
instrument, so a track keeps its name, number and page when another instrument goes on it. The order
is the E16's: the kick, the drums, the low end, the music. A track without a number has no channel of
its own.

| TRACK | DEVICES | IN / MONITOR | OUT |
|---|---|---|---|
| CONTROL | SF CONTROL | MIDI, Monitor In | Main |
| 1 KICK | SF KICK; SF ENERGY, PROFILE DRUMS | MIDI, Monitor In | LIVE BUS |
| DRUMS (group) | SF ENERGY, PROFILE DRUMS; SF REPEAT | | LIVE BUS |
| 2 CLOSED HAT | SF PERC, HAT 909-TYPE; SF ENERGY, PROFILE DRUMS | MIDI, Monitor In | DRUMS |
| 3 OPEN HAT | SF PERC, HAT 909-TYPE, DECAY 0.8; SF ENERGY, PROFILE DRUMS | MIDI, Monitor In | DRUMS |
| 4 CLAP | SF PERC, CLAP; SF ENERGY, PROFILE DRUMS | MIDI, Monitor In | DRUMS |
| 5 PERC | SF STRIKE; SF ENERGY, PROFILE DRUMS | MIDI, Monitor In | DRUMS |
| LOW (group) | SF ENERGY, PROFILE BASS | | LIVE BUS |
| 6 BASS | SF BASS; SF ENERGY, PROFILE BASS | MIDI, Monitor In | LOW |
| 7 RUMBLE | SF RUMBLE, SOURCE KICK BUS; SF ENERGY, PROFILE BASS | MIDI No Input (stays), Monitor In | LOW |
| MUSIC (group) | SF ENERGY, PROFILE MUSIC | | LIVE BUS |
| 8 LEAD | SF LEAD, ACID 303-TYPE; SF ENERGY, PROFILE MUSIC | MIDI, Monitor In | MUSIC |
| 9 CHORD | SF LEAD, STAB DUB; SF ENERGY, PROFILE MUSIC | MIDI, Monitor In | MUSIC |
| 10 PAD | SF PAD, GRANULAR; SF ENERGY, PROFILE MUSIC | MIDI, Monitor In | MUSIC |
| 11 VOICE | SF VOICE; SF ENERGY, PROFILE VOICE | MIDI, Monitor In | MUSIC |
| BRIDGE | your audio clips | No Input, Monitor Off | Main |
| DECK | SF DECK | MIDI, Monitor In | Main |
| LIVE BUS | SF ISOLATOR > BUILD > GLUE > TONE > LOWEND; SF ALLOC, ENABLE off | No Input, Monitor In, sends disabled | Main |
| PA | SF ROOM EQ > SF LIMIT (safety: CEILING -0.5 dB, CLIP 0 %, AUTO off) | Main, Post Mixer; Monitor In, sends disabled | Ext. Out 1/2 |
| REC | none | Main, Post Mixer; Monitor Off, sends disabled | Sends Only |
| ROOM | SF CAPTURE | Ext. In 1/2 (the room mics, a stereo pair), Monitor In, sends disabled | Ext. Out 3/4 |
| A DUB (return) | SF DELAY, DRY off | | LIVE BUS |
| B SPACE (return) | SF REVERB, DRY off | | LIVE BUS |
| Main | SF LIMIT | | Ext. Out 5/6 |

- Every other plugin setting is the plugin's default. Every SF plugin's parameter panel is unfolded,
  so every mapped parameter is in view.
- **E16:** every address in `controllers/e16/sf-show-map.csv` is mapped (210, both scenes), the
  presses and knob 1's second destination (SLOW ALL on SF CONTROL) included. Every volume and send is
  mapped up to 0 dB; DUB FEEDBACK 0-70 %. A track's E16 page sends on the track's own channel, so it
  can be mapped to another instrument on that track without touching the others.
- **Songs** are called from the E16: encoder 15 of any SF SHOW page turns SF CONTROL's NEXT SONG, its
  press (GO) calls it on the next bar line. Live sends a VST3 no Program Change, so the ONE calls no
  song.
- Every instrument listens to one OXI One port and channel (section 2's table), never All Ins. RUMBLE
  has no MIDI: it follows the kick. Cue Out 3/4. Tempo 120, 4/4, eight empty scenes. Colours: groups
  white, instruments light grey, the rest grey; red is PA, dark red REC and ROOM.

### WHERE THE SOUND GOES

Every part reaches **LIVE BUS** (the kick and the returns directly, the rest through their group),
the bus goes to the **Main**, and so do BRIDGE and DECK. The Main is the set: SF LIMIT holds it
at -1 dBTP, and its own output goes nowhere (5/6). Two tracks take the Main after its fader:

- **PA** adds the venue's correction (SF ROOM EQ) and a safety limiter that never touches the set
  (its peaks are already at -1 dBTP; it only catches what the EQ's phase pushes past -0.5 dB), and
  feeds the speakers on 1/2. Keep its fader at 0 dB: the PA's level, and the way to silence the room
  in an emergency, is the interface's output knob. The recording carries on regardless.
- **REC** records the set without the venue's correction.

Don't send LIVE BUS to PA instead: BRIDGE, DECK and SF LIMIT would be left out of the PA, and REC
would record only BRIDGE and DECK.

**ROOM** is the room mics. Armed, it records them with the set, for the mix. Its SF CAPTURE never
passes them (the set never hears the mics); for SF TERROIR it plays the sweep to a speaker on the
headphone out and records the place (the operator's manual, chapter 8, CAPTURING THE ROOM).

## 1. BEFORE OPENING

- Install the suite (one build: the set needs SF CONTROL's NEXT SONG and GO, Nov 8) and turn VST3 on
  in Live (Settings > Plug-Ins > Use VST3 Plug-In System Folders). The set uses the VST3s: SF ALLOC
  lists its sources by track name only in VST3, and GLIDE RINGS finds the knobs of a plugin on
  several tracks (SF LEAD, SF PERC, SF ENERGY) by it.
- Live's MIDI ports (Settings > Tempo & MIDI; before Live 12.4, Link, Tempo & MIDI), once per computer:

  | PORT | TRACK | SYNC | REMOTE |
  |---|---|---|---|
  | OXI ONE MKII (Port 1), input | **on** | off | off |
  | OXI ONE MKII (Port 2), input | **on** | off | off |
  | OXI ONE MKII (Port 1), output | off | **on** | off |
  | OXI E16 (Port 1), input and output | off | off | **on** |
  | every other port | off | off | off |

  Takeover Mode: **Value Scaling**. Audio: 48 kHz, buffer 256.

  **MIDI Clock Sync Delay** (the ONE's Port 1 output row, unfolded with the triangle at its left):
  leave it at **0**. The ONE's notes come back to Live a little late (the clock goes out, the notes
  come back, buffers both ways: on Nov 7 the kick came 40 ms after Live's beat), and nothing needs
  them on the beat: SF BUILD lands its drop on the kick it hears (LAND ON KICK; its KICK readout
  shows how late the kick reaches it), and Pocket and the effects that cut on the beat (GATE, STRIP,
  ENERGY's chop, DUCK's GRID, REPEAT, DECK) follow SF KICK. On Nov 7, -70 ms there moved the kick
  not at all.
- Live's preferences live outside any set: nothing in this file can set them.
- The OXI One's own setup (sync, its config, the startup project): `controllers/oxi-one`. Live sends
  its clock only while it plays: start and stop from Live.

## 2. OPEN IT

1. **MIDI From** is set on twelve tracks, under the names Live gives the OXI One MkII's USB ports on a
   Mac: **OXI ONE MKII (Port 1)** is the ONE's port A, **(Port 2)** its port B. If your Live lists the
   ONE under another name, its tracks show the port as missing: set each by this table. Never All Ins
   (every instrument would play every part). A numbered track listens to port A on its own number.

   | TRACK | MIDI FROM |
   |---|---|
   | CONTROL | ONE port A, ch 16 (a Program Change there reaches only an AU of SF CONTROL) |
   | 1 KICK ... 5 PERC | ONE port A, ch 1 ... 5 |
   | 6 BASS | ONE port A, ch 6 |
   | 7 RUMBLE | none: it follows the kick |
   | 8 LEAD ... 11 VOICE | ONE port A, ch 8 ... 11 |
   | DECK | ONE port B, ch 7 (hot cues; optional) |

2. **Inputs and outputs**, set for an interface with the PA on 1/2 and headphones on 3/4 (a Babyface:
   its main outs and its phones). PA to the pair that feeds the PA (the set has 1/2). Cue Out, on the
   Main track, to the headphones (3/4; Live cues only when it differs from Main's), and ROOM there
   too. Main to a pair that goes nowhere (5/6: on an interface without it, Main stays silent). Check
   all three before any sound reaches a PA: the PA pair must carry PA and nothing else. The room
   mics on inputs 1 and 2 (1: the mic on your left as you face the speaker); in Settings > Audio >
   Input Config, 1/2 as a stereo pair, or ROOM has no input.
3. **Options:** Delay Compensation on; Reduced Latency When Monitoring off.
4. **Save:** File > Save Live Set As Template, or Save Live Set As into your show folder. The set
   came from a script; from this save on it is one Live wrote.

For DECK's headphone cue, add an audio track: Audio From DECK, CUE; out to the headphones.

## 3. CHECKS

1. Live opens the set without a message and lists no SF plugin as missing. A missing plugin: the
   suite isn't installed, or VST3 is off.
2. Cmd-M: the MIDI Mappings list shows the E16 mappings (210). Turn a few knobs on different pages
   of both scenes: the parameter named in `sf-show-map.csv` moves. On PERFORM, press an ENERGY knob:
   it goes back to the centre. `python3 controllers/live/check_set.py "SF SHOW.als"` says the same
   from the file: 210 of 210.
3. **Songs.** Store a song, then on any SF SHOW page turn knob 15: SF CONTROL shows NEXT nn, PRESS GO
   (amber) and nothing changes. Press it: the song lands on the next bar line.
4. Play the ONE: each instrument plays only its own part; the LIVE BUS, Main and PA meters move;
   REC, armed, records the set. Cue a track: it reaches the headphones and not the PA. ROOM's
   SF CAPTURE shows the room on IN L and IN R; armed, ROOM records it.
5. Then the manual's chapter 7, BEFORE THE FIRST SHOW.

## A SET OF YOUR OWN

A set you saved before a mapping existed, or one mapped by hand, keeps the mappings it was saved
with: rebuilding SF SHOW.als doesn't reach it. A set mapped before Nov 8 has the old addresses and
track names (every address moved, and the bus tracks lost their numbers: the E16 layout's
ADDRESSES); start from the new SF SHOW.als rather than remap it. Check a set:

```
python3 controllers/live/check_set.py "My Show.als"
```

It lists every E16 address that is missing or on the wrong target, with where it sits on the E16, and
any computer keys mapped. Map what it lists (Cmd-M: click the target, turn or press the knob), save,
run it again. It finds targets by the template's track names and plugin names, VST3 or AU.

## SF TERROIR

The set ships with SF REVERB on B SPACE. To use your captured places there instead, swap in SF
TERROIR (DRY off) and map six controls: `controllers/e16/README.md`, SF TERROIR ON B SPACE. With
TERROIR in the set, ROOM captures the room you play in straight into a PLACE slot: transport
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
