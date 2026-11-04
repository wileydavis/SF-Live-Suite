#!/usr/bin/env python3
"""Builds the OXI E16 scene "SF SHOW.oxie16" and the address map sf-show-map.csv from layout.py.

    python3 make_scene.py                    # the scene and the map, next to this script
    python3 make_scene.py --relative-shared  # turns that sit on more than one page send CC Rel1

The file follows a scene exported by the OXI App 2.x (title, icon, scene settings, 12 pages of 16
encoders, one push and two turn actions each), with every key in the App's own order. Turns are
CC Abs (type 3) on their own channel; the CC number is in nr1. Mode 14 is the App's default "Scene":
the knob speed follows the scene's acceleration setting. defaultValue is the knob's anchor.

Pushes follow a test scene exported by the OXI App on 3 Nov 2026 ("presd types"): type 2 = CC, 1 = Note,
9 = Swap Destination, 0 = Off; mode 1 = Press & Release. A CC push keeps its number in nr1, P.Val in
upper and R.Val in lower; a note push its note in nr1 and its velocity in nr2. Output: 0 = ALL (as the
turns and pages send), 15 = OFF (sends nothing). The swap push is the App's own, as exported. A swap's
second destination is turn action 2, with color2 as its ring colour, and mode 14 like turn action 1
(the App's default there; its default for turn action 2 is 3, no acceleration), so both feel the
same. An encoder with two destinations and no swap press would send both on every turn: check()
refuses one.

GLIDE RINGS: the scene carries glide_rings.lua (code: minified to one line, as the App uploads it;
fullScript: the source with CRLF), with the knob map filled in. The plugins' side of the same map is
Source/core/E16Map.h, written here too: every SF plugin knob of the scene, so a plugin can tell SF
CONTROL which of its travelling knobs sit where.
"""
import argparse
import copy
import csv
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import layout as L  # noqa: E402

# A blank scene as the OXI App 2.x exports it: the reference for every object written below.
REF_HEADER = {
    "title": "Scene", "icon": [0] * 32, "selectedPreset": 0, "color": 82, "transmitMode": 0, "pcOnEntry": 0,
    "bankOnEntry": 65535, "recentPage": 0, "recentPreset": 0, "transmitOnSceneEntry": 0,
    "transmitOnPageSwitch": 0, "transmitOnPresetLoad": 0, "smartTransmit": 15, "outputOnEntry": 0,
    "holdMode": 0, "acceleration": 3, "code": {"code": "", "fullScript": "", "scriptName": ""}, "pages": [],
}
REF_PAGE = {"title": "P.1", "output": 0, "channel": 1, "encoders": []}
REF_PUSH_OFF = {"instrument": 127, "parameter": 0, "type": 0, "display": 0, "mode": 0, "channel": 0, "lower": 0,
                "upper": 0, "nr1": 1, "nr2": 1, "output": 0, "scriptId": 0}   # the App's blank: nr1 = nr2 = encoder
REF_TURN_1 = {"instrument": 127, "parameter": 0, "type": 0, "display": 10, "mode": 14, "channel": 0, "lower": 0,
              "upper": 127, "defaultValue": 0, "nr1": 1, "nr2": 0, "output": 12, "scriptId": 0}
REF_TURN_2 = {"instrument": 127, "parameter": 0, "type": 0, "display": 10, "mode": 3, "channel": 0, "lower": 0,
              "upper": 127, "defaultValue": 0, "nr1": 0, "nr2": 0, "output": 12, "scriptId": 0}
REF_ENCODER = {"name": "", "abbr": "", "color": 0, "push_action": REF_PUSH_OFF,
               "turn_actions": [REF_TURN_1, REF_TURN_2], "color2": 0}

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SCRIPT = os.path.join(HERE, "glide_rings.lua")
SCRIPT_NAME = "sf_glide_rings"
CPP_MAP = os.path.join(ROOT, "Source", "core", "E16Map.h")
CC_ABS, CC_REL1 = 3, 1
PUSH_OFF, PUSH_NOTE, PUSH_CC, PUSH_SWAP = 0, 1, 2, 9
PRESS_AND_RELEASE = 1
OUT_ALL, OUT_OFF = 0, 15
# The App's own objects from the test scene (P.1: encoder 1 swap, 2 CC 21 ch 15, 3 note 25 ch 12).
REF_PUSH_CC = {"instrument": 127, "parameter": 0, "type": 2, "display": 0, "mode": 1, "channel": 15, "lower": 0,
               "upper": 127, "nr1": 21, "nr2": 2, "output": 0, "scriptId": 0}
