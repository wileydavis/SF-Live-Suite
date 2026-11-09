#!/usr/bin/env python3
"""Builds the OXI E16 scenes "PERF.oxie16" and "MIX.oxie16" and the address map sf-show-map.csv (both
scenes) from layout.py.

    python3 make_scene.py                    # the scenes and the map, next to this script
    python3 make_scene.py --relative-shared  # turns that sit on more than one page send CC Rel1

Each file follows a scene exported by the OXI App 2.x (title, icon, scene settings, 12 pages of 16
encoders, one push and two turn actions each), with every key in the App's own order. Turns are CC Abs
(type 3) on their own channel; the CC number is in nr1. Mode 14 is the App's default "Scene": the knob
speed follows the scene's acceleration setting. defaultValue is the knob's anchor.

An encoder with nothing on it is named "-" and its turn and press are Off (type 0), so it sends
nothing: the App's own new encoder sends CC n on its page's channel, which would land on a track's
knobs here. A scene with fewer than 12 pages (MIX) is filled with pages of them, titled and
channelled as the App numbers a new page (P.4 on channel 4, ...). Names, page titles and ring
colours follow the PERF scene the performer edited in the App on Nov 8 (layout.py: CODE, the colours).

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
PAGE_OUTPUT = 0          # the App's default for a page ("ALL"): Live listens on E16 Port 1 only
TITLES = [s for s, _ in L.SCENES]

def icon(lit):
    """16 x 16, one bit per pixel, two bytes per row, leftmost pixel in the top bit."""
    out = []
    for r in range(16):
        row = sum(1 << (15 - c) for c in range(16) if lit(r, c))
        out += [row >> 8, row & 0xFF]
    return out


ICONS = {L.SHOW_SCENE: icon(lambda r, c: r % 4 in (1, 2) and c % 4 in (1, 2)),     # a 4 x 4 grid of dots
         L.MIX_SCENE: icon(lambda r, c: any(c in (x, x + 1) and 14 - h < r <= 14      # three level bars
                                            for x, h in ((2, 8), (7, 12), (12, 5))))}


def turn(c, kind):
    t = copy.deepcopy(REF_TURN_1)
    t.update(type=kind, channel=c.channel, nr1=c.number, nr2=0, defaultValue=c.default)
    return t


def push_off(i):
    return dict(REF_PUSH_OFF, nr1=i, nr2=i)


def push(p, i):
    """The encoder's press: a HOLD is a CC (127 on press, 0 on release), a TOGGLE a note (velocity 127),
    a CENTRE a CC of 64 on press and on release, on its knob's own address (Live sets the parameter to
    its middle, and sends the knob that value back), all Press & Release to every output; a SWAP is the
    App's Swap Destination."""
    if p is None:
        return push_off(i)
    if p.mode == "HOLD":
        return dict(REF_PUSH_CC, channel=p.channel, nr1=p.number, nr2=i, lower=0, upper=127, output=OUT_ALL)
    if p.mode == "CENTRE":
        return dict(REF_PUSH_CC, channel=p.channel, nr1=p.number, nr2=i, lower=64, upper=64, output=OUT_ALL)
    if p.mode == "TOGGLE":
        return dict(REF_PUSH_NOTE, channel=p.channel, nr1=p.number, nr2=127, output=OUT_ALL)
    assert p.mode == "SWAP", p.mode
    return dict(REF_PUSH_SWAP)


def second_turn(c, kind):
    t = copy.deepcopy(REF_TURN_2)
    t.update(type=kind, mode=REF_TURN_1["mode"], channel=c.channel, nr1=c.number, nr2=0, defaultValue=c.default)
    return t


def blank_encoder(i):
    """An encoder with nothing on it: "-", its turn and its press Off."""
    e = copy.deepcopy(REF_ENCODER)
    e.update(name="-", abbr="-", color=L.WHITE, push_action=push_off(i))
    e["turn_actions"][0] = dict(REF_TURN_1, nr1=i)
    return e


