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
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
import layout as L  # noqa: E402

SET = os.path.join(ROOT, "controllers", "live", "SF SHOW.als")
# KICK TUNE (1 KICK, knob 1) and the knob after it, KICK PUNCH: turns on one page each.
TUNE, PUNCH = (L.encoders("1 KICK")[k] for k in (0, 1))
assert (TUNE.pid, PUNCH.pid) == ("tune", "punch") and TUNE.channel == PUNCH.channel
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
    # KICK TUNE's address also on KICK PUNCH.
    _, v, _, _ = by_address(root, PUNCH.channel, PUNCH.number)
    v["NoteOrController"].set("Value", str(TUNE.number))


def low_top(root):
    _, _, owner, _ = by_address(root, TUNE.channel, TUNE.number)
    owner.find("MidiControllerRange/Max").set("Value", "0.5")


def high_bottom(root):
    _, _, owner, _ = by_address(root, TUNE.channel, TUNE.number)
    owner.find("MidiControllerRange/Min").set("Value", "0.2")


def relative(root):
    _, v, _, _ = by_address(root, TUNE.channel, TUNE.number)
    v["ControllerMapMode"].set("Value", "3")


def removed(root):
    k, _, owner, _ = by_address(root, TUNE.channel, TUNE.number)
    owner.remove(k)


with tempfile.TemporaryDirectory() as tmp:
    code, out = run(SET)
    n = len(L.addresses())
    check(code == 0 and f"{n} of {n}" in out, f"SF SHOW.als: exit {code}, every address mapped ({n})")
    for name, change, words in (("twice", twice, "MAPPED TWICE"), ("low top", low_top, "max 0.500"),
                                ("high bottom", high_bottom, "min 0.2"), ("relative", relative, "MODE"),
                                ("removed", removed, "MISSING")):
        code, out = run(mutated(name.replace(" ", "_"), change))
        check(code == 1 and words in out, f"{name}: exit {code}, says {words!r}")
    code, out = run(mutated("relative_shared", relative), "--relative-shared")
    check(code == 1 and "MODE" in out, "--relative-shared: a turn on one page (KICK TUNE) mapped relative still fails")

print(("PASS" if fails == 0 else "FAIL") + f": {fails} failure(s)")
sys.exit(1 if fails else 0)
