"""The OXI E16 layout for the show template (manual, chapter 7): two scenes, PERF and MIX.

The one source of truth for the scene files and the address map (make_scene.py), the show set's
mappings (controllers/live/make_set.py), the plugins' copy of the map (Source/core/E16Map.h), and the
manuals' E16 tables and page drawings. Change the layout here, then run make_scene.py and make_set.py
and rebuild both manuals.

One number per track [Nov 8]
  A track's number is its MIDI channel, on the OXI One (port A) and on the E16, and its page in PERF:
  1 KICK, 2 CLOSED HAT, 3 OPEN HAT, 4 CLAP, 5 PERC, 6 BASS, 7 RUMBLE, 8 LEAD, 9 CHORD, 10 PAD, 11 VOICE
  (7 RUMBLE gets no notes: it follows the kick). 12 is A DUB, 13 B SPACE, 14 the groups (DRUMS, LOW,
  MUSIC, and DRUMS' REPEAT), 15 the LIVE BUS, 16 CONTROL. Nothing else on the E16 uses a track's
  channel, so a track's page can be mapped to any other instrument put on that track.
  On the E16 a track goes by its code (the performer's, from the PERF scene he edited on Nov 8): KCK,
  CHH, OHH, CLP, PRC, BAS, RMB, LED, CRD, PAD, VOX, and the groups DRM, LOW, MUS. A page's title is
  its track's code; an ENERGY knob is its code + NRG (KCK NRG), a volume its code + VOL.

The grid
  The E16 is a 4 x 4 grid, numbered row by row from the top left: 1-4 the top row, 13-16 the bottom row.
  PERF, pages 1-11: one per track. Rows 1-2 are its instrument's own knobs; row 3 is GATE (DECAY where
  the instrument has none), DENSITY, FILTER and LEVEL on every page; row 4 is SEND A, SEND B, NEXT SONG
  (press: GO) and BUILD (press: DROP). Page 12, NRG: the ENERGY knobs, knob n for track n (12-14 the
  groups DRUMS, LOW, MUSIC; each press back to the centre), then NEXT SONG and BUILD. Knob 1's press
  swaps it to SLOW ALL on every PERF page.
  MIX: LEVELS (the volumes, in NRG's places; 15 and 16 the returns), SPACE (both returns, THROW and
  FREEZE on their presses), MACROS (MACRO 1-6, SLOW ALL, REPEAT with ROLL on its press).
  Rings by role (the performer's colours): white an instrument's own sound, SHAPE a track's GATE or
  DECAY, DENSITY, FILTER and its NRG, light blue a level (LEVEL, a volume, a send), GROUP a group's NRG,
  SET NEXT SONG and BUILD, green SLOW ALL.

Addresses
  On a track's channel: its page's knobs 1-12 CC 12-23 (the place + 11), its ENERGY CC 28 (the groups on
  14: DRUMS 28, LOW 29, MUSIC 30), its volume CC 102 (the groups: 102-104), SEND A 103, SEND B 104.
  Anything else in PERF: the place + 11 for a turn, + 27 for a press (SLOW ALL 12, NEXT SONG 26,
  BUILD 27, GO 42, DROP 43); in MIX: the place + 43 for a turn, + 59 for a press. A knob on two pages
  sends the same address from both (one Live parameter takes one mapping).
  Turns send CC. A HOLD press sends CC (127 on press, 0 on release); a TOGGLE press a note (Live flips a
  switch on every Note On); a CENTRE press sends 64 on its knob's own address (an ENERGY back to neutral).
  Never used: CC 6, 38 and 96-101 (data entry and RPN/NRPN, which Live and many devices treat as parts
  of a longer message) and 120-127 (channel mode).
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
SPECS = json.load(open(os.path.join(HERE, "..", "..", "docs", "manual", "specs.json")))
SYSTEM = ("slow", "macro_src", "macro_amt")


def module_params(module):
    """The SF plugin's own parameter ids in order (the shared system rows sit after them)."""
    return [p["id"] for p in SPECS[module]["params"] if p["id"] not in SYSTEM and p["id"][:3] not in ("m1_", "m2_")]


