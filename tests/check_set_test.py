#!/usr/bin/env python3
"""Tests controllers/live/check_set.py, the mapping check for a Live set: SF SHOW.als passes; a copy
with one fault each (an address mapped twice, a range that tops out low, a range that starts high, a
turn mapped relative, a mapping taken away) fails with exit status 1 and names the fault.

    python3 tests/check_set_test.py
"""
import gzip
import os
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SET = os.path.join(ROOT, "controllers", "live", "SF SHOW.als")
CHECK = os.environ.get("CHECK_SET", os.path.join(ROOT, "controllers", "live", "check_set.py"))   # CHECK_SET: test another copy
fails = 0


def check(ok, msg):
    global fails
    print(("ok   " if ok else "FAIL ") + msg)
    fails += 0 if ok else 1


def run(path, *extra):
    r = subprocess.run([sys.executable, "-I", CHECK, path, *extra], capture_output=True, text=True)
    return r.returncode, r.stdout


def keymidis(root):
    parent = {c: p for p in root.iter() for c in p}
    out = []
    for k in root.iter("KeyMidi"):
        v = {c.tag: c for c in k}
        if v["PersistentKeyString"].get("Value"):
            continue
        out.append((k, v, parent[k]))
    return out, parent


def mutated(name, change):
    root = ET.fromstring(gzip.decompress(open(SET, "rb").read()))
    change(root)
    path = os.path.join(tmp, name + ".als")
    with open(path, "wb") as f:
        f.write(gzip.compress(ET.tostring(root)))
    return path


def by_address(root, ch, num):
    rows, parent = keymidis(root)
    for k, v, owner in rows:
        if v["IsNote"].get("Value") == "false" and int(v["Channel"].get("Value")) + 1 == ch and int(v["NoteOrController"].get("Value")) == num:
            return k, v, owner, parent
    raise KeyError((ch, num))


def twice(root):
    # KICK TUNE's address (CC 20 ch 1) also on KICK DECAY.
    _, _, _, parent = by_address(root, 1, 20)
    k21, v21, owner21, _ = by_address(root, 1, 21)
    v21["NoteOrController"].set("Value", "20")


def low_top(root):
    _, _, owner, _ = by_address(root, 1, 20)
    owner.find("MidiControllerRange/Max").set("Value", "0.5")


def high_bottom(root):
    _, _, owner, _ = by_address(root, 1, 20)
    owner.find("MidiControllerRange/Min").set("Value", "0.2")


def relative(root):
    _, v, _, _ = by_address(root, 1, 20)
    v["ControllerMapMode"].set("Value", "3")


def removed(root):
    k, _, owner, _ = by_address(root, 1, 20)
    owner.remove(k)


with tempfile.TemporaryDirectory() as tmp:
    code, out = run(SET)
    check(code == 0 and "108 of 108" in out, f"SF SHOW.als: exit {code}, every address mapped")
    for name, change, words in (("twice", twice, "MAPPED TWICE"), ("low top", low_top, "max 0.500"),
                                ("high bottom", high_bottom, "min 0.2"), ("relative", relative, "MODE"),
                                ("removed", removed, "MISSING")):
        code, out = run(mutated(name.replace(" ", "_"), change))
        check(code == 1 and words in out, f"{name}: exit {code}, says {words!r}")
    code, out = run(mutated("relative_shared", relative), "--relative-shared")
    check(code == 1 and "MODE" in out, "--relative-shared: a turn on one page (KICK TUNE) mapped relative still fails")

print(("PASS" if fails == 0 else "FAIL") + f": {fails} failure(s)")
sys.exit(1 if fails else 0)
