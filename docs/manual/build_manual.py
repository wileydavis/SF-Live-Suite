#!/usr/bin/env python3
"""Builds docs/SF-Live-Suite-Manual.pdf from specs.json (dumped from the code) + content.py."""
import json, os, sys
from reportlab.lib.pagesizes import letter
from reportlab.lib.units import inch
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.enums import TA_LEFT
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, PageTemplate, Frame, Paragraph, Spacer, Table, TableStyle,
                                PageBreak, Image, KeepTogether, CondPageBreak, NextPageTemplate)
from reportlab.platypus.tableofcontents import TableOfContents
from reportlab.platypus.flowables import Flowable

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
import content as C
import layout as E16          # the E16 scenes' layout (PERF, MIX): one source for the scene files and these tables

from manual_style import *     # fonts, colours, styles, page chrome, shared drawings

# ------------------------------------------------------------------ build
specs = json.load(open(os.path.join(HERE, "specs.json")))
story = []
story += [NextPageTemplate("body"), PageBreak()]

# Contents
story.append(Paragraph("CONTENTS", S["h1"]))
story.append(Rule())
toc = TableOfContents()
toc.levelStyles = [ParagraphStyle("t0", fontName="MonoB", fontSize=9.5, leading=15, leftIndent=0, spaceBefore=6),
                   ParagraphStyle("t1", fontName="Mono", fontSize=8.5, leading=12.5, leftIndent=16, textColor=INK)]
toc.dotsMinLevel = 0
story += [toc, PageBreak()]

# ---------------- PART 1
story.append(H1("1  HOW THE SUITE WORKS"))
story.append(Rule())
story.append(P("The suite is built for one kind of performance: very simple note content, often one note in a pattern that never changes, with all the variation played on knobs. Everything follows from that. Every plugin is designed to be played, not programmed, and every plugin talks to the others.", "summary"))

story.append(H2("THE RIG"))
story.append(P("One Live set that never changes. The track layout is fixed: one track per role (kick, percussion, rumble, bass, lead, voice, pad, deck). The OXI One MkII sequences everything; the OXI E16 plays the knobs. Songs are not projects: a song is a stored set of values in every plugin, recalled on the bar. You switch songs without loading anything."))
story.append(H2("THE PERFORMANCE ROW"))
story.append(P("The first eight controls of every plugin are its performance row: the first eight parameters Live shows for the device. On the E16 a track has a page of its own, with these and more of its instrument: one page plays one track."))
story.append(H2("THE PLUGIN WINDOW"))
story.append(P("Every plugin's window is a narrow strip, so several sit side by side. Top to bottom it goes from what you play to what you set once:"))
story += bullets(["<b>Header:</b> SONG, the OUT meter (FAULT, PANIC), PRESET, STORE, LOAD.",
                  "<b>E16:</b> the plugin's page of the E16 scenes (PERF or MIX, named beside the page), as the E16 shows it: its own encoders lit where they sit, the others dark. The rings fill as the E16's do (16 LEDs, from just left of the bottom, clockwise) in the scenes' colours: white, light blue for a level, green for SLOW ALL. A press sits under its knob (THROW, FREEZE, DROP, ROLL, GO); click it. A plugin the scenes have on several tracks (LEAD, PERC, ENERGY) finds its encoders by the track's name, which only the VST3 knows; without it, a line says so and its knobs go to PERFORM. A plugin the scenes don't use has no grid.",
                  "<b>PERFORM:</b> the rest of the performance row, and what else is played (DECK's PLAY and CUE, CONTROL's PANIC).",
                  "<b>The display</b>, then <b>DETAIL</b>: the sound's other controls, MODEL first.",
                  "<b>ENGINE</b> (BASS, LEAD, PAD): the controls one model has of its own, folded: FM 4-OP's operators and WAVETABLE's TABLE.",
                  "<b>LAYER</b> (KICK, BASS, LEAD, PAD): the second layer, LAYER MODEL, LAYER, SPLIT and SPLIT HZ, folded, how it is set on its title line.",
                  "<b>MOTION</b>, <b>SYSTEM</b> (SLOW, MACRO) and <b>SETUP</b> (how the plugin sits in the set: ALLOC, KEY FOLLOW, DRY ...) come folded, their settings on their title line. Click a group's title line to open or fold it; the plugin keeps it so, with the set.",
                  "<b>Taller than the screen</b> (some strips pass 1000 pixels with every group open): the window stops at the screen's height and the strip scrolls under the header, by the thin bar at the right or the wheel. While it scrolls, the wheel scrolls and turns no knob; a group you open at the bottom scrolls into view.",
                  "<b>On every knob:</b> green, the sound travelling to where you set it (SLOW), and the ringed LED is where it is going; a green dot outside the ring, where MACRO or MOTION have it now; the value in cyan while you touch it. A knob whose middle is neutral (FILTER, TUNE) fills from the top either way, except in the E16 grid, which fills as the E16 does."])
story.append(H2("DENSITY, GATE, FILTER"))
story.append(P("Three controls cover most of what changes in a track, and they appear wherever they make sense. <b>DENSITY</b> is how many events happen (ghost notes fill the gaps). <b>GATE</b> is how long each event lasts. <b>FILTER</b> is what each event sounds like: a bipolar DJ filter, centre open, left low-pass, right high-pass. Play two against each other: density up with gate down is a tight, nervous build; density down with gate open is spacious dub; density up with the filter closed is tension waiting to be released."))
story.append(H2("SYSTEM CONTROLS ON EVERY PLUGIN"))
story.append(P("At the bottom of every plugin's window, folded: MOTION (two modulators) and SYSTEM (SLOW, MACRO). They are the same on every plugin and are not repeated in the module tables."))
story.append(table([["CONTROL", "RANGE", "WHAT IT DOES"]] + [[a, b, c] for a, b, c in C.SYSTEM], [1.1 * inch, 1.75 * inch, W - 2.85 * inch]))
story.append(H2("MOTION"))
story.append(P("Two modulators in every plugin, M1 and M2. Pick a SOURCE, a RATE, the TARGET knob and a DEPTH. Everything is locked to Live's song position: every plugin's modulators move together, a 64-bar ramp lands on the 64th bar, and a set replays the same way every time, RANDOM and DRIFT included (their seeds are saved). Modulation adds to the knob, the macro and SLOW; a green dot outside the knob's ring shows where it is. Steps (SQUARE, RANDOM, a ramp restart) are smoothed over 3 ms. MOTION is part of songs and presets."))
story += bullets(["<b>Hypnotic movement:</b> DRIFT on a filter at 8 or 16 BAR, DEPTH 10-20 %: it never repeats, so neither does the loop.",
                  "<b>Long builds:</b> RAMP UP at 32 or 64 BAR on ENERGY or a filter; turn DEPTH to set how far it goes.",
                  "<b>Polyrhythm:</b> the odd RATEs (3/16, 5/16, 7/16, 1/4D, 1/2D, 1/4T, 1/2T, 3, 5, 7 BAR) turn against the 4/4: a 5/16 SINE on a filter meets the bar again every 5 bars.",
                  "<b>Kick-driven:</b> KICK on a bass's DRIVE or a pad's CUTOFF: every kick pushes it, RATE sets the fall.",
                  "<b>Played:</b> NOTES on an instrument (velocity) or INPUT on an effect (level) makes the sound respond to how hard it is played."])
story.append(H2("KEY"))
story.append(P("Write parts in C. SF CONTROL's KEY (stored per song) then plays them in the song's key on every SF instrument: BASS, LEAD, PAD and VOICE transpose by the nearest interval (-6 to +5 semitones); KICK, RUMBLE and STRIKE land their own pitch on the key root, so the low end is always in tune with the track. Notes already sounding follow a KEY change. KEY FOLLOW on each instrument opts out. OFF changes nothing, and without SF CONTROL running every instrument plays in C."))
story.append(P("Mind the low register. KEY moves parts up to 6 semitones either way, and KICK, RUMBLE and STRIKE snap to the nearest root, so the low end can land high or very low: a kick body at 50 Hz becomes 69 Hz in C# and 37 Hz in D. Check every song's KEY on a sub. TUNE (stored per song) moves the kick from the snapped root: -12 takes the 69 Hz kick an octave down, +7 or -5 puts it on the fifth. A bass too deep: move the part up an octave on the OXI."))
story.append(H2("NOTHING CAN HURT THE PA"))
story.append(P("Every knob is smoothed, so 7-bit MIDI never steps audibly. Resonance is level-compensated. Feedback above 100 % saturates instead of exploding. Every output is clamped at +12 dBFS. A plugin that produces an invalid value, or sits at the clamp for 250 ms (a runaway), outputs silence, resets itself, shows <b>FAULT</b> in red and carries on. <b>PANIC</b> on SF CONTROL fades every SF plugin out over 5 ms and clears every voice, tail and delay line: for a runaway loop, a stuck note or a wrong recall. While held, everything stays silent (no input, no new notes; DECK keeps its place; every header shows PANIC in red); let go and it fades back in over 5 ms. A set never reopens with PANIC held. Grab any knob in the dark."))
_cc_head = [H2("THE COLOUR CODE"), P("The plugins are black, white and grey. Colour only carries information:")]
_RINGS = [E16.WHITE, E16.SHAPE, E16.LIGHT_BLUE, E16.GROUP, E16.SET, E16.GREEN]
_RING_ROLES = ["an instrument's own sound (1-8)", "a track's GATE or DECAY, DENSITY and FILTER (9-11) and its NRG", "a level or a send",
               "NRG's ACID, DIST and WARP", "NEXT SONG and BUILD", "SLOW ALL, and a knob while it glides"]