def encoder(c, kind, i, p=None, kind2=CC_ABS):
    if c is None:
        assert p is None, i
        return blank_encoder(i)
    e = copy.deepcopy(REF_ENCODER)
    e.update(name=c.name, abbr=c.abbr, color=c.colour, push_action=push(p, i))
    e["turn_actions"][0] = turn(c, kind)
    if p is not None and p.mode == "SWAP":
        e["turn_actions"][1] = second_turn(p.ctl, kind2)
        e["color2"] = p.ctl.colour
    return e


def glide_map(scene):
    """Three characters per knob: channel - 1 (one hex digit), number (two), "---" for a knob that
    never travels (a Live control, nothing) or that swaps to a second destination (knob 1 of a PERF
    page / SLOW ALL): the script can't tell which one it is on, so the E16 shows that knob's own value,
    always. Knobs 1-16 of each of the scene's 12 pages."""
    def code(c, title, i):
        p = L.press_for(title, i)
        if c is None or not c.module or (p is not None and p.mode == "SWAP"):
            return "---"
        return f"{c.channel - 1:x}{c.number:02x}"
    pages = dict(L.SCENES)[scene]
    out = "".join(code(c, title, i) for title, controls in pages for i, c in enumerate(controls, 1))
    return out + "---" * 16 * (12 - len(pages))


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


def script_source(scene):
    src = open(SCRIPT).read()
    assert src.count("@MAP@") == 1 and src.count("@GREEN@") == 1
    return src.replace("@MAP@", glide_map(scene)).replace("@GREEN@", str(L.GREEN))


def e16_knobs():
    """Every SF plugin parameter on the scene once: (track, module, parameter index, channel, number,
    top of Live's mapping in the parameter's unit or None)."""
    seen = {}
    ctls = [c for _, controls in L.PAGES for c in controls] + \
        [p.ctl for p in list(L.PRESSES_ALL.values()) + list(L.PRESSES_PAGE.values()) if p.mode == "SWAP"]
    for c in ctls:
        if c is None or not c.module:
            continue
        k = (c.track, c.module, L.module_params(c.module).index(c.pid), c.channel, c.number, c.max_value)
        assert seen.get(c.address, k) == k, (c.address, seen[c.address], k)
        seen[c.address] = k
    rows = list(seen.values())
    for t, m, p, ch, n, mx in list(rows):           # a stand-in (TERROIR for REVERB) finds the same knobs
        pid = L.module_params(m)[p]
        rows += [(t, alt, p, ch, n, mx) for alt in L.alternates(m, pid)]
    return sorted(rows, key=lambda k: (k[3], k[4], k[1]))


def cpp_str(x):
    return "nullptr" if x is None else '"' + x.replace('"', '\\"') + '"'


def cpp_turn(c):
    """{ abbr, name, track, modules ("REVERB|TERROIR": the plugin and its stand-ins), param, colour
    (the App's colour number), max }: one turn of an encoder, as the plugins draw it."""
    if c is None:
        return "{ nullptr, nullptr, nullptr, nullptr, -1, 0, -1.0000f }"
    if c.module:
        mods, param = "|".join([c.module] + L.alternates(c.module, c.pid)), L.module_params(c.module).index(c.pid)
    else:
        mods, param = "", -1
    colour = c.colour
    mx = c.max_value if (c.module and c.max_value is not None) else -1.0
    return (f"{{ {cpp_str(c.abbr)}, {cpp_str(c.label)}, {cpp_str(c.track)}, {cpp_str(mods)}, {param}, {colour}, "
            f"{mx:.4f}f }}")


def cpp_press(p):
    """{ name, track, modules (with stand-ins), param, mode (1 hold, 2 toggle) }: a press that sends a
    parameter of its own (a swap sends nothing; a centre press moves its knob's own parameter)."""
    if p is None or p.mode in ("SWAP", "CENTRE"):
        return "{ nullptr, nullptr, nullptr, -1, 0 }"
    mods = "|".join([p.module] + L.alternates(p.module, p.pid))
    return (f"{{ {cpp_str(p.name)}, {cpp_str(p.track)}, {cpp_str(mods)}, "
            f"{L.module_params(p.module).index(p.pid)}, {1 if p.mode == 'HOLD' else 2} }}")


