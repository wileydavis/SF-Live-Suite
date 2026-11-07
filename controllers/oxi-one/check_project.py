"""Checks an OXI One MkII project (.oxipro) against the rig: README.md, THE PROJECT. Reads, never writes.

    python3 controllers/oxi-one/check_project.py PROJECT.oxipro [--start]

Prints each sequencer as the ONE has it (mode, channel, Out, the slots in use and what is in them,
sequencer 8's Program Change) and what doesn't match the card. --start: the project is SF START, so
sequencer 8's Program Change should be off; without it, the project is a song's and sequencer 8
must call it. Exit 1 if anything doesn't match.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import oxipro as X  # noqa: E402

# The rig (README.md, THE PROJECT). Mono channels are stored 0-based: 16 is 1B.
MONO_PARTS = {2: ("BASS", 16), 3: ("ACID", 17), 4: ("STAB", 18), 5: ("TEXTURE", 19),
              6: ("VOICE", 20), 7: ("STRIKE", 21), 8: ("CONTROL", 31)}
# Drum tracks: what each Live track listens to (track channels are stored 1-based: 1 is 1A).
DRUM_PARTS = {1: "KICK", 2: "CLOSED", 3: "OPEN", 4: "CLAP", 22: "STRIKE"}
NEEDED_DRUMS = ("KICK", "CLOSED", "OPEN", "CLAP")


def drum_index(p, part, slot=1):
    """The track index (0 is the ONE's track 8) whose channel feeds `part`, or None."""
    for i, v in enumerate(p.tracks(1, slot)):
        if DRUM_PARTS.get(v) == part:
            return i
    return None


def read(p, start=False):
    """(lines, problems): the project as the ONE has it, and what doesn't match the card."""
    lines, problems = [], []
    bad = p.bad_crcs()
    lines.append(f"{p.name()}: tempo {p.tempo():g} (used only when the ONE runs on its own clock); "
                 + ("every checksum verifies" if not bad else "CHECKSUMS FAIL: " + ", ".join(bad)))
    problems += [f"checksum fails: {b}" for b in bad]
    for s in range(1, X.SEQS + 1):
        part = "DRUMS" if s == 1 else MONO_PARTS[s][0]
        slots = [q for q in range(1, X.SLOTS + 1) if p.kind(s, q) != X.EMPTY]
        if not slots:
            problems.append(f"sequencer {s} ({part}): no pattern at all")
            continue
        kind = p.kind(s, 1)
        ch = X.channel_name(p.field(s, 1, X.M_CH)) if kind == X.MONO else "-"
        lines.append(f"{s}  {part:<8} {X.MODES.get(kind, kind):<10} {ch:<4} Out {X.OUTS.get(p.out(s, 1), p.out(s, 1))}")
        for q in slots:
            k = p.kind(s, q)
            if k != kind:
                problems.append(f"sequencer {s} slot {q}: {X.MODES.get(k, k)}, slot 1 is {X.MODES.get(kind, kind)}")
                continue
            n = p.notes(s, q)
            what = (f"{sum(len(v) for v in n.values())} hits" if k == X.MULTI else f"{len(n)} note" + "s" * (len(n) != 1)) if n else "empty"
            pc = ""
            if k == X.MONO:
                v = p.program(s, q)
                pc = "" if s != 8 and v is None else f", PC {v} (SONG {v:02d})" if v else ", PC off"
            lines.append(f"     slot {q} {p.slot_name(s, q)!r}: {what}{pc}")
            if X.OUTS.get(p.out(s, q)) != "USB":
                problems.append(f"sequencer {s} slot {q}: Out {X.OUTS.get(p.out(s, q), p.out(s, q))}, not USB")
            if k == X.MONO and s != 8 and p.program(s, q) is not None:
                problems.append(f"sequencer {s} slot {q}: sends PC {p.program(s, q)}; only sequencer 8 calls the song")
        if s == 1:
            if kind != X.MULTI:
                problems.append(f"sequencer 1: {X.MODES.get(kind, kind)}, not MULTITRACK")
                continue
            names = [f"{8 - i} {DRUM_PARTS.get(v, '?')} {X.track_channel_name(v)}" if v else f"{8 - i} off"
                     for i, v in enumerate(p.tracks(1, 1))]
            lines.append("     tracks, bottom row up: " + ", ".join(names))
            for q in slots:
                if p.tracks(1, q) != p.tracks(1, 1):
                    problems.append(f"sequencer 1 slot {q}: its tracks' channels differ from slot 1's")
            for need in NEEDED_DRUMS:
                if drum_index(p, need) is None:
                    problems.append(f"sequencer 1: no track on {need}'s channel")
            for i, v in enumerate(p.tracks(1, 1)):
                if v and v not in DRUM_PARTS:
                    problems.append(f"sequencer 1 track {8 - i}: {X.track_channel_name(v)}, a channel no drum listens to")
        else:
            want = MONO_PARTS[s][1]
            if kind != X.MONO:
                problems.append(f"sequencer {s} ({part}): {X.MODES.get(kind, kind)}, not MONO")
            elif p.field(s, 1, X.M_CH) != want:
                problems.append(f"sequencer {s} ({part}): channel {ch}, not {X.channel_name(want)}")
            for q in slots:
                if p.kind(s, q) == X.MONO and p.field(s, q, X.M_CH) != p.field(s, 1, X.M_CH):
                    problems.append(f"sequencer {s} slot {q}: channel differs from slot 1's")
    if p.kind(8, 1) == X.MONO:
        pc = p.program(8, 1)
        if start and pc is not None:
            problems.append(f"sequencer 8 slot 1 calls SONG {pc:02d}: SF START calls no song (each copy sets its own)")
        if not start and pc is None:
            problems.append("sequencer 8 slot 1 sends no Program Change: loading the project won't call its song")
    return lines, problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("project")
    ap.add_argument("--start", action="store_true", help="the project is SF START: no song called")
    a = ap.parse_args()
    with open(a.project, "rb") as f:
        p = X.Project(f.read())
    lines, problems = read(p, a.start)
    print("\n".join(lines))
    if problems:
        print("\nDOESN'T MATCH THE CARD:")
        print("\n".join(f"  {x}" for x in problems))
        sys.exit(1)
    print("\nMatches the card.")


if __name__ == "__main__":
    main()