cc = [["", "COLOUR", "MEANS"], [Swatch(RED), "RED", "Clip, FAULT (the plugin reset itself), missing file, true peak at the ceiling."], [Swatch(AMBER), "AMBER", "Conflict or warning: an ALLOC conflict, a deck armed and waiting, RETURN KNOB on BUILD."],
      [Swatch(GREEN), "GREEN", "Active or locked: bus connected, kick reporting, a song recall or a drop waiting for the bar, a value gliding (SLOW)."],
      [Swatch(CYAN), "CYAN", "The value of the control you are touching."],
      [RingSwatch(E16.RING_RGB[k] for k in _RINGS), "E16 RINGS", "In the E16 grid at the top of a window, each ring has its colour on the E16, by role: "
       + ", ".join(f"{E16.RING_NAME[k]} {what}" for k, what in zip(_RINGS, _RING_ROLES)) + "."]]
story.append(KeepTogether(_cc_head + [table(cc, [0.3 * inch, 0.8 * inch, W - 1.1 * inch])]))
story.append(PageBreak())

story.append(H1("2  SONGS AND PRESETS"))
story.append(Rule())
story.append(P("Every SF plugin stores 16 song slots of its own settings, saved with the Live set. SF CONTROL chooses the song; every plugin recalls its slot on the same bar line while Live plays (at once when stopped).", "summary"))
story.append(H2("SAVING A SONG"))
story += bullets(["Set every plugin how the song should start.", "Press <b>STORE ALL</b> on SF CONTROL. Every SF plugin stores its current values into the song in force (SONG).",
                  "<b>Into another song:</b> choose it in <b>STORE TO</b> on SF CONTROL, then press STORE ALL. That song isn't recalled first, so nothing you hear changes; it becomes the song in force (SONG follows) and STORE TO goes back to ACTIVE SONG. Choosing the song on SONG instead would recall it, and what you had set would be gone before you stored it.",
                  "If a song change is waiting for its bar (WAIT BAR) when you STORE TO, the change still lands and stays the song in force.",
                  "STORE in any plugin's header stores just that plugin into the selected song."])
story.append(H2("RECALLING A SONG"))
story += bullets(["Send Program Change on MIDI channel 16 to SF CONTROL (program 1 = song 01). Or turn SF CONTROL's SONG.",
                  "SF CONTROL decides the bar line and sends it with the song (and the song's KEY), so every plugin changes on the same bar whatever order Live processes the tracks in. A Program Change up to a sixteenth after a bar line counts for that bar (a sequencer sends it on its downbeat, and one clocked by Live plays a little after Live's), a turn of SONG up to 30 ms after it; later in the bar, for the next one. Inside an Arrangement loop that never reaches that bar, every plugin changes on the next bar line after the loop jumps back.",
                  "The header of every plugin shows SONG nn, and WAIT BAR in green while the recall is waiting for the bar line.",
                  "Recalled values are reported to Live, so the E16 rings jump to the new song's settings. A recalled knob is yours again the moment you turn it.",
                  "A plugin with nothing stored in that song stays as it is.",
                  "With SLOW ALL (SF CONTROL) or a plugin's own SLOW on, the recall glides instead of jumping: a song change becomes an 8- or 16-bar transition.",
                  "Arrangement automation wins over recalled values, as on any device.",
                  "A preset loaded right after a recall (Live's browser or undo, or the PRESET menu) wins, whatever its values. A song change still waiting for its bar, or a STORE not yet made, in that plugin when a preset from Live comes in is dropped there: ask again."])
story.append(H2("VST3 AND PROGRAM CHANGE"))
story.append(P("VST3 plugins don't receive MIDI Program Change as MIDI. SF CONTROL therefore lists 16 programs (SONG 01-16); Live 12.2 and later turn a Program Change on the track into a program change for the VST3, older versions don't. The AU version reads Program Change directly from MIDI in any version. If song changes don't arrive in the VST3, use the AU."))
story.append(H2("PRESETS"))
story += bullets(["The <b>PRESET</b> button in every header opens the menu: factory presets grouped by style (DUB, ACID, HYPNOTIC ...), your own presets, SAVE USER PRESET and SHOW PRESET FOLDER. The button shows the preset that is loaded.",
                  "A preset sets every sound control of the plugin, MOTION included, and Live is told about every change, so the E16 rings follow. It never touches SLOW, MACRO, ALLOC, ROLE, a source setting or KEY FOLLOW.",
                  "219 factory presets across 27 plugins. Use them as starting points, then store the result into a song.",
                  "<b>RANDOMISE THE UNLOCKED</b> (in the PRESET menu) sets every unlocked sound control anywhere in its range, MODEL and other menus included; <b>NUDGE THE UNLOCKED</b> moves the unlocked knobs a little (up to 15 %) from where they are. Right-click a knob to lock or unlock it (a small padlock by its ring). At first LEVEL, FILTER, DENSITY, MIX, TUNE, SHIFT, ACID, WARP and ENERGY are locked, so a random patch stays in key and at its level. The locks are saved with the set.",
                  "User presets are files in Music/SF Live Suite/Presets/&lt;PLUGIN&gt;. Copy the folder to take them to another computer."])

story += [CondPageBreak(3 * inch), Spacer(1, 14)]
story.append(H1("3  POCKET: KICK-AWARE DUCKING"))
story.append(Rule())
story.append(P("SF KICK reports every hit to the other SF plugins: when it hit, how hard, its pitch sweep, how long it rings and how its filter is set. Any module with Pocket carves a kick-shaped hole in itself. No sidechain routing.", "summary"))
story.append(H2("TWO KNOBS"))
story.append(P("<b>DEPTH</b> (or PUMP, DUCK) sets how deep. <b>SHAPE</b> is one knob from transparent to pumping:"))
story += bullets(["<b>Left:</b> a narrow cut that follows the kick's pitch sweep down to its fundamental. The bass keeps its body; you feel more space without hearing a duck.",
                  "<b>Middle:</b> the low band (below about three times the kick's fundamental) ducks for the kick's length.",
                  "<b>Right:</b> full-band pump, release tied to the tempo."])
story.append(H2("IT FOLLOWS THE KICK"))
story += bullets(["<b>Velocity:</b> ghost kicks get shallow pockets.", "<b>DECAY:</b> lengthen the kick and every pocket lengthens.",
                  "<b>Pitch:</b> the cut follows the sweep wherever TUNE and PUNCH put it.",
                  "<b>Kick FILTER:</b> high-pass the kick and the low pockets close: the bass and rumble take over the sub. Bring the kick's low end back on the drop and the pockets open on the downbeat.",
                  "<b>Rolls:</b> on transparent settings fast hits merge into one dip; on pumping settings each hit pumps.",
                  "<b>Safety:</b> if kick tails overlap so much that the pocket would never let go, it shrinks to the attack.",
                  "<b>Breakdowns:</b> when the kick stops, pockets close; when it returns they fade back in over a beat."])
story.append(H2("WHO HAS POCKET"))
story.append(P("BASS, LEAD, PAD, VOICE (POCKET DEPTH / SHAPE), RUMBLE (PUMP), DELAY and REVERB (DUCK, on the wet signal only), DUCK (on anything), TAP (on any track). SF DUCK can also follow a sidechain input instead of the kick."))
story.append(H2("TIMING"))
story.append(P("Pocket looks 512 samples ahead (about 11 ms at 48 kHz) so a kick report is always there before it's needed. Live compensates the delay. Keep Live's buffer size at 512 or less. If the pump lands early or late in your setup, trim POCKET OFFSET on SF CONTROL (+/-20 ms) once, by ear."))
story.append(H2("AFTER A DROP"))
story.append(P("SF BUILD's DROP DUCK (off by default, up to 12 dB) makes every Pocket stage dip under each kick for DUCK TIME after the drop (default 1 bar), whatever its DEPTH, so the first kicks of the drop land on top of the parts and the tail coming back. It follows the kick: back within half a beat of each hit, and easing out over the second half of DUCK TIME. It starts with the first kick on the drop's bar line: a roll into the drop doesn't duck. TAP and STRIP set to ROLE KICK, PERC or DECK don't dip, like SF KICK, PERC, STRIKE and DECK."))
story.append(H2("LOOPS, RESTARTS AND JUMPS"))
story.append(P("Everything follows Live's song position. When it jumps (arrangement loop brace, stop and restart, clicking to a new position), kick reports from before the jump are ignored, so nothing ducks for a kick that is no longer playing. RUMBLE re-arms. Playing DECK tracks keep running and are re-anchored by whole beats, so they stay on the beat."))
story.append(PageBreak())

story.append(H1("4  ALLOC: THE MIX ASSIGNER"))
story.append(Rule())
story.append(P("ALLOC keeps sources from piling up in the same frequencies. You decide the order of importance; each frequency band has a priority curve over that order; the display shows what every source keeps and loses. Nothing is decided behind your back.", "summary"))
story.append(H2("THE ORDER"))
story += bullets(["Every SF instrument, STRIP, TAP and DECK reports its energy in ten fixed bands and appears in the SOURCES list (by its Live track name with VST3, otherwise by role: PERC, PERC 2 ...).",
                  "Drag the list into order, top = most important: the order for ALL BANDS. It is saved with the set and only changes when you drag it. New sources are added at the bottom.",
                  "Low bands and high bands usually want different orders (kick and bass matter at 60 Hz, hats and voice at 3 kHz). Click a band to show its order in the list and drag: the band now has its OWN order. LINK TO ALL returns it to the global order.",
                  "Each source applies its own cuts: audio never leaves its track."])
