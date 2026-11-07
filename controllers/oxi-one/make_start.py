"""Builds SFSTART.oxipro, the OXI One MkII startup project, from a project the ONE saved.

    python3 controllers/oxi-one/make_start.py --base SAVED.oxipro [--out SFSTART.oxipro]

The base is a project the ONE wrote with every sequencer already set as README.md's table has them
(modes, channels, Out, the drum tracks' channels); check_project.py must pass on it, Program Change
aside. This script writes only what was checked against the ONE's own saves: the notes, the slots in
use and their names, sequencer 8's Program Change (off) and the project's name. Every other byte is
the base's. Columns 1-4 become four scenes (SCENES below); slots 5-16 are left empty.
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
MAIN = {"KICK": BEATS, "CLOSED": EIGHTHS, "OPEN": OFFBEATS, "CLAP": [5, 13]}
# Column: (name, drum parts and their steps, bass {step: velocity}, stab steps). Notes on the root.
SCENES = [
    ("SPARSE", {"KICK": BEATS, "OPEN": OFFBEATS}, {}, []),
    ("MAIN", MAIN, ROLL, [7]),
    ("VARIATION", {**MAIN, "STRIKE": [12, 15]}, ROLL, [7, 15]),
    ("EXIT", {"KICK": BEATS, "CLOSED": EIGHTHS}, {}, [7]),
]
NAME = "SFSTART"


def build(base):
    p = X.Project(base)
    _, problems = C.read(p, start=True)
    problems = [x for x in problems if "Program Change" not in x and "calls SONG" not in x]
    if problems:
        raise SystemExit("The base doesn't match the card; set these on the ONE and save it again:\n  "
                         + "\n  ".join(problems))
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
    for q, (_name, drums, bass, stab) in enumerate(SCENES, 1):
        for part, steps in drums.items():
            for st in steps:
                p.set_trigger(1, q, st, index[part])
        for st, vel in bass.items():
            p.set_note(2, q, st, 0, vel)
        for st in stab:
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