def param_name(module, pid):
    for p in SPECS[module]["params"]:
        if p["id"] == pid:
            return p["name"]
    raise KeyError(f"{module} has no parameter {pid}")


# Destination: Live track, SF plugin, channel (the track's number; the OXI One sends its notes there too).
PLUGINS = {
    "KICK":       ("1 KICK", "KICK", 1),
    "CLOSED HAT": ("2 CLOSED HAT", "PERC", 2),
    "OPEN HAT":   ("3 OPEN HAT", "PERC", 3),
    "CLAP":       ("4 CLAP", "PERC", 4),
    "PERC":       ("5 PERC", "STRIKE", 5),
    "BASS":       ("6 BASS", "BASS", 6),
    "RUMBLE":     ("7 RUMBLE", "RUMBLE", 7),
    "LEAD":       ("8 LEAD", "LEAD", 8),
    "CHORD":      ("9 CHORD", "LEAD", 9),
    "PAD":        ("10 PAD", "PAD", 10),
    "VOICE":      ("11 VOICE", "VOICE", 11),
    "DUB":        ("A DUB", "DELAY", 12),
    "SPACE":      ("B SPACE", "REVERB", 13),
    "DRUMS EN":   ("DRUMS", "ENERGY", 14),
    "LOW EN":     ("LOW", "ENERGY", 14),
    "MUSIC EN":   ("MUSIC", "ENERGY", 14),
    "REPEAT":     ("DRUMS", "REPEAT", 14),
    "BUILD":      ("LIVE BUS", "BUILD", 15),
    "CONTROL":    ("CONTROL", "CONTROL", 16),
}
# The instrument tracks in their order (track n on channel n), and each one's SF ENERGY (last on the
# track) with the profile for its part.
TRACKS = ["KICK", "CLOSED HAT", "OPEN HAT", "CLAP", "PERC", "BASS", "RUMBLE", "LEAD", "CHORD", "PAD", "VOICE"]
ENERGY_TRACKS = {"KICK": "DRUMS", "CLOSED HAT": "DRUMS", "OPEN HAT": "DRUMS", "CLAP": "DRUMS", "PERC": "DRUMS",
                 "BASS": "BASS", "RUMBLE": "BASS", "LEAD": "MUSIC", "CHORD": "MUSIC", "PAD": "MUSIC", "VOICE": "VOICE"}
for _dest in ENERGY_TRACKS:
    PLUGINS[_dest + " EN"] = (PLUGINS[_dest][0], "ENERGY", PLUGINS[_dest][2])
assert [PLUGINS[t][2] for t in TRACKS] == list(range(1, len(TRACKS) + 1))
assert all(PLUGINS[t][0].split(" ", 1)[0] == str(PLUGINS[t][2]) for t in TRACKS), "a track's number is its channel"

# Plugins that can stand in for another on its track with the E16 unchanged: the parameters the scene
# uses have the same ids at the same places. SF TERROIR (convolution, your own places) swaps in for SF
# REVERB on B SPACE: the SPACE page's knobs and presses land on its SIZE, DECAY, DARK, DUCK, THROW and
# FREEZE once Live is mapped to it (Cmd-M; controllers/live/check_set.py accepts either).
ALTERNATES = {"REVERB": ["TERROIR"]}


def alternates(module, pid):
    """The modules that may stand in for `module` for parameter `pid` (same id, same place)."""
    out = []
    for alt in ALTERNATES.get(module, []):
        ids = module_params(alt)
        assert pid in ids and ids.index(pid) == module_params(module).index(pid), f"{alt} {pid} is not where {module} has it"
        out.append(alt)
    return out