story.append(H2("THE CURVE (PER BAND)"))
story += bullets(["<b>PRIORITY</b> moves the peak of the curve up and down the list. The source at the peak is protected.",
                  "<b>DEPTH</b> sets how hard everything away from the peak is cut, up to MAX CUT. At 0 the band is untouched.",
                  "<b>Q</b> sets the width. Narrow: one source owns the band. Wide: the sources next to the peak share it.",
                  "Each knob always does the same thing to the same band, so once set it stays predictable."])
story.append(H2("DYNAMIC OR STATIC"))
story.append(P("<b>DYNAMIC</b> (default): a source is cut only while a more favoured source is actually playing in that band. When the bass stops, the pad gets 180 Hz back within a fraction of a second. <b>STATIC</b>: the cuts stay on all the time, like a fixed EQ."))
story.append(H2("READING THE DISPLAY"))
story += bullets(["Each band is a column; each source a lane, in that band's order and named in the lane. A selected band has a white frame; OWN marks a band with its own order.", "<b>Grey</b>: what the source plays in that band after ALLOC. <b>Amber</b>: what ALLOC takes away.",
                  "The white line is the curve (right = protected); it turns cyan while you turn one of that band's knobs.", "The name at the bottom of the band is the source the peak sits on."])
bands = [["BAND", "RANGE"]] + [[str(i + 1), r] for i, r in enumerate(["20-60 Hz", "60-100 Hz", "100-150 Hz", "150-220 Hz", "220-320 Hz", "320-480 Hz", "480-750 Hz", "750-1500 Hz", "1.5-4 kHz", "4-20 kHz"])]
story.append(H2("THE BANDS"))
story.append(table(bands, [0.9 * inch, 2.4 * inch]))

story.append(PageBreak())
story.append(H1("5  SOUND AND LEVELS"))
story.append(Rule())
story.append(P("The sets are the releases, so every plugin is built to release quality: nothing you hear should come from the implementation. These figures are measured by the offline tests on every build.", "summary"))
story.append(H2("GAIN STAGING"))
story.append(P("Every model of every instrument is calibrated to its role's loudness at default settings, so switching models never jumps the level and a rig with every LEVEL at its default is already a balanced rough mix with headroom into GLUE and LIMIT. SAMPLE models play the file at its own level."))
story.append(table([["ROLE", "TARGET (LUFS, INTEGRATED)", "ROLE", "TARGET"],
                    ["KICK", "-15", "LEAD", "-20"], ["PERC", "-22", "PAD", "-22"], ["RUMBLE", "-20", "VOICE", "-20"], ["BASS", "-18", "STRIKE", "-20"]],
                   [1.0 * inch, 1.9 * inch, 1.0 * inch, 1.2 * inch]))
story.append(H2("NO ALIASING"))
story.append(P("Every stage that distorts runs oversampled, all the time (switching an oversampler in and out would click), and every drive knob fades in from exactly clean: synth voices at 2x (the ladder, MS20-TYPE and SYNC models at 4x), KICK and PERC at 2x, DRIVE and STRIP at 4x, the drive in FILTER, ENERGY and VOICE, the saturation inside the DELAY loop, RUMBLE's GRIT, LOWEND's SUB CLIP, STRIKE's fracture and TONE at 2x, LIMIT's clipper at 4x. The filters reject images and aliases by 112 dB or more and are flat to 20 kHz. Oscillators are band-limited (wavetables per octave, corrected hard sync): measured alias products sit 77 to 105 dB below the note."))
story.append(H2("THE MASTER"))
story += bullets(["SF LIMIT measures true peak (the level between samples that a DAC or a codec will produce) at 8x, and its output never exceeds CEILING. Use -1 dB for venues and releases.",
                  "Its meter follows ITU-R BS.1770-4 / EBU R128 exactly: MOMENTARY (400 ms), SHORT-TERM (3 s), INTEGRATED (gated, over the whole set since RESET). It passes the EBU Tech 3341 level, gating and short-term cases within 0.02 LU.",
                  "SF LIMIT AUTO keeps the loudest moments of the last minute at TARGET and holds through breakdowns, so the drop comes back where it was. AUTO MODE DOWN ONLY (the default) never lifts the set above unity.",
                  "SF TONE voices the whole set (LOW, MUD, PRESENCE, AIR, TILT); SF ROOM EQ corrects the room, per venue. Keep them separate: TONE is the record, ROOM EQ is the venue, and only the venue hears ROOM EQ (below)."])
story.append(H2("ANY BUFFER SIZE, ANY SAMPLE RATE"))
story.append(P("Every plugin processes in 64-sample steps on a fixed grid of the audio stream, whatever Live's buffer size, so a set sounds exactly the same at buffer 64 and buffer 2048 (bit for bit) and at 44.1, 48 or 96 kHz. Song recalls land on the exact sample of the bar line."))
story.append(H2("RECORDING A SET"))
story += bullets(["Record the Main output (after SF LIMIT, before SF ROOM EQ) at the session's sample rate, 24-bit or 32-bit float: an audio track REC with Audio From = Main, Post Mixer, Monitor Off, armed. Live records what comes into a track, before its devices.",
                  "The room's correction stays out of the release: SF ROOM EQ sits on the PA track (see Setting up Live), or do the venue EQ in the audio interface's own mixer, after Live.",
                  "Press RESET on SF LIMIT before you start: INTEGRATED then reads the loudness of the whole set.",
                  "MOTION's RANDOM and DRIFT are seeded and the seeds are saved with the set, so they move the same way every time the set is played."])

story += [CondPageBreak(3 * inch), Spacer(1, 14)]
story.append(H1("6  SETTING UP LIVE"))
story.append(Rule())
story.append(H2("TRACKS"))
lay = [["WHERE", "WHAT"], ["MIDI track", "SF CONTROL, input from the OXI One. Makes no sound."],
       ["MIDI tracks", "SF KICK, SF PERC (2-3), SF RUMBLE (no clips needed), SF BASS, SF LEAD (1-2), SF VOICE, SF PAD, SF STRIKE, SF DECK."],
       ["Groups", "SF ENERGY on each group, PROFILE to match (DRUMS, BASS, MUSIC, VOICE)."],
       ["Returns", "SF DELAY, SF REVERB, SF DENSITY, each with DRY off (the return gives only the effect)."],
       ["Main (master)", "SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND > SF LIMIT. SF ALLOC anywhere on it. Its output to an output pair you don't use."],
       ["Audio track PA", "Audio From = Main, Post Mixer (the Main output), Monitor In, SF ROOM EQ, then SF LIMIT as a safety (CEILING -0.5 dB, CLIP 0 %, AUTO off), Audio To = the PA outputs, fader at 0 dB. The venue hears ROOM EQ."],
       ["Audio track REC", "Audio From = Main, Post Mixer, Monitor Off, armed: records the set without ROOM EQ."],
       ["Audio track ROOM", "Audio From = the room mics (a stereo pair), Monitor In, SF CAPTURE, Audio To = the outputs a speaker can go on (the headphones), sends disabled. SF TERROIR captures the place from here (SF CAPTURE); armed, it records the room with the set."],
       ["Live instruments / hardware", "SF STRIP last on the track: the suite's performance row, Pocket and ALLOC for any sound."],
       ["Stems", "SF TAP with ROLE set (no sound of its own), or SF STRIP to play them."]]
story.append(table(lay, [1.3 * inch, W - 1.3 * inch]))
story.append(H2("THE E16"))
story += bullets(["Map with Live's MIDI Map (Cmd-M): unfold the plugin's parameter panel (the triangle in its title bar), click the parameter there and turn the E16 knob. A parameter that isn't listed: Configure, touch it in the plugin window, Configure off. Turn to a PERF page before you click a parameter: a PERF page sends TRACK FOCUS's CC as it comes up, and Cmd-M would map the selected parameter to it.",
                  "In Live's MIDI preferences turn <b>Remote</b> on for the E16's Port 1, input <i>and output</i> (Ports 2 and 3 off). Live then sends values back, so the rings follow song recalls and ALLOC slot changes and the knobs hold the new values.",
                  "Absolute 0-127 CC is right; the plugins smooth every step. Chapter 7 has two ready-made E16 scenes for the show template, PERF and MIX, and the show set with every address already mapped."])
