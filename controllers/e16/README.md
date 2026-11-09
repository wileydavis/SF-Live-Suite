# PERF and MIX: the OXI E16 scenes for the show template

Two E16 scenes for the whole rig of the manual's chapter 7: **PERF**, one page per track and the NRG
page, and **MIX**, the levels, the returns and the macros. Load them once, open the show
set (already mapped: `controllers/live`), save it as your template: nothing is remapped after that.

| FILE | WHAT |
|---|---|
| `PERF.oxie16` | Scene 1: 12 pages, every turn set (name, ring colour, channel, CC), every press, knob 1's second destination, and the script (GLIDE RINGS, TRACK FOCUS). Its names and colours are the ones you set in the App on Nov 8. |
| `MIX.oxie16` | Scene 2: LEVELS, SPACE and MACROS; pages 4-12 are left empty (their knobs send nothing). |
| `sf-show-map.csv` | Every address of both scenes once: channel, CC or note, the Live target, the mapping range, the scene, where it sits on the E16. Map Live from this. |
| `layout.py` | The layout. One source for the scenes, the map, the show set's mappings, the ONE's channels and the manuals' E16 tables. |
| `glide_rings.lua` | The scenes' script: travelling knobs show green, walking with the sound (GLIDE RINGS below), and a PERF page tells Live its track (TRACK FOCUS below). |
| `make_scene.py` | Rebuilds both scenes, the map and the plugins' copy of it (`Source/core/E16Map.h`: the knobs GLIDE RINGS reports, and every page as each plugin's window draws it at its top, rings in the E16's colours): `python3 make_scene.py`. |
| `../live/SF_E16` | Live's side of TRACK FOCUS: a control surface that selects the track a PERF page names. |

**Since Nov 8** (load both scenes, with the new SF SHOW.als and SF START): one number per track. A
track's number is its MIDI channel on the E16 and on the OXI One, and its page in PERF: 1 KICK,
2 CLOSED HAT, 3 OPEN HAT, 4 CLAP, 5 PERC, 6 BASS, 7 RUMBLE, 8 LEAD, 9 CHORD, 10 PAD, 11 VOICE, in
the set's order from the top. On the E16 each goes by its code:

| TRACK | 1 KICK | 2 CLOSED HAT | 3 OPEN HAT | 4 CLAP | 5 PERC | 6 BASS | 7 RUMBLE | 8 LEAD | 9 CHORD | 10 PAD | 11 VOICE | DRUMS | LOW | MUSIC |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| CODE | KCK | CHH | OHH | CLP | PRC | BAS | RMB | LED | CRD | PAD | VOX | DRM | LOW | MUS |

A page's title is its track's code; an ENERGY knob reads KCK NRG (short KCK), a volume KCK VOL.

## THE GRID

Encoders are numbered row by row from the top left: 1-4 the top row, 13-16 the bottom row.

```
PERF, pages 1-11: one per track (page n is track n; the title its code: KCK ... VOX)
 1 its sound  [press: SLOW ALL]   2 its sound      3 its sound       4 its sound
 5 its sound                      6 its sound      7 its sound       8 its sound
 9 GATE (DECAY)                  10 DENSITY       11 FILTER         12 LEVEL
13 SEND A (DUB)                  14 SEND B (SPACE) 15 NEXT SONG [GO] 16 BUILD [DROP]

PERF, page 12: NRG, the ENERGY knobs (knob n for track n)
 1 KCK NRG [SLOW ALL]   2 CHH NRG           3 OHH NRG           4 CLP NRG
 5 PRC NRG              6 BAS NRG           7 RMB NRG           8 LED NRG
 9 CRD NRG             10 PAD NRG          11 VOX NRG          12 DRM NRG (group)
13 LOW NRG (group)     14 MUS NRG (group)  15 NEXT SONG [GO]   16 BUILD [DROP]

MIX
LEVELS: the volumes in NRG's places (KCK VOL ... VOX VOL, 12-14 the groups), 15 DUB and 16 SPACE
        the returns
SPACE:  1 DUB TIME [THROW]  2 DUB FDBK [FREEZE]  3 DUB TONE  4 DUB DRV
        5 SPC SIZE [THROW]  6 SPC DCY [FREEZE]   7 SPC DARK  8 SPC DUCK
MACROS: 1-6 MACRO 1-6  7 SLOW ALL  9 RPT LEN [ROLL]  10 RPT PTCH
```

