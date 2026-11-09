"""Checks an OXI One MkII project (.oxipro) against the rig: README.md, THE PROJECT. Reads, never writes.

    python3 controllers/oxi-one/check_project.py PROJECT.oxipro [--start]

Prints each sequencer as the ONE has it (mode, channel, Out, the slots in use and what is in them,
any Program Change) and what doesn't match the card. A part's channel is its Live track's number, on
port A (controllers/e16/layout.py: one number per track, on the ONE and the E16) [Nov 8]. Songs are
called from the E16 (encoder 15 of every PERF page: NEXT SONG, its press GO), so a project calls
none: a Program Change reaches SF CONTROL only as an AU, and the show set's is a VST3. --start: the
project is SF START, which must send none (an AU of SF CONTROL would take it). Sequencers 7 and 8 are
spare: reported, not checked. Exit 1 if anything doesn't match.
"""
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "e16"))
import oxipro as X  # noqa: E402
import layout as L  # noqa: E402

# The rig (README.md, THE PROJECT): the Live track each sequencer plays, by its role, on port A and
# the track's channel. Mono channels are stored 0-based (0 is 1A, 16 is 1B).
MONO_PARTS = {s: (part, L.PLUGINS[part][2] - 1) for s, part in ((2, "BASS"), (3, "LEAD"), (4, "CHORD"), (5, "PAD"), (6, "VOICE"))}
SPARE = (7, 8)
# What SF START puts on the spare sequencers: 7 on 7B, DECK's channel (its hot cues), 8 on 16A, SF
# CONTROL's (a Program Change, for an AU of it).
SPARE_CHANNELS = {7: 16 + 7 - 1, 8: 16 - 1}
# Drum tracks: what each Live track listens to (track channels are stored 1-based: 1 is 1A).
DRUM_PARTS = {L.PLUGINS[part][2]: part for part in ("KICK", "CLOSED HAT", "OPEN HAT", "CLAP", "PERC")}
NEEDED_DRUMS = ("KICK", "CLOSED HAT", "OPEN HAT", "CLAP")
# Sequencer 1's tracks, index 0 (the ONE's track 8, the bottom row) first: the kick on the bottom row.
DRUM_ROWS = ["KICK", "PERC", "CLAP", "OPEN HAT", "CLOSED HAT", None, None, None]
assert set(MONO_PARTS.values()) == {("BASS", 5), ("LEAD", 7), ("CHORD", 8), ("PAD", 9), ("VOICE", 10)}
assert DRUM_PARTS == {1: "KICK", 2: "CLOSED HAT", 3: "OPEN HAT", 4: "CLAP", 5: "PERC"}


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
        part = "DRUMS" if s == 1 else "SPARE" if s in SPARE else MONO_PARTS[s][0]
        slots = [q for q in range(1, X.SLOTS + 1) if p.kind(s, q) != X.EMPTY]
        if not slots:
            if s not in SPARE:
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
            if k == X.MONO and (v := p.program(s, q)) is not None:
                pc = f", PC {v} (SONG {v:02d} for an AU of SF CONTROL; the VST3 ignores it)"
            lines.append(f"     slot {q} {p.slot_name(s, q)!r}: {what}{pc}")
            if X.OUTS.get(p.out(s, q)) != "USB" and s not in SPARE:
                problems.append(f"sequencer {s} slot {q}: Out {X.OUTS.get(p.out(s, q), p.out(s, q))}, not USB")
            if k == X.MONO and start and p.program(s, q) is not None:
                problems.append(f"sequencer {s} slot {q} sends PC {p.program(s, q)}: SF START calls no song")
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
        elif s not in SPARE:
            want = MONO_PARTS[s][1]
            if kind != X.MONO:
                problems.append(f"sequencer {s} ({part}): {X.MODES.get(kind, kind)}, not MONO")
            elif p.field(s, 1, X.M_CH) != want:
                problems.append(f"sequencer {s} ({part}): channel {ch}, not {X.channel_name(want)}")
            for q in slots:
                if p.kind(s, q) == X.MONO and p.field(s, q, X.M_CH) != p.field(s, 1, X.M_CH):
                    problems.append(f"sequencer {s} slot {q}: channel differs from slot 1's")
    return lines, problems


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("project")
    ap.add_argument("--start", action="store_true", help="the project is SF START: it sends no Program Change")
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