# The scenes' titles (the E16 shows 7 characters): PERF, the show; MIX, the levels, returns and macros.
SHOW_SCENE, MIX_SCENE = "PERF", "MIX"
# Numbers: per scene, a turn its place + TURN, a press its place + PRESS; a track's ENERGY 28; the mixer
# 102 volume, 103 send A, 104 send B (MIDI's undefined numbers: 100 and 101 select an RPN).
TURN = {SHOW_SCENE: 11, MIX_SCENE: 43}
PRESS = {SHOW_SCENE: 27, MIX_SCENE: 59}
ENERGY = 28
SENDS = {"volume": 102, "send A": 103, "send B": 104}
RESERVED_CC = {6, 38, 96, 97, 98, 99, 100, 101} | set(range(120, 128))
# Track focus [Nov 8]: when a page comes up, the scenes' script sends FOCUS with the number of the
# page's track (a PERF track page: 1-11), and Live's control surface SF E16 (controllers/live/SF_E16)
# selects the track whose name starts with that number. NRG and MIX's pages send nothing: the
# selection stays where it was. CC 119 on CONTROL's channel: no knob or press sends it.
FOCUS = ("CC", 16, 119)

# Ring colours: the OXI App's colour numbers (0-100), by role. The performer set SHAPE, GROUP and SET
# in the PERF scene on Nov 8, for a page that reads at a glance while playing. Green, the suite's
# colour for a glide: the knob is on its second destination, SLOW ALL (Swap Destination's second
# colour). Change them here, not in the App: make_scene.py rewrites the files.
WHITE = 34          # an instrument's own sound (a track's knobs 1-8), and anything without a role below
SHAPE = 63          # a track's GATE or DECAY, DENSITY and FILTER (its knobs 9-11), and its NRG
LIGHT_BLUE = 18     # a level: a plugin's LEVEL, a volume, a send
GROUP = 50          # a group's NRG (DRUMS, LOW, MUSIC)
SET = 9             # NEXT SONG and BUILD: the set's own moves
GREEN = 90          # SLOW ALL, knob 1's second destination; a travelling knob (GLIDE RINGS)
# How each one looks, as the plugins' windows draw the rings (E16Map.h kE16Rings): RGB. White, light
# blue, pink and green are what E16 owners see for those numbers (green, a mint, is also the plugins'
# own glide colour, Ui.h kGreen). SHAPE red and SET violet are where the App's palette puts 63 (its
# row 60-69, dark red to red) and 9 (the end of row 0-9, periwinkle), not yet held up against the
# hardware: if the E16 shows them otherwise, change them here, run make_scene.py and rebuild.
RING_RGB = {WHITE: 0xEEF2FF, SHAPE: 0xFF3F5F, LIGHT_BLUE: 0x8CC4FF, GROUP: 0xFF80C8, SET: 0xA894FF, GREEN: 0x4CF59B}
RING_NAME = {WHITE: "white", SHAPE: "red", LIGHT_BLUE: "light blue", GROUP: "pink", SET: "violet", GREEN: "green"}

FADER = "Max 0 dB"
BIPOLAR = ("energy", "filter", "tune")      # centre (64) is neutral


def anchor(module, pid):
    """The knob's anchor (the value a Set to Anchor press would send, and the ring before Live reports):
    the centre for a centre-neutral parameter, the top for one whose default is its maximum, else 0."""
    p = next(x for x in SPECS[module]["params"] if x["id"] == pid)
    if p["type"] == "menu":                         # the default's place in the menu
        ch = p["choices"].split("|")
        return round(ch.index(p["def"]) / (len(ch) - 1) * 127)
    if pid in BIPOLAR or (p["def"] == "OFF" and p["min"] != "OFF" and p["max"] != "OFF"):
        return 64
    for unit in (" %", " st"):                      # linear ranges: the default's place in them
        v = [p[k][:-len(unit)] for k in ("min", "max", "def") if p[k].endswith(unit)]
        try:
            lo, hi, d = (float(x) for x in v)
            return round((d - lo) / (hi - lo) * 127)
        except ValueError:
            pass
    return 127 if p["def"] == p["max"] else 0

