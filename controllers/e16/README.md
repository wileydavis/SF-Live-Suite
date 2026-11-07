# SF SHOW: the OXI E16 scene for the show template

One E16 scene for the whole rig of the manual's chapter 7. Load it once, open the show set (already
mapped: `controllers/live`), save it as your template: nothing is remapped after that.

| FILE | WHAT |
|---|---|
| `SF SHOW.oxie16` | The scene: 12 pages, all 192 turns set (name, ring colour, channel, CC), every press, knob 1's second destination, and the GLIDE RINGS script. |
| `sf-show-map.csv` | Every address once: channel, CC or note, the Live target, the mapping range, where it sits on the E16. Map Live from this. |
| `layout.py` | The layout. One source for the scene, the map, the show set's mappings and the manual's E16 tables. |
| `glide_rings.lua` | The scene's script: travelling knobs show green, walking with the sound (GLIDE RINGS below). |
| `make_scene.py` | Rebuilds the scene, the map and the plugins' copy of it (`Source/core/E16Map.h`: the knobs GLIDE RINGS reports, and every page as each plugin's window draws it at its top): `python3 make_scene.py`. |

## THE GRID

```
 1 LOW ENERGY      2 DRUMS ENERGY    3 MUSIC ENERGY    4 BUILD           press 1: SLOW ALL, press 4: DROP
 5 KICK VOLUME     6 LOW VOLUME      7 DRUMS VOLUME    8 MUSIC VOLUME
 9  ...           10  ...           11  ...           12  ...            rows 3 and 4: the page
13  ...           14  ...           15  ...           16  ...
```

