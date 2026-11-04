"""SF SHOW: the OXI E16 layout for the show template (manual, chapter 7).

The one source of truth for the scene file and the address map (make_scene.py) and for the manual's
E16 tables (docs/manual/build_manual.py). Change the layout here, then run make_scene.py and rebuild
the manual.

The grid
  The E16 is a 4 x 4 grid, numbered row by row from the top left: 1-4 the top row, 13-16 the bottom
  row. Encoders 1-8 (rows 1 and 2) are the same on every page; 9-16 (rows 3 and 4) change with the page.

Addresses
  Channel = the destination track: one E16 channel per track (channels 14-16 carry several plugins).
  Number  = block base + the parameter's position in its SF plugin. The plugins keep their parameter
            order for good (new parameters are only ever appended), so an address never moves. One
            plugin on a channel: base 20, so its performance row is always 20-27.
  Mixer   = 100 volume, 101 send A, 102 send B. On the group channels 14 and 15, 100-102 are the
            volumes of their tracks.
  Turns send CC. A HOLD press sends the same number as a CC (127 on press, 0 on release); a TOGGLE
  press sends it as a note (Live flips a switch on every Note On).
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


# Destination: Live track, SF plugin, E16 channel, block base.
PLUGINS = {
    "KICK":     ("01 KICK", "KICK", 1, 20),
    "BASS":     ("11 BASS", "BASS", 2, 20),
    "RUMBLE":   ("12 RUMBLE", "RUMBLE", 3, 20),
    "CLOSED":   ("21 CLOSED", "PERC", 4, 20),
    "OPEN":     ("22 OPEN", "PERC", 5, 20),
    "CLAP":     ("23 CLAP", "PERC", 6, 20),
    "STRIKE":   ("24 STRIKE", "STRIKE", 7, 20),
    "ACID":     ("31 ACID", "LEAD", 8, 20),
    "STAB":     ("32 STAB", "LEAD", 9, 20),
    "TEXTURE":  ("33 TEXTURE", "PAD", 10, 20),
    "VOICE":    ("34 VOICE", "VOICE", 11, 20),
    "DUB":      ("A DUB", "DELAY", 12, 20),
    "SPACE":    ("B SPACE", "REVERB", 13, 20),
    "LOW EN":   ("10 LOW", "ENERGY", 14, 20),
    "DRUMS EN": ("20 DRUMS", "ENERGY", 14, 30),
    "MUSIC EN": ("30 MUSIC", "ENERGY", 14, 40),
    "REPEAT":   ("20 DRUMS", "REPEAT", 14, 50),
    "BUILD":    ("90 LIVE BUS", "BUILD", 15, 20),
    "CONTROL":  ("00 CONTROL", "CONTROL", 16, 20),
}

# Blocks kept free for later, so a new mapping never takes an address that is already promised.
RESERVED = [
    (14, "60-99", "free"),
    (15, "30-39", "SF ISOLATOR on 90 LIVE BUS"), (15, "40-49", "SF GLUE"), (15, "50-59", "SF TONE"),
    (15, "60-69", "SF LOWEND"), (15, "70-79", "SF LIMIT on Main"),
    (15, "102", "41 DECK volume"),
    (16, "40-99", "free"),
]

# Live mixer controls: (track, control) -> (channel, number).
MIXER = {
    ("01 KICK", "volume"): (1, 100),
    ("32 STAB", "send A"): (9, 101),
    ("A DUB", "volume"): (12, 100),
    ("B SPACE", "volume"): (13, 100),
    ("10 LOW", "volume"): (14, 100),
    ("20 DRUMS", "volume"): (14, 101),
    ("30 MUSIC", "volume"): (14, 102),
    ("90 LIVE BUS", "volume"): (15, 100),
    ("40 BRIDGE", "volume"): (15, 101),
}

# Ring colours: indexes into the OXI App's 10 x 10 colour picker (34 white, 18 light blue).
# Light blue marks volume; everything else is white. Change them in the App to taste.
WHITE, LIGHT_BLUE = 34, 18

FADER = "Max 0 dB"
BIPOLAR = ("energy", "filter", "tune")      # centre (64) is neutral


def anchor(module, pid):
    """The knob's anchor (the value a Set to Anchor press would send, and the ring before Live reports):
    the centre for a centre-neutral parameter, the top for one whose default is its maximum, else 0."""
    p = next(x for x in SPECS[module]["params"] if x["id"] == pid)
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
    "attack": "ATK", "signal": "SIG", "echo": "ECHO",
}


class Ctl:
    """One encoder turn and where it lands in Live: track + SF plugin + parameter id, or track + mixer
    control ("volume", "send A"). max_value caps the mapping (the parameter's own unit; gain for volume)."""
    def __init__(self, name, abbr, channel, number, target, colour, default=0, live_range="", label="",
                 track="", module=None, pid=None, mixer=None, max_value=None):
        assert 1 <= len(name) <= 8 and name == name.upper(), name
        assert 1 <= len(abbr) <= 4 and abbr == abbr.upper(), abbr
        assert 1 <= channel <= 16 and 0 <= number <= 119, (name, channel, number)
        self.name, self.abbr, self.channel, self.number, self.target = name, abbr, channel, number, target
        self.colour, self.default, self.live_range = colour, default, live_range
        self.label = label or name          # the full name, for the manual
        self.track, self.module, self.pid, self.mixer, self.max_value = track, module, pid, mixer, max_value

    @property
    def address(self):
        return ("CC", self.channel, self.number)


def P(dest, pid, name=None, abbr=None, live_range="", label=None, max_value=None):
    """A turn on an SF plugin parameter."""
    track, module, ch, base = PLUGINS[dest]
    pname = param_name(module, pid)
    return Ctl(name or pname[:8], abbr or ABBR.get(pid, pname[:4]), ch, base + module_params(module).index(pid),
               f"{track} > SF {module} > {pname}", LIGHT_BLUE if pid == "level" else WHITE,
               anchor(module, pid), live_range, label or pname, track, module, pid, None, max_value)


def M(track, control, name, abbr, live_range="", label=None):
    """A turn on a Live mixer control. A fader stops at 0 dB (gain 1)."""
    ch, n = MIXER[(track, control)]
    return Ctl(name, abbr, ch, n, f"{track} > mixer > {control}", LIGHT_BLUE if control == "volume" else WHITE, 0,
               live_range, label, track, None, None, control, 1.0 if live_range == FADER else None)


class Press:
    """One encoder press. HOLD: CC, 127 on press and 0 on release (the parameter is on while held).
    TOGGLE: a note (Live flips the switch on every Note On, so its state never disagrees with Live's)."""
    def __init__(self, dest, pid, mode):
        track, module, ch, base = PLUGINS[dest]
        assert mode in ("HOLD", "TOGGLE")
        self.mode, self.channel = mode, ch
        self.number = base + module_params(module).index(pid)
        self.name = param_name(module, pid)
        self.target = f"{track} > SF {module} > {self.name}"
        self.track, self.module, self.pid = track, module, pid

    @property
    def address(self):
        return ("CC" if self.mode == "HOLD" else "NOTE", self.channel, self.number)

    def message(self):
        if self.mode == "HOLD":
            return f"CC {self.number} ch {self.channel}, Press & Release, P.Val 127, R.Val 0"
        return f"Note {self.number} ({note_name(self.number)}) ch {self.channel}, Press & Release, velocity 127"


class Key:
    """A Live control on a computer key (Key Map, Cmd-K): a key flips a switch on every press."""
    def __init__(self, target, does):
        self.target, self.does = target, does


def note_name(n):
    return ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"][n % 12] + str(n // 12 - 2)


# Encoders 1-8: the same on every page.
TOP = [
    P("LOW EN", "energy", "LOW ENGY", "E LO", label="LOW ENERGY"),
    P("DRUMS EN", "energy", "DRM ENGY", "E DR", label="DRUMS ENERGY"),
    P("MUSIC EN", "energy", "MUS ENGY", "E MU", label="MUSIC ENERGY"),
    P("BUILD", "build", "BUILD", "BLD"),
    M("01 KICK", "volume", "KICK VOL", "KICK", FADER, "KICK VOLUME"),
    M("10 LOW", "volume", "LOW VOL", "LOW", FADER, "LOW VOLUME"),
    M("20 DRUMS", "volume", "DRUM VOL", "DRUM", FADER, "DRUMS VOLUME"),
    M("30 MUSIC", "volume", "MUS VOL", "MUS", FADER, "MUSIC VOLUME"),
]


def row(dest, ids):
    return [P(dest, pid) for pid in ids]


SYNTH = ["cutoff", "res", "env_amt", "gate", "density", "filter", "drive", "level"]

# Encoders 9-16, page by page. On every instrument page 13 is DENSITY, 14 FILTER and 16 LEVEL, and 12
# is GATE wherever there is one.
PAGES = [
    ("PERFORM", [
        P("ACID", "cutoff", "ACID CUT", "ACID", label="ACID CUTOFF"),
        M("32 STAB", "send A", "STAB>DUB", "STB>", label="STAB SEND A"),
        P("TEXTURE", "color", "TEX COL", "TXCL", label="TEXTURE COLOR"),
        P("VOICE", "filter", "VOX FILT", "VXFL", label="VOICE FILTER"),
        M("A DUB", "volume", "DUB RET", "DUB", FADER, "DUB RETURN"),
        M("B SPACE", "volume", "SPC RET", "SPC", FADER, "SPACE RETURN"),
        M("90 LIVE BUS", "volume", "LIVE BUS", "LIVE", FADER, "LIVE BUS VOLUME"),
        M("40 BRIDGE", "volume", "BRIDGE", "BRDG", FADER, "BRIDGE VOLUME"),
    ]),
    ("KICK", row("KICK", ["tune", "decay", "punch", "click", "density", "filter", "drive", "level"])),
    ("BASS", row("BASS", SYNTH)),
    ("RUMBLE", row("RUMBLE", ["size", "dark", "grit", "gate", "density", "filter", "pump", "level"])),
    ("HATS", [
        P("CLOSED", "density", "CL DENS", "CLDN", label="CLOSED DENSITY"),
        P("CLOSED", "decay", "CL DECAY", "CLDC", label="CLOSED DECAY"),
        P("CLOSED", "tone", "CL TONE", "CLTN", label="CLOSED TONE"),
        P("OPEN", "density", "OP DENS", "OPDN", label="OPEN DENSITY"),
        P("OPEN", "decay", "OP DECAY", "OPDC", label="OPEN DECAY"),
        P("OPEN", "tone", "OP TONE", "OPTN", label="OPEN TONE"),
        P("CLAP", "density", "CLP DENS", "CPDN", label="CLAP DENSITY"),
        P("CLAP", "decay", "CLP DCY", "CPDC", label="CLAP DECAY"),
    ]),
    ("STRIKE", row("STRIKE", ["power", "distance", "size", "decay", "density", "filter", "material", "level"])),
    ("ACID", row("ACID", SYNTH)),
    ("STAB", row("STAB", ["cutoff", "res", "env_amt", "gate", "density", "filter", "space", "level"])),
    ("TEXTURE", row("TEXTURE", ["cutoff", "color", "detune", "gate", "density", "filter", "attack", "level"])),
    ("VOICE", row("VOICE", ["signal", "tone", "echo", "gate", "density", "filter", "drive", "level"])),
    ("SPACE", [
        P("DUB", "time", "DUB TIME", "DTIM", label="DUB TIME"),
        P("DUB", "feedback", "DUB FDBK", "DFDB", "0-70 % at first", "DUB FEEDBACK", max_value=0.70),
        P("DUB", "tone", "DUB TONE", "DTON", label="DUB TONE"),
        P("DUB", "drive", "DUB DRV", "DDRV", label="DUB DRIVE"),
        P("SPACE", "size", "SPC SIZE", "SSIZ", label="SPACE SIZE"),
        P("SPACE", "decay", "SPC DCY", "SDCY", label="SPACE DECAY"),
        P("SPACE", "dark", "SPC DARK", "SDRK", label="SPACE DARK"),
        P("SPACE", "duck", "SPC DUCK", "SDCK", label="SPACE DUCK"),
    ]),
    ("MACROS", [
        P("CONTROL", "macro1", "MACRO 1", "M1"), P("CONTROL", "macro2", "MACRO 2", "M2"),
        P("CONTROL", "macro3", "MACRO 3", "M3"), P("CONTROL", "macro4", "MACRO 4", "M4"),
        P("CONTROL", "macro5", "MACRO 5", "M5"), P("CONTROL", "macro6", "MACRO 6", "M6"),
        P("REPEAT", "length", "RPT LEN", "RLEN", label="REPEAT LENGTH"),
        P("REPEAT", "pitch", "RPT PTCH", "RPCH", label="REPEAT PITCH"),
    ]),
]

# Presses. A knob is easily pressed while it is turned, so a press must be harmless when it happens by
# accident: a throw, a roll, a freeze or a drop is a musical mistake the set plays through. What would
# stop the set goes on a computer key instead (KEYS), never on a press.
PRESSES_ALL = {
    4: Press("BUILD", "drop", "HOLD"),
}
PRESSES_PAGE = {
    ("SPACE", 9): Press("DUB", "throw", "HOLD"),
    ("SPACE", 10): Press("DUB", "freeze", "TOGGLE"),
    ("SPACE", 13): Press("SPACE", "throw", "HOLD"),
    ("SPACE", 14): Press("SPACE", "freeze", "TOGGLE"),
    ("MACROS", 15): Press("REPEAT", "roll", "HOLD"),
}
KEYS = [
    Key("00 CONTROL > SF CONTROL > PANIC", "press: every SF plugin silent (headers show PANIC); press again: back"),
    Key("92 PA > track activator", "press: the speakers off, REC still recording; press again: on"),
]


def page_channel(controls):
    """The page's own channel: its first page control's (every control also carries its own)."""
    return controls[0].channel


def press_for(title, enc):
    return PRESSES_PAGE.get((title, enc)) or PRESSES_ALL.get(enc)


def encoders(title):
    """The 16 turns of one page, encoder 1 first."""
    return TOP + dict(PAGES)[title]


def addresses():
    """Every address once: {address: (target, [where], kind, live_range, item)}."""
    out = {}
    for title, _ in PAGES:
        for i, c in enumerate(encoders(title), 1):
            e = out.setdefault(c.address, [c.target, [], "TURN", c.live_range, c])
            assert e[0] == c.target, (c.address, e[0], c.target)
            e[1].append((title, i))
        for i in range(1, 17):
            p = press_for(title, i)
            if p:
                e = out.setdefault(p.address, [p.target, [], p.mode, "", p])
                assert e[0] == p.target and e[2] == p.mode, (p.address, e[0], p.target)
                e[1].append((title, i))
    return out


def where(places):
    """'every page, encoder 4' or 'SPACE 9, ...'."""
    if len(places) == len(PAGES) and len({i for _, i in places}) == 1:
        return f"every page, encoder {places[0][1]}"
    return ", ".join(f"{t} {i}" for t, i in places)


if __name__ == "__main__":
    a = addresses()
    shared = [k for k, v in a.items() if v[2] == "TURN" and len({t for t, _ in v[1]}) > 1]
    print(f"{len(PAGES)} pages, {len(a)} addresses ({sum(1 for v in a.values() if v[2] == 'TURN')} turns, "
          f"{sum(1 for v in a.values() if v[2] != 'TURN')} presses), {len(shared)} turns on more than one page")