ABBR = {
    "cutoff": "CUT", "res": "RES", "env_amt": "ENV", "gate": "GATE", "density": "DENS", "filter": "FILT",
    "drive": "DRV", "level": "LVL", "tune": "TUNE", "decay": "DCY", "punch": "PNCH", "click": "CLCK",
    "size": "SIZE", "dark": "DARK", "grit": "GRIT", "pump": "PUMP", "tone": "TONE", "power": "POWR",
    "distance": "DIST", "material": "MATL", "space": "SPC", "color": "COL", "detune": "DTUN",
    "attack": "ATK", "signal": "SIG", "echo": "ECHO", "accent": "ACNT", "glide": "GLID", "pitch_time": "PTIM",
    "layer": "LAYR", "echo_space": "ESPC", "echo_swing": "ESWG", "echo_prob": "EPRB", "spread": "SPRD",
    "position": "POS", "tension": "TENS", "pocket_depth": "PCKT", "pocket_shape": "PSHP", "predelay": "PRE",
    "width": "WIDE", "echo_time": "ETIM", "echo_fb": "EFBK",
}
SHORT = {"pitch_time": "PITCH T", "echo_space": "ECHO SPC", "echo_swing": "ECHO SWG", "echo_prob": "ECHO PRB",
         "pocket_depth": "PCKT DEP", "pocket_shape": "PCKT SHP", "echo_time": "ECHO TME", "echo_fb": "ECHO FBK"}


class Ctl:
    """One encoder turn and where it lands in Live: track + SF plugin + parameter id, or track + mixer
    control ("volume", "send A", "send B"). max_value caps the mapping (the parameter's own unit; gain
    for a volume). second: the encoder's second destination, reached with its Swap Destination press.
    An SF plugin turn's number comes from where it sits (place(), below)."""
    second = False
    def __init__(self, name, abbr, channel, number, target, colour, default=0, live_range="", label="",
                 track="", module=None, pid=None, mixer=None, max_value=None):
        assert 1 <= len(name) <= 8 and name == name.upper(), name
        assert 1 <= len(abbr) <= 4 and abbr == abbr.upper(), abbr
        assert 1 <= channel <= 16 and (number is None or 0 <= number <= 119), (name, channel, number)
        self.name, self.abbr, self.channel, self.number, self.target = name, abbr, channel, number, target
        self.colour, self.default, self.live_range = colour, default, live_range
        self.label = label or name          # the full name, for the manual
        self.track, self.module, self.pid, self.mixer, self.max_value = track, module, pid, mixer, max_value

    @property
    def address(self):
        return ("CC", self.channel, self.number)


def P(dest, pid, name=None, abbr=None, live_range="", label=None, max_value=None):
    """A turn on an SF plugin parameter (its number: place())."""
    track, module, ch = PLUGINS[dest]
    pname = param_name(module, pid)
    return Ctl(name or SHORT.get(pid, pname[:8]), abbr or ABBR.get(pid, pname[:4]), ch, None,
               f"{track} > SF {module} > {pname}", LIGHT_BLUE if pid == "level" else WHITE,
               anchor(module, pid), live_range, label or pname, track, module, pid, None, max_value)


def channel_of(track):
    for t, _m, ch in PLUGINS.values():
        if t == track:
            return ch
    raise KeyError(track)


GROUP_ORDER = ["DRUMS", "LOW", "MUSIC"]      # channel 14's volumes 102-104, as the set has them


def M(track, control, name, abbr, live_range="", label=None):
    """A turn on a Live mixer control (a track's volume or send; a group's volume), a level: light blue.
    A fader stops at 0 dB (gain 1), and so does a send."""
    ch = channel_of(track)
    n = SENDS["volume"] + GROUP_ORDER.index(track) if ch == 14 else SENDS[control]
    return Ctl(name, abbr, ch, n, f"{track} > mixer > {control}", LIGHT_BLUE, 0,
               live_range or FADER, label, track, None, None, control, 1.0)


