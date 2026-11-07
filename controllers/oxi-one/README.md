# SF START: the ONE's startup project

One OXI One MkII project with every sequencer set for the rig and four scenes of simple patterns.
Every song starts as a copy of it.

| FILE | WHAT |
|---|---|
| `SFSTART.oxipro` | The startup project. Built by `make_start.py` from a project the ONE saved, with only the notes, the slots in use and their names, sequencer 8's Program Change and the project's name written; every other byte is the ONE's own. |
| `check_project.py` | Reads any ONE project and checks it against this card: modes, channels, Out, the drum tracks, the slots in use and what is in them, sequencer 8's Program Change. Never writes. |
| `make_start.py` | Builds `SFSTART.oxipro` from a saved project (`--base`). |
| `oxipro.py` | The project file, as far as it is read (THE FILE FORMAT, below). |

## ONCE PER ONE: THE CONFIG

Global settings: the ONE keeps them, a project doesn't. **Shift + Back** opens the config; in it,
hold **Shift + Back** to save.

| SETTING | VALUE |
|---|---|
| Sync (ONE stopped: **Shift + Stop** until it shows) | **USB**, not INT, in the header |
| MIDI › Start Stop Msgs Ignore | **No** |
| MIDI › MIDI Transport Msgs Send | **Off** |
| MIDI › MIDI USB Thru | **Off** |
| Performance › Auto Sync | **On**: the ONE starts with Live |

Live sends its clock only while it plays. Start and stop from Live: with Live stopped, the ONE falls
back to its own clock and runs at the project's own tempo (120 in SF START).

## THE PROJECT

**Shift + Seq n** opens a sequencer's setup: **Knob 1** Ch, **Knob 2** Out, **Knob 3** PC, **Knob 4**
Mode. A value showing a * is set only when its knob is pressed. Set Mode first: the other knobs
depend on it.

| SEQ | PART | MODE | CH | OUT | PC |
|---|---|---|---|---|---|
| 1 | DRUMS | MULTITRACK | per track, bottom row up: 8 KICK 1A, 7 STRIKE 6B, 6 CLAP 4A, 5 OPEN 3A, 4 CLOSED 2A; 3-1 off | USB | off |
| 2 | BASS | MONO | 1B | USB | off |
| 3 | ACID | MONO | 2B | USB | off |
| 4 | STAB | MONO | 3B | USB | off |
| 5 | TEXTURE | MONO | 4B | USB | off |
| 6 | VOICE | MONO | 5B | USB | off |
| 7 | STRIKE | MONO | 6B | USB | off |
| 8 | CONTROL | MONO, no notes | 16B | USB | the song's number, in slot 1 |

- **Drum tracks:** the ONE shows the bottom row as track 8. Hold a track's first pad to select it,
  then **Track**, **Knob 1** for its channel. Any order works, since each track names its channel
  (the lessons build theirs from the bottom: KICK, CLOSED, OPEN, CLAP).
- **STAB and TEXTURE play single notes:** SF LEAD's CHORD (or its model's own chord) voices the stab.
  For whole chords from the ONE, set that sequencer to CHORD or POLY and the plugin's CHORD to NONE.
  Never both.
- **The scenes:** each sequencer has 16 pattern slots; a column across all eight is a scene. On the
  Load page, **Shift + tap a column** loads it on every sequencer.

| SCENE (COLUMN) | DRUMS | BASS | STAB |
|---|---|---|---|
| 1 SPARSE | KICK 1 5 9 13, OPEN 3 7 11 15 | none | none |
| 2 MAIN | KICK on the beats, CLOSED every eighth, OPEN on the off-beats, CLAP 5 13 | steps 2-4, 6-8, 10-12, 14-16, velocity 20, 35, 55 in each group | step 7 |
| 3 VARIATION | MAIN, and STRIKE 12 15 | as MAIN | steps 7 and 15 |
| 4 EXIT | KICK on the beats, CLOSED every eighth | none | step 7 |

Every note is on the root. ACID, TEXTURE, VOICE, STRIKE and CONTROL have empty patterns in all four
columns, so loading a scene resets them too. Columns 5-16 are empty. Steps are sixteenths, 1-16.

## ON THE ONE

1. Copy `SFSTART.oxipro` to the ONE the way projects come off it (the OXI App, or the card), into a
   spare project slot. Keep a backup of the card first.
2. **Load** it. The Load page shows four patterns lit in columns 1-4 on every row.
3. Live playing, on the Load page: **Shift + tap column 1**, then 2, 3 and 4. Each scene lands
   (SPARSE, MAIN, VARIATION, EXIT), every part on its own track.

## A NEW SONG

1. **Load** SF START.
2. **Save**, Knob 1 to an empty slot, press it, turn to **Duplic proj here**, press, **Yes**. Load the
   copy.
