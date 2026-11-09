"""Tests SF E16 (controllers/live/SF_E16), Live's control surface for the E16's TRACK FOCUS, without
Live: stand-ins for Live's _Framework (a control surface, a CC element that calls its listeners) and
for the show set's tracks (controllers/live/make_set.py SHOW), with a Live that either selects a track
inside a folded group or refuses to (both are handled).

    python3 tests/sf_e16_test.py
"""
import contextlib
import os
import sys
import types

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
sys.path.insert(0, os.path.join(ROOT, "controllers", "live"))
import layout as L  # noqa: E402
import make_set  # noqa: E402

fails = 0


def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg)
    fails += 0 if ok else 1


# ---- stand-ins for Live's modules, as the script uses them
class EncoderElement:
    made = []

    def __init__(self, msg_type, channel, identifier, map_mode, *a, **k):
        self.msg_type, self.channel, self.identifier, self.map_mode = msg_type, channel, identifier, map_mode
        self.listeners = []
        EncoderElement.made.append(self)

    def add_value_listener(self, f):
        self.listeners.append(f)

    def remove_value_listener(self, f):
        self.listeners.remove(f)

    def receive_value(self, v):                  # Live's forwarded CC, as _Framework hands it on
        for f in list(self.listeners):
            f(v)


class ControlSurface:
    def __init__(self, c_instance=None, *a, **k):
        self._c_instance, self.log = c_instance, []

    def song(self):
        return self._c_instance.song()

    @contextlib.contextmanager
    def component_guard(self):
        yield

    def log_message(self, *m):
        self.log.append(" ".join(map(str, m)))

    def disconnect(self):
        pass


def module(name, **attrs):
    m = types.ModuleType(name)
    m.__dict__.update(attrs)
    sys.modules[name] = m
    return m


module("Live", MidiMap=types.SimpleNamespace(MapMode=types.SimpleNamespace(absolute="absolute")))
module("_Framework")
module("_Framework.ControlSurface", ControlSurface=ControlSurface)
module("_Framework.EncoderElement", EncoderElement=EncoderElement)
module("_Framework.InputControlElement", MIDI_CC_TYPE=1)

import SF_E16  # noqa: E402
from SF_E16 import focus  # noqa: E402


# ---- the show set's tracks, as Live lists them (song.tracks: groups and what's in them; returns apart)
class Track:
    def __init__(self, name, group=None, foldable=False):
        self.name, self.group_track, self.is_foldable, self.fold_state = name, group, foldable, False

    @property
    def is_grouped(self):
        return self.group_track is not None

    def hidden(self):
        g = self.group_track
        while g is not None:
            if g.fold_state:
                return True
            g = g.group_track
        return False


class View:
    def __init__(self, strict):
        self.strict, self._selected, self.sets = strict, None, 0

    @property
    def selected_track(self):
        return self._selected

    @selected_track.setter
    def selected_track(self, t):
        if self.strict and t.hidden():
            raise RuntimeError("the track is hidden in a folded group")
        self._selected, self.sets = t, self.sets + 1


def song(strict=False):
    tracks, groups = [], {}
    for spec in make_set.SHOW:
        if spec["kind"] == "return":
            continue
        t = Track(spec["name"], groups.get(spec.get("group")), spec["kind"] == "group")
        if spec["kind"] == "group":
            groups[spec["name"]] = t
        tracks.append(t)
    return types.SimpleNamespace(tracks=tracks, view=View(strict), groups=groups)


class CInstance:
    def __init__(self, s):
        self.s = s

    def song(self):
        return self.s


# ---- the tests
check((focus.FOCUS_CHANNEL + 1, focus.FOCUS_CC) == L.FOCUS[1:], f"the script listens where the scenes send: CC {focus.FOCUS_CC} channel {focus.FOCUS_CHANNEL + 1} ({L.FOCUS})")

s = song()
surface = SF_E16.create_instance(CInstance(s))
el = EncoderElement.made[-1]
check(len(EncoderElement.made) == 1 and (el.msg_type, el.channel, el.identifier) == (1, L.FOCUS[1] - 1, L.FOCUS[2]),
      "one CC element, on the FOCUS address")
check(any("SF E16" in m for m in surface.log), "says so in Live's log")

picked = []
for n in L.focus(L.SHOW_SCENE):
    if n:
        el.receive_value(n)
        picked.append(s.view.selected_track.name)
want = [L.PLUGINS[t][0] for t in L.TRACKS]
check(picked == want, f"PERF's pages 1-11 select {picked}")

before = s.view.selected_track
for v in (0, 12, 16, 99, 127):
    el.receive_value(v)
check(s.view.selected_track is before, "0 and numbers no track starts with (12, 16, 99, 127): the selection stays")
check(focus.numbered(s.tracks, 1).name == "1 KICK" and focus.numbered(s.tracks, 10).name == "10 PAD",
      "1 is 1 KICK, not 10 PAD or 11 VOICE; 10 is 10 PAD")

s.groups["DRUMS"].fold_state = True
el.receive_value(2)
check(s.view.selected_track.name == "2 CLOSED HAT" and s.groups["DRUMS"].fold_state,
      "DRUMS folded, a Live that selects inside it: 2 CLOSED HAT selected, DRUMS stays folded")

strict = song(strict=True)
other = SF_E16.create_instance(CInstance(strict))
el2 = EncoderElement.made[-1]
strict.groups["MUSIC"].fold_state = True
el2.receive_value(9)
check(strict.view.selected_track.name == "9 CHORD" and not strict.groups["MUSIC"].fold_state,
      "MUSIC folded, a Live that won't select a hidden track: MUSIC unfolded, 9 CHORD selected")
el2.receive_value(1)
check(strict.view.selected_track.name == "1 KICK", "1 KICK (in no group): selected")

surface.disconnect()
el.receive_value(6)
check(s.view.selected_track.name == "2 CLOSED HAT" and el.listeners == [], "after disconnect: nothing")

print(("PASS" if fails == 0 else "FAIL") + f": {fails} failure(s)")
sys.exit(1 if fails else 0)
