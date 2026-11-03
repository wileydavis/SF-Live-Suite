# SF SHOW: the OXI E16 scene for the show template

One E16 scene for the whole rig of the manual's chapter 7. Load it once, map Live once, save the Live
set as your template: nothing is remapped after that.

| FILE | WHAT |
|---|---|
| `SF SHOW.oxie16` | The scene: 12 pages, all 192 turns set (name, ring colour, channel, CC). |
| `sf-show-map.csv` | Every address once: channel, CC or note, the Live target, the mapping range, where it sits on the E16. Map Live from this. |
| `layout.py` | The layout. One source for the scene, the map and the manual's E16 tables. |
| `make_scene.py` | Rebuilds the scene and the map: `python3 make_scene.py`. |

## THE GRID

```
 1 LOW ENERGY      2 DRUMS ENERGY    3 MUSIC ENERGY    4 BUILD           press 4: DROP
 5 KICK VOLUME     6 LOW VOLUME      7 DRUMS VOLUME    8 MUSIC VOLUME
 9  ...           10  ...           11  ...           12  ...            rows 3 and 4: the page
13  ...           14  ...           15  ...           16  ...
```

Encoders are numbered row by row from the top left. Rows 1 and 2 are the same on every page (the
E16's screen shows them as LOW ENGY, DRM ENGY, MUS ENGY, BUILD, KICK VOL, LOW VOL, DRUM VOL, MUS VOL).
Rows 3 and 4 are the page: PERFORM, KICK, BASS, RUMBLE, HATS, STRIKE, ACID, STAB, TEXTURE, VOICE,
SPACE, MACROS (the manual, chapter 7, THE E16). Rings: light blue is a volume, white is everything else.

## ADDRESSES

- **Channel = track.** 1 KICK, 2 BASS, 3 RUMBLE, 4 CLOSED, 5 OPEN, 6 CLAP, 7 STRIKE, 8 ACID, 9 STAB,
  10 TEXTURE, 11 VOICE, 12 DUB, 13 SPACE, 14 the groups (LOW, DRUMS, MUSIC and DRUMS' REPEAT),
  15 LIVE BUS and BRIDGE, 16 CONTROL.
- **CC = block base + the parameter's place in its SF plugin's own order**, counting from 0. One
  plugin on a channel: base 20, so its performance row (its first eight parameters) is CC 20-27.
  Channel 14: LOW ENERGY 20, DRUMS ENERGY 30, MUSIC ENERGY 40, REPEAT 50. Channel 15: BUILD 20;
  30-79 are kept for ISOLATOR, GLUE, TONE, LOWEND and LIMIT. Channel 16: CONTROL 20 (MACRO 1-6 are
  20-25).
- **Mixer:** CC 100 volume, 101 send A, 102 send B. Channel 14: 100-102 the LOW, DRUMS and MUSIC
  volumes. Channel 15: 100 LIVE BUS, 101 BRIDGE (102 kept for DECK).
- A parameter on two pages sends the same address from both. The plugins never reorder their
  parameters, so an address never moves.

## 1. LOAD THE SCENE

1. OXI App > Scenes. If no computer folder is assigned yet, assign one (Assign PC/Mac folder).
2. Copy `SF SHOW.oxie16` into the **Scenes** folder inside it and click **Refresh Views**.
3. Drag SF SHOW from **On Computer** onto a free slot under **On Device**.

## 2. E16 SETTINGS

- **Auto Transmit off**, for the scene and in the global settings. Sending stored values on scene
  entry, page change or preset load would pull Live's faders to whatever the E16 last held.
- **MIDI Thru off** (incoming MIDI passed to the outputs).
- Knob speed: every SF SHOW turn follows the scene's acceleration setting.
- Stay in SF SHOW for the whole show.

## 3. LIVE SETTINGS (Settings > Link, Tempo & MIDI)

| PORT | TRACK | SYNC | REMOTE |
|---|---|---|---|
| OXI E16 Port 2, input | off | off | **on** |
| OXI E16 Port 2, output | off | off | **on** |
| OXI E16 Port 1 and Port 3, input and output | off | off | off |

- Port 1 belongs to the OXI App. The E16 sends every message on all its ports, so a second port with
  Remote on would double it: a FREEZE press would flip twice and do nothing.
- Remote on the **output** sends Live's values back: the rings follow song recalls, mouse moves and
  ALLOC slot changes, and the knobs hold the new values.
- **Takeover Mode: Value Scaling.** A knob that disagrees with Live (after a song recall, say) moves
  the parameter smoothly into agreement instead of jumping.

## 4. MAP LIVE, ONCE

1. Open the show set (tracks named as in chapter 7).
2. Unfold every SF plugin's parameter panel (the triangle in its title bar). A parameter that isn't
   listed: Configure, touch it in the plugin window, Configure off.
3. **Cmd-M.** Page by page on the E16: click the target in Live (`sf-show-map.csv` names it), turn
   the knob. The top half and the PERFORM page's shared knobs need mapping only once.
4. In the Mapping Browser: every volume (faders and returns) **Max 0 dB**; DUB FEEDBACK 0-70 %.
5. **Cmd-K** (Key Map): two computer keys, never E16 presses, because a knob is easily pressed while
   it is turned and neither of these may ever happen by accident.
   - **PANIC** on SF CONTROL: press, every SF plugin silent (every header shows PANIC); press again,
     back. Pick a key you can find in the dark.
   - **92 PA**'s activator: press, the speakers off and REC still recording; press again, on.
6. Cmd-M off. Save. Then **File > Save Live Set As Template** (or as the Default Set): MIDI and key
   mappings belong to the Live set, so a new set made from it starts mapped.

## 5. PRESSES

The scene file has no presses yet: the App's encoding of a CC or note press isn't published, and a
guess could ship a press that sends nothing. Two ways:

- **Encode them:** export a small test scene from the OXI App (two minutes) and make_scene.py can be
  taught the App's press format, so the file carries every press. Page 1 only: encoder 1 push = CC 21,
  channel 15, Press & Release, P.Val 127, R.Val 0; encoder 2 push = Note 25, channel 12, Press &
  Release, velocity 127; page 1 output = USB2.
- **Or add them in the App now.** Every press: Output = Same as Page, the channel as listed. DROP
  goes on encoder 4 of all 12 pages. Loading a new SF SHOW file replaces them: add them again after.

| WHERE | ENCODER | PRESS | MESSAGE | LIVE TARGET |
|---|---|---|---|---|
| all 12 pages | 4 | DROP, hold | CC 21 ch 15, Press & Release, P.Val 127, R.Val 0 | 90 LIVE BUS > SF BUILD > DROP |
| SPACE | 9 | DUB THROW, hold | CC 24 ch 12, Press & Release, P.Val 127, R.Val 0 | A DUB > SF DELAY > THROW |
| SPACE | 10 | DUB FREEZE, toggle | Note 25 (C#0) ch 12, Press & Release, velocity 127 | A DUB > SF DELAY > FREEZE |
| SPACE | 13 | SPACE THROW, hold | CC 26 ch 13, Press & Release, P.Val 127, R.Val 0 | B SPACE > SF REVERB > THROW |
| SPACE | 14 | SPACE FREEZE, toggle | Note 25 (C#0) ch 13, Press & Release, velocity 127 | B SPACE > SF REVERB > FREEZE |
| MACROS | 15 | REPEAT ROLL, hold | CC 50 ch 14, Press & Release, P.Val 127, R.Val 0 | 20 DRUMS > SF REPEAT > ROLL |

A hold press is a CC: 127 on press, 0 on release, and the parameter is on for as long as the knob is
down. A toggle press is a note: Live flips the switch on every Note On, so the E16 can never think
FREEZE is on while Live has it off. Every press here is one the set plays through if it happens by
accident (a throw, a roll, a freeze, a drop); what would stop the set is on a computer key.

## 6. CHECKS

1. Every knob: turn it, the right parameter moves. Every press: the right switch.
2. **Holds.** Hold DROP, a THROW and ROLL for three seconds each: the parameter stays on for the whole
   hold and goes off on release. (A long press on the E16 also opens its own functions, such as
   group status; if one interferes, make that press a toggle in `layout.py`.)
3. **Shared knobs.** On page 2, turn LOW ENERGY fully left. Go to page 1 and turn it one step right:
   the sound must move one step, not jump back. If it jumps, the E16 keeps a separate value per page:
   run `python3 make_scene.py --relative-shared`, load the new file (and add the presses again), and
   remap the turns the new `sf-show-map.csv` marks *relative*, turning each knob slowly **left** while
   mapping so Live picks Relative, Twos Complement. Set Max 0 dB again on the remapped volumes.
4. **Page change.** Change pages and leave and re-enter SF SHOW: Live's MIDI input indicator stays
   dark (nothing sent).
5. **Recall.** Switch songs from the ONE: the rings follow, and a recalled knob moves smoothly from
   the new value when turned.
6. **PANIC** from its key: silence, every SF header shows PANIC. Again: back without a click.

## CHANGING THE LAYOUT

Edit `layout.py`: `TOP` is rows 1-2, `PAGES` is rows 3-4 of each page, `PRESSES_*` the presses, `KEYS`
the computer keys. Run `python3 make_scene.py` (it checks that every address names one Live target),
load the new file, add the presses again, and map only what changed. `python3
docs/manual/build_manual.py` puts the new layout in the manual.