REF_PUSH_NOTE = {"instrument": 127, "parameter": 0, "type": 1, "display": 0, "mode": 1, "channel": 12, "lower": 0,
                 "upper": 0, "nr1": 25, "nr2": 127, "output": 15, "scriptId": 0}      # output OFF there: ALL here
REF_PUSH_SWAP = {"instrument": 127, "parameter": 0, "type": 9, "display": 0, "mode": 0, "channel": 0, "lower": 0,
                 "upper": 0, "nr1": 17, "nr2": 11, "output": 15, "scriptId": 0}     # nr1, nr2: what the App left there
PAGE_OUTPUT = 0          # the App's default for a page ("ALL"): Live listens on E16 Port 2 only
TITLE = "SF SHOW"

# 16 x 16, one bit per pixel, two bytes per row, leftmost pixel in the top bit: a 4 x 4 grid of dots.
ICON = []
for r in range(16):
    ICON += [0b01100110, 0b01100110] if r % 4 in (1, 2) else [0, 0]


def turn(c, kind):
    t = copy.deepcopy(REF_TURN_1)
    t.update(type=kind, channel=c.channel, nr1=c.number, nr2=0, defaultValue=c.default)
    return t


def push_off(i):
    return dict(REF_PUSH_OFF, nr1=i, nr2=i)


def push(p, i):
    """The encoder's press: a HOLD is a CC (127 on press, 0 on release), a TOGGLE a note (velocity 127),
    both Press & Release to every output; a SWAP is the App's Swap Destination."""
    if p is None:
        return push_off(i)
    if p.mode == "HOLD":
        return dict(REF_PUSH_CC, channel=p.channel, nr1=p.number, nr2=i, lower=0, upper=127, output=OUT_ALL)
    if p.mode == "TOGGLE":
        return dict(REF_PUSH_NOTE, channel=p.channel, nr1=p.number, nr2=127, output=OUT_ALL)
    assert p.mode == "SWAP", p.mode
    return dict(REF_PUSH_SWAP)


def second_turn(c, kind):
    t = copy.deepcopy(REF_TURN_2)
    t.update(type=kind, mode=REF_TURN_1["mode"], channel=c.channel, nr1=c.number, nr2=0, defaultValue=c.default)
    return t


def encoder(c, kind, i, p=None, kind2=CC_ABS):
    e = copy.deepcopy(REF_ENCODER)
    e.update(name=c.name, abbr=c.abbr, color=c.colour, push_action=push(p, i))
    e["turn_actions"][0] = turn(c, kind)
    if p is not None and p.mode == "SWAP":
        e["turn_actions"][1] = second_turn(p.ctl, kind2)
        e["color2"] = p.ctl.colour
    return e


def glide_map():
    """Three characters per knob: channel - 1 (one hex digit), number (two), "---" for a knob that
    never travels (a Live control). Knobs 1-8, then knobs 9-16 of each page."""
    code = lambda c: f"{c.channel - 1:x}{c.number:02x}" if c.module else "---"
    return "".join(code(c) for c in L.TOP) + "".join(code(c) for _, bottom in L.PAGES for c in bottom)


def minify(src):
    """Comments and line breaks out, one line, as the App uploads a script. The script has double-quoted
    strings only (the map holds "---"), so a comment starts at a -- outside one."""
    out = []
    for line in src.splitlines():
        cut, quote = len(line), False
        for i, ch in enumerate(line):
            if ch == '"':
                quote = not quote
            elif not quote and line.startswith("--", i):
                cut = i
                break
        line = line[:cut].strip()
        if line:
            out.append(line)
    return " ".join(out) + "\n"


def script_source():
    src = open(SCRIPT).read()
    assert src.count("@MAP@") == 1 and src.count("@GREEN@") == 1
    return src.replace("@MAP@", glide_map()).replace("@GREEN@", str(L.GREEN))