A track's page: rows 1 and 2 are its instrument's own knobs; row 3 is the same four on every page
(GATE, or DECAY where the instrument has no gate, DENSITY, FILTER, LEVEL); row 4 is the track's two
sends and the song and build, on every page. Rows 1 and 2:

| PAGE | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| 1 KCK | TUNE | PUNCH | CLICK | DRIVE | PITCH T | LAYER | ECHO SPC | ECHO SWG |
| 2 CHH, 3 OHH, 4 CLP | TUNE | TONE | ACCENT | DRIVE | ECHO SPC | ECHO SWG | ECHO PRB | SPREAD |
| 5 PRC | POWER | DISTANCE | SIZE | MATERIAL | POSITION | TENSION | SPREAD | TUNE |
| 6 BAS | CUTOFF | RES | ENV AMT | DECAY | DRIVE | ACCENT | GLIDE | PCKT DEP |
| 7 RMB | SIZE | DARK | GRIT | PUMP | TUNE | PREDELAY | WIDTH | PCKT SHP |
| 8 LED | CUTOFF | RES | ENV AMT | DECAY | DRIVE | ACCENT | GLIDE | COLOR |
| 9 CRD | CUTOFF | RES | ENV AMT | DECAY | ATTACK | SPACE | DETUNE | PCKT DEP |
| 10 PAD | CUTOFF | RES | COLOR | DETUNE | ATTACK | DECAY | SPACE | PCKT DEP |
| 11 VOX | SIGNAL | TONE | TUNE | DRIVE | ECHO | ECHO TME | ECHO FBK | PCKT DEP |

**Rings, by role** (your colours from the App, Nov 8; `layout.py` holds them by number):

| RING | WHAT |
|---|---|
| white (34) | the instrument's own sound: a track's knobs 1-8 |
| red (63) | a track's GATE or DECAY, DENSITY and FILTER (9-11), and its NRG knob |
| light blue (18) | a level: LEVEL, the sends, every volume on MIX |
| pink (50) | a group's NRG knob (DRM, LOW, MUS) |
| violet (9) | NEXT SONG and BUILD (15, 16) |
| green (90) | knob 1 on SLOW ALL, and any knob while it travels (GLIDE RINGS) |

Each plugin's window draws its E16 knobs at its top with the same ring colours (`layout.py`
RING_RGB, written into `Source/core/E16Map.h`). White, light blue, pink and green are as other E16
owners have seen them on the hardware; red and violet are where the App's palette puts 63 and 9,
not yet held up against an E16. If yours lights either differently, change its RGB (and its name in
RING_NAME) in `layout.py`, run `make_scene.py` and rebuild.

**Presses:** on every PERF page, knob 1's press swaps it to SLOW ALL (below), 15's is **GO** (the
song NEXT SONG picked lands on the next bar line) and 16's is **DROP**. On NRG, every ENERGY knob's
press sends it back to the centre (neutral; with SLOW ALL on it glides there), except knob 1's. MIX:
the SPACE page's presses throw and freeze the returns; MACROS 9's rolls REPEAT.

**Knob 1 has two destinations** on every PERF page. A quick press flips it from the page's first
knob (KCK NRG on NRG) to **SLOW ALL** on SF CONTROL (CC 12, channel 16; the ring turns green and the
screen shows a bar under the value): turn it to set how long your next moves take, OFF (at once) to
128 BAR. Another quick press flips it back. Knobs already travelling keep the length they set off
with. SLOW ALL is also MACROS 7 in MIX, a plain knob there.

## ADDRESSES [Nov 8]

The channel is the track. A track's page sends CC 12-23 on its own channel whatever instrument is on
it, so a track's page can be mapped to any other instrument you put on that track (Cmd-M, click its
parameter, turn the knob) without touching another track. The OXI One plays each part on the same
channel (port A): `controllers/oxi-one`.

