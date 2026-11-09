"""Builds SFSTART.oxipro, the OXI One MkII startup project, from a project the ONE saved.

    python3 controllers/oxi-one/make_start.py --base SAVED.oxipro [--out SFSTART.oxipro]

The base is a project the ONE wrote with every sequencer's mode and Out already set as README.md's
table has them; check_project.py must pass on it, channels and Program Change aside. This script
writes the notes, the slots in use and their names, every Program Change (off: songs are called from
the E16) and the project's name, all checked against the ONE's own saves, and the channels (README.md's
table: a part on its Live track's number, port A [Nov 8]), read in every save but not yet written by
the ONE for a comparison. Every other byte is the base's. Columns 1-4 become four scenes (SCENES
below); slots 5-16 are left empty. Parts are named by the Live track they play (its role).
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oxipro as X  # noqa: E402
import check_project as C  # noqa: E402

BEATS, OFFBEATS, EIGHTHS = [1, 5, 9, 13], [3, 7, 11, 15], list(range(1, 17, 2))
# The rolling bass: three sixteenths after every kick, getting louder (velocity) within each group.
ROLL = {s: v for b in BEATS for s, v in zip((b + 1, b + 2, b + 3), (20, 35, 55))}
MAIN = {"KICK": BEATS, "CLOSED HAT": EIGHTHS, "OPEN HAT": OFFBEATS, "CLAP": [5, 13]}
# Column: (name, drum parts and their steps, bass {step: velocity}, chord steps). Notes on the root.
SCENES = [
    ("SPARSE", {"KICK": BEATS, "OPEN HAT": OFFBEATS}, {}, []),
    ("MAIN", MAIN, ROLL, [7]),
    ("VARIATION", {**MAIN, "PERC": [12, 15]}, ROLL, [7, 15]),
    ("EXIT", {"KICK": BEATS, "CLOSED HAT": EIGHTHS}, {}, [7]),
]
NAME = "SFSTART"


def build(base):
    p = X.Project(base)
    _, problems = C.read(p, start=True)
    problems = [x for x in problems if "sends PC" not in x and "channel" not in x]   # both written below
    if problems:
        raise SystemExit("The base doesn't match the card; set these on the ONE and save it again:\n  "
                         + "\n  ".join(problems))
    byte = {part: ch for ch, part in C.DRUM_PARTS.items()}             # a drum track's channel, 1-based
    p.set_track_channels(1, 1, [byte[part] if part else 0 for part in C.DRUM_ROWS])
    for s, (_part, ch) in C.MONO_PARTS.items():
        p.set_channel(s, 1, ch)
    for s, ch in C.SPARE_CHANNELS.items():
        if p.kind(s, 1) == X.MONO:
            p.set_channel(s, 1, ch)
    index = {part: C.drum_index(p, part) for part in {q for _, d, _, _ in SCENES for q in d}}
    missing = [q for q, i in index.items() if i is None]
    if missing:
        raise SystemExit(f"No drum track on the channel of: {', '.join(missing)}")
    for s in range(1, X.SEQS + 1):
        p.clear(s, 1)
        if p.kind(s, 1) == X.MONO:
            p.set_program(s, 1, None)
        for q in range(2, len(SCENES) + 1):
            p.copy_slot(s, 1, q)
        for q in range(len(SCENES) + 1, X.SLOTS + 1):
            p.empty_slot(s, q)
        for q, (name, *_rest) in enumerate(SCENES, 1):
            p.set_slot(s, q, name)
    for q, (_name, drums, bass, chord) in enumerate(SCENES, 1):
        for part, steps in drums.items():
            for st in steps:
                p.set_trigger(1, q, st, index[part])
        for st, vel in bass.items():
            p.set_note(2, q, st, 0, vel)
        for st in chord:
            p.set_note(4, q, st, 0)
    p.set_name(NAME)
    return p.finish()


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--base", required=True, help="a project the ONE saved, set as the card says")
    ap.add_argument("--out", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), NAME + ".oxipro"))
    a = ap.parse_args()
    with open(a.base, "rb") as f:
        data = build(f.read())
    p = X.Project(data)
    lines, problems = C.read(p, start=True)
    if problems:
        raise SystemExit("The result doesn't check:\n  " + "\n  ".join(problems))
    with open(a.out, "wb") as f:
        f.write(data)
    print("\n".join(lines))
    print(f"\n-> {a.out}")


if __name__ == "__main__":
    main()