3. **Shift + Seq 8**, **Knob 3**: the song's number; press it. Save it into slot 1 (**Seq 8**,
   **Save**, double-tap slot 1 on its row). The ONE stores PC n as n - 1, the number MIDI sends, and
   SF CONTROL takes 0 as SONG 01: PC n calls SONG n (lesson 6 tests it on the rig).
4. `python3 controllers/oxi-one/check_project.py SONG.oxipro` checks the copy: every setting, and
   that sequencer 8 calls a song. On SF START itself, add `--start`.

## THE FILES FROM NOV 7

Three saves of one project (SFSHOW), the ONE's own, made while this card was written:

| SAVE | WHAT CHANGED | WHAT IT SHOWED |
|---|---|---|
| 1 | as it was: seq 5 Chord, 7 and 8 Multitrack, Out ALL, a test beat and bass | the structure; mode and channel |
| 2 | Out USB on all eight; seq 5, 7, 8 Mono on 4B, 6B, 16B | Out; the step records |
| 3 | seq 1 saved into slot 2; seq 8 PC 5; tempo 120 | the slot table; the Program Change; the tempo |

The ONE showed sequencer 1's tracks as above and every Program Change off in save 2, which confirmed
the track channels. `SFSTART.oxipro` is save 3 with the scenes written in.

## THE FILE FORMAT, AS FAR AS IT IS READ

Seen in the saves above, guided by OXI-Desktop's MkI source (github.com/faqteur/OXI-Desktop); not a
spec. `oxipro.py` holds the same.

- A 0x3000-byte header, then 128 pattern slots of 0x3000: sequencer s, slot p (from 1) at 0x3000 ×
  (1 + 16(s - 1) + (p - 1)). An empty slot is type FF, then zeros.
- Header: 0x04 the name, 12 bytes; 0x10 the tempo × 10 (1320, then 1200 when set to 120); 0x12 the
  slot table, 128 entries of 16 bytes (in use 1/0, the sequencer's colour, the name, 14 bytes:
  "Pattern 2" went in use when slot 2 was saved); 4 songs of 256 arranger entries from 0x813; the
  CRC at 0x2A40, over bytes 0 to 0x2A3F. Two header bytes changed without an explanation (0x212E in
  save 2, 0x2123 in save 3): kept as the ONE wrote them.
- CRC32 as OXI-Desktop's crc32.h: poly 0x04C11DB7, init 0xFFFFFFFF, over 32-bit little-endian words,
  no final XOR.
- A pattern starts with its mode, a version and a u16 length; its CRC sits at the length rounded up
  to 4, over everything before it. The ONE sometimes leaves a byte or two after the CRC; they aren't
  covered by it.
- Mode: 0 Mono, 1 Poly, 2 Chord, 3 Multitrack (the MkI numbering; seen: 0, 2, 3).
- **Mono:** 128 step records of 24 bytes from +0x04: byte 0 the note (FF none, 0 the root), byte 1
  the velocity (75 by default), byte 3 the gate (50). The channel at +0x0CBD (0-15 port A ch 1-16,
  16-31 port B), Out at +0x0CBE (0 ALL, 2 USB), the Program Change at +0x0CC6 (FF off, else PC - 1:
  PC 5 is 04). Base, x and y at +0x0CB9 to +0x0CBB.
- **Multitrack:** 128 step records of 81 bytes from +0x04: byte 0 the tracks that play (bit i for
  track index i), the velocities at +9 + i (75). Track index 0 is the bottom row, which the ONE shows
  as track 8. The tracks' channels at +0x288C, one byte each, 0 off, 1-32 for 1A to 16B (the ONE
  showed 8 1A, 7 6B, 6 4A, 5 3A, 4 2A, 3-1 off; the bytes read 01 16 04 03 02 00 00 00). Out at
  +0x2A31; base, x and y at +0x2A2C.
- Not found: a multitrack track's own Out and Program Change, Poly and Chord's own fields, clock and
  sync (the ONE's config, not the project's).

## HOW SFSTART WAS CHECKED

`make_start.py` writes only fields whose writes reproduce the ONE's own saves byte for byte, CRC
included:

- Rewriting every CRC of each save changes nothing.
- Clearing sequencer 1's beat gives the empty pattern the ONE saved in slot 2.
- Writing that beat into the empty pattern gives the ONE's slot 1; writing sequencer 2's 16 notes
  into its cleared pattern gives the ONE's.
- Sequencer 8's PC off gives save 2's pattern; PC 5 gives save 3's.
- Marking slot 2 in use gives save 3's entry.

The result then reads back through `check_project.py --start` with nothing out of place, and the
only header bytes that differ from save 3 are the name, the entries of slots 1-4 and the CRC. Nothing
here has been loaded on a ONE yet: load it into a spare slot, with a backup of the card.