story.append(H2("SAMPLES"))
story.append(P("Every instrument has a SAMPLE model. Drop an audio file on the plugin window or choose LOAD &gt; FILE.... The sample is analysed (root note, decay, onsets) and embedded in the Live set, so the set travels without the file."))
story.append(P("<b>RESAMPLE</b> (LOAD &gt; RESAMPLE 1, 2 or 4 BARS): from the next bar line, what the plugin plays becomes its sample, and a WAV in Music/SF Live Suite/Resamples. LOAD reads WAIT until the bar, then REC. Hold a chord on PAD and resample it, then play it on SAMPLE, GRANULAR, or WAVETABLE with TABLE on SAMPLE (eight frames from the sample, COLOR moves across them)."))
story.append(P("The part of the sample that plays is set like an Ableton clip's: START and END, with FADE IN after START and FADE OUT before END (the SAMPLE group, folded at first). With a sample loaded the display shows its waveform, the part that plays bright, the fades as ramps and START and END as lines: drag either line (it takes the nearer one; near the sample's ends it snaps to them). VOICE draws them on its phrase. The instrument's own shaping (DECAY, ATTACK, GATE) still works on each note on top. The fades run in the sample's own time: played an octave up, they pass in half the time. A cut inside the waveform always fades for at least 1 ms, so it never clicks. At their defaults (0 %, 100 %, 0 ms, 0 ms) the sample plays whole, exactly as before these existed. Presets leave them alone (a preset has no sample); songs keep them like any knob."))
# The two pictures (docs/manual/make_sample_pictures.py, at twice the strip's scale), side by side.
pics, col = [], (W - 0.3 * inch) / 2
for name, caption in (("SAMPLE_KICK", "SF KICK with a one-shot: END at 62 %, FADE OUT 150 ms. The part that plays is bright."),
                      ("SAMPLE_VOICE", "SF VOICE: START on the second word, END in the last. The slices (C3, C#3 ...) are cut from the part between.")):
    img = os.path.join(ROOT, "docs", "snapshots", f"{name}.png")
    if os.path.exists(img):
        from PIL import Image as PI
        iw, ih = PI.open(img).size
        im = Image(img, width=col, height=col * ih / iw); im.hAlign = "LEFT"
        pics.append([im, Spacer(1, 3), P(esc(caption), "smallgrey")])
if pics:
    t = Table([pics], colWidths=[col + 0.3 * inch, col])
    t.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                           ("RIGHTPADDING", (0, 0), (0, 0), 0.3 * inch), ("RIGHTPADDING", (1, 0), (1, 0), 0), ("TOPPADDING", (0, 0), (-1, -1), 4), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
    story.append(t)
story.append(H2("FIRST SESSION CHECKS"))
story += bullets(["SF CONTROL shows <b>BUS SHARED</b>. Play SF KICK: it shows <b>KICK REPORTING</b>.",
                  "SF KICK + SF BASS with POCKET DEPTH up and SHAPE right: the pump lands exactly on the kick.",
                  "SF ALLOC on the master: every SF source appears in the list; raise DEPTH on a band where two of them overlap and watch the amber.",
                  "Store two songs; switch with Program Change: the E16 rings follow. With SF FILTER on tracks above and below SF CONTROL, every track changes on the same bar.",
                  "SF DECK: load a track, PLAY A: it starts on the bar, locked to Live's tempo.",
                  "Set KEY on SF CONTROL: BASS, LEAD and PAD transpose, KICK and RUMBLE retune.",
                  "Record a minute on REC with a deep cut on ROOM EQ: the PA has the cut, the recording doesn't."])
story.append(P("Chapter 7 is a complete show set built on this layout."))
story.append(PageBreak())

# ---------------- 7: the show template
story.append(H1("7  THE SHOW TEMPLATE"))
story.append(Rule())
story.append(P("One Live set for every show, built on fixed roles. The OXI ONE plays the notes and patterns; the E16 plays the sound, the balance and the transitions. One number per track: a track's number is its MIDI channel on the ONE and on the E16, and its page in the E16 scene PERF, so Live, the ONE and the E16 read the same way.", "summary"))
story.append(P("The set is built: <b>controllers/live/SF SHOW.als</b> in the repository holds every track, group and return below, every SF plugin with its starting settings, the routing, and every address of both E16 scenes mapped. Every instrument's MIDI From is set to its OXI One port and channel; PA and REC take the Main. Set Live's own preferences once per computer (its README, section 1: the MIDI ports, audio, Takeover Mode), check its outputs against your interface before any sound reaches a PA, play, and save it as your template."))

