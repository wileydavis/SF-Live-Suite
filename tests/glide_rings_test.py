#!/usr/bin/env python3
"""Tests the E16 scene's GLIDE RINGS script as shipped (the minified code inside SF SHOW.oxie16)
against a mock of the E16's Lua API (1-based encoders and pages, 14-bit ring values, leds.reset
hands a ring back to the firmware). Needs lupa (pip install lupa): Lua 5.4.

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


scene = json.load(open(os.path.join(ROOT, "controllers", "e16", "SF SHOW.oxie16")))
code = scene["code"]["code"]
check(code.count("\n") == 1 and code.endswith("\n"), f"the script is one line, {len(code)} bytes")

lua = lua54.LuaRuntime(unpack_returned_tuples=True)
lua.execute("""
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
""")
lua.execute(code)
G = lua.globals()
E = G.E


def ring(i):
    r = E.rings[i]
    return None if r is None else (r.v, r.c)


def sysex(*data):
    return lua.table_from([0xF0, 0x7D, 0x53, 0x46] + list(data) + [0xF7])


def move(*knobs):
    """knobs: (channel, number, value)"""
    out = [1]
    for ch, num, v in knobs:
        out += [ch - 1, num, v >> 7, v & 127]
    G.controller.onSysex(sysex(*out))


def arrive(*knobs):
    out = [2]
    for ch, num in knobs:
        out += [ch - 1, num]
    G.controller.onSysex(sysex(*out))


def update(n=1):
    for _ in range(n):
        G.system.update()


def goto(p):
    prev, E.page = E.page, p
    G.page.onPageChange(prev, p)


def turn(i, inc=1):
    G.controller.onEncoderTurn(lua.table_from({"index": i, "page": E.page, "increment": inc, "value": 0}))


G.page.onInit()
check(E.rate == 50, "updates every 50 ms")
pages = [t for t, _ in L.PAGES]
low = L.TOP[0]                                        # LOW ENERGY, knob 1 on every page
kick_tune = dict(L.PAGES)["KICK"][0]                  # KICK page, knob 9
GREEN = L.GREEN

move((low.channel, low.number, 9000))
check(ring(1) == (9000, GREEN), f"LOW ENERGY travelling: ring 1 green at 9000 ({ring(1)})")
move((kick_tune.channel, kick_tune.number, 4000))
check(all(ring(i) is None for i in range(2, 17)), "KICK TUNE travelling while PERFORM is up: no other ring touched")

goto(pages.index("KICK") + 1)
check(all(ring(i) is None for i in range(1, 17)), "page change: every ring back to the knobs")
move((kick_tune.channel, kick_tune.number, 4100), (low.channel, low.number, 9100))
check(ring(9) == (4100, GREEN) and ring(1) == (9100, GREEN), f"KICK page: knob 9 and knob 1 green ({ring(9)}, {ring(1)})")

turn(9)
check(ring(9) is None, "knob 9 turned: its ring shows the hand")
move((kick_tune.channel, kick_tune.number, 4200))
check(ring(9) is None, "a report right after the turn: still the hand")
update(17)
move((kick_tune.channel, kick_tune.number, 4300), (low.channel, low.number, 9300))
check(ring(9) == (4300, GREEN), f"0.85 s later: green again ({ring(9)})")

update(10)
move((low.channel, low.number, 9400))
update(15)
check(ring(9) is None and ring(1) == (9400, GREEN), "1.25 s without a report: knob 9 back; knob 1, reported, stays")
arrive((low.channel, low.number))
check(ring(1) is None, "LOW ENERGY arrived: ring 1 back")

move((1, 127, 16383))
check(all(ring(i) is None for i in range(1, 17)), "an address the page doesn't have: nothing")
G.controller.onSysex(lua.table_from([0xF0, 0x41, 0x10, 0x00, 1, 0, 20, 1, 2, 0xF7]))
G.controller.onSysex(lua.table_from([0xF0, 0x7D, 0x53, 0x46, 1, 0xF7]))
G.controller.onSysex(lua.table_from([0xF0, 0x7D, 0x53, 0x46, 1, 0, 20, 0xF7]))
check(all(ring(i) is None for i in range(1, 17)), "other SysEx, empty and short messages: ignored")

goto(pages.index("MACROS") + 1)
macros = dict(L.PAGES)["MACROS"]
move(*[(c.channel, c.number, 1000 + 100 * k) for k, c in enumerate(macros[:6])] +
     [(L.TOP[k].channel, L.TOP[k].number, 2000 + k) for k in range(4)] +
     [(kick_tune.channel, kick_tune.number, 5)] * 4)
check(all(ring(9 + k) == (1000 + 100 * k, GREEN) for k in range(6)) and all(ring(1 + k) == (2000 + k, GREEN) for k in range(4)),
      "a full message of 14: six macros and the four top knobs green")
check(all(ring(i) is None for i in (5, 6, 7, 8, 15, 16)), "volumes and the REPEAT knobs untouched")

lua.execute("page.onInit()")
check(all(ring(i) is None for i in range(1, 17)), "the script reloaded: every ring back")

print(("PASS" if fails == 0 else "FAIL") + f": {fails} failure(s)")
sys.exit(1 if fails else 0)