def e16_knobs():
    """Every SF plugin parameter on the scene once: (track, module, parameter index, channel, number,
    top of Live's mapping in the parameter's unit or None)."""
    seen = {}
    ctls = list(L.TOP) + [c for _, bottom in L.PAGES for c in bottom] + \
        [p.ctl for p in list(L.PRESSES_ALL.values()) + list(L.PRESSES_PAGE.values()) if p.mode == "SWAP"]
    for c in ctls:
        if not c.module:
            continue
        k = (c.track, c.module, L.module_params(c.module).index(c.pid), c.channel, c.number, c.max_value)
        assert seen.get(c.address, k) == k, (c.address, seen[c.address], k)
        seen[c.address] = k
    return sorted(seen.values(), key=lambda k: (k[3], k[4]))


def write_cpp_map(path):
    rows = [f'    {{ "{t}", "{m}", {p}, {ch}, {n}, {mx if mx is not None else -1.0:.4f}f }},' for t, m, p, ch, n, mx in e16_knobs()]
    text = ("#pragma once\n"
            "// Generated by controllers/e16/make_scene.py from layout.py: do not edit. Every SF plugin knob of\n"
            "// the E16 scene SF SHOW: the Live track and SF plugin it lands on, the parameter's index, its E16\n"
            "// address, and the top of Live's mapping in the parameter's own unit (-1: the whole range). A\n"
            "// plugin finds its own knobs here, so SF CONTROL can show the travelling ones on the E16.\n"
            "namespace sf\n{\n"
            "struct E16Knob { const char* track; const char* module; int param, channel, number; float max; };\n"
            "inline constexpr E16Knob kE16Knobs[] = {\n" + "\n".join(rows) + "\n};\n}\n")
    with open(path, "w") as f:
        f.write(text)
    return len(rows)


def build(relative_shared):
    shared = {a for a, v in L.addresses().items() if v[2] == "TURN" and len({t for t, _ in v[1]}) > 1}
    scene = copy.deepcopy(REF_HEADER)
    src = script_source()
    scene.update(title=TITLE, icon=ICON, color=L.WHITE,
                 code={"code": minify(src), "fullScript": src.replace("\n", "\r\n"), "scriptName": SCRIPT_NAME})
    for title, bottom in L.PAGES:
        page = copy.deepcopy(REF_PAGE)
        page.update(title=title, output=PAGE_OUTPUT, channel=L.page_channel(bottom))
        kind = lambda c: CC_REL1 if relative_shared and c.address in shared else CC_ABS
        page["encoders"] = []
        for i, c in enumerate(L.encoders(title), 1):
            p = L.press_for(title, i)
            page["encoders"].append(encoder(c, kind(c), i, p, kind(p.ctl) if p is not None and p.mode == "SWAP" else CC_ABS))
        scene["pages"].append(page)
    return scene, shared


def same_shape(obj, ref, path="scene"):
    """Every object has the reference's keys, in the reference's order, with values of the same type."""
    if isinstance(ref, dict):
        assert isinstance(obj, dict) and list(obj) == list(ref), (path, list(obj), list(ref))
        for k in ref:
            if k == "pages":
                for i, p in enumerate(obj[k]): same_shape(p, REF_PAGE, f"{path}.pages[{i}]")
            elif k == "encoders":
                for i, e in enumerate(obj[k]): same_shape(e, REF_ENCODER, f"{path}.encoders[{i}]")
            else:
                same_shape(obj[k], ref[k], f"{path}.{k}")
    elif isinstance(ref, list) and path.endswith("turn_actions"):
        assert len(obj) == 2, path
        for i, t in enumerate(obj): same_shape(t, ref[i], f"{path}[{i}]")
    elif isinstance(ref, list):
        assert isinstance(obj, list) and all(type(v) is int for v in obj), path
    else:
        assert type(obj) is type(ref), (path, obj, ref)


