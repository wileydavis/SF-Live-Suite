#!/usr/bin/env python3
"""Checks a Live set's mappings against the E16 scene SF SHOW: every address in
controllers/e16/layout.py mapped once, to the right parameter on the right track, with the right range;
and any computer keys mapped. Reads the set, never writes it.

    python3 check_set.py "My Show.als" [--relative-shared]

Use it on a set of your own (one saved before a mapping existed, or mapped by hand): it lists what to
map with Cmd-M. SF SHOW.als, the built set, passes with every address mapped and no keys.

A target is found by track name (as in the show template: "01 KICK", "A DUB") and the plugin's name
("SF KICK"), VST3 or AU. SF TERROIR counts wherever the scene has SF REVERB (it swaps in on B SPACE).
Each mapping's range is checked at both ends (an SF parameter from 0; a Live fader or send from its
-70 dB floor; the top where the scene caps it), and its mode: absolute, as the scene sends (with
--relative-shared, for a scene built with that option, the turns on more than one page are left to
you: map them relative, Twos Complement). Exit status: 0 when every address is right; 1 when one is
missing, on the wrong target, mapped twice, or has the wrong range or mode.
"""
import argparse
import gzip
import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "e16"))
import layout as L  # noqa: E402

TRACK_TAGS = ("MidiTrack", "AudioTrack", "GroupTrack", "ReturnTrack", "MainTrack", "MasterTrack")
LIVE_FLOOR = 0.0003162277571      # the bottom of a Live fader or send (-70 dB), where Live's own mapping starts


def load(path):
    raw = open(path, "rb").read()
    return ET.fromstring(gzip.decompress(raw) if raw[:2] == b"\x1f\x8b" else raw)


def mappings(root):
    """Every KeyMidi: (address or key, track, target, range max, range min, map mode)."""
    parent = {c: p for p in root.iter() for c in p}

    def up(e, tags):
        while e is not None and e.tag not in tags:
            e = parent.get(e)
        return e

    out = []
    for k in root.iter("KeyMidi"):
        v = {c.tag: c.get("Value") for c in k}
        owner = parent[k]
        t = up(owner, TRACK_TAGS)
        # The name you gave the track (Live 12.4 writes a return's EffectiveName as "A-A DUB"), else Live's own.
        names = [t.find("Name/" + k) for k in ("UserName", "EffectiveName")] if t is not None else []
        names = [n.get("Value") for n in names if n is not None and n.get("Value")]
        track = names[0] if names else ("Main" if t is not None else "?")
        if owner.tag == "ParameterValue":
            dev = up(owner, ("PluginDevice", "AuPluginDevice"))
            pn = dev.find(".//PluginDesc/*/Name") if dev is not None else None
            target = f"{pn.get('Value') if pn is not None else '?'} > {parent[owner].find('ParameterName').get('Value')}"
        elif owner.tag == "Volume":
            target = "volume"
        elif owner.tag == "Send":
            holders = [h for h in parent[parent[owner]] if h.tag == "TrackSendHolder"]
            target = "send " + "ABCDEFGHIJKL"[holders.index(parent[owner])] if parent[owner] in holders else "send"
        elif owner.tag == "Speaker":
            target = "activator"
        else:
            target = owner.tag
        rng, rmin = owner.find("MidiControllerRange/Max"), owner.find("MidiControllerRange/Min")
        key = v.get("PersistentKeyString", "")
        address = ("KEY", key) if key else ("NOTE" if v.get("IsNote") == "true" else "CC", int(v["Channel"]) + 1, int(v["NoteOrController"]))
        out.append((address, track, target, float(rng.get("Value")) if rng is not None else None,
                    float(rmin.get("Value")) if rmin is not None else None, int(v.get("ControllerMapMode", "0"))))
    return out


