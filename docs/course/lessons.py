"""SF LIVE SUITE: BUILD THE FIRST SET. The lessons, step by step.

Each step: where your hands are, a short title, the lines on the phone, and `say`, what the guide
speaks. Knobs, parameters and presets come through course.py, which checks them against the scene
(controllers/e16/layout.py), the plugins (docs/manual/specs.json) and the factory presets.
OXI One MkII procedures follow OXI's MkII manual v2.0a (firmware 2.2.10 and later).
Build with build_course.py.
"""
from course import (S, L, K, e16, one, preset, param, choice, knob, page_no, SLOW_ALL,
                    QUARTERS, EIGHTHS, OFFBEATS, BACKBEAT)

P = preset
pg = page_no


def shift_page(title):
    return f"hold **Shift** (the O) and tap knob **{pg(title)}**"


def Shift_page(title):
    return f"Hold **Shift** (the O) and tap knob **{pg(title)}**"


def slots(title, rows, right="", foot=""):
    """The ONE's Load or Save view: sequencer 1 on the top row, pattern slots 1-16 across."""
    return one(title, [(9 - seq, f"SEQ {seq}", cols) for seq, cols in rows], right=right, foot=foot)


# The ONE's grid, as the lessons draw it.
KIT = "SEQUENCER 1 · MULTITRACK"
SAVEV = "SAVE VIEW · SLOTS 1-16 ACROSS"
BASS = "SEQUENCER 2 · MONO"
STAB = "SEQUENCER 4 · MONO"
L1_KICK = (1, "1 KICK", QUARTERS)
L2_CLOSED = (2, "2 CLOSED", EIGHTHS)
L3_OPEN = (3, "3 OPEN", OFFBEATS)
L4_CLAP = (4, "4 CLAP", BACKBEAT)
ACID_LINE = [(1, "C", [1, 3, 6, 9, 11, 14]), (4, "D#", [4, 12]), (8, "G", [7, 15])]

MIDI_FROM = [
    ["00 CONTROL", "Port 2 (B) · ch 16"],
    ["01 KICK", "Port 1 (A) · ch 1"],
    ["21 CLOSED", "Port 1 (A) · ch 2"],
    ["22 OPEN", "Port 1 (A) · ch 3"],
    ["23 CLAP", "Port 1 (A) · ch 4"],
    ["11 BASS", "Port 2 (B) · ch 1"],
    ["31 ACID", "Port 2 (B) · ch 2"],
    ["32 STAB", "Port 2 (B) · ch 3"],
    ["33 TEXTURE", "Port 2 (B) · ch 4"],
    ["34 VOICE", "Port 2 (B) · ch 5"],
    ["24 STRIKE", "Port 2 (B) · ch 6"],
    ["41 DECK", "Port 2 (B) · ch 7"],
]

STORE = ["00 CONTROL", "SF CONTROL", param("CONTROL", "store_all")]
STORE_TO = lambda song: ["00 CONTROL", "SF CONTROL", "SETUP", param("CONTROL", "store_to"), choice("CONTROL", "store_to", song)]

# ====================================================================================== 1
L1 = L(1, "SET UP", "Live, the ONE and the E16 talking to each other. Once only: skip it if your rig already plays.", "25 MIN", [
    S("START", "Set the rig up, once",
      ["Live, the ONE and the E16, talking to each other. You do this once. After it, nothing on the rig is routed or mapped again.",
       ["Live's audio and MIDI settings", "SF SHOW, saved as your own set", "Which track listens to which part of the ONE",
        "The E16 scene", "The ONE following Live's clock"],
       "**Already set up and playing?** Go back to LESSONS and start lesson 2."],
      "This first lesson sets the rig up, once. By the end of it, Live, the Oxy One and the E16 are talking to each other, "
      "and you won't route or map anything again. If your rig already plays, go back to the lessons and start at lesson two. "
      "Otherwise: laptop open, the One and the E16 plugged in over USB. Tap next when you're ready.", wait=3),
    S("LAPTOP", "Audio settings",
      ["Sample Rate **48 kHz**. Buffer Size **256** samples.", "Audio output: the interface your speakers or headphones are on."],
      "Open Live's settings, on the Audio tab. Set the sample rate to forty-eight kilohertz and the buffer size to two hundred "
      "and fifty-six samples, and choose the interface your speakers or headphones are on.",
      path=["Live", "Settings", "Audio"],
      why="Pocket, the ducking that keeps every part out of the kick's way, needs a buffer of 512 or less.", wait=8),
    S("LAPTOP", "MIDI ports",
      ["Turn on only what the table shows; everything else in the list off.", "Live calls the ONE's ports **OXI ONE MKII (Port 1)** and **(Port 2)**: the ONE's A and B.",
       "Unfold the **OXI ONE MKII (Port 1)** output row (the triangle at its left): **MIDI Clock Sync Delay -70 ms**."],
      "Now the Tempo and MIDI tab. The One shows up as two ports, Port 1 and Port 2: on the One they're A, and B. Turn Track on "
      "for both of its inputs. On Port 1's output, turn Sync on, and on nothing else: that's the clock the One follows. Then unfold "
      "that output row with the little triangle at its left, and set MIDI Clock Sync Delay to minus seventy milliseconds. The One's "
      "notes come back to Live late, and this sends the clock early to make up for it. For the E16, turn Remote "
      "on for its Port 1, input and output, and leave its ports two and three off.",
      path=["Live", "Settings", "Tempo & MIDI"],
      table=[["OXI ONE MKII (Port 1), in", "Track **on**"], ["OXI ONE MKII (Port 2), in", "Track **on**"], ["OXI ONE MKII (Port 1), out", "Sync **on**, delay **-70 ms**"],
             ["OXI E16 (Port 1), in and out", "Remote **on**"], ["Every other port", "all **off**"]],
      why="Without the clock delay the ONE plays about 70 ms behind Live's beat: a drop opens before the kick, and a song change lands a bar late. Sync on one port only: two clocks confuse the ONE. A second Remote port would double every E16 message.", wait=15),
    S("LAPTOP", "Takeover: Value Scaling",
      ["Same tab: **Takeover Mode › Value Scaling**."],
      "Same tab, further down: set Takeover Mode to Value Scaling. When a song changes, the knobs and Live disagree for a moment. "
      "With value scaling, your next turn moves the sound smoothly from where it is, instead of jumping.",
      path=["Tempo & MIDI", "Takeover Mode", "Value Scaling"], wait=5),
    S("LAPTOP", "Open SF SHOW, keep it as yours",
      ["Open **SF SHOW.als** from the repository: controllers/live.", "**File › Save Live Set As…** into a folder of your own. Name it **FIRST SET**."],
      "Open SF SHOW from the repository's controllers, live folder. Every track, every plugin, the routing, which track listens "
      "to which part of the One, and all the E16 mappings are already in it. Save it straight away under a new name, First Set, "
      "in a folder of your own. The songs you're about to build are stored inside this set.",
      why="Every track, plugin, route, MIDI From and E16 mapping is already in it. Your songs will live in this set.", wait=8),
    S("LAPTOP", "Two options",
      ["**Delay Compensation**: on.", "**Reduced Latency When Monitoring**: off."],
      "In Live's Options menu: Delay Compensation on, and Reduced Latency When Monitoring off.",
      path=["Live", "Options"], wait=5),
    S("LAPTOP", "Where you hear it",
      ["Track **92 PA** takes the **Main** after its fader, through **SF ROOM EQ** and a safety **SF LIMIT**, out on **1/2**. Speakers or headphones on another pair: set its **Audio To** there.",
       "Keep **92 PA**'s fader at **0 dB**. **Main** goes to a pair that goes nowhere (5/6): on purpose."],
      "Find track ninety-two, PA, near the right. That's what you hear. It takes the Main after its fader, through the room "
      "correction and a safety limiter, out on outputs one and two. If your speakers or headphones are on another pair, set its "
      "Audio To there. Leave its fader at zero. The Main itself goes to a pair that goes nowhere, and that's on purpose: "
      "ninety-two PA carries the set to the speakers, and ninety-three, REC, records the same signal without the room correction.",
      why="A venue's correction goes on 92 PA, never into the recording. Its limiter never touches the set: it only catches what the correction pushes past -0.5 dB.", wait=8),
    S("LAPTOP", "Each track listens to its part",
      ["**MIDI From** is already set on every instrument track: the ONE's port, then the channel. Check a few against the table.",
       "A port shown as missing: your Live names the ONE differently. Set that track from the table.",
       "**12 RUMBLE** stays on No Input: it follows the kick. Never **All Ins**: every instrument would play every part."],
      "Every instrument track already listens to its own part of the One: MIDI From is the One's port, and the channel "
      "underneath. Check a few against the table on screen. The drums are on Port 1, channels one to four. Everything pitched, "
      "and SF Control, is on Port 2. If a track shows its port as missing, your Live names the One differently: set that track "
      "from the table. Rumble stays on No Input: it follows the kick by itself. And never All Ins, or every instrument plays every part.",
      table=MIDI_FROM, wait=15),
    S("LAPTOP", "The emergency stop",
      ["The interface's output knob (the Babyface's big knob) is the PA's volume, and the emergency stop.",
       "Turn it down: the room goes quiet, the recording carries on. Nothing in Live or on the E16 stops the set."],
      "One thing to know before any sound. The PA's volume, and the emergency stop, is the output knob on the audio interface: "
      "the Babyface's big knob. Turn it down and the room goes quiet, while the recording carries on. Nothing in Live, and "
      "nothing on the E16, stops the set: a knob you press by accident can only do something the set plays through.",
      why="Feedback running away, a stuck note: the knob first, then find the cause.", wait=5),
    S("E16", "Load the scene",
      ["**OXI App › Scenes**: copy **SF SHOW.oxie16** (controllers/e16) into the Scenes folder, then **Refresh Views**.",
       "Drag **SF SHOW** from On Computer onto one of the first seven slots under On Device (S.1 to S.7).",
       "**Auto Transmit off**, in the scene and in the global settings. **MIDI Thru off**."],
      "Now the E16, from the laptop. In the Oxy app, open Scenes. Copy the SF SHOW scene file from the repository's controllers, "
      "E16 folder into the app's Scenes folder, and refresh. Then drag SF SHOW onto one of the first seven slots on the device: "
      "those are the ones the home screen opens. Turn Auto Transmit "
      "off, in the scene and in the global settings, and MIDI Thru off.",
      sub="IN THE OXI APP",
      why="Auto Transmit would pull Live's faders to whatever the E16 last held, every time you change page.", wait=20),
    S("E16", "Open it, change pages",
      ["On the E16's home screen, tap the knob under **SF SHOW** (knobs 1 to 7 open scenes S.1 to S.7).",
       "**Pages:** hold **Shift**, the button in the O, and tap a knob: knob 1 is page 1, up to knob 12."],
      "On the E16's home screen, tap the knob under SF SHOW to open it. Pages work like this: hold Shift, the button inside the O, "
      "and tap a knob. Knob one is page one, up to knob twelve for page twelve. Try it. Shift and two: the kick page. "
      "Shift and one: back to Perform. The top two rows stay the same on every page.",
      e16=e16("PERFORM", foot="PAGES: HOLD SHIFT (THE O), TAP KNOB 1-12"), wait=10),
    S("ONE", "Follow Live's clock",
      ["ONE stopped: **Shift + Stop** until the header shows **USB**, not INT.",
       "**Shift + Back** opens the config: **MIDI › Start Stop Msgs Ignore: No**, **MIDI › MIDI Transport Msgs Send: Off**; **Performance › Auto Sync: On**. Hold **Shift + Back** to save it.",
       "Start and stop from **Live**, never the ONE's Play: Live sends its clock only while it plays.",
       "**Check:** play in Live, change Live's tempo: the ONE's tempo follows. Stop Live: the ONE stops."],
      "Now the One. While it's stopped, hold Shift and press Stop, and keep pressing until the header shows USB. If it shows "
      "I.N.T., that's the One's own clock. Then hold Shift and press Back for the config: in MIDI, set Start Stop Messages Ignore "
      "to No, and MIDI Transport Messages Send to Off; in Performance, set Auto Sync on. Hold Shift and Back to save the config. "
      "From now on, start and stop everything from Live, never with the One's own Play button: Live sends its clock only while it "
      "plays. Press play in Live: the One starts with it. Change Live's tempo: the One follows. Stop Live, and it stops.",
      why="Started from its own Play, the ONE runs at its own tempo. Not following Live: the header must show USB; Sync on OXI ONE MKII (Port 1)'s output only; hold Shift + Back for 2 s for the ONE's MIDI monitor: the clock arrives while Live plays. Still not: Sync on Port 2's output instead, never both.", wait=25),
    S("LAPTOP", "Check the bus",
      ["Open **SF CONTROL**'s window: track **00 CONTROL**, the wrench on the device.",
       "**BUS SHARED** shows green: every SF plugin hears the others.", "**KICK REPORTING** turns green once the kick plays: next lesson."],
      "Last check. Open SF Control's window: click the control track, then the wrench icon on the device. Bus Shared shows in "
      "green, which means every SF plugin in the set can hear the others. Kick Reporting turns green once the kick plays, "
      "in the next lesson.", wait=6),
    S("LAPTOP", "Save",
      ["**Cmd-S**."],
      "Save the set: Command S. That's the rig set up. From here on you only play it.",
      done="Live plays and the ONE follows its tempo; the E16 changes pages; every instrument track listens to its own channel.", wait=3),
], ["Live at 48 kHz, buffer 256, Takeover: Value Scaling", "FIRST SET saved from SF SHOW", "MIDI From checked on the instrument tracks",
    "The interface's knob: the PA's volume and its stop", "SF SHOW on the E16, Auto Transmit off", "The ONE starts, stops and changes tempo with Live"],
    "The rig is set. Next, the first sound.")