def check(scene):
    same_shape(scene, REF_HEADER)
    code = scene["code"]
    assert code["scriptName"] == SCRIPT_NAME and len(code["code"].encode()) <= 7000, len(code["code"])
    assert "@" not in code["fullScript"] and len(glide_map()) == 3 * (8 + 8 * len(L.PAGES))
    printable = lambda s: all(32 <= ord(ch) < 127 for ch in s)
    assert 1 <= len(scene["title"]) <= 7 and printable(scene["title"])
    assert len(scene["icon"]) == 32 and all(0 <= b <= 255 for b in scene["icon"])
    assert len(scene["pages"]) == 12
    for page in scene["pages"]:
        title = page["title"]
        assert 1 <= len(title) <= 11 and printable(title), title
        assert 1 <= page["channel"] <= 16 and len(page["encoders"]) == 16
        for i, e in enumerate(page["encoders"], 1):
            assert 1 <= len(e["name"]) <= 8 and 1 <= len(e["abbr"]) <= 4, e["name"]
            assert printable(e["name"]) and printable(e["abbr"]) and 0 <= e["color"] <= 99
            t, t2, pa = e["turn_actions"][0], e["turn_actions"][1], e["push_action"]
            assert t["type"] in (CC_ABS, CC_REL1) and 1 <= t["channel"] <= 16 and 0 <= t["nr1"] <= 119
            assert pa == push(L.press_for(title, i), i), (title, i, pa)
            if pa["type"] == PUSH_SWAP:
                # A second destination only with the swap that keeps it apart from the first.
                assert t2["type"] in (CC_ABS, CC_REL1) and 1 <= t2["channel"] <= 16 and 0 <= t2["nr1"] <= 119
                assert (t2["channel"], t2["nr1"]) != (t["channel"], t["nr1"]) and 0 <= e["color2"] <= 99
            else:
                assert pa["type"] in (PUSH_OFF, PUSH_CC, PUSH_NOTE), (title, i, pa["type"])
                assert t2 == REF_TURN_2 and e["color2"] == 0, (title, i)
            if pa["type"] in (PUSH_CC, PUSH_NOTE):
                assert pa["mode"] == PRESS_AND_RELEASE and pa["output"] == OUT_ALL and 1 <= pa["channel"] <= 16
            if pa["type"] == PUSH_CC:
                assert (pa["upper"], pa["lower"]) == (127, 0) and 0 <= pa["nr1"] <= 119
            if pa["type"] == PUSH_NOTE:
                assert pa["nr2"] == 127 and 0 <= pa["nr1"] <= 127


def write_map(path, relative, shared):
    rows = []
    for (msg, ch, n), (target, places, kind, rng, item) in L.addresses().items():
        if kind == "TURN":
            how = "relative: Twos Complement" if relative and (msg, ch, n) in shared else "absolute"
        elif kind == "HOLD":
            how = "press: 127 on press, 0 on release"
        else:
            how = "press: every Note On flips it"
        num = f"{n} ({L.note_name(n)})" if msg == "NOTE" else str(n)
        rows.append((ch, msg, n, [str(ch), msg, num, kind, target, how, rng, L.where(places, getattr(item, "second", False))]))
    rows.sort(key=lambda r: r[:3])
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["CHANNEL", "MESSAGE", "NUMBER", "KIND", "LIVE TARGET", "LIVE MAPPING", "RANGE", "ON THE E16"])
        for r in rows: w.writerow(r[3])
    return len(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--relative-shared", action="store_true",
                    help="turns on more than one page send CC Rel1; map them in Live as relative (Twos Complement)")
    ap.add_argument("--out", default=HERE, help="folder for the scene and the map (default: next to this script)")
    a = ap.parse_args()
    scene, shared = build(a.relative_shared)
    check(scene)
    path = os.path.join(a.out, TITLE + ".oxie16")
    with open(path, "w", encoding="ascii") as f:
        f.write(json.dumps(scene, separators=(",", ":")))
    n = write_map(os.path.join(a.out, "sf-show-map.csv"), a.relative_shared, shared)
    k = write_cpp_map(CPP_MAP)
    print(f"{os.path.relpath(CPP_MAP, ROOT)}: {k} SF plugin knobs; script {len(scene['code']['code'])} bytes")
    print(f"{path}: 12 pages, {sum(len(p['encoders']) for p in scene['pages'])} encoders")
    print(f"sf-show-map.csv: {n} addresses; {len(shared)} turns on more than one page"
          + (" (sent as CC Rel1)" if a.relative_shared else ""))
    print("Presses in the scene:")
    for title, _ in L.PAGES:
        for i in range(1, 17):
            p = L.press_for(title, i)
            if p and (i not in L.PRESSES_ALL or title == L.PAGES[0][0]):
                where = "every page" if i in L.PRESSES_ALL and (title, i) not in L.PRESSES_PAGE else title
                print(f"  {where:<11} encoder {i:<2}  {p.name:<8} {p.mode:<6}  {p.message()}")


if __name__ == "__main__":
    main()