def expected():
    """{address: (track, target, max or None, min, what it is on the E16, kind, on more than one page)}"""
    want = {}
    for addr, (target, places, kind, rng, item) in L.addresses().items():
        mn = 0.0
        if getattr(item, "mixer", None):
            tgt = item.mixer
            mx = item.max_value
            mn = LIVE_FLOOR
        else:
            tgt = [f"SF {m} > {L.param_name(m, item.pid)}" for m in [item.module] + L.alternates(item.module, item.pid)]
            mx = None
            if getattr(item, "max_value", None) is not None:
                # max_value is in the parameter's unit; the mapping range is normalised (linear knobs only).
                p = next(q for q in L.SPECS[item.module]["params"] if q["id"] == item.pid)
                lo, hi = (float(p[k].split()[0]) for k in ("min", "max"))
                scale = 100.0 if p["max"].endswith("%") else 1.0
                mx = (item.max_value * scale - lo) / (hi - lo)
        where = L.where(places, getattr(item, "second", False))
        shared = kind == "TURN" and len({t for t, _ in places}) > 1
        want[addr] = (item.track, tgt, mx, mn, where + (f", press ({kind})" if kind != "TURN" else ""), kind, shared)
    return want


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("set", help="the Live set (.als)")
    ap.add_argument("--relative-shared", action="store_true", help="the scene was built with make_scene.py --relative-shared")
    a = ap.parse_args()
    have = mappings(load(a.set))
    want = expected()
    by_addr = {}
    for addr, track, target, mx, mn, mode in have:
        by_addr.setdefault(addr, []).append((track, target, mx, mn, mode))
    missing, wrong, ranges, twice, modes = [], [], [], [], []
    for addr, (track, target, mx, mn, where, kind, shared) in sorted(want.items(), key=lambda a: (a[0][1], a[0][0], a[0][2])):
        got = by_addr.get(addr, [])
        label = f"{addr[0]:4s} {addr[2]:3d} ch {addr[1]:2d}"
        if not got:
            missing.append(f"{label}  {track} > {' or '.join(target) if isinstance(target, list) else target}  ({where})")
            continue
        if len(got) > 1:
            twice.append(f"{label}  " + "; ".join(f"{t} > {g}" for t, g, *_ in got))
        targets = target if isinstance(target, list) else [target]
        if not any(t == track and g in targets for t, g, *_ in got):
            wrong.append(f"{label}  wants {track} > {' or '.join(targets)}, is on " + "; ".join(f"{t} > {g}" for t, g, *_ in got))
            continue
        _, g, m, lo, mode = next(r for r in got if r[0] == track and r[1] in targets)
        top = mx if mx is not None else 1.0
        if m is not None and abs(m - top) > 0.01:
            ranges.append(f"{label}  {track} > {g}: max {m:.3f}, the scene's is {top:.3f}")
        if lo is not None and abs(lo - mn) > 1e-6:
            ranges.append(f"{label}  {track} > {g}: min {lo:.6g}, the scene's is {mn:.6g}")
        if addr[0] == "CC" and mode != 0 and not (a.relative_shared and shared):
            modes.append(f"{label}  {track} > {g}: relative (mode {mode}); the scene sends absolute")
    e16_channels = {a[1] for a in want}
    extra = [f"{a[0]:4s} {a[2]:3d} ch {a[1]:2d}  {t} > {g}" for a, rows in sorted(by_addr.items(), key=str)
             if a[0] != "KEY" and a not in want and a[1] in e16_channels for t, g, *_ in rows]
    keys = [(a[1], t, g) for a, rows in by_addr.items() if a[0] == "KEY" for t, g, *_ in rows]
    print(f"{a.set}: {len(want) - len(missing)} of {len(want)} E16 addresses mapped")
    for title, rows in (("MISSING (map with Cmd-M)", missing), ("ON THE WRONG TARGET", wrong), ("MAPPED TWICE", twice),
                        ("RANGE", ranges), ("MODE", modes), ("OTHER MAPPINGS ON E16 CHANNELS (yours: not checked)", extra)):
        if rows:
            print(f"\n{title}:")
            for r in rows:
                print("  " + r)
    if keys:
        print("\nCOMPUTER KEYS (yours: not checked):")
        for k, t, g in sorted(keys):
            print(f"  {k!r}: {t} > {g}")
    bad = missing or wrong or twice or ranges or modes
    print("\n" + ("Something to fix: see above." if bad else "Every E16 address is mapped to its target, with its range and mode."))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
