#!/usr/bin/env python3
"""Builds the OXI E16 scene "SF SHOW.oxie16" and the address map sf-show-map.csv from layout.py.

    python3 make_scene.py                    # the scene and the map, next to this script
    python3 make_scene.py --relative-shared  # turns that sit on more than one page send CC Rel1

The file follows a scene exported by the OXI App 2.x (title, icon, scene settings, 12 pages of 16
encoders, one push and two turn actions each), with every key in the App's own order. Turns are
CC Abs (type 3) on their own channel; the CC number is in nr1. Mode 14 is the App's default "Scene":
the knob speed follows the scene's acceleration setting. defaultValue is the knob's anchor. Pushes are
left Off, exactly as the App writes a blank one, until a scene exported with real pushes shows how the
App stores CC and note pushes.
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

CC_ABS, CC_REL1 = 3, 1
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


def encoder(c, kind, i):
    e = copy.deepcopy(REF_ENCODER)
    e.update(name=c.name, abbr=c.abbr, color=c.colour, push_action=push_off(i))
    e["turn_actions"][0] = turn(c, kind)
    return e


def build(relative_shared):
    shared = {a for a, v in L.addresses().items() if v[2] == "TURN" and len({t for t, _ in v[1]}) > 1}
    scene = copy.deepcopy(REF_HEADER)
    scene.update(title=TITLE, icon=ICON, color=L.WHITE)
    for title, bottom in L.PAGES:
        page = copy.deepcopy(REF_PAGE)
        page.update(title=title, output=PAGE_OUTPUT, channel=L.page_channel(bottom))
        page["encoders"] = [encoder(c, CC_REL1 if relative_shared and c.address in shared else CC_ABS, i)
                            for i, c in enumerate(L.encoders(title), 1)]
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
    printable = lambda s: all(32 <= ord(ch) < 127 for ch in s)
    assert 1 <= len(scene["title"]) <= 7 and printable(scene["title"])
    assert len(scene["icon"]) == 32 and all(0 <= b <= 255 for b in scene["icon"])
    assert len(scene["pages"]) == 12
    for p in scene["pages"]:
        assert 1 <= len(p["title"]) <= 11 and printable(p["title"]), p["title"]
        assert 1 <= p["channel"] <= 16 and len(p["encoders"]) == 16
        for i, e in enumerate(p["encoders"], 1):
            assert 1 <= len(e["name"]) <= 8 and 1 <= len(e["abbr"]) <= 4, e["name"]
            assert printable(e["name"]) and printable(e["abbr"]) and 0 <= e["color"] <= 99
            t = e["turn_actions"][0]
            assert t["type"] in (CC_ABS, CC_REL1) and 1 <= t["channel"] <= 16 and 0 <= t["nr1"] <= 119
            assert e["push_action"] == push_off(i) and e["turn_actions"][1] == REF_TURN_2


def write_map(path, relative, shared):
    rows = []
    for (msg, ch, n), (target, places, kind, rng, _) in L.addresses().items():
        if kind == "TURN":
            how = "relative: Twos Complement" if relative and (msg, ch, n) in shared else "absolute"
        elif kind == "HOLD":
            how = "press: 127 on press, 0 on release"
        else:
            how = "press: every Note On flips it"
        num = f"{n} ({L.note_name(n)})" if msg == "NOTE" else str(n)
        rows.append((ch, msg, n, [str(ch), msg, num, kind, target, how, rng, L.where(places)]))
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
    print(f"{path}: 12 pages, {sum(len(p['encoders']) for p in scene['pages'])} encoders")
    print(f"sf-show-map.csv: {n} addresses; {len(shared)} turns on more than one page"
          + (" (sent as CC Rel1)" if a.relative_shared else ""))
    print("Presses to add in the OXI App:")
    for title, _ in L.PAGES:
        for i in range(1, 17):
            p = L.press_for(title, i)
            if p and (i not in L.PRESSES_ALL or title == L.PAGES[0][0]):
                where = "every page" if i in L.PRESSES_ALL and (title, i) not in L.PRESSES_PAGE else title
                print(f"  {where:<11} encoder {i:<2}  {p.name:<7} {p.mode:<6}  {p.message()}")


if __name__ == "__main__":
    main()