class Press:
    """One encoder press. HOLD: CC, 127 on press and 0 on release (the parameter is on while held).
    TOGGLE: a note (Live flips the switch on every Note On, so its state never disagrees with Live's).
    CENTRE: CC 64 on press and release, on its knob's own address: the knob's parameter to its middle
    (an ENERGY back to neutral). No mapping of its own: the knob's."""
    def __init__(self, dest, pid, mode):
        track, module, ch = PLUGINS[dest]
        assert mode in ("HOLD", "TOGGLE", "CENTRE")
        self.mode, self.channel = mode, ch
        self.number = None                  # place()
        self.name = param_name(module, pid)
        self.target = f"{track} > SF {module} > {self.name}"
        self.track, self.module, self.pid = track, module, pid

    @property
    def address(self):
        return ("NOTE" if self.mode == "TOGGLE" else "CC", self.channel, self.number)

    def message(self):
        if self.mode == "HOLD":
            return f"CC {self.number} ch {self.channel}, Press & Release, P.Val 127, R.Val 0"
        if self.mode == "CENTRE":
            return f"CC {self.number} ch {self.channel}, Press & Release, P.Val 64, R.Val 64 (its knob's address)"
        return f"Note {self.number} ({note_name(self.number)}) ch {self.channel}, Press & Release, velocity 127"

    def short(self):
        return f"{self.name} ({self.mode})"


class Swap:
    """Swap Destination: a quick press flips the encoder to its second destination, a turn of its own
    (the ring takes the second colour, the screen shows a bar under the value); another press flips it
    back. The press itself sends nothing; both destinations are plain CC turns with Live's feedback.
    Without the swap press an encoder with two destinations would send both on every turn."""
    mode = "SWAP"
    address = None

    def __init__(self, ctl):
        self.ctl = ctl
        ctl.second, ctl.colour = True, GREEN
        self.name, self.target = ctl.label, ctl.target
        self.channel, self.number = ctl.channel, ctl.number

    def message(self):
        return f"Swap Destination; Destination 2 = CC {self.number} ch {self.channel}"

    def short(self):
        return self.name                    # fits a grid cell: "PRESS: SLOW ALL"


