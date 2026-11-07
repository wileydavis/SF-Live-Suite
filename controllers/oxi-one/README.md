# SF START: the ONE's startup project

One OXI One MkII project with every sequencer set for the rig and no notes. Every song starts as a
copy of it.

The ONE's project file (`.oxipro`) is undocumented and only partly read (below), so SF START is made
on the ONE, not written by a script: set it up once from this card and save it. That device-written
file is the startup project. It isn't in the repository yet.

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
back to its own clock and runs at the project's own tempo.

## THE PROJECT

**Shift + Seq n** opens a sequencer's setup: **Knob 1** Ch, **Knob 2** Out, **Knob 3** PC, **Knob 4**
Mode. A value showing a * is set only when its knob is pressed. Set Mode first: the other knobs
depend on it.

| SEQ | PART | MODE | CH | OUT | PC |
|---|---|---|---|---|---|
| 1 | DRUMS | MULTITRACK | per track, top row down: 8 KICK 1A, 7 STRIKE 6B, 6 CLAP 4A, 5 OPEN 3A, 4 CLOSED 2A; 3-1 off | USB | off |
| 2 | BASS | MONO | 1B | USB | off |
| 3 | ACID | MONO | 2B | USB | off |
| 4 | STAB | MONO | 3B | USB | off |
| 5 | TEXTURE | MONO | 4B | USB | off |
| 6 | VOICE | MONO | 5B | USB | off |
| 7 | STRIKE | MONO | 6B | USB | off |
| 8 | CONTROL | MONO, no notes | 16B | USB | set per song, on pattern 1 |

- **Drum tracks:** track 1 is the bottom row, track 8 the top. Hold a track's first pad to select
  it, then **Track**, **Knob 1** for its channel. Any order works, since each track names its channel;
  SF START keeps the kick on top (the lessons build theirs from the bottom: 1 KICK, 2 CLOSED, 3 OPEN,
  4 CLAP).
- **STAB and TEXTURE play single notes:** SF LEAD's CHORD (or its model's own chord) voices the stab.
  For whole chords from the ONE, set that sequencer to CHORD or POLY and the plugin's CHORD to NONE.
  Never both.
- **No notes:** select a sequencer, **Shift + tap Paste** (its Clear): its notes go, its settings
  stay. Shift + *hold* Paste resets the settings too.
- **Tempo:** the set's (120 to start). The ONE uses it only on its own clock, so set it with sync on
  INT (**Shift + Knob 1**), then put sync back on USB.
- **Save it** into a slot of its own, named SF START: **Save**, Knob 1 to the slot, press it; rename
  it there, then Save Project and press again.
- The lessons build this same project step by step, one sequencer at a time (lessons 2 to 6).

## A NEW SONG

1. **Load** SF START.
2. **Save**, Knob 1 to an empty slot, press it, turn to **Duplic proj here**, press, **Yes**. Load the
   copy.
3. **Shift + Seq 8**, **Knob 3**: the song's Program Change; press it. Save sequencer 8's pattern 1
   (**Seq 8**, **Save**, double-tap slot 1 on its row). Lesson 6 tests whether your ONE counts
   programs from 0 or 1.

## THE FILE FROM NOV 7 (SFSHOW), BEFORE AND AFTER

Two saves of the same project, before and after the changes this card asks for. Read against the
table:

| SEQ | BEFORE | AFTER |
|---|---|---|
| 1 | Multitrack, Out ALL; a beat | Multitrack, Out USB; the same beat |
| 2 | Mono 1B, Out ALL; an off-beat bass line | Mono 1B, Out USB; a note on all 16 steps |
| 3, 4, 6 | Mono 2B, 3B, 5B, Out ALL | the same, Out USB |
| 5 | Chord 4B, Out ALL | Mono 4B, Out USB |
| 7 | Multitrack, no channel | Mono 6B, Out USB |
| 8 | Multitrack, channel byte 8A | Mono 16B, Out USB; PC off |

The beat on sequencer 1 (a test, not meant to stay): KICK on the beats, CLOSED on every eighth, OPEN
on the off-beats. Its tracks, as the ONE shows them: 8 KICK 1A, 7 STRIKE 6B, 6 CLAP 4A, 5 OPEN 3A, 4
CLOSED 2A, 3-1 off. Every sequencer's Program Change is off.

## THE FILE FORMAT, AS FAR AS IT IS READ

Notes from the file above, guided by OXI-Desktop's MkI source (github.com/faqteur/OXI-Desktop); not a
spec.

- A 0x3000-byte project header, then 128 pattern slots of 0x3000: sequencer s, pattern p (0-based)
  at 0x3000 × (1 + 16s + p). An empty slot starts with type FF.
- Header: 0x02 its length (0x2A44); 0x04 the name, 12 bytes; 0x10 1320 (tempo × 10, 132.0 BPM: a
  guess); the 8 × 16 pattern names and colours; 4 songs of 256 arranger entries; a CRC32 at 0x2A40.
- CRC32 as OXI-Desktop's crc32.h: poly 0x04C11DB7, init 0xFFFFFFFF, over 32-bit words, no final
  XOR. It verifies for the header and every pattern in use.
- A pattern starts with type, version and a u16 length; its CRC sits at the length rounded up to 4.
- Mode (first byte): 0 Mono, 1 Poly, 2 Chord, 3 Multitrack (the MkI numbering; sure for 0 and 3).
  Channel, Mono: +0x0CB9; Chord: 0x100 earlier; 0-15 is port A ch 1-16, 16-31 port B.
- **Out** (from the second save, where exactly these bytes changed when Out went to USB on all eight):
  the byte after the channel, Mono +0x0CBE, Multitrack +0x2A31. 0 ALL (the default), 2 USB.
- **Mono steps:** 128 records of 24 bytes from +0x04. Byte 0 the note (FF none, 0 the root), byte 1
  the velocity (75 by default), byte 3 the gate (50).
- **Multitrack:** the tracks are stored top row first: index 0 is track 8, index 7 track 1.
  - Steps: 128 records of 81 bytes from +0x04. Byte 0 the tracks that play (bit 0 track 8), then
    per track: the velocity at +9 to +16 (75), the gate at +17 to +24 (50).
  - Track channels: +0x288C, one byte per track, 1-based (1 is 1A, 17 is 1B), 0 off. Confirmed: the
    ONE showed 8 1A, 7 6B, 6 4A, 5 3A, 4 2A, 3-1 off; the bytes read 01 16 04 03 02 00 00 00.
  - +0x2894 holds 1 to 8 in every multitrack (track numbers, or notes).
  - Every byte between an empty multitrack and one with a beat is a trigger, a track channel or the
    group's base, x and y; between an empty Mono and one with notes, a note, a velocity, the base or
    the channel.
- **Header, per pattern slot:** 128 entries of 16 bytes from 0x12 (sequencer s, slot p at 0x12 + 16
  × (16s + p)): a flag (1 on every slot 1, the only slots in use), the sequencer's colour, the name
  ("Pattern 1"). Whether the flag means "in use" or "loaded" needs a save with another slot in use.
- **Program Change:** not found. All eight are off in both saves; the one header byte that changed
  between them (0x212E, 0 to 1) is something else.
- Tempo, probably: 0x10 reads 1320 in both saves.
- Not found: clock and sync (the ONE's config, not the project's).

No script writes a project: a record with a correct CRC and a wrong field would look valid to the
ONE. Only once a writer reproduces device-saved files byte for byte, and then in a spare slot with
a backup.