# ====================================================================================== 2
L2 = L(2, "THE KICK", "An empty ONE project, a kick on every beat, its sound on the E16. You store song 01.", "20 MIN", [
    S("START", "One sound at a time",
      ["An empty project on the ONE, a kick on every beat, and its sound from the E16. At the end you store **SONG 01**."],
      "This is where the first song starts. You'll make an empty project on the One, put a kick on every beat, shape its sound "
      "on the E16, and store it as song one. The idea behind the whole rig: the notes stay very simple, and the knobs do the "
      "moving. So the first song starts with the simplest pattern there is. One kick, four times a bar.",
      why="Simple notes, all the variation on knobs: the suite is built for that, and so is this course.", wait=3),
    S("LAPTOP", "Tempo 120",
      ["Live's tempo: **120**. The ONE follows it."],
      "In Live, set the tempo to one hundred and twenty. The One follows Live, so that's the only tempo you ever set.", wait=4),
    S("ONE", "An empty project",
      ["**Load**, turn **Knob 1** to an empty project slot, press **Knob 1** twice.", "Remember the slot: song 01 lives there."],
      "On the One, press Load, turn knob one to an empty project slot, and press knob one twice. That's an empty project. "
      "Remember the slot number: song one lives there. Every song in this course is one project on the One.",
      why="One ONE project per song. Its patterns will be the song's sections.", wait=12),
    S("ONE", "Sequencer 1 is the drums",
      ["**Shift + Seq 1**: its settings.", "**Knob 4**: Mode **MULTITRACK**, then press Knob 4 to confirm. **Knob 2**: Out **USB**."],
      "Hold Shift and press sequencer button one to open its settings. Turn knob four until the mode reads Multitrack, and press "
      "knob four to confirm it. Turn knob two, the output, to USB. One rule for every setting on the One: a value showing a star "
      "isn't set until you press its knob. "
      "In multitrack mode each of the eight rows is its own drum lane, lane one at the bottom.",
      one=one(KIT, [], foot="8 LANES · LANE 1 IS THE BOTTOM ROW"), wait=12),
    S("ONE", "Lane 1 plays the kick",
      ["Lane **1**, the bottom row, is selected when its first pad is yellow (tap it if not).", "Tap **Track**: **Knob 1** to **1A** (press it if a * shows)."],
      "Lane one, the bottom row, is selected when its first pad is yellow. In a new project it usually is; if not, tap that pad. Tap the Track button and turn "
      "knob one to one A: channel one on port A. Press it if a star shows. That's the channel track zero one, the kick, is "
      "listening to.",
      one=one(KIT, [(1, "1 KICK", [])], right="CH 1A"), wait=12),
    S("ONE", "Four kicks",
      ["On the bottom row, tap until steps **1, 5, 9, 13** are lit, and nothing else."],
      "Now the notes. Lane one is selected, so every tap on its row places or removes a step. Tap until steps one, five, nine "
      "and thirteen are lit, and nothing else. Four kicks: one on every beat.",
      one=one(KIT, [L1_KICK], right="PATTERN 1"), wait=8),
    S("LISTEN", "Press play",
      ["Space bar in Live. The ONE starts, the kick plays.", "Silent? **01 KICK**'s MIDI From: port A, ch 1. And lane 1's channel: **1A**."],
      "Press the space bar in Live. The One starts with it, and the kick plays on every beat. If it's silent, check track zero "
      "one's MIDI From, port A, channel one, and lane one's channel on the One.",
      listen="One kick on every beat.", wait=8),
    S("LAPTOP", "The kick reports",
      ["**SF CONTROL**: **KICK REPORTING** is green now."],
      "Look at SF Control's window: Kick Reporting is green. From now on every SF plugin knows when the kick hits, and that's "
      "what Pocket ducks around.",
      why="Every SF plugin now knows when the kick hits: the bass and the rumble will duck around it.", wait=4),
    S("LAPTOP", "A starting kick",
      ["Open **SF KICK**'s window (the wrench on 01 KICK).", f"**PRESET › {P('KICK', 'DEEP/ROUND 909')}**.", "Then double-click **TUNE**: back to 0."],
      "Open SF Kick's window on track zero one. At the top, click Preset, then Deep, then Round nine-oh-nine. Then double-click "
      "the Tune knob to put it back to zero, so the song's key will land the kick right on its note. "
      "A preset is only a starting point. You shape it by hand next, and the song stores what you end up with.",
      path=["01 KICK", "SF KICK", "PRESET", P("KICK", "DEEP/ROUND 909")], wait=8),
    S("E16", "The kick page",
      [f"On the E16, {shift_page('KICK')}.", "Rows 3 and 4 are now SF KICK. Rows 1 and 2 never change."],
      "On the E16, hold Shift and tap knob two: the kick page. The bottom two rows are now SF Kick. The top two rows are the same "
      "on every page. Leave Tune at zero: in lesson five, the song's key tunes the kick for you.",
      e16=e16("KICK", K("TUNE"), K("DECAY"), K("PUNCH"), K("CLICK")), wait=6),
    S("E16", "DECAY: the kick's length",
      [f"Knob **{knob('KICK', 'DECAY')}**. Left: short and tight. Right: long, a boom that fills the bar."],
      f"Turn knob {knob('KICK', 'DECAY')}, Decay. To the left, the kick gets short and tight. To the right, long, a boom that "
      "fills the bar. Settle where the tail just ends before the next kick.",
      e16=e16("KICK", K("DECAY", "turn")),
      listen="Settle where the tail just ends before the next kick.",
      why="DECAY is the kick's gate. RUMBLE and the bass follow it on their own.", wait=15),
    S("E16", "PUNCH and CLICK",
      [f"**PUNCH** (knob {knob('KICK', 'PUNCH')}): how hard the pitch drops.", f"**CLICK** (knob {knob('KICK', 'CLICK')}): the tick on top."],
      f"Knob {knob('KICK', 'PUNCH')}, Punch, is how hard the pitch drops at the start. Knob {knob('KICK', 'CLICK')}, Click, "
      "is the tick on top. Less click wears better over a long set. More cuts through a loud room.",
      e16=e16("KICK", K("PUNCH", "turn"), K("CLICK", "turn")),
      listen="Less click wears better over a long set; more cuts through a loud room.", wait=15),
    S("E16", "FILTER and DENSITY",
      [f"**FILTER** (knob {knob('KICK', 'FILTER')}): centre open, left low-pass, right high-pass. Back to centre.",
       f"**DENSITY** (knob {knob('KICK', 'DENSITY')}): ghost kicks between the beats. Up slowly, then back to 0."],
      f"Two more. Knob {knob('KICK', 'FILTER')} is Filter: open in the centre, low-pass to the left, high-pass to the right. "
      f"Bring it back to the centre. Knob {knob('KICK', 'DENSITY')} is Density: ghost kicks between the beats. Bring it up "
      "slowly, listen, then back to zero.",
      e16=e16("KICK", K("DENSITY", "turn"), K("FILTER", "turn")),
      listen="The pattern on the ONE never changed. Every difference came from a knob.", wait=20),
    S("PLAY", "One knob, thirty-two bars",
      ["Start the timer. Take **DECAY** from short to long over 16 bars, and back over 16.", "Slower than feels natural."],
      "Now play. Start the timer on screen: that's thirty-two bars at one hundred and twenty. Take Decay from short to long over "
      "sixteen bars, and back over sixteen. Go slower than feels natural. If you're playing along, pause me while you play.",
      e16=e16("KICK", K("DECAY", "turn")), timer=64,
      why="Long, slow moves are the whole style. Your hands learn the speed here.", wait=6),
    S("LAPTOP", "Store song 01",
      ["**SF CONTROL**: SONG shows **SONG 01**. Press **STORE ALL**."],
      "Back to SF Control. Song shows song one. Press Store All. A song in this rig is every SF plugin's settings, stored "
      "together and recalled together, on the bar. You'll store at the end of every lesson.",
      path=STORE,
      why="A song is every SF plugin's settings, stored together and recalled on the bar.", wait=5),
    S("ONE", "Save the pattern and the project",
      ["Press **Seq 1**, then **Save**: the grid shows every sequencer's slots, sequencer 1 on the top row.",
       "Double-tap slot **1** on the top row: **PATTERN SAVED**. Then **Shift + Save**: the project."],
      "On the One, save twice. Press sequencer one, then Save: the grid now shows every sequencer's pattern slots, sequencer one "
      "on the top row. Double-tap slot one on the top row, and the screen says pattern saved. Then hold Shift and press Save for "
      "the whole project. The One drops unsaved changes to a pattern when another one loads, so save whenever you've changed one.",
      one=slots(SAVEV, [(1, [1])], right="SAVE"),
      why="Unsaved pattern edits are lost when another pattern loads.", wait=12),
    S("LAPTOP", "Save the set",
      ["**Cmd-S**."],
      "And save the set in Live: Command S.",
      done="A kick on every beat from the ONE, shaped on the E16, stored as SONG 01; the ONE project and the set saved.", wait=3),
], ["A kick on every beat from lane 1", "Its sound shaped on the KICK page", "KICK REPORTING green", "SONG 01 stored, the project and the set saved"],
    "One sound, shaped by hand and stored. Next, the bed under it.")