| CHANNEL | TRACK | ITS PAGE (PERF) | ENERGY (NRG) | VOLUME (LEVELS) | SEND A, B (ITS PAGE 13, 14) | ALSO |
|---|---|---|---|---|---|---|
| 1-11 | 1 KICK ... 11 VOICE | knobs 1-12: CC 12-23 | CC 28 (knob n) | CC 102 (knob n) | CC 103, 104 | |
| 12 | A DUB | | | CC 102 (15) | | SPACE 1-4: CC 44-47; THROW CC 60, FREEZE note 61 (C#3) |
| 13 | B SPACE | | | CC 102 (16) | | SPACE 5-8: CC 48-51; THROW CC 64, FREEZE note 65 (F3) |
| 14 | DRUMS, LOW, MUSIC | | CC 28, 29, 30 (12-14) | CC 102, 103, 104 (12-14) | | DRUMS' REPEAT, MACROS 9-10: CC 52, 53; ROLL CC 68 |
| 15 | LIVE BUS | | | | | BUILD CC 27, DROP CC 43 (PERF 16) |
| 16 | CONTROL | | | | | SLOW ALL CC 12 (knob 1's swap; MACROS 7), NEXT SONG CC 26 and GO CC 42 (PERF 15), MACRO 1-6 CC 44-49 (MACROS 1-6); TRACK FOCUS CC 119 (the script, as a PERF page comes up: its track's number; for SF E16, never mapped) |

- **The rule:** in PERF a knob sends its place + 11 and a press its place + 27; in MIX a knob its
  place + 43 and a press its place + 59; always on the channel of the track it works on. A track's
  ENERGY is CC 28 (the groups, sharing channel 14: 28, 29, 30), its volume CC 102, its sends CC 103
  and 104 (the groups' volumes: 102, 103, 104). An ENERGY knob's press sends 64 on the knob's own
  address: Live sets the knob to its centre and sends the E16 the value back.
- A parameter on two pages sends the same address from both (Live takes one mapping per parameter):
  NEXT SONG, BUILD, SLOW ALL.
- Never used: CC 6, 38 and 96-101 (data entry and RPN/NRPN: Live and many devices read them as parts
  of a longer message) and 120-127. `layout.py` refuses them.
- Every volume and send stops at 0 dB (Max 0 dB in Live's mapping).

## 1. LOAD THE SCENES

1. OXI App > Scenes. If no computer folder is assigned yet, assign one (Assign PC/Mac folder).
2. Copy `PERF.oxie16` and `MIX.oxie16` into the **Scenes** folder inside it and click **Refresh
   Views**. They replace SF SHOW and SF MIX (and your own PERF: these carry its names and colours).
3. Drag PERF, then MIX, from **On Computer** onto two free slots under **On Device**, among the
   scenes the E16's home screen shows.

## 2. E16 SETTINGS

- **Auto Transmit off**, for both scenes and in the global settings. Sending stored values on scene
  entry, page change or preset load would pull Live's faders to whatever the E16 last held.
- **MIDI Thru off** (incoming MIDI passed to the outputs).
- Knob speed: every turn follows its scene's acceleration setting.
- PERF is the show; MIX is for setting levels and the returns. Changing scenes sends nothing to a
  knob's address; a PERF page that comes up sends its track's number for TRACK FOCUS (below).

## 3. LIVE SETTINGS (Settings > Tempo & MIDI; before Live 12.4, Link, Tempo & MIDI)

| PORT | TRACK | SYNC | REMOTE |
|---|---|---|---|
| OXI E16 Port 1, input | off | off | **on** |
| OXI E16 Port 1, output | off | off | **on** |
| OXI E16 Port 2 and Port 3, input and output | off | off | off |

- Port 1 is the port the E16 sends its knobs and presses on. Remote on one E16 port only: a message
  that reached Live on two Remote ports would be taken twice, and a FREEZE press would flip twice and
  do nothing. Track off: the E16's CCs never reach an instrument.
- Remote on the **output** sends Live's values back: the rings follow song recalls, mouse moves and
  ALLOC slot changes, and the knobs hold the new values.
- **Takeover Mode: Value Scaling.** A knob that disagrees with Live (after a song recall, say) moves
  the parameter smoothly into agreement instead of jumping.
- **Control Surface (optional, for TRACK FOCUS): SF E16**, Input OXI E16 (Port 1), Output None.
  Install it first (TRACK FOCUS, below).

## 4. MAP LIVE, ONCE

**The show set comes mapped.** `controllers/live/SF SHOW.als` holds every address of both scenes,
presses included, with every volume and send at Max 0 dB and DUB FEEDBACK at 0-70 %: open it (its
README has Live's own settings, set once per computer). Map by hand only in a set of your own;
`python3 controllers/live/check_set.py "My Show.als"` lists what it is missing:

1. Open the show set (tracks named as in chapter 7).
2. Unfold every SF plugin's parameter panel (the triangle in its title bar). A parameter that isn't
   listed: Configure, touch it in the plugin window, Configure off.
3. **Cmd-M.** Page by page on the E16: click the target in Live (`sf-show-map.csv` names it), turn
   the knob. NEXT SONG, BUILD and SLOW ALL need mapping only once. **Before you turn to another PERF
   page, click an empty spot** so no target is selected: a PERF page sends TRACK FOCUS's CC as it comes
   up, and Cmd-M would map the selected target to it (`check_set.py` finds one that was).
4. In the Mapping Browser: every volume and send **Max 0 dB**; DUB FEEDBACK 0-70 %.
5. Cmd-M off. Save. Then **File > Save Live Set As Template** (or as the Default Set): MIDI mappings
   belong to the Live set, so a new set made from it starts mapped.

## 5. PRESSES

The presses are in the scene files, in the OXI App's own encoding (learned from a test scene exported
from the App on 3 Nov): every press sends to Output ALL, like the turns. Nothing to add in the App.

| WHERE | ENCODER | PRESS | MESSAGE | LIVE TARGET |
|---|---|---|---|---|
| every PERF page | 1 | SLOW ALL, swap | Swap Destination (sends nothing); Destination 2 = CC 12 ch 16 | CONTROL > SF CONTROL > SLOW ALL |
| every PERF page | 15 | GO, hold | CC 42 ch 16, Press & Release, P.Val 127, R.Val 0 | CONTROL > SF CONTROL > GO |
| every PERF page | 16 | DROP, hold | CC 43 ch 15, Press & Release, P.Val 127, R.Val 0 | LIVE BUS > SF BUILD > DROP |
| NRG | 2-14 | ENERGY to the centre | CC 28 on the knob's own channel (the groups 28, 29, 30 on ch 14), Press & Release, P.Val 64, R.Val 64 | that knob's SF ENERGY |
| SPACE (MIX) | 1 | DUB THROW, hold | CC 60 ch 12, Press & Release, P.Val 127, R.Val 0 | A DUB > SF DELAY > THROW |
| SPACE (MIX) | 2 | DUB FREEZE, toggle | Note 61 (C#3) ch 12, Press & Release, velocity 127 | A DUB > SF DELAY > FREEZE |
| SPACE (MIX) | 5 | SPACE THROW, hold | CC 64 ch 13, Press & Release, P.Val 127, R.Val 0 | B SPACE > SF REVERB > THROW |
| SPACE (MIX) | 6 | SPACE FREEZE, toggle | Note 65 (F3) ch 13, Press & Release, velocity 127 | B SPACE > SF REVERB > FREEZE |
| MACROS (MIX) | 9 | REPEAT ROLL, hold | CC 68 ch 14, Press & Release, P.Val 127, R.Val 0 | DRUMS > SF REPEAT > ROLL |

A hold press is a CC: 127 on press, 0 on release, and the parameter is on for as long as the knob is
down. A toggle press is a note: Live flips the switch on every Note On, so the E16 can never think
FREEZE is on while Live has it off. A centre press is its knob's own CC with the value 64: no mapping
of its own. The swap sends nothing: it only chooses which destination knob 1 turns. GO is safe by
accident: a song only changes when NEXT SONG picked another one first (SF CONTROL shows it in amber,
PRESS GO). Every press here is one the set plays through if it happens by accident (a throw, a roll,
a freeze, a drop, an ENERGY back to neutral, a flipped knob 1). Nothing that would stop the set is on
the E16: the PA's emergency stop is the audio interface's output knob.

Never give an encoder a second destination without the Swap Destination press: it would send both on
every turn. `make_scene.py` refuses to write one.

## GLIDE RINGS

While a knob travels (SLOW ALL, or a plugin's own SLOW), its ring turns **green** and walks with the
sound to where you set it; when the knob arrives, the ring is white again and shows the knob's value
as before. Turn a travelling knob and for a moment the ring shows your hand (where you are setting
it); the green walk picks up again when you let go. Song recalls with SLOW ALL on show every recalled
knob walking.

- **How:** every SF plugin finds its own knobs in the scenes (by its Live track's name; a plugin the
  scenes have on one track only also without it) and reports the travelling ones. SF CONTROL sends
  their positions to the E16 as SysEx on **E16 RINGS** (SF CONTROL, setup: OFF, PORT 1-3; default
  PORT 3, which Live doesn't use). Each scene carries the script with its own knob map.
- **Nothing depends on it.** The knobs are the scenes' plain CC knobs. If the script stops, SF CONTROL
  goes, or E16 RINGS is OFF, the rings show the knobs' values as before (a green ring lets go within
  1.2 s).
- **Needs** E16 firmware with Lua scripting (1.2 or later; 1.3.1 here) and the scenes loaded from this
  folder (the script is inside them; your own PERF from Nov 8 has it too).
- SF CONTROL's display shows **E16 RINGS PORT 3** in green once it has the port; **NO PORT 3** while
  the E16 isn't there (it looks again every 2 s).
- Knob 1 of a PERF page keeps its own ring (it swaps, and the script can't tell which destination
  is up); a Live volume or send never goes green.
- No green: the plugins must come from the same build (SF CONTROL: BUS SHARED); try E16 RINGS PORT 1
  (Live's port: the SysEx goes in beside Live's feedback). An AU of SF LEAD, PERC or ENERGY (on
  several tracks in the scenes) doesn't know its track name: use the VST3s.
- Message format and timing: `Source/core/E16Glide.h`. Tests: `SFTests --rings` (the plugins and SF
  CONTROL), `python3 tests/glide_rings_test.py` (the script as shipped in both scenes, on a mock of the
  E16's Lua).

## TRACK FOCUS

Turn to a PERF track page and Live selects that track: page 6 (BAS) selects 6 BASS, as a click on
its name would. NRG and MIX's pages leave the selection where it is, so you can turn to NRG and
still see the track you came from. With Live's Auto-Hide Plug-In Windows on, the selected track's
open plugin windows come up with its page and the others hide: the page you play is the window you
see.

- **How:** when a page comes up (and when the scene loads), the scene's script sends **CC 119 on
  channel 16** with the page's track number, to every output, as the knobs send. **SF E16**, a
  control surface for Live (`controllers/live/SF_E16`), takes that CC and selects the first track
  whose name starts with the number. The numbers are the set's own (one
  number per track), so a renamed track still works while its name starts with its number.
- **Install, once per computer:** copy the folder `controllers/live/SF_E16` into
  `~/Music/Ableton/User Library/Remote Scripts/` (make Remote Scripts if it isn't there) and restart
  Live. Settings > Tempo & MIDI, a free Control Surface slot: **SF E16**, Input **OXI E16 (Port 1)**,
  Output **None**. Leave the E16's Remote switches as in chapter 3. Then Settings > Plug-Ins:
  **Multiple Plug-In Windows** and **Auto-Hide Plug-In Windows** on, and open each instrument's SF
  window once (Live hides and shows windows that are open; it doesn't open them).
- **Nothing depends on it.** Without SF E16 the CC reaches Live on a Remote port with nothing mapped
  to it and does nothing; an E16 firmware without the script's MIDI out skips it (the rings still
  work). Mapping by hand: in Cmd-M, a page change with a target selected maps that target to CC 119
  (chapter 4, step 3); `check_set.py` fails a set with anything mapped there.
- A track inside a folded group: selected all the same; if Live won't select a hidden track, SF E16
  unfolds its group first.
- **Not yet tried in Live** (Nov 8): the script and SF E16 are tested here against stand-ins for the
  E16's Lua and for Live's control-surface framework. When Live loads SF E16, its Log.txt says
  `SF E16: E16 pages select their tracks`. If a page doesn't select its track: check the Control
  Surface row, then that the scenes in the E16 are the ones from this folder (the script carries the
  numbers).
- Tests: `python3 tests/glide_rings_test.py` (the script: each PERF page sends its number, NRG and
  MIX nothing), `python3 tests/sf_e16_test.py` (SF E16 on the show set's tracks, folded groups).

## SF TERROIR ON B SPACE

SF TERROIR (convolution, your own places) can take SF REVERB's place on B SPACE: its first eight
controls are SF REVERB's, in the same order, so MIX's SPACE page fits as it is.

1. On B SPACE, put SF TERROIR in place of SF REVERB and turn its **DRY off** (a return is wet only).
2. Cmd-M, then on MIX's SPACE page: turn 5 SPC SIZE, 6 SPC DCY, 7 SPC DARK, 8 SPC DUCK, and press
   5 (THROW) and 6 (FREEZE), each with its SF TERROIR control clicked. Cmd-M off, save.
3. `python3 controllers/live/check_set.py "My Show.als"` counts SF TERROIR wherever the scenes have
   SF REVERB: 210 of 210 again. GLIDE RINGS find it too.

## 6. CHECKS

1. Every knob: turn it, the right parameter moves. Every press: the right switch. A track's page
   moves that track's instrument only.
2. **Knob 1.** A quick press: the ring turns green; turn it: SLOW ALL moves on SF CONTROL and the
   page's first knob stays put. Press again: white, and the turn moves the first knob. Do it on two
   pages.
3. **Holds.** Hold DROP, a THROW and ROLL for three seconds each: the parameter stays on for the
   whole hold and goes off on release. On NRG, press an ENERGY knob turned away from the centre:
   it goes back to the centre, and so does its ring. (A long press on the E16 also opens its own
   functions, such as group status; if one interferes, make that press a toggle in `layout.py`.)
4. **Shared knobs.** On page 1 (KCK), turn NEXT SONG (15) fully left. Go to page 6 (BAS) and turn it
   one step right: the pick must move one step, not jump back. If it jumps, the E16 keeps a separate value
   per page: run `python3 make_scene.py --relative-shared`, load the new files, and remap the turns
   the new `sf-show-map.csv` marks *relative*, turning each knob slowly **left** while mapping so Live
   picks Relative, Twos Complement.
5. **Page and scene change.** Change pages, and change to MIX and back: Live's MIDI input indicator
   stays dark (nothing sent).
6. **Recall.** Turn NEXT SONG (15) to another song and press it: on the next bar line the rings
   follow, and a recalled knob moves smoothly from the new value when turned.
7. **Glide rings.** SLOW ALL to 4 BAR, turn a knob on a track's page and let go: its ring turns green
   and walks to where you set it over four bars, then turns white. Turn it while it walks: the ring
   shows your hand, then green again. SF CONTROL shows E16 RINGS PORT 3 in green.

## CHANGING THE LAYOUT

Edit `layout.py`: `CODE` the tracks' codes, `SOUND` each track page's rows 1 and 2 and its GATE or
DECAY, `NRG` the ENERGY page, `LEVELS`, `SPACE_PAGE` and `MACROS_PAGE` MIX's pages, `PRESSES_PAGE` the
presses, and the ring colours by role (WHITE, SHAPE, LIGHT_BLUE, GROUP, SET, GREEN). A name or colour
changed in the App goes back into `layout.py`, or the next `make_scene.py` writes over it.
Run `python3 make_scene.py` (it checks that every address names one Live target), load the new files
and map only what changed. It also rewrites `Source/core/E16Map.h`, the plugins' copy of the knob map
for GLIDE RINGS and their windows' E16 grid: rebuild the suite after a layout change. `python3
controllers/live/make_set.py` maps a new set, and `python3 docs/manual/build_manual.py` and `python3
docs/manual/build_show_manual.py` put the new layout in the manuals.