def write_cpp_map(path):
    rows = [f'    {{ "{t}", "{m}", {p}, {ch}, {n}, {mx if mx is not None else -1.0:.4f}f }},' for t, m, p, ch, n, mx in e16_knobs()]
    pages = []
    for title, _ in L.PAGES:
        cells = []
        for i, c in enumerate(L.encoders(title), 1):
            p = L.press_for(title, i)
            swap = p.ctl if (p is not None and p.mode == "SWAP") else None
            cells.append(f"        {{ {cpp_turn(c)}, {cpp_turn(swap)}, {cpp_press(p)} }},")
        pages.append(f'    {{ "{L.scene_of(title)}", "{title}", {{\n' + "\n".join(cells) + "\n    } },")
    text = ("#pragma once\n"
            "// Generated by controllers/e16/make_scene.py from layout.py: do not edit. Every SF plugin knob of\n"
            "// the E16 scenes PERF and MIX: the Live track and SF plugin it lands on, the parameter's index, its E16\n"
            "// address, and the top of Live's mapping in the parameter's own unit (-1: the whole range). A\n"
            "// plugin finds its own knobs here, so SF CONTROL can show the travelling ones on the E16.\n"
            "namespace sf\n{\n"
            "struct E16Knob { const char* track; const char* module; int param, channel, number; float max; };\n"
            "inline constexpr E16Knob kE16Knobs[] = {\n" + "\n".join(rows) + "\n};\n\n"
            "// The scenes page by page, as the plugins draw them (the editor's E16 grid): each page's scene, name\n"
            "// and 16 encoders. A turn: its E16 abbreviation, its name, the Live track and the SF\n"
            "// plugins it lands on (several: stand-ins with the same parameter in the same place), the\n"
            "// parameter's index (-1: a Live mixer control), the ring's colour (the App's colour number: 34 white,\n"
            "// 18 light blue, 90 green, ...)\n"
            "// and the top of Live's mapping. second: the turn its press swaps to; press: what the press sends.\n"
            "struct E16Turn { const char* abbr; const char* name; const char* track; const char* modules; int param, colour; float max; };\n"
            "struct E16Press { const char* name; const char* track; const char* modules; int param, mode; };\n"
            "struct E16Cell { E16Turn turn, second; E16Press press; };\n"
            "struct E16Page { const char* scene; const char* name; E16Cell cells[16]; };\n"
            f"inline constexpr const char* kE16ShowScene = \"{L.SHOW_SCENE}\";   // the scene with a page per track\n"
            "inline constexpr E16Page kE16Pages[] = {\n" + "\n".join(pages) + "\n};\n}\n")
    with open(path, "w") as f:
        f.write(text)
    return len(rows)