# ====================================================================================== 3
L3 = L(3, "THE LOW END", "RUMBLE under the kick, and kick and rumble as one low end. No new notes.", "15 MIN", [
    S("START", "The bed under the kick",
      ["**SF RUMBLE** plays no notes. It listens to SF KICK and builds a dark bed after every hit.",
       "This lesson makes kick and rumble one low end, and puts your left hand on rows 1 and 2."],
      "The rumble is the bed under the kick, the low, dark roll that hypnotic techno sits on. It plays no notes: it listens to "
      "SF Kick and builds the bed after every hit. In this lesson, kick and rumble become one low end, and your left hand finds "
      "the top two rows of the E16.", wait=3),
    S("LISTEN", "It's already there",
      ["Press play. Track **12 RUMBLE** is already listening: a low bed after each kick."],
      "Press play. Track twelve, Rumble, is already listening to the kick: you'll hear a low bed after each hit. "
      "Nothing to program.",
      listen="A low, dark tail after every kick.", wait=8),
    S("LAPTOP", "A starting bed",
      ["Open **SF RUMBLE**'s window on 12 RUMBLE.", f"**PRESET › {P('RUMBLE', 'REVERB/DEEP')}**."],
      "Open SF Rumble's window on track twelve. Preset, then Reverb, then Deep.",
      path=["12 RUMBLE", "SF RUMBLE", "PRESET", P("RUMBLE", "REVERB/DEEP")], wait=6),
    S("E16", "SIZE and DARK",
      [f"On the E16, {shift_page('RUMBLE')}.", f"**SIZE** (knob {knob('RUMBLE', 'SIZE')}) and **DARK** (knob {knob('RUMBLE', 'DARK')}) set the room."],
      f"On the E16, hold Shift and tap knob {pg('RUMBLE')}: the rumble page. Knob {knob('RUMBLE', 'SIZE')}, Size, and knob "
      f"{knob('RUMBLE', 'DARK')}, Dark, set the room the bed lives in. Darker sits under everything. Brighter will fight the bass later.",
      e16=e16("RUMBLE", K("SIZE", "turn"), K("DARK", "turn")),
      listen="Darker sits under everything. Brighter starts to fight the bass you'll add later.", wait=15),
    S("E16", "PUMP: the breathing",
      [f"**PUMP** (knob {knob('RUMBLE', 'PUMP')}): how far the bed ducks under each kick."],
      f"Knob {knob('RUMBLE', 'PUMP')}, Pump. It sets how far the bed ducks under each kick. Turn it up and the bed swells "
      "between the kicks. That swell is the pulse of the whole style.",
      e16=e16("RUMBLE", K("PUMP", "turn")),
      listen="Up: the bed swells between the kicks. That swell is the pulse.", wait=12),
    S("E16", "GATE and GRIT",
      [f"**GATE** (knob {knob('RUMBLE', 'GATE')}): below 100 % cuts the tail before the next kick.",
       f"**GRIT** (knob {knob('RUMBLE', 'GRIT')}): dirt on the bed. A little goes a long way."],
      f"Knob {knob('RUMBLE', 'GATE')}, Gate: below full, it cuts the tail before the next kick, tight and rhythmic. "
      f"Knob {knob('RUMBLE', 'GRIT')}, Grit, puts dirt on the bed. A little goes a long way.",
      e16=e16("RUMBLE", K("GRIT", "turn"), K("GATE", "turn")), wait=15),
    S("E16", "The kick drives the bed",
      [f"Back to the kick: {shift_page('KICK')}. Turn **DECAY**.", "The rumble follows the kick's length and pitch."],
      "Go back to the kick page, Shift and two, and turn Decay. The rumble follows the kick's length and its pitch, without you "
      "touching it.",
      e16=e16("KICK", K("DECAY", "turn")),
      listen="The bed changes with the kick, untouched.", wait=12),
    S("E16", "LOW ENERGY",
      ["Knob **1**: centre is neutral. Left takes energy out: low-pass, space, softness. Right puts it in: a high-pass edge, drive, echo, chop.",
       "Try both sides, then back to centre."],
      "Now the top row. Knob one is Low Energy. Centre is neutral. Left takes energy out: low-pass, space, softness. "
      "Right puts it in: a high-pass edge, drive, echo and chop. Try both sides, then bring it back to the centre. "
      "The low group is the rumble and, later, the bass. The kick sits outside it, so it holds the floor while the rest moves.",
      e16=e16("KICK", K("LOW ENERGY", "turn"), top_only=True), wait=15),
    S("E16", "The balance",
      ["**KICK VOL** (5) and **LOW VOL** (6): kick on top, the bed just under it."],
      "Knobs five and six are the kick's volume and the low group's. Set the balance: the kick on top, the bed just under it.",
      e16=e16("KICK", K("KICK VOLUME", "turn"), K("LOW VOLUME", "turn"), top_only=True),
      listen="The kick on top, the bed just under it.", wait=12),
    S("PLAY", "Breathe for two minutes",
      ["Only knobs **1**, **5** and **6**. Take the low end down to almost nothing and back, over two minutes."],
      "Play. Start the timer. Only knobs one, five and six, for two minutes: take the low end down to almost nothing and back "
      "up again. Slowly.",
      e16=e16("KICK", K("LOW ENERGY", "turn"), K("KICK VOLUME", "turn"), K("LOW VOLUME", "turn"), top_only=True), timer=120,
      why="Rows 1 and 2 are the arrangement. You'll play most of a set from them.", wait=6),
    S("LAPTOP", "Store",
      ["**STORE ALL** on SF CONTROL (SONG 01). Then **Cmd-S**."],
      "Store: Store All on SF Control, still song one. Then save the set.",
      path=STORE, done="Kick and rumble move as one low end; knobs 1, 5 and 6 ride it; SONG 01 stored.", wait=4),
], ["RUMBLE shaped: SIZE, DARK, PUMP, GATE, GRIT", "The kick's DECAY moves the bed", "LOW ENERGY both ways", "SONG 01 stored again"],
    "The floor is down. Next, the drums.")

