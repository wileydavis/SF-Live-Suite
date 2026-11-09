"""Tests the E16 scenes' GLIDE RINGS script as shipped (the minified code inside PERF.oxie16 and
MIX.oxie16, each with its own knob map) against a mock of the E16's Lua API (1-based encoders and
pages, 14-bit ring values, leds.reset hands a ring back to the firmware). Needs lupa (pip install
lupa): Lua 5.4.

    python3 tests/glide_rings_test.py
"""
import json
import os
import sys

from lupa import lua54

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
import layout as L  # noqa: E402

fails = 0


def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg)
    fails += 0 if ok else 1


MOCK = """
E = {rings = {}, page = 1, rate = 0}
leds = {
  updateByIndex = function(i, v, c)
    assert(math.type(i) == "integer" and i >= 1 and i <= 16, "led index " .. tostring(i))
    assert(math.type(v) == "integer" and v >= 0 and v <= 16383, "led value " .. tostring(v))
    assert(c >= 0 and c <= 100, "led color")
    E.rings[i] = {v = v, c = c}
  end,
  reset = function(i) assert(i >= 1 and i <= 16); E.rings[i] = nil end,
}
controller = {getPage = function() return E.page end}
page = {}
system = {setUpdateRate = function(ms) E.rate = ms end}
slots = {update = function() end, reset = function() end}
midi = {}
"""


class E16:
    """One scene's script running in a fresh Lua."""
    def __init__(self, title):
        scene = json.load(open(os.path.join(ROOT, "controllers", "e16", title + ".oxie16")))
        code = scene["code"]["code"]
        check(code.count("\n") == 1 and code.endswith("\n"), f"{title}: the script is one line, {len(code)} bytes")
        self.lua = lua54.LuaRuntime(unpack_returned_tuples=True)
        self.lua.execute(MOCK)
        self.lua.execute(code)
        self.G = self.lua.globals()
        self.E = self.G.E
        self.pages = [t for t, _ in dict(L.SCENES)[title]]
        self.G.page.onInit()

    def ring(self, i):
        r = self.E.rings[i]
        return None if r is None else (r.v, r.c)

    def rings(self):
        return {i: self.ring(i) for i in range(1, 17) if self.ring(i) is not None}

    def sysex(self, *data):
        self.G.controller.onSysex(self.lua.table_from([0xF0, 0x7D, 0x53, 0x46] + list(data) + [0xF7]))

    def move(self, *knobs):
        """knobs: (control, value)"""
        out = [1]
        for c, v in knobs:
            out += [c.channel - 1, c.number, v >> 7, v & 127]
        self.sysex(*out)

    def arrive(self, *knobs):
        out = [2]
        for c in knobs:
            out += [c.channel - 1, c.number]
        self.sysex(*out)

    def update(self, n=1):
        for _ in range(n):
            self.G.system.update()

    def goto(self, title):
        p = self.pages.index(title) + 1 if isinstance(title, str) else title
        prev, self.E.page = self.E.page, p
        self.G.page.onPageChange(prev, p)

    def turn(self, i, inc=1):
        self.G.controller.onEncoderTurn(self.lua.table_from({"index": i, "page": self.E.page, "increment": inc, "value": 0}))


GREEN = L.GREEN
knobs = L.encoders

show = E16(L.SHOW_SCENE)
check(show.E.rate == 50, "updates every 50 ms")
check(show.E.page == 1 and show.pages[0] == "KCK", "PERF opens on KCK, the kick's page")
kick, chord = knobs("KCK"), knobs("CRD")
show.move((kick[0], 9000))
check(show.ring(1) is None, f"KICK TUNE travelling: ring 1 stays the E16's own (knob 1 swaps to SLOW ALL; the script can't tell which) ({show.ring(1)})")
show.move((kick[1], 9000), (kick[9], 7000))
check(show.ring(2) == (9000, GREEN) and show.ring(10) == (7000, GREEN), f"KICK PUNCH and DENSITY travelling: rings 2 and 10 green ({show.rings()})")
show.move((chord[1], 4000), (chord[9], 4000))
check(show.rings() == {2: (9000, GREEN), 10: (7000, GREEN)}, "CHORD's knobs travelling while KCK is up: no ring touched (same places, another channel)")
check(all(kick[i].mixer for i in (12, 13)), "knobs 13-14 are the sends")
show.move(*[(c, 100) for c in kick if c.mixer])
check(show.ring(13) is None and show.ring(14) is None, "the sends (a Live control) never go green")