Encoders are numbered row by row from the top left. Rows 1 and 2 are the same on every page (the
E16's screen shows them as LOW ENGY, DRM ENGY, MUS ENGY, BUILD, KICK VOL, LOW VOL, DRUM VOL, MUS VOL).
Rows 3 and 4 are the page: PERFORM, KICK, BASS, RUMBLE, HATS, STRIKE, ACID, STAB, TEXTURE, VOICE,
SPACE, MACROS (the manual, chapter 7, THE E16). Rings: light blue is a volume, white is everything else,
green is knob 1 on SLOW ALL.

**Knob 1 has two destinations.** A quick press flips it from LOW ENERGY to **SLOW ALL** on SF CONTROL
(CC 34, channel 16; the ring turns green and the screen shows a bar under the value): turn it to set how
long your next moves take, OFF (at once) to 128 BAR. Another quick press flips it back. Knobs already
travelling keep the length they set off with. SF SHOW.als has it mapped; in a set of your own (or one
saved before SLOW ALL existed), map it once: Cmd-M, click SLOW ALL on SF CONTROL, press knob 1 (green)
and turn it, Cmd-M off, save.

## ADDRESSES

- **Channel = track.** 1 KICK, 2 BASS, 3 RUMBLE, 4 CLOSED, 5 OPEN, 6 CLAP, 7 STRIKE, 8 ACID, 9 STAB,
  10 TEXTURE, 11 VOICE, 12 DUB, 13 SPACE, 14 the groups (LOW, DRUMS, MUSIC and DRUMS' REPEAT),
  15 LIVE BUS and BRIDGE, 16 CONTROL.
- **CC = block base + the parameter's place in its SF plugin's own order**, counting from 0. One
  plugin on a channel: base 20, so its performance row (its first eight parameters) is CC 20-27.
  Channel 14: LOW ENERGY 20, DRUMS ENERGY 30, MUSIC ENERGY 40, REPEAT 50. Channel 15: BUILD 20
  (its first ten parameters: DUCK TIME, the eleventh, has no address); 30-79 are kept for ISOLATOR,
  GLUE, TONE, LOWEND and LIMIT. Channel 16: CONTROL 20 (MACRO 1-6 are
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
| OXI E16 Port 1, input | off | off | **on** |
| OXI E16 Port 1, output | off | off | **on** |
| OXI E16 Port 2 and Port 3, input and output | off | off | off |

- Port 1 is the port the E16 sends its knobs and presses on. Remote on one E16 port only: a message
  that reached Live on two Remote ports would be taken twice, and a FREEZE press would flip twice and
  do nothing.
- Remote on the **output** sends Live's values back: the rings follow song recalls, mouse moves and
  ALLOC slot changes, and the knobs hold the new values.
- **Takeover Mode: Value Scaling.** A knob that disagrees with Live (after a song recall, say) moves
  the parameter smoothly into agreement instead of jumping.

## 4. MAP LIVE, ONCE

**The show set comes mapped.** `controllers/live/SF SHOW.als` holds every address below, presses
included, with every volume at Max 0 dB and DUB FEEDBACK at 0-70 %: open it (its README has Live's
own settings, set once per computer). Map by hand only in a set of your own;
`python3 controllers/live/check_set.py "My Show.als"` lists what it is missing (a set saved before
SLOW ALL or the presses existed lacks those):

1. Open the show set (tracks named as in chapter 7).
2. Unfold every SF plugin's parameter panel (the triangle in its title bar). A parameter that isn't
   listed: Configure, touch it in the plugin window, Configure off.
3. **Cmd-M.** Page by page on the E16: click the target in Live (`sf-show-map.csv` names it), turn
   the knob. The top half and the PERFORM page's shared knobs need mapping only once.
4. In the Mapping Browser: every volume (faders and returns) **Max 0 dB**; DUB FEEDBACK 0-70 %.
5. Cmd-M off. Save. Then **File > Save Live Set As Template** (or as the Default Set): MIDI mappings
   belong to the Live set, so a new set made from it starts mapped.

## 5. PRESSES

The presses are in the scene file, in the OXI App's own encoding (learned from a test scene exported
from the App on 3 Nov): every press sends to Output ALL, like the turns. Nothing to add in the App.

| WHERE | ENCODER | PRESS | MESSAGE | LIVE TARGET |
|---|---|---|---|---|
| all 12 pages | 1 | SLOW ALL, swap | Swap Destination (sends nothing); Destination 2 = CC 34 ch 16 | 00 CONTROL > SF CONTROL > SLOW ALL |
| all 12 pages | 4 | DROP, hold | CC 21 ch 15, Press & Release, P.Val 127, R.Val 0 | 90 LIVE BUS > SF BUILD > DROP |
| SPACE | 9 | DUB THROW, hold | CC 24 ch 12, Press & Release, P.Val 127, R.Val 0 | A DUB > SF DELAY > THROW |
| SPACE | 10 | DUB FREEZE, toggle | Note 25 (C#0) ch 12, Press & Release, velocity 127 | A DUB > SF DELAY > FREEZE |
| SPACE | 13 | SPACE THROW, hold | CC 26 ch 13, Press & Release, P.Val 127, R.Val 0 | B SPACE > SF REVERB > THROW |
| SPACE | 14 | SPACE FREEZE, toggle | Note 25 (C#0) ch 13, Press & Release, velocity 127 | B SPACE > SF REVERB > FREEZE |
| MACROS | 15 | REPEAT ROLL, hold | CC 50 ch 14, Press & Release, P.Val 127, R.Val 0 | 20 DRUMS > SF REPEAT > ROLL |

A hold press is a CC: 127 on press, 0 on release, and the parameter is on for as long as the knob is
down. A toggle press is a note: Live flips the switch on every Note On, so the E16 can never think
FREEZE is on while Live has it off. The swap sends nothing: it only chooses which destination knob 1
turns. Every press here is one the set plays through if it happens by accident (a throw, a roll, a
freeze, a drop, a flipped knob 1). Nothing that would stop the set is on the E16: the PA's emergency
stop is the audio interface's output knob.

Never give an encoder a second destination without the Swap Destination press: it would send both on
every turn. `make_scene.py` refuses to write one.

## GLIDE RINGS

While a knob travels (SLOW ALL, or a plugin's own SLOW), its ring turns **green** and walks with the
sound to where you set it; when the knob arrives, the ring is white again and shows the knob's value
as before. Turn a travelling knob and for a moment the ring shows your hand (where you are setting
it); the green walk picks up again when you let go. Song recalls with SLOW ALL on show every recalled
knob walking.

- **How:** every SF plugin finds its own knobs in the scene (by its Live track's name; a plugin the
  scene has on one track only also without it) and reports the travelling ones. SF CONTROL sends
  their positions to the E16 as SysEx on **E16 RINGS** (SF CONTROL, setup: OFF, PORT 1-3; default
  PORT 3, which Live doesn't use). The scene's script paints those rings.
- **Nothing depends on it.** The knobs are the scene's plain CC knobs. If the script stops, SF CONTROL
  goes, or E16 RINGS is OFF, the rings show the knobs' values as before (a green ring lets go within
  1.2 s).
- **Needs** E16 firmware with Lua scripting (1.2 or later; 1.3.1 here) and the SF SHOW scene loaded
  from this folder (the script is inside it).
- SF CONTROL's display shows **E16 RINGS PORT 3** in green once it has the port; **NO PORT 3** while
  the E16 isn't there (it looks again every 2 s).
- Knob 1 is green both on SLOW ALL and while LOW ENERGY travels: a travelling ring moves.
- No green: the plugins must come from the same build (SF CONTROL: BUS SHARED); try E16 RINGS PORT 1
  (Live's port: the SysEx goes in beside Live's feedback). An AU of SF LEAD, PERC or ENERGY (on
  several tracks in the scene) doesn't know its track name: use the VST3s.
- Message format and timing: `Source/core/E16Glide.h`. Tests: `SFTests --rings` (the plugins and SF
  CONTROL), `python3 tests/glide_rings_test.py` (the script as shipped, on a mock of the E16's Lua).

## SF TERROIR ON B SPACE

SF TERROIR (convolution, your own places) can take SF REVERB's place on B SPACE: its first eight
controls are SF REVERB's, in the same order, so the E16's SPACE page fits as it is.

1. On B SPACE, put SF TERROIR in place of SF REVERB and turn its **DRY off** (a return is wet only).
2. Cmd-M, then on the E16's SPACE page: turn 13 SPC SIZE, 14 SPC DCY, 15 SPC DARK, 16 SPC DUCK, and
   press 13 (THROW) and 14 (FREEZE), each with its SF TERROIR control clicked. Cmd-M off, save.
3. `python3 controllers/live/check_set.py "My Show.als"` counts SF TERROIR wherever the scene has
   SF REVERB: 108 of 108 again. GLIDE RINGS find it too.

## 6. CHECKS

1. Every knob: turn it, the right parameter moves. Every press: the right switch.
2. **Knob 1.** A quick press: the ring turns green; turn it: SLOW ALL moves on SF CONTROL and LOW ENERGY
   stays put. Press again: white, and the turn moves LOW ENERGY. Do it on two pages.
3. **Holds.** Hold DROP, a THROW and ROLL for three seconds each: the parameter stays on for the whole
   hold and goes off on release. (A long press on the E16 also opens its own functions, such as
   group status; if one interferes, make that press a toggle in `layout.py`.)
4. **Shared knobs.** On page 2, turn LOW ENERGY fully left. Go to page 1 and turn it one step right:
   the sound must move one step, not jump back. If it jumps, the E16 keeps a separate value per page:
   run `python3 make_scene.py --relative-shared`, load the new file, and
   remap the turns the new `sf-show-map.csv` marks *relative*, turning each knob slowly **left** while
   mapping so Live picks Relative, Twos Complement. Set Max 0 dB again on the remapped volumes.
5. **Page change.** Change pages and leave and re-enter SF SHOW: Live's MIDI input indicator stays
   dark (nothing sent).
6. **Recall.** Switch songs from the ONE: the rings follow, and a recalled knob moves smoothly from
   the new value when turned.
7. **Glide rings.** SLOW ALL to 4 BAR, turn a knob on the page and let go: its ring turns green and
   walks to where you set it over four bars, then turns white. Turn it while it walks: the ring shows
   your hand, then green again. SF CONTROL shows E16 RINGS PORT 3 in green.

## CHANGING THE LAYOUT

Edit `layout.py`: `TOP` is rows 1-2, `PAGES` is rows 3-4 of each page, `PRESSES_*` the presses.
Run `python3 make_scene.py` (it checks that every address names one Live target),
load the new file and map only what changed. It also rewrites `Source/core/E16Map.h`, the plugins'
copy of the knob map for GLIDE RINGS: rebuild the suite after a layout change. `python3
docs/manual/build_manual.py` and `python3 docs/manual/build_show_manual.py` put the new layout in the
manuals.