# ====================================================================================== 4
L4 = L(4, "THE DRUMS", "Hats and clap on the ONE's drum lanes. DENSITY turns eighths into sixteenths without touching the pattern.", "20 MIN", [
    S("START", "Hats and a clap",
      ["Three more drum lanes on the ONE, three SF PERC tracks in Live.", "Then the move this rig is built on: more notes from a knob, not from the pattern."],
      "Three more drum lanes on the One, three SF Perc tracks in Live. Then the move this whole rig is built on: more notes from "
      "a knob, not from the pattern.", wait=3),
    S("ONE", "Lane 2: closed hat",
      ["Tap the first pad of row **2**: lane 2 lights yellow. **Track**: **Knob 1** to **2A**.",
       "Tap until every other step is lit: **1, 3, 5, 7, 9, 11, 13, 15**."],
      "On the One, tap the first pad of the second row: lane two lights yellow. Tap Track and turn knob one to two A. "
      "Then tap every other step on that row: one, three, five, and so on up to fifteen. Eighth notes. The first tap only "
      "selected the lane, so tap step one once more if it isn't lit.",
      one=one(KIT, [L1_KICK, L2_CLOSED], right="CH 2A"),
      why="The first tap on a new row selects its lane; after that, taps place steps.", wait=18),
    S("LAPTOP", "Closed hat sound",
      [f"**21 CLOSED** › SF PERC › **PRESET › {P('PERC', 'HATS/909 CLOSED')}**."],
      "In Live, open SF Perc on track twenty-one, closed. Preset, Hats, nine-oh-nine Closed.",
      path=["21 CLOSED", "SF PERC", "PRESET", P("PERC", "HATS/909 CLOSED")], wait=6),
    S("ONE", "Lane 3: open hat",
      ["Row **3**: tap its first pad to select it. **Track**: **3A**.", "Tap the offbeats: **3, 7, 11, 15**."],
      "Row three: tap its first pad to select it, then Track, and knob one to three A. Tap the offbeats: steps three, seven, "
      "eleven and fifteen.",
      one=one(KIT, [L1_KICK, L2_CLOSED, L3_OPEN], right="CH 3A"), wait=15),
    S("LAPTOP", "Open hat sound",
      [f"**22 OPEN** › SF PERC › **PRESET › {P('PERC', 'HATS/909 OPEN')}**."],
      "Track twenty-two, open. Preset, Hats, nine-oh-nine Open.",
      path=["22 OPEN", "SF PERC", "PRESET", P("PERC", "HATS/909 OPEN")], wait=6),
    S("ONE", "Lane 4: clap",
      ["Row **4**: tap its first pad to select it. **Track**: **4A**.", "Tap **5** and **13**: beats two and four."],
      "Row four: select it with its first pad, then Track, and knob one to four A. Tap steps five and thirteen: beats two and four.",
      one=one(KIT, [L1_KICK, L2_CLOSED, L3_OPEN, L4_CLAP], right="CH 4A"), wait=15),
    S("LAPTOP", "Clap sound",
      [f"**23 CLAP** › SF PERC › **PRESET › {P('PERC', 'CLAP/LONG ROOM')}**.", "On the ONE: **Seq 1**, **Save**, double-tap slot **1** on the top row; **Shift + Save**."],
      "Track twenty-three, clap. Preset, Clap, Long Room. Then save on the One: sequencer one, Save, double-tap slot one on the "
      "top row, and Shift and Save.",
      path=["23 CLAP", "SF PERC", "PRESET", P("PERC", "CLAP/LONG ROOM")], wait=8),
    S("LISTEN", "The whole kit",
      ["Kick, closed eighths, open offbeats, clap on two and four.", "**DRUM VOL** (knob 7) against the kick."],
      "Listen to the whole kit: kick, closed hats on the eighths, open hats on the offbeats, a clap on two and four. "
      "Use knob seven, the drums' volume, to sit them against the kick.",
      e16=e16("KICK", K("DRUMS VOLUME", "turn"), top_only=True), wait=12),
    S("E16", "Eighths into sixteenths",
      [f"{Shift_page('HATS')}: the hats page.",
       f"**CL DENS** (knob {knob('HATS', 'CLOSED DENSITY')}): up to 50 %, an echo a sixteenth later fades in. Eighths become sixteenths. Past 50 %: rolls."],
      f"Hold Shift and tap knob {pg('HATS')}: the hats page. Knob {knob('HATS', 'CLOSED DENSITY')} is the closed hat's Density. "
      "Bring it up slowly. Up to halfway, an echo one sixteenth later fades in, and the eighths become sixteenths. Past halfway, "
      "they start to roll. The One is still playing eighths. The density is the knob.",
      e16=e16("HATS", K("CLOSED DENSITY", "right")),
      listen="The ONE still plays eighths. The density is the knob.", wait=20),
    S("E16", "Closed into open",
      [f"**CL DECAY** (knob {knob('HATS', 'CLOSED DECAY')}): the closed hat opens.", f"**CL TONE** (knob {knob('HATS', 'CLOSED TONE')}): brighter or darker."],
      f"Knob {knob('HATS', 'CLOSED DECAY')}, the closed hat's Decay, opens it up. Knob {knob('HATS', 'CLOSED TONE')}, Tone, "
      "makes it brighter or darker.",
      e16=e16("HATS", K("CLOSED DECAY", "turn"), K("CLOSED TONE", "turn")), wait=15),
    S("E16", "Open hat and clap",
      [f"**OP DENS**, **OP DECAY** (knobs {knob('HATS', 'OPEN DENSITY')}, {knob('HATS', 'OPEN DECAY')}); **CLP DENS**, **CLP DCY** (knobs {knob('HATS', 'CLAP DENSITY')}, {knob('HATS', 'CLAP DECAY')}).",
       "Try each, then bring them back."],
      f"The open hat and the clap have the same two moves: density on knobs {knob('HATS', 'OPEN DENSITY')} and "
      f"{knob('HATS', 'CLAP DENSITY')}, decay on knobs {knob('HATS', 'OPEN DECAY')} and {knob('HATS', 'CLAP DECAY')}. "
      "Try each one, then bring them back.",
      e16=e16("HATS", K("OPEN DENSITY", "turn"), K("OPEN DECAY", "turn"), K("CLAP DENSITY", "turn"), K("CLAP DECAY", "turn")), wait=20),
    S("E16", "DRUMS ENERGY",
      ["Knob **2**: the whole drum group. Left: softer, further away. Right: harder, choppier."],
      "Knob two in the top row is Drums Energy, the whole drum group at once. Left, softer and further away. Right, harder and "
      "choppier. The kick isn't in the group: it holds the floor.",
      e16=e16("HATS", K("DRUMS ENERGY", "turn"), top_only=True), wait=12),
    S("PLAY", "One move per sixteen bars",
      ["Five minutes. Every 16 bars, move **one** knob a little: CL DENS, CL DECAY, DRUMS ENERGY, LOW ENERGY.", "Never two at once."],
      "Play for five minutes. Every sixteen bars, move one knob a little: the closed density, the closed decay, drums energy, "
      "low energy. Never two at once. If you're playing along, pause me while you play.",
      e16=e16("HATS", K("CLOSED DENSITY", "turn"), K("CLOSED DECAY", "turn"), K("DRUMS ENERGY", "turn"), K("LOW ENERGY", "turn")), timer=300,
      why="One move at a time is what makes a long set hypnotic instead of busy.", wait=6),
    S("LAPTOP", "Store",
      ["**STORE ALL** (SONG 01). **Shift + Save** on the ONE. **Cmd-S**."],
      "Store All on SF Control. Shift and Save on the One, and save the set in Live.",
      path=STORE, done="A full kit from four lanes; DENSITY turns eighths into sixteenths; SONG 01 stored.", wait=4),
], ["Lanes 2, 3 and 4: closed, open, clap", "DENSITY doubles the hats without a new note", "DRUMS ENERGY both ways", "SONG 01 stored, project and set saved"],
    "The groove is done. Next, the music: one bass note and one chord.")