def note_name(n):
    return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12] + str(n // 12 - 2)


# ---------------------------------------------------------------------------------------- PERF
# A track's page: rows 1-2 its instrument's own knobs, row 3 GATE (or DECAY), DENSITY, FILTER, LEVEL,
# row 4 SEND A, SEND B, NEXT SONG, BUILD.
SOUND = {
    "KICK":       (["tune", "punch", "click", "drive", "pitch_time", "layer", "echo_space", "echo_swing"], "decay"),
    "CLOSED HAT": (["tune", "tone", "accent", "drive", "echo_space", "echo_swing", "echo_prob", "spread"], "decay"),
    "OPEN HAT":   (["tune", "tone", "accent", "drive", "echo_space", "echo_swing", "echo_prob", "spread"], "decay"),
    "CLAP":       (["tune", "tone", "accent", "drive", "echo_space", "echo_swing", "echo_prob", "spread"], "decay"),
    "PERC":       (["power", "distance", "size", "material", "position", "tension", "spread", "tune"], "decay"),
    "BASS":       (["cutoff", "res", "env_amt", "decay", "drive", "accent", "glide", "pocket_depth"], "gate"),
    "RUMBLE":     (["size", "dark", "grit", "pump", "tune", "predelay", "width", "pocket_shape"], "gate"),
    "LEAD":       (["cutoff", "res", "env_amt", "decay", "drive", "accent", "glide", "color"], "gate"),
    "CHORD":      (["cutoff", "res", "env_amt", "decay", "attack", "space", "detune", "pocket_depth"], "gate"),
    "PAD":        (["cutoff", "res", "color", "detune", "attack", "decay", "space", "pocket_depth"], "gate"),
    "VOICE":      (["signal", "tone", "tune", "drive", "echo", "echo_time", "echo_fb", "pocket_depth"], "gate"),
}


def coloured(c, colour):
    c.colour = colour
    return c


def next_song():
    return coloured(P("CONTROL", "next_song", "NEXT SNG", "SONG", label="NEXT SONG"), SET)


def build_knob():
    return coloured(P("BUILD", "build", "BUILD", "BLD"), SET)


def track_page(dest):
    track = PLUGINS[dest][0]
    sound, gate = SOUND[dest]
    return [P(dest, pid) for pid in sound] + [coloured(P(dest, pid), SHAPE) for pid in (gate, "density", "filter")] + [
        P(dest, "level"),
        M(track, "send A", "SEND A", "SNDA", label="SEND A (DUB)"),
        M(track, "send B", "SEND B", "SNDB", label="SEND B (SPACE)"),
        next_song(), build_knob()]


# Each track's and group's code: the E16's name for it (page titles, NRG, the volumes).
CODE = {"KICK": "KCK", "CLOSED HAT": "CHH", "OPEN HAT": "OHH", "CLAP": "CLP", "PERC": "PRC", "BASS": "BAS",
        "RUMBLE": "RMB", "LEAD": "LED", "CHORD": "CRD", "PAD": "PAD", "VOICE": "VOX",
        "DRUMS": "DRM", "LOW": "LOW", "MUSIC": "MUS"}
assert set(CODE) == set(TRACKS) | set(GROUP_ORDER) and len(set(CODE.values())) == len(CODE)


def E(t):
    """Track or group t's ENERGY knob, KCK NRG on the E16 (its press, CENTRE, sets it back to neutral)."""
    return coloured(P(t + " EN", "energy", CODE[t] + " NRG", CODE[t], label=t + " ENERGY"), GROUP if t in GROUP_ORDER else SHAPE)


NRG = [E(t) for t in TRACKS] + [E(g) for g in GROUP_ORDER] + [next_song(), build_knob()]
SHOW_PAGES = [(CODE[t], track_page(t)) for t in TRACKS] + [("NRG", NRG)]

# ---------------------------------------------------------------------------------------- MIX
def V(track, code):
    """A volume on LEVELS, named by its code (KCK VOL), in NRG's place."""
    return M(track, "volume", CODE[code] + " VOL", CODE[code], label=track + " VOLUME")


LEVELS = [V(PLUGINS[t][0], t) for t in TRACKS] + [V(g, g) for g in GROUP_ORDER] + \
         [M("A DUB", "volume", "DUB RET", "DUB", label="DUB RETURN"), M("B SPACE", "volume", "SPC RET", "SPC", label="SPACE RETURN")]
SPACE_PAGE = [
    P("DUB", "time", "DUB TIME", "DTIM", label="DUB TIME"),
    P("DUB", "feedback", "DUB FDBK", "DFDB", "0-70 % at first", "DUB FEEDBACK", max_value=0.70),
    P("DUB", "tone", "DUB TONE", "DTON", label="DUB TONE"),
    P("DUB", "drive", "DUB DRV", "DDRV", label="DUB DRIVE"),
    P("SPACE", "size", "SPC SIZE", "SSIZ", label="SPACE SIZE"),
    P("SPACE", "decay", "SPC DCY", "SDCY", label="SPACE DECAY"),
    P("SPACE", "dark", "SPC DARK", "SDRK", label="SPACE DARK"),
    P("SPACE", "duck", "SPC DUCK", "SDCK", label="SPACE DUCK"),
] + [None] * 8
MACROS_PAGE = [P("CONTROL", f"macro{i}", f"MACRO {i}", f"M{i}") for i in range(1, 7)] + \
              [P("CONTROL", "slow_all", "SLOW ALL", "SLOW"), None,
               P("REPEAT", "length", "RPT LEN", "RLEN", label="REPEAT LENGTH"),
               P("REPEAT", "pitch", "RPT PTCH", "RPCH", label="REPEAT PITCH")] + [None] * 6
MIX_PAGES = [("LEVELS", LEVELS), ("SPACE", SPACE_PAGE), ("MACROS", MACROS_PAGE)]

SCENES = [(SHOW_SCENE, SHOW_PAGES), (MIX_SCENE, MIX_PAGES)]
PAGES = [p for _, pages in SCENES for p in pages]                 # every page that has knobs, scene by scene
TOP = []                                                          # no knobs shared by every page since Nov 8
assert all(len(c) == 16 for _, c in PAGES) and len({t for t, _ in PAGES}) == len(PAGES)
assert all(len(pages) <= 12 for _, pages in SCENES)


def scene_of(title):
    return next(s for s, pages in SCENES if title in dict(pages))


# Presses. A knob is easily pressed while it is turned, so a press must be harmless when it happens by
# accident: a throw, a roll, a freeze, a drop, an ENERGY back to neutral or GO (it changes nothing unless
# NEXT SONG picked another song first) is one the set plays through, and a swap only flips the knob
# (SLOW ALL never moves the sound, only how the next moves go). Nothing that would stop the set is on
# the E16: the PA's emergency stop is the audio interface's output knob.
def _centre(c):
    """The press that sets an ENERGY knob back to the centre."""
    dest = next(d for d, v in PLUGINS.items() if (v[0], v[1]) == (c.track, c.module))
    return Press(dest, c.pid, "CENTRE")


SLOW_ALL = Swap(P("CONTROL", "slow_all", "SLOW ALL", "SLOW"))
PRESSES_PAGE = {}
for _title, _ in SHOW_PAGES:
    PRESSES_PAGE[(_title, 1)] = SLOW_ALL                         # knob 1's press: SLOW ALL, every PERF page
    PRESSES_PAGE[(_title, 15)] = Press("CONTROL", "song_go", "HOLD")
    PRESSES_PAGE[(_title, 16)] = Press("BUILD", "drop", "HOLD")
for _k, _c in enumerate(NRG[:14], 1):
    if _k > 1:
        PRESSES_PAGE[("NRG", _k)] = _centre(_c)
PRESSES_PAGE.update({
    ("SPACE", 1): Press("DUB", "throw", "HOLD"),
    ("SPACE", 2): Press("DUB", "freeze", "TOGGLE"),
    ("SPACE", 5): Press("SPACE", "throw", "HOLD"),
    ("SPACE", 6): Press("SPACE", "freeze", "TOGGLE"),
    ("MACROS", 9): Press("REPEAT", "roll", "HOLD"),
})
PRESSES_ALL = {}                                                  # (kept for the scripts that read it)


def press_for(title, enc):
    return PRESSES_PAGE.get((title, enc))


def encoders(title):
    """The 16 turns of one page, encoder 1 first (None: nothing on that encoder)."""
    return dict(PAGES)[title]


def page_channel(controls):
    """The page's own channel: its first control's (every control also carries its own)."""
    return next(c.channel for c in controls if c is not None)


def place():
    """Every SF plugin turn's and press's number, from where it sits (the module's docstring). A control
    sits on one page only, or at the same place on several (NEXT SONG, BUILD, SLOW ALL), or on NRG's
    or MACROS' copy of it: it keeps the address it was first given, since a Live parameter takes one
    mapping. Refused: one control with two addresses. A CENTRE press takes its knob's address."""
    home, energies = {}, {}
    def turn(c, scene, knob):
        if c is None or c.mixer is not None:
            return
        key = (c.track, c.module, c.pid)
        if key in home:
            c.number = home[key]
            return
        if c.module == "ENERGY" and c.pid == "energy":
            c.number = ENERGY + energies.get(c.channel, 0)
            energies[c.channel] = energies.get(c.channel, 0) + 1
        else:
            c.number = knob + TURN[scene]
        home[key] = c.number
    turn(SLOW_ALL.ctl, SHOW_SCENE, 1)
    SLOW_ALL.number = SLOW_ALL.ctl.number
    for scene, pages in SCENES:
        for title, controls in pages:
            for k, c in enumerate(controls, 1):
                turn(c, scene, k)
    for (title, enc), p in PRESSES_PAGE.items():
        if p.mode == "SWAP":
            continue
        if p.mode == "CENTRE":
            p.number = home[(p.track, p.module, p.pid)]
            knob = encoders(title)[enc - 1]
            assert (knob.track, knob.module, knob.pid) == (p.track, p.module, p.pid), (title, enc)
        else:
            p.number = enc + PRESS[scene_of(title)]
    numbers = {}
    for c in [c for _, cs in PAGES for c in cs if c is not None] + [SLOW_ALL.ctl]:
        assert c.number is not None and 0 <= c.number <= 119, c.target
        numbers.setdefault((c.track, c.module, c.pid, c.mixer), set()).add(c.number)
    assert all(len(v) == 1 for v in numbers.values()), "a control with two addresses"
    for c in [c for _, cs in PAGES for c in cs if c is not None]:
        assert c.number not in RESERVED_CC, (c.target, c.number)
    for p in PRESSES_PAGE.values():
        assert p.mode == "TOGGLE" or p.number not in RESERVED_CC, (p.target, p.number)


place()


def addresses():
    """Every address once: {address: (target, [where], kind, live_range, item)}."""
    out = {}
    for title, controls in PAGES:
        for i, c in enumerate(controls, 1):
            if c is not None:
                e = out.setdefault(c.address, [c.target, [], "TURN", c.live_range, c])
                assert e[0] == c.target, (c.address, e[0], c.target)
                e[1].append((title, i))
            p = press_for(title, i)
            if p and p.mode == "SWAP":                  # the press sends nothing; its second turn does
                c = p.ctl
                e = out.setdefault(c.address, [c.target, [], "TURN", c.live_range, c])
                assert e[0] == c.target, (c.address, e[0], c.target)
                e[1].append((title, i))
            elif p and p.mode == "CENTRE":                # the knob's own address and mapping
                assert out[p.address][0] == p.target and out[p.address][2] == "TURN", (p.address, p.target)
            elif p:
                e = out.setdefault(p.address, [p.target, [], p.mode, "", p])
                assert e[0] == p.target and e[2] == p.mode, (p.address, e[0], p.target)
                e[1].append((title, i))
    return out


def focus(scene):
    """The Live track each of the scene's 12 pages selects when it comes up (FOCUS): the track's number
    (its channel, the number its Live name starts with) for a PERF track page; 0 for a page that
    leaves the selection where it is (NRG, MIX's pages, the empty pages)."""
    tracks = {CODE[t]: PLUGINS[t][2] for t in TRACKS} if scene == SHOW_SCENE else {}
    titles = [t for t, _ in dict(SCENES)[scene]]
    return [tracks.get(t, 0) for t in titles] + [0] * (12 - len(titles))


assert FOCUS not in addresses() and FOCUS[2] not in RESERVED_CC, "FOCUS is a knob's or a press's address"
assert all(c is None or c.colour in RING_RGB for _, cs in PAGES for c in cs + [SLOW_ALL.ctl]), "a ring colour without its RGB"


def where(places, second=False):
    """'every PERF page, encoder 16' or 'SPACE 1, ...'. A swap's turn (second) is reached with the
    press on PERF: ' (second destination)' after those places."""
    tail = " (second destination)" if second else ""
    show = [t for t, _ in SHOW_PAGES]
    on_show = [(t, i) for t, i in places if t in show]
    rest = [f"{t} {i}" for t, i in places if t not in show]
    if len(on_show) == len(show) and len({i for _, i in on_show}) == 1:
        return ", ".join([f"every {SHOW_SCENE} page, encoder {on_show[0][1]}{tail}"] + rest)
    return ", ".join([f"{t} {i}{tail}" for t, i in on_show] + rest)


def track_channels():
    """[(channel, [tracks])] in channel order: what each E16 channel works on."""
    out = {}
    for track, module, ch in PLUGINS.values():
        out.setdefault(ch, [])
        if track not in out[ch]:
            out[ch].append(track)
    return sorted(out.items())


if __name__ == "__main__":
    a = addresses()
    shared = [k for k, v in a.items() if v[2] == "TURN" and len({t for t, _ in v[1]}) > 1]
    print(f"{len(PAGES)} pages in {len(SCENES)} scenes, {len(a)} addresses ({sum(1 for v in a.values() if v[2] == 'TURN')} turns, "
          f"{sum(1 for v in a.values() if v[2] != 'TURN')} presses), {len(shared)} turns on more than one page")