show.goto("CRD")
check(show.rings() == {}, "page change: every ring back to the knobs")
show.move((chord[1], 4100), (chord[9], 4100), (kick[1], 9100))
check(show.rings() == {2: (4100, GREEN), 10: (4100, GREEN)}, f"CRD: knobs 2 and 10 green, the kick's not here ({show.rings()})")

show.turn(10)
check(show.ring(10) is None, "knob 10 turned: its ring shows the hand")
show.move((chord[9], 4200))
check(show.ring(10) is None, "a report right after the turn: still the hand")
show.update(17)
show.move((chord[9], 4300), (chord[1], 4300))
check(show.ring(10) == (4300, GREEN), f"0.85 s later: green again ({show.ring(10)})")

show.update(10)
show.move((chord[1], 4400))
show.update(15)
check(show.ring(10) is None and show.ring(2) == (4400, GREEN), "1.25 s without a report: knob 10 back; knob 2, reported, stays")
show.arrive(chord[1])
check(show.ring(2) is None, "CHORD RES arrived: ring 2 back")

show.sysex(1, 0, 127, 127, 127)
check(show.rings() == {}, "an address the page doesn't have: nothing")
show.G.controller.onSysex(show.lua.table_from([0xF0, 0x41, 0x10, 0x00, 1, 0, 20, 1, 2, 0xF7]))
show.G.controller.onSysex(show.lua.table_from([0xF0, 0x7D, 0x53, 0x46, 1, 0xF7]))
show.G.controller.onSysex(show.lua.table_from([0xF0, 0x7D, 0x53, 0x46, 1, 0, 20, 0xF7]))
check(show.rings() == {}, "other SysEx, empty and short messages: ignored")

show.goto("NRG")
perform = knobs("NRG")
check([c.label for c in perform[:3]] == ["KICK ENERGY", "CLOSED HAT ENERGY", "OPEN HAT ENERGY"], "NRG: knob n is track n's ENERGY")
show.move(*[(c, 1000 + 100 * k) for k, c in enumerate(perform[:14])])
check(show.ring(1) is None and all(show.ring(1 + k) == (1000 + 100 * k, GREEN) for k in range(1, 14)),
      f"a full message of 14 ENERGYs: rings 2-14 green, knob 1 (it swaps) its own ({show.rings()})")
check(show.ring(15) is None and show.ring(16) is None, "NEXT SONG and BUILD, not in the message: untouched")
show.lua.execute("page.onInit()")
check(show.rings() == {}, "the script reloaded: every ring back")

mix = E16(L.MIX_SCENE)
check(mix.pages == ["LEVELS", "SPACE", "MACROS"], f"MIX: {mix.pages}")
mix.move(*[(c, 500) for c in knobs("LEVELS") if c.number is not None])
check(mix.rings() == {}, "LEVELS: volumes never go green")
mix.goto("SPACE")
space = knobs("SPACE")
mix.move(*[(c, 2000 + k) for k, c in enumerate(space[:8])])
check(all(mix.ring(1 + k) == (2000 + k, GREEN) for k in range(8)) and all(mix.ring(i) is None for i in range(9, 17)),
      f"SPACE: knobs 1-8 green, 9-16 (nothing on them) untouched ({mix.rings()})")
mix.goto("MACROS")
macros = knobs("MACROS")
slow_all = macros[6]
check(slow_all.label == "SLOW ALL" and slow_all.channel == L.SLOW_ALL.ctl.channel and slow_all.number == L.SLOW_ALL.ctl.number,
      "MACROS knob 7 is SLOW ALL, on the address of PERF's knob-1 swap")
mix.move(*[(c, 3000 + k) for k, c in enumerate(macros) if c is not None])
check(all(mix.ring(1 + k) == (3000 + k, GREEN) for k in range(7)) and mix.ring(9) == (3008, GREEN) and mix.ring(10) == (3009, GREEN),
      f"MACROS: MACRO 1-6, SLOW ALL (a plain knob here) and the REPEAT knobs green ({mix.rings()})")
check(all(mix.ring(i) is None for i in (8, 11, 12, 13, 14, 15, 16)), "knobs with nothing on them: untouched")
mix.goto(4)
mix.move(*[(c, 100) for c in macros if c is not None] + [(c, 100) for c in space[:8]])
check(mix.rings() == {}, "an empty page (P.4): nothing")

print(("PASS" if fails == 0 else "FAIL") + f": {fails} failure(s)")
sys.exit(1 if fails else 0)