# ====================================================================================== 5
L5 = L(5, "THE MUSIC", "One bass note, one chord stab, the song's key and the dub delay. Song 01 is complete.", "25 MIN", [
    S("START", "One note can carry a track",
      ["A bass that plays one note, a stab that plays one chord, and the key the song is in.", "Everything else is knobs."],
      "Now the music, and it's very little. A bass that plays one note, a stab that plays one chord, and the key the song is in. "
      "Everything else is knobs.", wait=3),
    S("ONE", "Sequencer 2: the bass",
      ["**Shift + Seq 2**: **Knob 1** Ch **1B**, **Knob 2** Out **USB**, **Knob 4** Mode **MONO**. Press each knob that shows a *."],
      "On the One, hold Shift and press sequencer two. Knob one, the channel: one B. Knob two, the output: USB. Knob four, the "
      "mode: Mono. Press each knob that shows a star. That's track eleven, the bass.",
      one=one(BASS, [], right="CH 1B"), wait=15),
    S("ONE", "One note, on the offbeats",
      ["Press **Seq 2**. Bottom row, **C**: tap the offbeats **3, 7, 11, 15**. (Another page showing? Tap the **16/80** button.)",
       "**Knob 2** moves the octave. Turn it until the bass sits with the kick: deep, but still a note."],
      "Press sequencer two, so the grid shows it; if it shows a later page, tap the sixteen button. The bottom row is the note C. Tap the offbeats: steps three, seven, eleven, "
      "fifteen. Then turn knob two, the octave, until the bass sits with the kick: deep, but still a note you can hear.",
      one=one(BASS, [(1, "C", OFFBEATS)], right="PATTERN 1"),
      listen="Deep, but you can still hear its pitch.", wait=15),
    S("LAPTOP", "The bass sound",
      [f"**11 BASS** › SF BASS › **PRESET › {P('BASS', 'OFFBEAT/DUB')}**."],
      "Track eleven, SF Bass. Preset, Offbeat, Dub: the rubbery Berlin offbeat bass.",
      path=["11 BASS", "SF BASS", "PRESET", P("BASS", "OFFBEAT/DUB")], wait=6),
    S("LAPTOP", "The song's key: A",
      [f"**SF CONTROL › KEY › {choice('CONTROL', 'key', 'A')}**.", "The ONE plays C. Every SF instrument now plays it in A, and KICK and RUMBLE tune themselves to A.", "The bass moved down 3 semitones: check its octave again (ONE, **Knob 2**)."],
      "Open SF Control and set Key to A. The One still plays C, but every SF instrument now plays it in A, and the kick and the "
      "rumble tune themselves to A. Write everything in C; the key is a setting of the song. The bass just moved down three "
      "semitones, so check its octave again with knob two: deep, but still a note.",
      path=["00 CONTROL", "SF CONTROL", "KEY", "A"],
      listen="The kick and the bass land on the same note, about 55 Hz.",
      why="Write everything in C. The key belongs to the song, so the next song can use the same patterns in another key.", wait=10),
    S("E16", "The bass page",
      [f"{Shift_page('BASS')}.", "**CUTOFF**, **RES**, **ENV**, **GATE** (knobs 9-12): one note, four knobs. That's the performance."],
      f"On the E16, Shift and {pg('BASS')}: the bass page. Cutoff, resonance, envelope and gate, knobs nine to twelve. One note "
      "and four knobs: that's the whole performance. Open the cutoff, then the envelope, then shorten the gate.",
      e16=e16("BASS", K("CUTOFF", "turn"), K("RES", "turn"), K("ENV AMT", "turn"), K("GATE", "turn")),
      listen="CUTOFF and ENV open it up; GATE makes every note shorter or longer.", wait=20),
    S("E16", "Bass DENSITY",
      [f"**DENSITY** (knob {knob('BASS', 'DENSITY')}): ghost notes fill the gaps. The offbeats start to roll."],
      f"Knob {knob('BASS', 'DENSITY')}, Density. Ghost notes fill the gaps, and the offbeats start to roll. Find the point where "
      "it rolls without crowding the kick, then back off a little.",
      e16=e16("BASS", K("DENSITY", "right")), wait=15),
    S("ONE", "Sequencer 4: the stab",
      ["**Shift + Seq 4**: Ch **3B**, Out **USB**, Mode **MONO**.", "Press **Seq 4**, tap one **C** on step **7**. **Knob 2** up until the chord sits in the middle."],
      "Hold Shift and press sequencer four: channel three B, output USB, mode Mono, pressing any knob that shows a star. Then press sequencer four "
      "and tap one C on step seven. One note, once a bar. Turn knob two up until the chord sits in the middle, clear, not muddy.",
      one=one(STAB, [(1, "C", [7])], right="CH 3B"), wait=18),
    S("LAPTOP", "The stab sound",
      [f"**32 STAB** › SF LEAD › **PRESET › {P('LEAD', 'STAB/DUB CHORD')}**.", "One note in, a minor seventh chord out.",
       "On the ONE: **Seq 2**, **Save**, double-tap slot **1** on its row. **Seq 4**, **Save**, double-tap slot **1** on its row. **Shift + Save**."],
      "Track thirty-two, the stab. Preset, Stab, Dub Chord. One note goes in, and the plugin plays a minor seventh chord. "
      "Then save on the One: press sequencer two, Save, double-tap slot one on its row; the same for sequencer four; then Shift and Save.",
      path=["32 STAB", "SF LEAD", "PRESET", P("LEAD", "STAB/DUB CHORD")], wait=8),
    S("LAPTOP", "The dub delay",
      [f"Return **A DUB** › SF DELAY › **PRESET › {P('DELAY', 'DUB/1/8D')}**."],
      "Now the return track A, Dub. Open SF Delay and choose Preset, Dub, dotted eighth.",
      path=["A DUB", "SF DELAY", "PRESET", P("DELAY", "DUB/1/8D")], wait=6),
    S("E16", "Send the stab to the dub",
      [f"{Shift_page('PERFORM')}: PERFORM.", f"**STAB>DUB** (knob {knob('PERFORM', 'STAB SEND A')}): how much stab goes in. **DUB RET** (knob {knob('PERFORM', 'DUB RETURN')}): how loud the repeats are."],
      f"On the E16, Shift and one: the Perform page. Knob {knob('PERFORM', 'STAB SEND A')} sends the stab into the dub delay. "
      f"Knob {knob('PERFORM', 'DUB RETURN')} is the delay's return, how loud the repeats are. Bring both up until the stab "
      "trails off behind itself.",
      e16=e16("PERFORM", K("STAB SEND A", "right"), K("DUB RETURN", "right")),
      listen="The stab, then its echoes walking away a dotted eighth apart.", wait=15),
    S("E16", "A throw",
      [f"{Shift_page('SPACE')}: SPACE.", f"Hold knob **{knob('SPACE', 'DUB TIME')}** just before a stab, let go after it: **DUB THROW**."],
      f"Shift and {pg('SPACE')}: the Space page. Press and hold knob {knob('SPACE', 'DUB TIME')} just before a stab, and let go "
      "after it. That's a throw: the stab goes into the delay hard, and its repeats ring on after you let go.",
      e16=e16("SPACE", K("DUB TIME", "hold", name="THROW"), foot="PRESS 9: DUB THROW, ON WHILE HELD"),
      listen="The stab thrown into the delay, the repeats ringing on after you let go.", wait=15),
    S("E16", "MUSIC ENERGY",
      ["Knob **3**: the stab, and later the other music parts. **MUS VOL** (8): their level. The bass is in LOW."],
      "Knob three is Music Energy: the stab, and later the acid, the texture and the voice. Knob eight is the music group's "
      "volume. The bass belongs to the low group, with the rumble.",
      e16=e16("SPACE", K("MUSIC ENERGY", "turn"), K("MUSIC VOLUME", "turn"), top_only=True), wait=12),
    S("PLAY", "Ten minutes, rows 1 and 2",
      ["Song 01 is complete. Ten minutes with **only the top two rows**.", "From the kick and the low end alone to everything and back."],
      "Song one is complete. Play it for ten minutes with only the top two rows: from the kick and the low end alone, to everything, and "
      "back again. If you're playing along, pause me while you play.",
      e16=e16("PERFORM", K("LOW ENERGY", "turn"), K("DRUMS ENERGY", "turn"), K("MUSIC ENERGY", "turn"), K("KICK VOLUME", "turn"),
              K("LOW VOLUME", "turn"), K("DRUMS VOLUME", "turn"), K("MUSIC VOLUME", "turn"), top_only=True), timer=600,
      why="Most of a set is played from rows 1 and 2. The pages are for detail.", wait=6),
    S("LAPTOP", "Store",
      ["**STORE ALL** (SONG 01). **Shift + Save** on the ONE. **Cmd-S**."],
      "Store All, Shift and Save on the One, and save the set.",
      path=STORE, done="Song 01 is whole: kick, rumble, kit, bass, stab, dub, in A.", wait=4),
], ["Bass: one C on the offbeats, played in A", "Stab: one C, a minor seventh chord, into the dub", "A throw on the SPACE page", "SONG 01 stored, project and set saved"],
    "Song 01 is whole. Next, its four sections and the call that brings it back.")