story.append(H2("TRACKS"))
story.append(table([["TRACK", "DEVICES", "MIDI FROM"],
    ["CONTROL", "SF CONTROL", "ONE port A, ch 16"],
    ["1 KICK", "SF KICK; SF ENERGY", "ONE port A, ch 1"],
    ["DRUMS (group)", "SF ENERGY, PROFILE DRUMS; SF REPEAT", ""],
    ["2 CLOSED HAT", "SF PERC: closed hat; SF ENERGY", "port A, ch 2"],
    ["3 OPEN HAT", "SF PERC: open hat or ride; SF ENERGY", "port A, ch 3"],
    ["4 CLAP", "SF PERC: clap or rim; SF ENERGY", "port A, ch 4"],
    ["5 PERC", "SF STRIKE; SF ENERGY", "port A, ch 5"],
    ["LOW (group)", "SF ENERGY, PROFILE BASS", ""],
    ["6 BASS", "SF BASS; SF ENERGY", "port A, ch 6"],
    ["7 RUMBLE", "SF RUMBLE, SOURCE KICK BUS; SF ENERGY", "none: it follows the kick"],
    ["MUSIC (group)", "SF ENERGY, PROFILE MUSIC", ""],
    ["8 LEAD", "SF LEAD: ACID 303-TYPE or another mono lead; SF ENERGY", "port A, ch 8"],
    ["9 CHORD", "SF LEAD: chords and stabs; SF ENERGY", "port A, ch 9"],
    ["10 PAD", "SF PAD, often GRANULAR; SF ENERGY", "port A, ch 10"],
    ["11 VOICE", "SF VOICE: phrases, radio fragments, choir; SF ENERGY", "port A, ch 11"],
    ["BRIDGE", "Live audio clips", ""],
    ["DECK", "SF DECK", "port B, ch 7 (hot cues; optional)"],
    ["LIVE BUS", "SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND; SF ALLOC (off at first)", ""],
    ["PA", "SF ROOM EQ > SF LIMIT (safety)", "audio: Main, Post Mixer"],
    ["REC", "nothing", "audio: Main, Post Mixer"],
    ["ROOM", "SF CAPTURE", "audio: Ext. In 1/2, the room mics"],
    ["A DUB (return)", "SF DELAY, DRY off", ""],
    ["B SPACE (return)", "SF REVERB, DRY off", ""],
    ["Main", "SF LIMIT", ""]], [1.35 * inch, 3.25 * inch, W - 4.6 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Tracks are named by their role, not their instrument: a track keeps its name, number, channel and E16 page when another instrument goes on it. Map its page to the new instrument (Cmd-M) and nothing else moves.",
                  "Every instrument track ends in SF ENERGY, with the profile of its part: its ENERGY knob on the E16's NRG page. The groups have one each too.",
                  "The kick stays outside DRUMS and LOW, so the drum group's REPEAT and ENERGY never take it, and the bass and the rumble fade together while the kick holds the floor. In each section decide whether BASS or RUMBLE carries the sustained low end: one of two knobs then steers it.",
                  "The PERCs are separate instruments: CHOKE works inside one track only, so a closed hat doesn't cut the open hat on its own track. Write the open hat's rhythm and DECAY for that.",
                  "CHORD: if the ONE sends whole chords, set the plugin's CHORD to NONE; if it sends single notes, CHORD (or the model's own chord) voices them. Not both.",
                  "LEAD's ACID 303-TYPE slides when notes overlap, always over 60 ms like a 303: write legato notes for slides. GLIDE doesn't change it.",
                  "VOICE with a phrase loaded: C3 plays slice 1, C#3 slice 2 and so on; below C3, the whole phrase. Sequence it as phrase choreography, not as a pitched sampler.",
                  "Keep the names: SF ALLOC lists its sources by Live track name, and SF LEAD, PERC and ENERGY find their E16 knobs by it (VST3)."])

story.append(H2("ROUTING"))
story.append(RoutingDiagram())
story.append(Spacer(1, 6))
story.append(table([["TRACK", "INPUT / MONITOR", "OUTPUT"],
    ["Instrument tracks", "MIDI From as above, Monitor In", "their group; 1 KICK to LIVE BUS"],
    ["Groups, returns A and B", "", "LIVE BUS"],
    ["LIVE BUS", "the tracks routed to it, Monitor In, every send disabled", "Main"],
    ["BRIDGE", "clips, Monitor Off (clips still play)", "Main"],
    ["DECK", "MIDI From port B ch 7, Monitor In", "Main"],
    ["Main", "", "an output pair that goes nowhere"],
    ["PA", "Audio From Main, Post Mixer, Monitor In, sends disabled", "the PA outputs"],
    ["REC", "Audio From Main, Post Mixer, Monitor Off, sends disabled", "Sends Only"],
    ["ROOM", "Ext. In 1/2 (the room mics, a stereo pair), Monitor In, sends disabled", "the headphone outputs: a speaker on them for a capture"],
    ["Cue", "Live's Cue Out; DECK's CUE track", "the headphone outputs"]], [1.75 * inch, 2.75 * inch, W - 4.5 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Main is the set. Exporting or resampling it gives the release mix; the venue's correction (ROOM EQ) exists only on PA, so the recording never hears it.",
                  "<b>Disable every send on LIVE BUS</b> (right-click a send, Disable Send). The returns feed LIVE BUS, and Live can't compensate a return's delay on a track whose matching send is active, even at zero (Ableton's Delay Compensation FAQ).",
                  "Never send LIVE BUS to PA: BRIDGE, DECK and SF LIMIT would be left out of the PA, and REC would record only BRIDGE and DECK.",
                  "Pull LIVE BUS down and the live instruments and their effects go while BRIDGE and DECK keep playing. The audio interface's output knob is the PA's volume and the emergency stop: turn it down and the room goes quiet while REC keeps recording. Nothing that would stop the set is in Live or on the E16.",
                  "BRIDGE is for transitions and regrouping. It still needs Live: it is not a backup for a computer failure.",
                  "PA's SF LIMIT is a safety: CEILING -0.5 dB, no clip, AUTO off. The set's peaks are already at -1 dBTP, so it only catches what the room EQ's phase pushes past. Keep PA's fader at 0 dB: the PA's level is the interface's.",
                  "ROOM is the room: armed, it records the mics with the set, for the mix. Its SF CAPTURE never passes them, so the set never hears the mics; for SF TERROIR it plays the sweep to a speaker on the headphone out (transport stopped) and records the place (SF CAPTURE)."])

story.append(H2("THE MASTER DEVICES"))
story += bullets(["Start GLUE, TONE and LOWEND gently: the set doesn't need every processor working all the time. Soundcheck the busiest passage of the night and set LIMIT's INPUT for 1-3 dB of limiting there.",
                  "LIMIT AUTO off while you set the gain staging, then on, AUTO MODE DOWN ONLY, for shows: it stops the set creeping above TARGET and holds through breakdowns.",
                  "SF ALLOC off at first. Turn it on band by band once the arrangement is known. It measures each source before its LEVEL and before Live's fader, so a source you've faded out still claims its bands: take a source out with its lane mute or its FILTER, or switch its ALLOC off."])

story.append(H2("THE OXI ONE"))
story.append(P("Every part plays on port A, on its Live track's number: channel = track. SF SHOW.als listens to each on its own track (Live lists port A as OXI ONE MKII (Port 1), port B as (Port 2)), never All Ins / All Channels."))
story.append(table([["SEQUENCER", "MODE", "OUTPUT", "TRACK"],
    ["1 DRUMS", "Multitrack: a track per drum, the kick on the bottom row", "1A-5A", "1 KICK, 2 CLOSED HAT, 3 OPEN HAT, 4 CLAP, 5 PERC"],
    ["2", "Mono", "6A", "6 BASS"],
    ["3", "Mono", "8A", "8 LEAD"],
    ["4", "Mono to start; Chord or Poly for whole chords", "9A", "9 CHORD"],
    ["5", "Mono to start; Poly for chords", "10A", "10 PAD"],
    ["6", "Mono", "11A", "11 VOICE"],
    ["7", "spare (SF START: Mono)", "7B", "DECK's hot cues"],
    ["8", "spare (SF START: Mono)", "16A", "CONTROL (a Program Change, for an AU of it)"]], [1.0 * inch, 1.9 * inch, 0.8 * inch, W - 3.7 * inch]))
story.append(Spacer(1, 6))
story += bullets(["The multitrack's rows, bottom up: 8 KICK 1A, 7 PERC 5A, 6 CLAP 4A, 5 OPEN HAT 3A, 4 CLOSED HAT 2A. 7 RUMBLE needs no lane: SOURCE KICK BUS follows SF KICK.",
                  "KICK lane on C1: with KEY TRACK on, any other note transposes the kick (C1 = TUNE), so C1 is right either way. PERC plays any note.",
                  "KEY OFF on SF CONTROL to start with: the ONE's notes set the pitch, and KICK and RUMBLE are tuned per song with TUNE. Turn KEY on for songs that should change key without reprogramming the ONE (mind the low register, chapter 1).",
                  "One ONE project per track, with a pattern per section: SPARSE, MAIN, VARIATION, EXIT, each a copy of the startup project SF START (controllers/oxi-one: every sequencer as above, four scenes of simple patterns). The song comes from the E16 (NEXT SONG and GO), never from the project: any song plays with any project's patterns. Program Change stays off on every sequencer.",
                  "When patterns change is set on the ONE (its launch and arranger settings): Live's launch quantization doesn't reach it."])

story.append(H2("CLOCK, MIDI AND LATENCY"))
story.append(table([["LIVE MIDI PORT", "TRACK", "SYNC", "REMOTE"],
    ["OXI ONE MKII (Port 1) in: the ONE's port A", "on", "off", "off"], ["OXI ONE MKII (Port 2) in: its port B", "on", "off", "off"],
    ["OXI ONE MKII (Port 1) out: the clock", "off", "on", "off"], ["OXI E16 (Port 1) in", "off", "off", "on"], ["OXI E16 (Port 1) out", "off", "off", "on"],
    ["Every other port, E16 Ports 2 and 3 included", "off", "off", "off"]],
    [2.6 * inch, 0.9 * inch, 0.9 * inch, W - 4.4 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Live is the clock master. On the ONE: sync USB (stopped, Shift + Stop until the header shows USB, not INT); in its config, Start Stop Msgs Ignore No, MIDI Transport Msgs Send Off, MIDI Thru off, Auto Sync on (it starts with Live). Never send clock both ways.",
                  "Live sends its clock only while it plays: start and stop from Live. Started with its own Play, the ONE runs at its own tempo and ignores Live's.",
                  "Audio: 48 kHz, buffer 256 (Pocket needs 512 or less), Delay Compensation on, <b>Reduced Latency When Monitoring off</b> (it skips compensation on monitored tracks, so returns and Pocket drift), launch quantization 1 bar, record quantization off.",
                  "The ONE plays a little after Live's beat (the clock goes out, the notes come back: on Nov 7, 40 ms), and nothing needs it on the beat: SF BUILD lands its drop on the kick it hears (LAND ON KICK, with a KICK readout), and Pocket and the effects that cut on the beat (GATE, STRIP, ENERGY's chop, DUCK's GRID, REPEAT, DECK) follow where SF KICK's kicks land. Leave the ONE port's MIDI Clock Sync Delay at 0 (on Nov 7, -70 ms moved nothing). Leave POCKET OFFSET at 0 unless the pump sounds off.",
                  "Every Pocket stage in a chain adds its 512-sample lookahead to the total latency (compensated, but it adds up): no SF TAP or SF DUCK on tracks that already hold SF instruments."])

story.append(CondPageBreak(E16Grid.H + 1.6 * inch))
story.append(H2("THE E16"))
story.append(P("Two E16 scenes run the whole rig, in the repository under controllers/e16 with their address map (sf-show-map.csv) and loading steps (README); SF SHOW.als has every address mapped. <b>PERF</b> is the show: pages 1-11 are the tracks (page n is track n, titled with its code), page 12 is NRG. <b>MIX</b> holds the levels, the returns and the macros. On the E16 every track goes by a three-letter code: KCK, CHH, OHH, CLP, PRC, BAS, RMB, LED, CRD, PAD, VOX, and the groups DRM, LOW, MUS (an ENERGY knob reads KCK NRG, a volume KCK VOL). The E16 is a 4 &times; 4 grid, numbered row by row from the top left. A track's page:"))
PRESS_DOES = {"SLOW ALL": "a quick press flips knob 1 to SLOW ALL on SF CONTROL (green ring): how long your next moves take, OFF to 128 BAR; another press flips it back",
              "GO": "calls the song NEXT SONG picked, on the next bar line (nothing if it is already the song in force)",
              "DROP": "first press: everything stops on the next sixteenth, the tails ring; second press: the drop on the next kick", "ENERGY": "the knob back to the centre, neutral",
              "THROW": "sends into the effect while held", "FREEZE": "holds the effect until pressed again",
              "ROLL": "the drums roll while held"}
def press_text(p, full=True):
    kind = {"SWAP": "swap", "HOLD": "hold", "TOGGLE": "toggle", "CENTRE": "to the centre"}[p.mode]
    name = "SLOW ALL" if p.mode == "SWAP" else p.name
    return f"{name} ({kind})" + (f": {PRESS_DOES[name]}" if full else "")
cells = {i: ("SOUND", "ITS OWN" if i > 1 else "PRESS: SLOW ALL", INK, True) for i in range(1, 9)}
cells.update({9: ("GATE", "OR DECAY", INK, False), 10: ("DENSITY", "", INK, False), 11: ("FILTER", "", INK, False),
              12: ("LEVEL", "", CYAN, False), 13: ("SEND A", "INTO A DUB", CYAN, False), 14: ("SEND B", "INTO B SPACE", CYAN, False),
              15: ("NEXT SONG", "PRESS: GO", INK, False), 16: ("BUILD", "PRESS: DROP", INK, False)})
story.append(Spacer(1, 2))
story.append(E16Grid(cells, bands=[(1, 2, "ITS SOUND", "ROWS 1 AND 2"), (3, 4, "EVERY TRACK", "ROWS 3 AND 4")],
                     note="LIGHT BLUE RING: A LEVEL OR A SEND.  15 AND 16 ARE THE SAME KNOBS ON EVERY PERF PAGE, NRG INCLUDED."))
story.append(Spacer(1, 8))
show_presses = {}
for t, _ in E16.SHOW_PAGES:
    for i in (1, 15, 16):
        show_presses[i] = E16.press_for(t, i)
story += bullets([f"<b>Press {i}, on every PERF page:</b> {esc(press_text(pr))}." for i, pr in sorted(show_presses.items())]
                 + ["A knob is easily pressed while it is turned, so every press on the E16 is one the set plays through if it happens by accident. Nothing that would stop the set is on the E16: the PA's emergency stop is the audio interface's output knob."])
story.append(CondPageBreak(E16Grid.H + 30))
story.append(P("NRG, page 12: every ENERGY knob, in the tracks' order, then ACID, DIST and WARP."))
cells = {}
for i, c in enumerate(E16.encoders("NRG"), 1):
    pr = E16.press_for("NRG", i)
    sub = c.label if pr is None else c.label.replace(" ENERGY", "") if pr.mode == "CENTRE" else "PRESS: SLOW ALL" if pr.mode == "SWAP" else f"PRESS: {pr.name}"
    cells[i] = (c.name, sub, INK, False)
story.append(E16Grid(cells, note="KNOB N IS TRACK N'S SF ENERGY: A PRESS SETS IT BACK TO THE CENTRE (KNOB 1: SLOW ALL). 12-14 ACID, DIST, WARP."))
story.append(Spacer(1, 8))
story += bullets(["The ENERGY knobs are a DJ filter per track, plus drive, space, echo and chop: centre is neutral, left takes energy out, right puts it in. Each track's own ENERGY shapes that part. A drum track's ENERGY (PROFILE DRUMS) at zero takes the part out entirely: KCK NRG down, no kick. The groups keep their SF ENERGY, at the centre (MACRO 1 can move it).",
                  "NRG 12-14 [Nov 10]: <b>ACID</b> (8 LEAD's SF LEAD: RES, ENV AMT, ACCENT, DRIVE and CUTOFF up together, the 303 squelch), <b>DIST</b> (SF DRIVE's MIX on the LIVE BUS: the whole mix from clean to driven, tube into fuzz, its lows under 120 Hz clean and its level held), <b>WARP</b> (10 PAD's SF PAD: tape wow and flutter on every voice its own way, more spread and ensemble; on GRANULAR the grains scatter). Each off at zero; their presses do nothing.",
                  "Each instrument's LEVEL belongs to its song; the volumes on MIX's LEVELS page are your balance on the night. Set every LEVEL at soundcheck so a volume at 0 dB gives the intended mix: every volume and send is mapped with a maximum of 0 dB.",
                  "Rings by role, in the OXI App's colour numbers (layout.py holds them): white (34) an instrument's own sound, knobs 1-8; red (63) a track's GATE or DECAY, DENSITY and FILTER (9-11) and its NRG knob; light blue (18) a level: LEVEL, the sends, every volume; pink (50) NRG's ACID, DIST and WARP; violet (9) NEXT SONG and BUILD; green (90) knob 1 on SLOW ALL. Each plugin's window draws its E16 knobs in the same colours. A knob that is travelling (SLOW) turns green and walks with the sound until it arrives (GLIDE RINGS: SF CONTROL's E16 RINGS and the scenes' script; controllers/e16 README).",
                  "<b>TRACK FOCUS:</b> turn to a PERF track page and Live selects that track (page 6, BAS: 6 BASS), with the control surface SF E16 installed (controllers/live/SF_E16, into the User Library's Remote Scripts; Settings > Tempo & MIDI: SF E16, Input E16 Port 1, Output None). NRG and MIX's pages leave the selection where it is. With Auto-Hide Plug-In Windows on (Settings > Plug-Ins), the track's open plugin windows come up with its page. The page sends CC 119 on channel 16: keep it unmapped (check_set.py fails a set that maps it)."])

story.append(H2("THE E16: PAGES"))
rows = [["PAGE", "KNOBS 1-8", "KNOBS 9-16", "PRESSES"]]
for scene, pages in E16.SCENES:
    for n, (title, controls) in enumerate(pages, 1):
        names = [c.name if c is not None else "-" for c in controls]
        pr = [f"{i} {press_text(E16.press_for(title, i), False)}" for i in range(1, 17)
              if E16.press_for(title, i) is not None and E16.press_for(title, i).mode != "CENTRE" and not (title, i) in [(t, k) for t, _ in E16.SHOW_PAGES for k in (1, 15, 16)]]
        if title == "NRG": pr.append("2-14 centre")
        while names and names[-1] == "-": names.pop()
        rows.append([f"{scene} {n}: {title}", esc(" / ".join(names[:8])), esc(" / ".join(names[8:])) or "-", esc(", ".join(pr))])
story.append(table(rows, [1.25 * inch, 2.35 * inch, 2.1 * inch, W - 5.7 * inch], pad=2.4))
story.append(Spacer(1, 6))
story += bullets(["On every track page row 3 is GATE (DECAY where the instrument has none), DENSITY, FILTER and LEVEL, and row 4 the track's two sends, NEXT SONG and BUILD: the hand learns one layout. Rows 1 and 2 are the instrument's own, with a swap where a model's character lives elsewhere (GRANULAR: COLOR is grain size, DETUNE is scatter; STAB DUB: SPACE is the ensemble).",
                  "The page titles are the tracks' codes: KCK is 1 KICK's page, CRD 9 CHORD's.",
                  "MIX's LEVELS page has NRG's places: knob n is track n's volume, 12-14 the groups', 15 and 16 the returns'. On SPACE the presses sit under their knobs: 1-4 are DUB, 5-8 SPACE. On MACROS, hold ROLL on 9 and turn LENGTH under the same finger.",
                  "The closed hat's page is the fill page: DENSITY with ECHO SPACE 1/16 turns 1/8 hats into 1/16s, the send-delay trick built in."])

story.append(H2("THE E16: ADDRESSES AND SETTINGS"))
story += bullets(["<b>Channel = track:</b> 1-11 the instrument tracks (1 KICK ... 11 VOICE), 12 A DUB, 13 B SPACE, 14 the groups (DRUMS, LOW, MUSIC and DRUMS' REPEAT), 15 LIVE BUS, 16 CONTROL. Nothing else uses a track's channel, so a track's page can be mapped to any instrument on that track without touching another.",
                  "<b>CC = the knob's place + 11 in PERF</b> (a track's page: CC 12-23), <b>+ 43 in MIX</b>; a press its place + 27 in PERF, + 59 in MIX. A track's ENERGY is CC 28 (the groups on channel 14: 28, 29, 30).",
                  "<b>Mixer:</b> CC 102 volume, 103 send A, 104 send B, on the track's channel; channel 14: 102-104 the DRUMS, LOW and MUSIC volumes. A parameter on two pages (NEXT SONG, BUILD, SLOW ALL) sends the same address from both: two ways to one mapping. CC 6, 38 and 96-101 (data entry and RPN/NRPN) are never used. The appendix lists every address.",
                  "<b>Presses:</b> a hold is a CC, 127 on press and 0 on release, so the parameter is on while the knob is down. A toggle is a note: Live flips a switch on every Note On, so the E16 can never think FREEZE is on while Live has it off. A centre press is the knob's own CC with the value 64. The presses are in the scene files.",
                  "<b>On the E16:</b> Auto Transmit off, for both scenes and globally (stored values sent on a page change would pull Live's faders to them), MIDI Thru off. Changing pages or scenes sends no knob's value; a PERF track page sends its track's number (TRACK FOCUS, below).",
                  "<b>In Live:</b> E16 Port 1 Remote on, input and output; Ports 2 and 3 off (Port 1 is the one the E16 sends on, and a second Remote port could take a message twice: a toggle would flip twice). Takeover Mode: Value Scaling, so a knob that disagrees with Live after a recall moves the sound smoothly instead of jumping.",
                  "<b>Mapped once, kept:</b> MIDI mappings belong to the Live set. SF SHOW.als comes with every address mapped, every volume and send at Max 0 dB and DUB FEEDBACK at 0-70 %; save it as a template (File &gt; Save Live Set As Template) and new sets start mapped. In a set of your own, map once from the appendix and do the same.",
                  "<b>Map the E16 straight to the SF parameters, not through Instrument Rack macros.</b> A song recall changes the plugin's value underneath a macro: the macro doesn't follow, its ring shows the wrong value and the next turn jumps the sound back. Ranges go in Live's MIDI Mappings (Min and Max per mapping): give resonance and feedback the part of their travel that matters, and widen it when the music asks.",
                  "Check the shared knobs once: turn NEXT SONG on one page, go to another and turn it a step. The pick must move a step, not jump back. If it jumps, the README's relative fallback fixes it.",
                  "Keep STORE ALL, MODEL, KEY and the calibration controls off the performance pages."])

story.append(H2("MACROS"))
story.append(P("SF CONTROL's six macros, on MIX's MACROS page. Every SF plugin follows one macro (MACRO SRC) by an amount (MACRO AMT, negative turns it the other way); a macro moves the knobs marked M in the module tables, adds to them and is stored per song. A starting plan:"))
story.append(table([["MACRO", "FOLLOWED BY", "MOVES"],
    ["1 ENERGY", "the group ENERGY plugins, BUILD", "every group's energy at once (a negative amount on LOW darkens the bass while the rest lifts)"],
    ["2 MOOD", "PAD, CHORD, VOICE", "cutoff, envelope and drive; the voice's SIGNAL and ECHO"],
    ["3 DENSITY", "CLOSED HAT, OPEN HAT, CLAP, PERC", "DECAY and DENSITY: the fills"],
    ["4 PULSE", "KICK, RUMBLE", "kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY"],
    ["5 SPACE", "DUB, SPACE returns", "delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX"],
    ["6 FRACTURE", "LEAD, BASS", "cutoff, envelope, density and drive: smooth to heavy"]], [1.0 * inch, 2.1 * inch, W - 3.1 * inch]))
story.append(P("Start the amounts at 30-50 %. One macro per plugin: a plugin can follow only one.", "smallgrey"))

story.append(H2("RETURNS AND POCKET"))
story += bullets(["A DUB: SF DELAY, DRY off, TIME synced, FEEDBACK mapped to 0-70 % at first, DUCK up so the repeats sit around the kick. INPUT THROW ONLY makes its THROW press the only way in: dub throws.",
                  "B SPACE: SF REVERB, DRY off, DUCK 30-60 % so the tail blooms in the gaps, LO CUT up.",
                  "Sends post-fader: turning a source down stops feeding its effect while the tail rings on. Every track page has the track's two sends (13, 14). No return-to-return sends at first.",
                  "Pocket needs no wiring: SF KICK reports on the bus. SF CONTROL should show BUS SHARED and KICK REPORTING.",
                  "Pulling the kick's volume down leaves the kick playing on the bus: Pocket and RUMBLE keep pumping to a silent kick, which can be the point. For a clean withdrawal, mute the kick lane on the ONE."])

story.append(H2("SONGS, PATTERNS AND SCENES"))
story.append(table([["SYSTEM", "RESPONSIBLE FOR"],
    ["OXI ONE projects and patterns", "Notes, rhythms, gates, accents, the pattern progression."],
    ["SF songs (SF CONTROL)", "The sound of every SF plugin, called from the E16 (NEXT SONG, GO) and recalled on the bar."],
    ["E16 pages", "Where your hands are."],
    ["Live Session scenes", "BRIDGE clips, reference material, rehearsal."],
    ["Live Arrangement", "Recording the performance."]], [2.0 * inch, W - 2.0 * inch], mono_first=False))
story.append(Spacer(1, 6))
story += bullets(["The roles stay put for the whole set: a new track changes what they play, not where they are.",
                  "A song and a ONE project are independent: turn NEXT SONG (15) to the next song, press it on the bar before the change, and load any project's patterns on the ONE.",
                  "Rehearse song recall, SF REPEAT and SF BUILD's drop (the cut and the way back, TAIL, DROP DUCK) in your Live before relying on them in a transition."])

story.append(H2("RECORDING AND REHEARSAL"))
story += bullets(["Instrument tracks on Monitor In: they play from the ONE whether or not they're armed.",
                  "To record: RESET on SF LIMIT, arm the instrument tracks, REC and ROOM, Automation Arm on, Arrangement record, play. The stereo recording on REC is the set, the one on ROOM the room mics for the mix; the MIDI and automation are for editing and rehearsal.",
                  "To replay recorded MIDI: stop the ONE, set those tracks to Monitor Auto and disarm them (Monitor In ignores a track's own clips).",
                  "Collect All and Save. SF samples are inside the set; DECK tracks are referenced by path, so keep the files where they are."])

story.append(H2("BUILD ORDER"))
story += numbered(["SF SHOW.als (controllers/live): Live's preferences set once, the outputs checked against the interface; saved as a template.",
                   "CONTROL, 1 KICK, 6 BASS, 7 RUMBLE, LIVE BUS, PA and REC playing. Clock and latency calibrated, a short recording made.",
                   "2 CLOSED HAT, 3 OPEN HAT, 4 CLAP and 5 PERC.",
                   "8 LEAD, 9 CHORD, 10 PAD and 11 VOICE, one at a time.",
                   "The returns; check the sends are disabled on LIVE BUS.",
                   "BRIDGE and DECK.",
                   "PERF and MIX on the E16 (controllers/e16): both scenes loaded, every page checked against the set's mappings; the macros.",
                   "Rehearse moving between BRIDGE, DECK and LIVE BUS.",
                   "Record a whole set before adding anything else."])
story.append(CondPageBreak(2.2 * inch))
story.append(H2("BEFORE THE FIRST SHOW"))
story += bullets(["CONTROL shows BUS SHARED and KICK REPORTING; the pump lands on the kick.",
                  "NEXT SONG (15) turned to another song changes nothing; its press (GO) lands the song on the next bar line, every track on the same bar, and a recalled knob is yours the moment you turn it.",
                  "The interface's output knob down: the speakers silent, REC still recording.",
                  "Every E16 hold press (DROP, THROW, ROLL) held for three seconds stays on for the whole hold and goes off on release.",
                  "The shared E16 knobs don't jump: NEXT SONG turned on one page, then a step on another, moves a step. Changing to NRG or a MIX page sends nothing (Live's MIDI input indicator stays dark); a PERF track page selects its track (with SF E16 installed).",
                  "A minute recorded on REC with a deep ROOM EQ cut: the cut isn't in the recording.",
                  "With SF TERROIR in the set: a capture from ROOM into an empty slot ends SWEEP x3, 30 dB or more over the noise, and the PA stays silent through it."])
story.append(PageBreak())

# ---------------- PART 2: modules
order = [("INSTRUMENTS", ["KICK", "PERC", "RUMBLE", "BASS", "LEAD", "PAD", "VOICE", "STRIKE", "DECK", "CONTROL"]),
         ("EFFECTS", ["STRIP", "FILTER", "GATE", "DENSITY", "DELAY", "REVERB", "TERROIR", "CAPTURE", "DRIVE", "RADIO", "REPEAT", "DUCK", "ENERGY"]),
         ("MASTER AND MIX", ["ISOLATOR", "BUILD", "GLUE", "TONE", "LOWEND", "LIMIT", "ROOMEQ", "ALLOC", "TAP"])]
SYSTEM_IDS = ("slow", "macro_src", "macro_amt")
def is_system(pid): return pid in SYSTEM_IDS or pid[:3] in ("m1_", "m2_")
chapter = 8
for group, mods in order:
    first = True
    for mid in mods:
        m = C.MODULES[mid]
        sp = [p for p in specs[mid]["params"] if not is_system(p["id"])]
        title = "SF " + ("ROOM EQ" if mid == "ROOMEQ" else mid)
        if first:
            story.append(H1(f"{chapter}  {group}", 0)); story.append(Rule()); first = False; chapter += 1
            story.append(Spacer(1, 4))
        h = Paragraph(title, ParagraphStyle("mt", fontName="MonoB", fontSize=17, leading=21, spaceBefore=4, spaceAfter=2)); h.toc_level = 1
        img = os.path.join(ROOT, "docs", "snapshots", f"{mid}.png")
        if os.path.exists(img):
            from PIL import Image as PI
            iw, ih = PI.open(img).size
            story.append(CondPageBreak(STRIP_W * ih / iw + 1.6 * inch))      # the title, the summary and the whole strip together
        else:
            story.append(CondPageBreak(4.5 * inch))
        story.append(h)
        story.append(Paragraph(("INSTRUMENT, MIDI TRACK" if specs[mid]["instrument"] else "AUDIO EFFECT") + (f"   LATENCY {specs[mid]['latency']} SAMPLES AT 48 KHZ" if specs[mid]["latency"] else ""), S["label"]))
        story.append(Rule(thick=0.5, color=LIGHT, space=2))
        story.append(P(esc(m["summary"]), "summary"))
        # The plugin's window (a strip) beside USING IT.
        img = os.path.join(ROOT, "docs", "snapshots", f"{mid}.png")
        using = [H2("USING IT")] + bullets([esc(u) for u in m["use"]])
        if os.path.exists(img):
            from PIL import Image as PI
            iw, ih = PI.open(img).size
            w = STRIP_W; hgt = w * ih / iw
            im = Image(img, width=w, height=hgt); im.hAlign = "LEFT"
            side = Table([[im, using]], colWidths=[STRIP_W + 0.22 * inch, W - STRIP_W - 0.22 * inch])
            side.setStyle(TableStyle([("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 0),
                                      ("RIGHTPADDING", (0, 0), (-1, -1), 0), ("TOPPADDING", (0, 0), (-1, -1), 0), ("BOTTOMPADDING", (0, 0), (-1, -1), 0)]))
            story.append(side); story.append(Spacer(1, 8))
        else:
            story += using
        if m["models"]:
            story.append(H2("OBJECTS" if mid == "STRIKE" else "CAPTURING A PLACE" if mid == "TERROIR" else "MODELS" if mid not in ("GATE", "ENERGY") else "REFERENCE"))
            story.append(table([["SOURCE", "HOW"] if mid == "TERROIR" else ["NAME", "SOUND"]] + [[esc(a), esc(b)] for a, b in m["models"]], [1.55 * inch, W - 1.55 * inch]))
        if m.get("field"):
            story.append(H2("IN THE FIELD"))
            story.append(table([["STEP", "WHAT"]] + [[esc(a), esc(b)] for a, b in m["field"]], [1.55 * inch, W - 1.55 * inch]))
        desc = dict(C.COMMON); desc.update(m["desc"])
        def rng(p):
            if p["type"] == "knob": return f"{esc(p['min'])} to {esc(p['max'])}<br/>default {esc(p['def'])}"
            if p["type"] == "menu":
                ch = p["choices"].split("|")
                listing = ", ".join(ch) if len(ch) <= 8 else ", ".join(ch[:6]) + f" ... ({len(ch)})"
                return f"{esc(listing)}<br/>default {esc(p['def'])}"
            if p["type"] == "toggle": return f"ON / OFF<br/>default {esc(p['def'])}"
            return "HOLD"
        played = lambda p: p["primary"] or p.get("perform")
        for label, rows in (("PERFORM", [p for p in sp if played(p)]),
                            ("DETAIL", [p for p in sp if not played(p) and not p.get("setup") and not p.get("sample") and not p.get("layer") and not p.get("engine") and p["id"] != "model"]),
                            ("ENGINE", [p for p in sp if not played(p) and p.get("engine")]),
                            ("LAYER", [p for p in sp if not played(p) and p.get("layer")]),
                            ("SAMPLE", [p for p in sp if not played(p) and p.get("sample")]),
                            ("SETUP", [p for p in sp if not played(p) and p.get("setup")])):
            if not rows: continue
            story.append(H2(label))
            data = [["CONTROL", "RANGE / DEFAULT", "WHAT IT DOES"]]
            for p in rows:
                d = desc.get(p["id"], "")
                if not d: print("missing description:", mid, p["id"], file=sys.stderr)
                name = esc(p["name"]) + ("  <font name='Mono' color='#6b6b6b' size='6.5'>M</font>" if p["macro"] else "")
                data.append([name, Paragraph(rng(p), S["cellrange"]), esc(d)])
            story.append(table(data, [1.2 * inch, 1.6 * inch, W - 2.8 * inch]))
        story.append(Spacer(1, 4))
        story.append(P("M = follows the plugin's MACRO (MACRO SRC / MACRO AMT).", "smallgrey"))
        story.append(PageBreak())

# ---------------- Appendix
story.append(H1(f"{chapter}  APPENDIX"))
story.append(Rule())
story.append(H2("MIDI NOTE MAP"))
story.append(table([["PLUGIN", "NOTES", "DOES"],
                    ["SF KICK, PERC", "any", "Plays the sound (KICK: KEY TRACK on makes C1 = TUNE)."],
                    ["SF VOICE", "C3, C#3, D3 ...", "Slice 1, 2, 3 ... of the phrase. Below C3: the whole phrase."],
                    ["SF STRIKE", "any", "Strikes the object (KEY TRACK on: the note sets the pitch, C3 = TUNE)."],
                    ["SF DECK", "C1-D#1", "Deck A hot cues 1-4 (jump on the next beat)."],
                    ["SF DECK", "C2-D#2", "Deck B hot cues 1-4."],
                    ["SF CONTROL", "Program Change ch 16", "Song 01-16 (channel set by PC CHANNEL), for the AU: Live sends a VST3 none. The show calls songs from the E16 (NEXT SONG, GO)."],
                    ["SF LEAD / BASS ACID", "velocity > ~108", "Accent. Overlapping notes slide."]], [1.5 * inch, 1.4 * inch, W - 2.9 * inch]))
story.append(H2("WHERE THINGS ARE SAVED"))
story += bullets(["Songs, samples, ALLOC choices, MOTION random seeds, DECK tracklist and venue EQs: inside the Live set (plugin state).",
                  "User presets: Music/SF Live Suite/Presets/&lt;PLUGIN&gt;, one file per preset.",
                  "DECK track log: Documents/SF Live Suite/tracklog.txt.",
                  "SF TERROIR: its places are inside the Live set; the sweep file is in Music/SF Live Suite/TERROIR, and every take captured in the set (SF CAPTURE) in Music/SF Live Suite/TERROIR/Captures, one WAV each.", "DECK tracks are referenced by file path, not embedded: keep the files where they are."])
story.append(H2("WHEN SOMETHING ISN'T RIGHT"))
story.append(table([["SYMPTOM", "CHECK"],
                    ["SF SHOW.als: a plugin is missing", "The suite isn't installed, or VST3 is off (Settings &gt; Plug-Ins). Install or turn VST3 on, then open the set again."],
                    ["An instrument doesn't play", "MIDI From: each track listens to one ONE port and channel (chapter 7). Shown as missing: your Live names the ONE's ports otherwise; set each by the table. Track on for both ONE ports in Live's MIDI preferences. Never All Ins: every instrument would play every part."],
                    ["A group or track is silent", "Its volume (MIX, LEVELS) or ENERGY knob (PERF, NRG) at the far left; SF ALLOC on LIVE BUS with ENABLE on (it cuts lower-priority sources in its bands: ENABLE off to check); the group's output (every group to LIVE BUS)."],
                    ["Nothing ducks", "SF CONTROL: BUS SHARED and KICK REPORTING. The kick must be an SF KICK (or use SF DUCK with SOURCE SIDECHAIN)."],
                    ["Pump early or late", "Live buffer size 512 or less. Then trim POCKET OFFSET on SF CONTROL by ear."],
                    ["The ONE doesn't follow Live's tempo", "Start from Live, not the ONE: Live sends clock only while it plays. The ONE's header shows USB (stopped: Shift + Stop until it does). Sync on OXI ONE MKII (Port 1)'s output and on no other port. Hold Shift + Back for 2 s: the ONE's MIDI monitor shows the clock arriving while Live plays. Still nothing: Sync on Port 2's output instead, never both."],
                    ["Songs don't change", "From the E16: turn NEXT SONG (encoder 15 of any PERF page) to the song, then press it (GO); SF CONTROL shows NEXT in amber, PRESS GO, until you do. The song must have been stored (SONG nn EMPTY otherwise). Program Change reaches only an AU of SF CONTROL."],
                    ["E16 rings don't follow", "Remote on for E16 Port 1's output in Live's MIDI preferences; map with Live's MIDI Map."],
                    ["An E16 press does nothing", "A scene file from before Nov 8 is loaded: load the current PERF and MIX. A toggle that flips back: Remote is on for two E16 ports, so the note arrives twice. Port 1 only."],
                    ["An E16 knob jumps when touched", "A shared knob holding an old value: controllers/e16 README, check 2. Or Auto Transmit is on: off in the scene and globally."],
                    ["ALLOC does nothing", "ENABLE on, DEPTH above 0 on the band, the instruments' ALLOC toggles on. In DYNAMIC, the favoured source must be playing in that band."],
                    ["DECK row says MISSING", "The file moved. Drop it again."],
                    ["A knob shows GLIDE", "SLOW ALL or the plugin's SLOW is on: the knob is travelling to where you set it."],
                    ["A knob moves on its own", "MOTION: check M1 / M2 TARGET (the plugin's MOTION group; its title line says where they go)."],
                    ["Header shows FAULT (red)", "The plugin caught an invalid value or a runaway and reset itself to silence. It carries on working. If it happens again, note what was playing: it is a bug."],
                    ["SF TERROIR won't capture", "Its status line says why. The transport must be stopped; NO SF CAPTURE: put it on the room mics' track (ROOM in SF SHOW), its device on; one take at a time."],
                    ["A capture says THE SWEEP DIDN'T COME BACK, or under 30 dB", "The speaker didn't play the sweep, or the mics didn't hear it: the speaker on ROOM's output (3/4), the track on and up; the mics on ROOM's input (Input Config: 1/2 a stereo pair), Monitor In. Then louder or closer. The slot stays as it was."],
                    ["A capture says NO SWEEP IN THE RIGHT CHANNEL", "That mic heard nothing (its cable, its gain, Input Config): the place came in mono from the other. Fix it and capture again."],
                    ["A capture says CAPTURE STOPPED: SF CAPTURE STOPPED ANSWERING", "ROOM's SF CAPTURE was removed or switched off, or its track's audio stood still. Put it back and capture again: nothing was stored."],
                    ["Something won't stop", "The audio interface's output knob down: the room goes quiet, the recording carries on. Then find it. PANIC on SF CONTROL, held in its window, fades every SF plugin out and clears every voice, tail and delay line; Live's own clips and devices aren't SF."],
                    ["Header shows PANIC (red)", "PANIC is on at SF CONTROL: held in its window, or switched on by a mapping. Let it go there."],
                    ["CONTROL shows LOCAL ONLY", "The plugins come from different builds. Rebuild and install the whole suite."],
                    ["A return doubles the dry signal", "DRY off on SF DELAY, SF REVERB or SF DENSITY on a return track."],
                    ["Pitch is wrong after a song change", "KEY on SF CONTROL is stored per song. KEY FOLLOW off on an instrument ignores it."],
                    ["Levels changed after the update", "Since Nov 3 every model is calibrated to its role's target (chapter 5). Re-balance the faders once."]], [1.7 * inch, W - 1.7 * inch], mono_first=False))

story.append(CondPageBreak(3 * inch))
story.append(H2("E16 ADDRESS MAP (PERF, MIX)"))
story.append(P("Every address of the two E16 scenes once, by channel: the Live target, the mapping's range, and where the knob is on the E16 (a PERF page is named by its track's code; LEVELS, SPACE and MACROS are MIX's). SF SHOW.als has them all mapped; this list is for checking it, and for mapping a set of your own (Cmd-M). The same list is controllers/e16/sf-show-map.csv."))
rows = [["CH", "MESSAGE", "LIVE TARGET", "MAPPING", "ON THE E16"]]
for (msg, ch, n), (target, places, kind, rng, item) in sorted(E16.addresses().items(), key=lambda kv: (kv[0][1], kv[0][0], kv[0][2])):
    m = f"CC {n}" if msg == "CC" else f"NOTE {n} ({E16.note_name(n)})"
    how = {"TURN": "absolute", "HOLD": "press: hold", "TOGGLE": "press: toggle"}[kind] + (f", {rng}" if rng else "")
    rows.append([str(ch), m, esc(target), esc(how), esc(E16.where(places, getattr(item, "second", False)))])
story.append(table(rows, [0.35 * inch, 1.05 * inch, 2.55 * inch, 1.25 * inch, W - 5.2 * inch], mono_first=False, pad=2.3))

doc = Doc(os.path.join(ROOT, "docs", "SF-Live-Suite-Manual.pdf"))
doc.multiBuild(story)
print("ok")
