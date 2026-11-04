#!/usr/bin/env python3
"""Checks a Live set's mappings against the E16 scene SF SHOW: every address in
controllers/e16/layout.py mapped once, to the right parameter on the right track, with the right range;
and which computer keys PANIC and 92 PA's activator are on. Reads the set, never writes it.

    python3 check_set.py "My Show.als"

Use it on a set of your own (one saved before a mapping existed, or mapped by hand): it lists what to
map with Cmd-M. SF SHOW.als, the built set, passes with every address mapped and no keys.

A target is found by track name (as in the show template: "01 KICK", "A DUB") and the plugin's name
("SF KICK"), VST3 or AU. SF TERROIR counts wherever the scene has SF REVERB (it swaps in on B SPACE). Exit status: 0 when every address is right, 1 when something is missing or wrong.
"""
import gzip
import os
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "e16"))
import layout as L  # noqa: E402

TRACK_TAGS = ("MidiTrack", "AudioTrack", "GroupTrack", "ReturnTrack", "MainTrack", "MasterTrack")


def load(path):
    raw = open(path, "rb").read()
    return ET.fromstring(gzip.decompress(raw) if raw[:2] == b"\x1f\x8b" else raw)


def mappings(root):
    """Every KeyMidi: (address or key, track, target, range max)."""
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
        name = t.find("Name/EffectiveName") if t is not None else None
        track = name.get("Value") if name is not None else ("Main" if t is not None else "?")
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
        rng = owner.find("MidiControllerRange/Max")
        key = v.get("PersistentKeyString", "")
        address = ("KEY", key) if key else ("NOTE" if v.get("IsNote") == "true" else "CC", int(v["Channel"]) + 1, int(v["NoteOrController"]))
        out.append((address, track, target, float(rng.get("Value")) if rng is not None else None))
    return out


def expected():
    """{address: (track, target, max or None, what it is on the E16)}"""
    want = {}
    for addr, (target, places, kind, rng, item) in L.addresses().items():
        if getattr(item, "mixer", None):
            tgt = item.mixer
            mx = item.max_value
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
        want[addr] = (item.track, tgt, mx, where + (f", press ({kind})" if kind != "TURN" else ""))
    return want


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    have = mappings(load(sys.argv[1]))
    want = expected()
    by_addr = {}
    for addr, track, target, mx in have:
        by_addr.setdefault(addr, []).append((track, target, mx))
    missing, wrong, ranges, twice = [], [], [], []
    for addr, (track, target, mx, where) in sorted(want.items(), key=lambda a: (a[0][1], a[0][0], a[0][2])):
        got = by_addr.get(addr, [])
        label = f"{addr[0]:4s} {addr[2]:3d} ch {addr[1]:2d}"
        if not got:
            missing.append(f"{label}  {track} > {' or '.join(target) if isinstance(target, list) else target}  ({where})")
            continue
        if len(got) > 1:
            twice.append(f"{label}  " + "; ".join(f"{t} > {g}" for t, g, _ in got))
        targets = target if isinstance(target, list) else [target]
        if not any(t == track and g in targets for t, g, _ in got):
            wrong.append(f"{label}  wants {track} > {' or '.join(targets)}, is on " + "; ".join(f"{t} > {g}" for t, g, _ in got))
            continue
        m = next(x for t, g, x in got if t == track and g in targets)
        if mx is not None and m is not None and abs(m - mx) > 0.01:
            ranges.append(f"{label}  {track} > {target}: max {m:.3f}, the scene's is {mx:.3f}")
    e16_channels = {a[1] for a in want}
    extra = [f"{a[0]:4s} {a[2]:3d} ch {a[1]:2d}  {t} > {g}" for a, rows in sorted(by_addr.items(), key=str)
             if a[0] != "KEY" and a not in want and a[1] in e16_channels for t, g, _ in rows]
    keys = [(a[1], t, g) for a, rows in by_addr.items() if a[0] == "KEY" for t, g, _ in rows]
    print(f"{sys.argv[1]}: {len(want) - len(missing)} of {len(want)} E16 addresses mapped")
    for title, rows in (("MISSING (map with Cmd-M)", missing), ("ON THE WRONG TARGET", wrong), ("MAPPED TWICE", twice),
                        ("RANGE", ranges), ("OTHER MAPPINGS ON E16 CHANNELS", extra)):
        if rows:
            print(f"\n{title}:")
            for r in rows:
                print("  " + r)
    print("\nCOMPUTER KEYS:")
    panic = [k for k, t, g in keys if g == "SF CONTROL > PANIC"]
    pa = [k for k, t, g in keys if t == "92 PA" and g == "activator"]
    print(f"  PANIC (SF CONTROL): {', '.join(repr(k) for k in panic) if panic else 'no key (Cmd-K)'}")
    print(f"  92 PA activator:    {', '.join(repr(k) for k in pa) if pa else 'no key (Cmd-K)'}")
    for k, t, g in sorted(keys):
        if (g == "SF CONTROL > PANIC") or (t == "92 PA" and g == "activator"):
            continue
        print(f"  {k!r}: {t} > {g}")
    bad = missing or wrong
    print("\n" + ("Something to map: see above." if bad else "Every E16 address is mapped to its target."))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