# ====================================================================================== 6
L6 = L(6, "THE SECTIONS", "SPARSE, MAIN, VARIATION, EXIT on the ONE, and the Program Change that calls the song.", "25 MIN", [
    S("START", "Four sections, one song",
      ["Four patterns per sequencer, one per section. Small differences: the knobs still do most of the arranging."],
      "A song on this rig has four sections, one pattern each on the One: sparse, main, variation and exit. The differences are "
      "small, because the knobs still do most of the arranging. Right now pattern one of each sequencer holds everything you've "
      "built, which is the main section. You'll copy it into slots two, three and four, then thin slot one down into sparse.",
      table=[["1 SPARSE", "kick, closed hats, bass"], ["2 MAIN", "everything: what you have now"],
             ["3 VARIATION", "MAIN with one change: a second stab"], ["4 EXIT", "kick, hats, clap, stab; no bass"]],
      why="The patterns change what's there. The knobs change how it sounds.", wait=4),
    S("ONE", "Copy slot 1 into 2, 3 and 4",
      ["**Save**: the grid shows every sequencer's slots, sequencer 1 on the top row.",
       "Hold **Copy** + hold slot **1**, then hold **Paste** + hold slot **2**. Again for **3** and **4**.",
       "On the rows of sequencers **1**, **2** and **4**."],
      "On the One, press Save. The grid now shows every sequencer's pattern slots: sequencer one on the top row, slots one to "
      "sixteen across. On sequencer one's row, hold Copy and hold slot one, then hold Paste and hold slot two. Do it again for "
      "slots three and four. Then the same on the rows of sequencers two and four.",
      one=slots(SAVEV, [(1, [1, 2, 3, 4]), (2, [1, 2, 3, 4]), (4, [1, 2, 3, 4])], right="SAVE"), wait=30),
    S("ONE", "Slot 1 becomes SPARSE",
      ["**Load**, then **Shift + tap** column **1**: every sequencer on slot 1.",
       "**Seq 1**: tap row 3's first pad to select it, then tap its steps off. Same for row 4.", "**Seq 4**: tap off the stab."],
      "Press Load, then hold Shift and tap column one: every sequencer loads slot one. Press sequencer one. Tap the first pad of "
      "row three to select the open hat lane, then tap its steps off. Do the same on row four, the claps. The first tap on a "
      "lane only selects it. Then press sequencer four and tap off the stab. That's sparse: kick, closed hats, bass.",
      one=one(KIT, [L1_KICK, L2_CLOSED], right="SLOT 1 · SPARSE"), wait=20),
    S("ONE", "Save SPARSE",
      ["**Seq 1**, **Save**, double-tap slot **1** on its row. Then **Seq 4**, **Save**, double-tap slot **1** on its row."],
      "Save it before loading anything else. Press sequencer one, then Save, and double-tap slot one on its row: pattern saved. "
      "Then sequencer four, Save, and double-tap slot one on its row.",
      one=slots(SAVEV, [(1, [1]), (4, [1])], right="SAVE"),
      why="An edited pattern that isn't saved is lost when another one loads.", wait=12),
    S("ONE", "Slot 3: VARIATION",
      ["**Load**, **Shift + tap** column **3**.", "**Seq 4**: add a second stab on step **15**. **Save**, double-tap slot **3** on sequencer 4's row."],
      "Load, then Shift and tap column three. Press sequencer four and add a second stab, on step fifteen. Then save it: Save, "
      "and double-tap slot three on sequencer four's row.",
      one=one(STAB, [(1, "C", [7, 15])], right="SLOT 3 · VARIATION"), wait=18),
    S("ONE", "Slot 4: EXIT",
      ["**Load**, **Shift + tap** column **4**.", "**Seq 2**: tap off the bass. **Save**, double-tap slot **4** on its row."],
      "Load, Shift and tap column four. Press sequencer two and tap off the bass notes. Save, and double-tap slot four on "
      "sequencer two's row. Exit is the drums and the stab, with no bass under them.",
      one=one(BASS, [(1, "C", [])], right="SLOT 4 · EXIT"), wait=15),
    S("ONE", "Sequencer 8 calls the song",
      ["**Load**, **Shift + tap** column **1**.",
       "**Shift + Seq 8**: **Knob 4** Mode **MONO** first, then **Knob 1** Ch **16B**, **Knob 2** Out **USB**, **Knob 3** PC **1**. Press each knob that shows a *. No notes.",
       "**Seq 8**, **Save**, double-tap slot **1** on its row (the bottom row)."],
      "Back to sparse: Load, Shift and tap column one. Now hold Shift and press sequencer eight. Knob four first, the mode: Mono. "
      "Then knob one, the channel: sixteen B. Knob two, the output: USB. Knob three, the Program Change: one. Press each knob "
      "that shows a star. Sequencer eight plays "
      "no notes; it only calls the song. Save it: press sequencer eight, then Save, and double-tap slot one on its row, the bottom one.",
      one=slots(SAVEV, [(8, [1])], right="PC 1 · CH 16B"),
      why="Slot 1 carries the call. A call for the song that's already playing changes nothing, so your live moves stay where they are.", wait=20),
    S("LISTEN", "Test the call",
      ["Live playing. Turn **DECAY** on the KICK page and **CUTOFF** on the BASS page well away from where they were.",
       "**SF CONTROL › SONG › SONG 16**: nothing is stored there, so nothing changes.",
       "**Load**, **Shift + tap** column **1**: **WAIT BAR**, and on the bar the knobs go back. SONG reads **SONG 01**."],
      "Test it, with Live playing. On the E16, turn the kick's Decay and the bass's Cutoff well away from where they were. On SF "
      "Control, set Song to song sixteen: nothing is stored there, so nothing changes. Now Load, Shift and tap column one. SF "
      "Control shows Wait Bar in green, and on the next bar line the knobs go back to song one as you stored it. If Song reads "
      "song two instead, your One counts programs from one: set sequencer eight's Program Change one lower, save it, and try again.",
      listen="On the bar line, the sound goes back to the song as you stored it.",
      why="Volumes (knobs 5-8) are Live's mixer: songs don't store them. If the PC can't go lower than 1, store each song one number up instead (song 01 into SONG 02).", wait=20),
    S("ONE", "Changes land on the bar",
      ["**Shift + Perf**: the project menu. **Global Quantization**: one bar (or the shortest setting in bars)."],
      "One more setting. Hold Shift and press Perf for the project menu, and set Global Quantization to one bar, or the shortest "
      "setting your One offers in bars. Now a pattern you load while playing waits for the bar line.",
      why="Load the next section a bar early; it lands on the bar line.", wait=10),
    S("PLAY", "Run the song",
      ["**Load**, **Shift + tap** a column, a bar early:", "**1** SPARSE 32 bars › **2** MAIN 64 › **3** VARIATION 32 › **2** MAIN 32 › **4** EXIT 32."],
      "Now run the song from top to bottom. Start the timer. Sparse for thirty-two bars, main for sixty-four, variation for "
      "thirty-two, main again for thirty-two, then exit. Load each section a bar early, and keep your left hand on the top row "
      "while you do. If you're playing along, pause me while you play.",
      timer=384,
      why="Sections are the coarse moves; the knobs carry everything in between.", wait=6),
    S("ONE", "Save it opening on SPARSE",
      ["**Load**, **Shift + tap** column **1**, then **Shift + Save**. **Cmd-S** in Live."],
      "Last, load column one again and save the project, so it opens on sparse and calls the song. And save the set.",
      done="Four sections; sequencer 8 calls SONG 01; a section change lands on the bar.", wait=4),
], ["Slots 1-4: SPARSE, MAIN, VARIATION, EXIT", "Sequencer 8, slot 1: PC calls SONG 01", "Global Quantization: one bar", "The song run from top to bottom"],
    "A whole song, sections and all. Next, a second song from the first.")