def build(scene_title, relative_shared):
    shared = {a for a, v in L.addresses().items() if v[2] == "TURN" and len({t for t, _ in v[1]}) > 1}
    scene = copy.deepcopy(REF_HEADER)
    src = script_source(scene_title)
    scene.update(title=scene_title, icon=ICONS[scene_title], color=L.WHITE,
                 code={"code": minify(src), "fullScript": src.replace("\n", "\r\n"), "scriptName": SCRIPT_NAME})
    pages = dict(L.SCENES)[scene_title]
    for title, controls in pages:
        page = copy.deepcopy(REF_PAGE)
        page.update(title=title, output=PAGE_OUTPUT, channel=L.page_channel(controls))
        kind = lambda c: CC_REL1 if relative_shared and c is not None and c.address in shared else CC_ABS
        page["encoders"] = []
        for i, c in enumerate(controls, 1):
            p = L.press_for(title, i)
            page["encoders"].append(encoder(c, kind(c), i, p, kind(p.ctl) if p is not None and p.mode == "SWAP" else CC_ABS))
        scene["pages"].append(page)
    for k in range(len(pages) + 1, 13):                           # pages with nothing on them
        page = copy.deepcopy(REF_PAGE)
        page.update(title=f"P.{k}", output=PAGE_OUTPUT, channel=k)
        page["encoders"] = [blank_encoder(i) for i in range(1, 17)]
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
    assert "@" not in code["fullScript"] and len(glide_map(scene["title"])) == 3 * 16 * 12
    printable = lambda s: all(32 <= ord(ch) < 127 for ch in s)
    assert 1 <= len(scene["title"]) <= 7 and printable(scene["title"])
    assert len(scene["icon"]) == 32 and all(0 <= b <= 255 for b in scene["icon"])
    assert len(scene["pages"]) == 12
    used = [t for t, _ in dict(L.SCENES)[scene["title"]]]
    for page in scene["pages"]:
        title = page["title"]
        assert 1 <= len(title) <= 11 and printable(title), title
        assert 1 <= page["channel"] <= 16 and len(page["encoders"]) == 16
        for i, e in enumerate(page["encoders"], 1):
            if title not in used or L.encoders(title)[i - 1] is None:
                assert e == blank_encoder(i) and e["turn_actions"][0]["type"] == 0, (title, i)  # sends nothing
                continue
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
                assert (pa["upper"], pa["lower"]) in ((127, 0), (64, 64)) and 0 <= pa["nr1"] <= 119
                if (pa["upper"], pa["lower"]) == (64, 64):           # a centre press: its own knob's address
                    assert (pa["channel"], pa["nr1"]) == (t["channel"], t["nr1"]), (title, i)
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
        scenes = ", ".join(t for t in TITLES if any(L.scene_of(p) == t for p, _ in places))
        rows.append((ch, msg, n, [str(ch), msg, num, kind, target, how, rng, scenes, L.where(places, getattr(item, "second", False))]))
    rows.sort(key=lambda r: r[:3])
    with open(path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["CHANNEL", "MESSAGE", "NUMBER", "KIND", "LIVE TARGET", "LIVE MAPPING", "RANGE", "SCENE", "ON THE E16"])
        for r in rows: w.writerow(r[3])
    return len(rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--relative-shared", action="store_true",
                    help="turns on more than one page send CC Rel1; map them in Live as relative (Twos Complement)")
    ap.add_argument("--out", default=HERE, help="folder for the scene and the map (default: next to this script)")
    a = ap.parse_args()
    for title in TITLES:
        scene, shared = build(title, a.relative_shared)
        check(scene)
        path = os.path.join(a.out, title + ".oxie16")
        with open(path, "w", encoding="ascii") as f:
            f.write(json.dumps(scene, separators=(",", ":")))
        used = len(dict(L.SCENES)[title])
        print(f"{path}: {used} pages of 12 used, script {len(scene['code']['code'])} bytes")
    n = write_map(os.path.join(a.out, "sf-show-map.csv"), a.relative_shared, shared)
    k = write_cpp_map(CPP_MAP)
    print(f"{os.path.relpath(CPP_MAP, ROOT)}: {k} SF plugin knobs, {len(L.PAGES)} pages")
    print(f"sf-show-map.csv: {n} addresses (both scenes); {len(shared)} turns on more than one page"
          + (" (sent as CC Rel1)" if a.relative_shared else ""))
    print("Presses:")
    show = [t for t, _ in L.SHOW_PAGES]
    same = lambda i: len({(q.mode, q.name, q.message()) for q in (L.press_for(t, i) for t in show) if q}) == 1 \
        and all(L.press_for(t, i) for t in show)
    for title, _ in L.PAGES:
        for i in range(1, 17):
            p = L.press_for(title, i)
            if p is None or (title in show and same(i) and title != show[0]):
                continue
            where = f"every {L.SHOW_SCENE} page" if title in show and same(i) else title
            print(f"  {where:<18} encoder {i:<2}  {p.name:<13} {p.mode:<6}  {p.message()}")


if __name__ == "__main__":
    main()