# ====================================================================================== 7
ACID = "SEQUENCER 3 · MONO"
L7 = L(7, "A SECOND SONG", "Song 02 from song 01: a new key, an acid line, darker colours, and SLOW ALL.", "25 MIN", [
    S("START", "A second song from the first",
      ["Same drums and bass, a new key, an acid line as the featured part, darker colours.",
       "That's how a set grows: one palette; each song a different key, sound and featured part."],
      "Song two grows out of song one. Same drums and bass, a new key, an acid line as the featured part, and darker colours. "
      "That's how a set grows on this rig: one palette, and each song is a different key, a different sound and one featured "
      "part. Twenty minutes, not an evening.", wait=3),
    S("ONE", "Copy the project",
      ["**Save**, **Knob 1** to an empty project slot, press, **Duplic proj here**, **Yes**.", "Then **Load** that slot: **Knob 1** to it, press twice."],
      "On the One, press Save, turn knob one to an empty project slot, press it, choose Duplicate project here, and confirm. "
      "Then load the copy: press Load, turn knob one to it, and press twice. You're working on song two's project now; song "
      "one's is untouched.", wait=15),
    S("ONE", "Sequencer 8 calls song 02",
      ["**Shift + Seq 8**: **Knob 3** PC one higher than song 01's. Press Knob 3.", "**Seq 8**, **Save**, double-tap slot **1** on its row."],
      "Hold Shift and press sequencer eight. Turn knob three, the Program Change, one higher than song one's, and press it. "
      "Save it: sequencer eight, Save, and double-tap slot one on its row.",
      one=slots(SAVEV, [(8, [1])], right="PC +1"), wait=12),
    S("LAPTOP", "Copy the sound into song 02",
      ["SF CONTROL, in its **SETUP** group: **STORE TO › SONG 02**, then **STORE ALL**.", "SONG now reads **SONG 02**. Nothing you hear changed."],
      "Now the sound. On SF Control, open the setup group, set Store To to song two, then press Store All. Song now reads song "
      "two, and nothing you hear has changed: what was playing went into song two, which is now the song in force.",
      path=STORE_TO("SONG 02"),
      why="STORE TO stores what's playing into another song without recalling it. It comes after the project copy, because loading a project can call its song.", wait=8),
    S("LAPTOP", "Key F",
      [f"**SF CONTROL › KEY › {choice('CONTROL', 'key', 'F')}**."],
      "On SF Control, set Key to F. The kick drops to F, around forty-four hertz, with the rumble. The bass and the stab move up "
      "to F: the One's C now plays five semitones higher.",
      path=["00 CONTROL", "SF CONTROL", "KEY", "F"],
      listen="The kick drops to F, about 44 Hz; the bass and the stab move up to F.", wait=8),
    S("ONE", "Sequencer 3: the acid line",
      ["**Shift + Seq 3**: Ch **2B**, Out **USB**, Mode **MONO** (press the knobs that show a *).",
       "Press **Seq 3**. Tap the notes below: **C** on row 1, **D#** on row 4, **G** on row 8. **Knob 2** until it sits above the bass."],
      "Hold Shift and press sequencer three: channel two B, output USB, mode Mono, pressing the knobs that show a star. Press "
      "sequencer three and tap the notes on screen: C on the bottom row, D sharp on the fourth row, G on the top row. Then turn "
      "knob two until the line sits above the bass.",
      one=one(ACID, ACID_LINE, right="CH 2B"), wait=30),
    S("ONE", "The acid in every section",
      ["**Seq 3**, **Save**, double-tap slot **1** on its row.", "Then **Copy** slot 1 › **Paste** into slots **2**, **3**, **4** on the same row."],
      "Save it: sequencer three, Save, and double-tap slot one on its row. Then, still in the save view, copy slot one into slots "
      "two, three and four on the same row: hold Copy and slot one, then hold Paste and the slot.",
      one=slots(SAVEV, [(3, [1, 2, 3, 4])], right="SAVE"), wait=20),
    S("LAPTOP", "The acid sound",
      [f"**31 ACID** › SF LEAD › **PRESET › {P('LEAD', 'ACID/LINE')}**."],
      "Track thirty-one, the acid. Preset, Acid, Line.",
      path=["31 ACID", "SF LEAD", "PRESET", P("LEAD", "ACID/LINE")], wait=6),
    S("E16", "The acid page",
      [f"{Shift_page('ACID')}.", "**CUTOFF** and **ENV** are the acid; **RES** is the squelch. Leave CUTOFF low: this song builds on it."],
      f"On the E16, Shift and {pg('ACID')}: the acid page. Cutoff and envelope are the acid; resonance is the squelch. Try them, "
      "then leave the cutoff low and dark. This song builds on opening it.",
      e16=e16("ACID", K("CUTOFF", "turn"), K("RES", "turn"), K("ENV AMT", "turn")), wait=20),
    S("LAPTOP", "Darker colours",
      [f"**32 STAB**: **PRESET › {P('LEAD', 'STAB/DUB DARK')}**.", f"**11 BASS**: **PRESET › {P('BASS', 'SUB/SQUARE')}**."],
      "Now the colours. On the stab, Preset, Stab, Dub Dark. On the bass, Preset, Sub, Square: round and deep. Same notes, a different song.",
      why="Same notes, a different song: the sound belongs to the song, not the pattern.", wait=10),
    S("E16", "SLOW ALL",
      ["Press knob **1**: its ring turns green. Turn it to **8 BAR**. Press it again: back to LOW ENERGY."],
      "Now the move that makes long builds easy. Press knob one: its ring turns green, and it's now Slow All. Turn it to eight "
      "bars, then press it again to go back to Low Energy. Every move you make now takes eight bars to arrive.",
      e16=e16("PERFORM", SLOW_ALL), wait=10),
    S("E16", "An eight-bar rise",
      [f"{Shift_page('PERFORM')}. **ACID CUT** (knob {knob('PERFORM', 'ACID CUTOFF')}): a quarter turn up, then hands off.", "The ring turns green and walks with the sound."],
      f"Shift and one: Perform. Knob {knob('PERFORM', 'ACID CUTOFF')} is the acid's cutoff, on the page you're on most. Give it a "
      "quarter turn up, and take your hand away. The ring turns green and walks with the sound, for eight bars.",
      e16=e16("PERFORM", K("ACID CUTOFF", "right")),
      listen="The acid opens evenly over eight bars, with your hands off.", wait=20),
    S("PLAY", "Rises and falls",
      ["Quarter turns of **ACID CUT**, up and down. Let each one arrive before the next."],
      "Play for about three minutes. Quarter turns of the acid cutoff, up and down, letting each one arrive before you make the next. "
      "If you're playing along, pause me while you play.",
      e16=e16("PERFORM", K("ACID CUTOFF", "turn")), timer=192,
      why="SLOW ALL turns a quick hand into a long build. It's how a three-minute rise stays even.", wait=6),
    S("E16", "SLOW ALL off",
      ["Press knob **1**, turn to **OFF**, press again."],
      "Press knob one, turn Slow All to off, and press again. Off means your moves land at once.",
      e16=e16("PERFORM", SLOW_ALL), wait=8),
    S("LAPTOP", "Store song 02",
      ["**ACID CUT** back down to where the song should start; wait until no ring is green.", "SONG reads **SONG 02**: **STORE ALL**.",
       "On the ONE: **Load**, **Shift + tap** column **1**, **Shift + Save**. **Cmd-S**."],
      "Before you store: bring the acid cutoff back down to where the song should start, and wait until no ring is green. "
      "Check that Song reads song two, then Store All. On the One, load column one and save the project, so it opens on sparse. "
      "Save the set.",
      path=STORE, wait=10),
    S("LISTEN", "Switch songs",
      ["A song change is two moves: load the song's project (**Load**, **Knob 1** to it, press twice), then **Load**, **Shift + tap** column **1**: the call.",
       "Song 01's project: **WAIT BAR**, then song 01, in A, no acid. Song 02's: F and the acid."],
      "Now switch songs. A song change on this rig is two moves on the One. Load the song's project, then press Load, hold Shift "
      "and tap column one: that's the call, and the first section. Do it for song one: Wait Bar, and on the bar song one is back, "
      "in A, without the acid. Then song two: F, and the acid.",
      listen="Each song lands on the bar.",
      why="Loading a project may already send its call; the column-1 load always does. A second call for the song that's playing changes nothing.",
      done="Two songs, each called by its own project, landing on the bar.", wait=12),
], ["SONG 02 made with STORE TO", "Project copied; sequencer 8 calls song 02", "An acid line in every section, in F", "SLOW ALL: eight-bar moves by hand"],
    "Two songs from one palette. Next, a third: a place and a voice.")

# ====================================================================================== 8
TEX = "SEQUENCER 5 · MONO"
VOX = "SEQUENCER 6 · MONO"
L8 = L(8, "A THIRD SONG", "Song 03 in G: a texture from a field recording and a spoken phrase over the same drums.", "30 MIN", [
    S("START", "A place and a voice",
      ["Song 03, in G: a granular texture from one of your field recordings, and a spoken phrase every four bars.",
       "Have two WAV or AIFF files ready: a recording of a place, and a voice."],
      "Song three is in G, and it brings in your own material: a granular texture made from a field recording, and a spoken "
      "phrase every four bars, over the same drums. Have two files ready: a recording of a place, and a voice. "
      "No files handy? The texture plays its own chord bed, and you can do the voice later.", wait=3),
    S("ONE", "Start from song 01",
      ["Song 01: load its project, then **Load**, **Shift + tap** column **1**. Let it land.", "**Save**, **Knob 1** to an empty slot, press, **Duplic proj here**, **Yes**. **Load** the copy."],
      "Bring song one back: load its project on the One, then Load, Shift and column one, and let it land. Then duplicate the "
      "project, as before: Save, knob one to an empty slot, press, Duplicate project here, Yes. And load the copy.", wait=15),
    S("LAPTOP", "Store into song 03",
      ["SF CONTROL, **SETUP › STORE TO › SONG 03**, then **STORE ALL**.", f"**KEY › {choice('CONTROL', 'key', 'G')}**."],
      "On SF Control, in the setup group, Store To song three, then Store All. Then set Key to G. The kick lands on G, about "
      "forty-nine hertz.",
      path=STORE_TO("SONG 03"),
      listen="The kick on G, about 49 Hz.", wait=10),
    S("ONE", "Sequencer 8 calls song 03",
      ["**Shift + Seq 8**: **Knob 3** PC one higher than song 02's. Press Knob 3.", "**Seq 8**, **Save**, double-tap slot **1** on its row."],
      "Shift and sequencer eight: set the Program Change one higher than song two's and press knob three. Then sequencer eight, "
      "Save, and double-tap slot one on its row.",
      one=slots(SAVEV, [(8, [1])], right="PC +2"), wait=10),
    S("LAPTOP", "The texture",
      [f"**33 TEXTURE** › SF PAD › **PRESET › {P('PAD', 'GRANULAR/CLOUD')}**.", "Then **LOAD** (in its header) and choose your field recording. Or drag it from Live's browser onto the window."],
      "Track thirty-three, the texture. Choose Preset, Granular, Cloud. Then click Load in SF Pad's header and choose your field "
      "recording, or drag it from Live's browser onto the window. Not from the Finder: Live hides plugin windows while the Finder "
      "is in front. The grains come from the recording.",
      path=["33 TEXTURE", "SF PAD", "PRESET", P("PAD", "GRANULAR/CLOUD")], wait=12),
    S("LAPTOP", "Where the grains come from",
      [f"In the **SAMPLE** group: drag **{param('PAD', 'smp_start')}** and **{param('PAD', 'smp_end')}** on the waveform around the part you want."],
      "Open the sample group in SF Pad's window. Drag the start and end markers on the waveform around the part of the "
      "recording you want the grains from: wind, a room, traffic far away.",
      why="Move START and END while a note holds and the grains wander through the recording.", wait=15),
    S("ONE", "Sequencer 5: one long note",
      ["**Shift + Seq 5**: Ch **4B**, Out **USB**, Mode **MONO** (press the knobs that show a *).",
       "Press **Seq 5**. Hold step **1** on the bottom row and tap step **16**: one C, tied across the bar.", "**Knob 2** up until it sits in the middle."],
      "Hold Shift and press sequencer five: channel four B, output USB, mode Mono. Press sequencer five. On the bottom row, hold "
      "step one and tap step sixteen: one C, tied across the whole bar. Knob two moves it up until it sits in the middle.",
      one=one(TEX, [(1, "C", [1], list(range(2, 17)))], right="CH 4B"), wait=20),
    S("ONE", "Save it everywhere",
      ["**Seq 5**, **Save**, double-tap slot **1** on its row; **Copy** › **Paste** it into **2**, **3**, **4**."],
      "Sequencer five, Save, and double-tap slot one on its row. Then copy it into slots two, three and four, as you did for the acid.",
      one=slots(SAVEV, [(5, [1, 2, 3, 4])], right="SAVE"), wait=15),
    S("E16", "The texture page",
      [f"{Shift_page('TEXTURE')}.", f"**COLOR** (knob {knob('TEXTURE', 'COLOR')}): grain size. **DETUNE** (knob {knob('TEXTURE', 'DETUNE')}): scatter. **ATTACK** (knob {knob('TEXTURE', 'ATTACK')})."],
      f"On the E16, Shift and {pg('TEXTURE')}: the texture page. Color is the grain size, Detune the scatter, Attack how slowly "
      "each note swells in. Small grains and wide scatter turn the place into a haze.",
      e16=e16("TEXTURE", K("COLOR", "turn"), K("DETUNE", "turn"), K("ATTACK", "turn")), wait=20),
    S("LAPTOP", "The voice",
      [f"**34 VOICE** › SF VOICE › **PRESET › {P('VOICE', 'RADIO/TRANSMISSION')}**.", "Then **LOAD** your voice recording (or drag it from Live's browser).",
       "A phrase keeps its own pitch, whatever the song's key."],
      "Track thirty-four, the voice. Preset, Radio, Transmission. Then Load your voice recording, or drag it from Live's browser. "
      "A phrase keeps its own pitch whatever key the song is in, so the voice never gets pulled out of shape.",
      path=["34 VOICE", "SF VOICE", "PRESET", P("VOICE", "RADIO/TRANSMISSION")], wait=15),
    S("LAPTOP", "Trim the phrase",
      ["Drag **START** and **END** on the phrase: keep the line you want.", "**FADE IN** and **FADE OUT** a little, so it doesn't click."],
      "Drag the start and end markers on the phrase until only the line you want is left. Give it a little fade in and fade out, "
      "so it doesn't click.", wait=15),
    S("ONE", "The phrase every four bars",
      ["**Shift + Seq 6**: Ch **5B**, Out **USB**, Mode **MONO**. Press each.",
       "Press **Seq 6**. Hold **End**, tap the **64/128** page button: four bars.", "Tap step **1** on the bottom row. **Seq 6**, **Save**, double-tap slot **1**; copy it to **2**, **3**, **4**."],
      "Hold Shift and press sequencer six: channel five B, output USB, mode Mono. Press sequencer six. Hold End and tap the "
      "page button marked sixty-four: the pattern is now sixty-four steps, four bars. Tap step one on the bottom row: a note "
      "below middle C plays the whole phrase. Save slot one, and copy it into two, three and four.",
      one=one(VOX, [(1, "C", [1])], right="64 STEPS"),
      why="Below C3 (Live's naming) VOICE plays the whole phrase; from C3 up, one slice per key.", wait=25),
    S("E16", "Out of the static",
      [f"{Shift_page('VOICE')}. **SIGNAL** (knob {knob('VOICE', 'SIGNAL')}): low is a lost transmission, up is clear.", f"**ECHO** (knob {knob('VOICE', 'ECHO')}): its own dub echo."],
      f"On the E16, Shift and {pg('VOICE')}: the voice page. Knob {knob('VOICE', 'SIGNAL')}, Signal: low, the voice is a lost "
      "transmission; turn it up and it comes in clear. Knob eleven, Echo, sends it into its own long dub echo.",
      e16=e16("VOICE", K("SIGNAL", "turn"), K("ECHO", "turn")),
      listen="Low SIGNAL: a voice somewhere in the static. Up: it's in the room.", wait=20),
    S("PLAY", "Bring the voice in",
      ["Knob **1** › SLOW ALL **16 BAR**.", "**SIGNAL** from low to clear in one move. Then back to low. Then SLOW ALL **OFF**."],
      "Play. Press knob one, set Slow All to sixteen bars, press again. Then turn Signal from low to clear in one move, and let "
      "the voice come in out of the static over sixteen bars. Bring it back down the same way. Then Slow All off. If you're playing along, pause me while you play.",
      e16=e16("VOICE", SLOW_ALL, K("SIGNAL", "turn")), timer=240, wait=6),
    S("LAPTOP", "Store song 03",
      ["**STORE ALL** (SONG 03). On the ONE: **Load**, **Shift + tap** column **1**, **Shift + Save**. **Cmd-S**."],
      "Store All: song three. Load column one on the One and save the project. Save the set.",
      path=STORE, done="Song 03: your place as a texture, your voice out of the static, in G.", wait=4),
], ["SONG 03 from song 01, in G", "A field recording as a granular texture", "A spoken phrase every four bars, KEY FOLLOW off", "SIGNAL brought up with SLOW ALL"],
    "Three songs. Next, play them as a set.")

# ====================================================================================== 9
L9 = L(9, "PLAY THE SET", "Thirty minutes through the three songs, recorded, with song changes on the bar. Listen back tomorrow.", "45 MIN", [
    S("START", "Three songs, one set",
      ["Thirty minutes, songs 01, 02, 03, recorded.",
       ["Patterns change no more often than every 32 bars", "One knob at a time", "A song changes in a build or a breakdown", "SLOW ALL at 16 BAR for the change"]],
      "This is the payoff: thirty minutes through all three songs, recorded. Four rules. Patterns change no more often than every "
      "thirty-two bars. One knob at a time. A song changes in a build or a breakdown, never in the middle of a groove. And Slow All at "
      "sixteen bars for every song change.", wait=3),
    S("ONE", "Start on song 01",
      ["Live stopped. Load song 01's project, then **Load**, **Shift + tap** column **1**.", "SF CONTROL: **SONG 01**, KEY **A**."],
      "With Live stopped, load song one's project on the One, then Load, Shift and column one. Stopped, the call lands at once: "
      "SF Control shows song one, key A.", wait=10),
    S("LAPTOP", "Record",
      ["Arm **93 REC**, then Live's **Arrangement Record**: the set starts.", "93 REC records the set as it leaves the master."],
      "In Live, arm track ninety-three, Rec, and press Arrangement Record: the transport starts and the One with it. "
      "Ninety-three records the whole set as it leaves the master, without the room correction.", wait=8),
    S("E16", "A song change, step by step",
      ["1. Knob **1**: SLOW ALL **16 BAR**, press back.", "2. Load the next song's project, then **Load**, **Shift + tap** column **1**, a bar early.",
       "3. **WAIT BAR**, then sixteen bars of green rings: every knob travels to the new song.", "4. When they're white again: SLOW ALL **OFF**."],
      "Here's a song change, step by step. First, Slow All to sixteen bars on knob one. Second, load the next song's project, "
      "then Load, Shift and column one, a bar early. "
      "Third, Wait Bar, and then sixteen bars of green rings while every knob travels to the new song. Menus, like a model, "
      "still change on the bar. Fourth, when the rings are white again, Slow All back to off.",
      e16=e16("PERFORM", SLOW_ALL), wait=10),
    S("ONE", "Take the kick out",
      ["Press **Seq 1**. Hold **Mute**, tap the first pad of the **bottom row**: the kick lane mutes, the playhead runs on.", "Same again: the kick is back."],
      "One more move for the breakdowns. Press sequencer one, hold Mute and tap the first pad of the bottom row: the kick lane "
      "mutes, and the pattern keeps running. Do it again and the kick is back, in time.",
      one=one(KIT, [(1, "1 KICK", [1])], right="MUTE + PAD"),
      why="Muting the lane stops the kick, and the rumble with it. For a silent kick that still pumps the rest, pull KICK VOL (knob 5) down instead.", wait=10),
    S("PLAY", "Thirty minutes",
      ["**0-10** song 01: SPARSE › MAIN › VARIATION › MAIN.", "**10-20** song 02: rises on ACID CUT (PERFORM, knob 9).", "**20-30** song 03: SIGNAL up on the VOICE page, then EXIT.",
       "Left hand on rows 1 and 2 throughout; the right hand visits the pages."],
      "Start the timer and play. Ten minutes of song one, through its sections. Ten of song two, with long rises on the acid "
      "cutoff, knob nine on Perform. Ten of song three: bring the voice in with Signal on the voice page, and finish on its exit. "
      "Your left hand stays on the top two rows the whole time; your right hand visits the pages. If you're playing along, "
      "pause me here.",
      e16=e16("PERFORM", K("LOW ENERGY"), K("DRUMS ENERGY"), K("MUSIC ENERGY"), K("BUILD"), K("KICK VOLUME"), K("LOW VOLUME"),
              K("DRUMS VOLUME"), K("MUSIC VOLUME"), K("ACID CUTOFF")), timer=1800, wait=6),
    S("LAPTOP", "Stop and save",
      ["Stop. **Cmd-S**."],
      "Stop, and save the set. The recording is on track ninety-three, in the arrangement.", wait=4),
    S("LISTEN", "Tomorrow, listen back",
      ["Not tonight: tomorrow.", "Note the minutes where it lifted and where it sagged.", "On stage a move feels twice as big as it is. The recording tells you the real pacing."],
      "Listen back tomorrow, not tonight. Write down the minutes where it lifted and the minutes where it sagged. On stage a move "
      "feels twice as big as it really is, and the recording tells you the real pacing. That's the habit that makes the next set "
      "better than this one.",
      done="Thirty minutes, three songs, recorded; notes on where it lifted and where it sagged.", wait=3),
], ["A song change with SLOW ALL", "The kick lane muted for a breakdown", "Thirty minutes recorded on 93 REC", "Notes from listening back"],
    "That's the first set: three songs you built, played and recorded. Next comes the playbook: the moves, one by one.")

COURSE = {
    "brand": "SF LIVE SUITE · LESSONS",
    "title": "Build the first set",
    "lede": "You build three songs on the rig, from an empty ONE project to a set you can play, and learn the moves as you go.",
    "howto": [
        "**Prop the phone by the laptop.** One step per screen. The guide speaks each step as you open it: do it, then tap **NEXT**.",
        "**↻** replays the voice; tap it while it speaks to pause. Swipe left or right to move between steps.",
        "Each step says where your hands go: **LAPTOP** (Live and the plugin windows), **ONE**, **E16**, **LISTEN**, or **PLAY**: a drill, with a timer.",
        "**▶ PLAY ALONG** plays the whole lesson as one track, with room between steps, and the screen follows the voice. Pause it whenever you need more time; **‹ ›** skip a step. Good with earbuds.",
        "Your place is kept on this phone.",
    ],
    "lessons": [L1, L2, L3, L4, L5, L6, L7, L8, L9],
}
