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
import layout as E16          # the SF SHOW scene's layout: one source for the scene file and these tables

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
story.append(P("The first eight controls of every plugin are its performance row: the first eight parameters Live shows for the device, so one page of the E16 plays one plugin."))
story.append(H2("THE PLUGIN WINDOW"))
story.append(P("Every plugin's window is a narrow strip, so several sit side by side. Top to bottom it goes from what you play to what you set once:"))
story += bullets(["<b>Header:</b> SONG, the OUT meter (FAULT, PANIC), PRESET, STORE, LOAD.",
                  "<b>E16:</b> the plugin's page of the SF SHOW scene, as the E16 shows it: its own encoders lit where they sit, the others dark. The rings fill as the E16's do (16 LEDs, from just left of the bottom, clockwise) in the scene's colours: white, light blue for a volume, green for SLOW ALL. A press sits under its knob (THROW, FREEZE, DROP, ROLL); click it. A plugin the scene has on several tracks (LEAD, PERC, ENERGY) finds its encoders by the track's name, which only the VST3 knows; without it, a line says so and its knobs go to PERFORM. A plugin the scene doesn't use has no grid.",
                  "<b>PERFORM:</b> the rest of the performance row, and what else is played (DECK's PLAY and CUE, CONTROL's PANIC).",
                  "<b>The display</b>, then <b>DETAIL</b>: the sound's other controls, MODEL first.",
                  "<b>MOTION</b>, <b>SYSTEM</b> (SLOW, MACRO) and <b>SETUP</b> (how the plugin sits in the set: ALLOC, KEY FOLLOW, DRY ...) come folded, their settings on their title line. Click a group's title line to open or fold it; the plugin keeps it so, with the set.",
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
cc = [["", "COLOUR", "MEANS"], [Swatch(RED), "RED", "Clip, FAULT (the plugin reset itself), missing file, true peak at the ceiling."], [Swatch(AMBER), "AMBER", "Conflict or warning: an ALLOC conflict, a deck armed and waiting, RETURN KNOB on BUILD."],
      [Swatch(GREEN), "GREEN", "Active or locked: bus connected, kick reporting, a song recall or a drop waiting for the bar, a value gliding (SLOW)."],
      [Swatch(CYAN), "CYAN", "The value of the control you are touching."],
      [Swatch(LIGHT_BLUE), "LIGHT BLUE", "In the E16 grid: a volume (LEVEL), the ring colour the E16 shows. The grid's other rings are white, as on the E16, and SLOW ALL's green."]]
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
                  "SF CONTROL decides the bar line and sends it with the song (and the song's KEY), so every plugin changes on the same bar whatever order Live processes the tracks in. A change sent within 30 ms after a bar line (on the downbeat) counts for that bar; later in the bar, for the next one. Inside an Arrangement loop that never reaches that bar, every plugin changes on the next bar line after the loop jumps back.",
                  "The header of every plugin shows SONG nn, and WAIT BAR in green while the recall is waiting for the bar line.",
                  "Recalled values are reported to Live, so the E16 rings jump to the new song's settings. A recalled knob is yours again the moment you turn it.",
                  "A plugin with nothing stored in that song stays as it is.",
                  "With SLOW ALL (SF CONTROL) or a plugin's own SLOW on, the recall glides instead of jumping: a song change becomes an 8- or 16-bar transition.",
                  "Arrangement automation wins over recalled values, as on any device."])
story.append(H2("VST3 AND PROGRAM CHANGE"))
story.append(P("VST3 plugins don't receive MIDI Program Change as MIDI. SF CONTROL therefore lists 16 programs (SONG 01-16); Live 12.2 and later turn a Program Change on the track into a program change for the VST3, older versions don't. The AU version reads Program Change directly from MIDI in any version. If song changes don't arrive in the VST3, use the AU."))
story.append(H2("PRESETS"))
story += bullets(["The <b>PRESET</b> button in every header opens the menu: factory presets grouped by style (DUB, ACID, HYPNOTIC ...), your own presets, SAVE USER PRESET and SHOW PRESET FOLDER. The button shows the preset that is loaded.",
                  "A preset sets every sound control of the plugin, MOTION included, and Live is told about every change, so the E16 rings follow. It never touches SLOW, MACRO, ALLOC, ROLE, a source setting or KEY FOLLOW.",
                  "191 factory presets across 26 plugins. Use them as starting points, then store the result into a song.",
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
story.append(P("SF BUILD's DROP DUCK (default 6 dB) makes every Pocket stage dip under each kick for DUCK TIME after the drop (default 1 bar), whatever its DEPTH, so the first kicks of the drop land on top of the parts and the tail coming back. It follows the kick: back within half a beat of each hit, and easing out over the second half of DUCK TIME. It starts with the first kick on the drop's bar line: a roll into the drop doesn't duck. TAP and STRIP set to ROLE KICK, PERC or DECK don't dip, like SF KICK, PERC, STRIKE and DECK."))
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
story += bullets(["Record the Main output (after SF LIMIT, before SF ROOM EQ) at the session's sample rate, 24-bit or 32-bit float: an audio track REC with Audio From = Resampling, Monitor Off, armed. Live records what comes into a track, before its devices.",
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
       ["Audio track PA", "Audio From = Resampling (the Main output), Monitor In, SF ROOM EQ, Audio To = the PA outputs. The venue hears ROOM EQ."],
       ["Audio track REC", "Audio From = Resampling, Monitor Off, armed: records the set without ROOM EQ."],
       ["Audio track ROOM", "Audio From = the room mics (a stereo pair), Monitor In, SF CAPTURE, Audio To = the outputs a speaker can go on (the headphones), sends disabled. SF TERROIR captures the place from here (SF CAPTURE); armed, it records the room with the set."],
       ["Live instruments / hardware", "SF STRIP last on the track: the suite's performance row, Pocket and ALLOC for any sound."],
       ["Stems", "SF TAP with ROLE set (no sound of its own), or SF STRIP to play them."]]
story.append(table(lay, [1.3 * inch, W - 1.3 * inch]))
story.append(H2("THE E16"))
story += bullets(["Map with Live's MIDI Map (Cmd-M): unfold the plugin's parameter panel (the triangle in its title bar), click the parameter there and turn the E16 knob. A parameter that isn't listed: Configure, touch it in the plugin window, Configure off.",
                  "In Live's MIDI preferences turn <b>Remote</b> on for the E16's Port 2, input <i>and output</i>. Live then sends values back, so the rings follow song recalls and ALLOC slot changes and the knobs hold the new values.",
                  "Absolute 0-127 CC is right; the plugins smooth every step. Chapter 7 has a ready-made E16 scene for the show template, SF SHOW, and the show set with every address already mapped."])
story.append(H2("SAMPLES"))
story.append(P("Every instrument has a SAMPLE model. Drop an audio file on the plugin window or click LOAD. The sample is analysed (root note, decay, onsets) and embedded in the Live set, so the set travels without the file."))
story.append(H2("FIRST SESSION CHECKS"))
story += bullets(["SF CONTROL shows <b>BUS SHARED</b>. Play SF KICK: it shows <b>KICK REPORTING</b>.",
                  "SF KICK + SF BASS with POCKET DEPTH up and SHAPE right: the pump lands exactly on the kick.",
                  "SF ALLOC on the master: every SF source appears in the list; raise DEPTH on a band where two of them overlap and watch the amber.",
                  "Store two songs; switch with Program Change: the E16 rings follow. With SF FILTER on tracks above and below SF CONTROL, every track changes on the same bar.",
                  "SF DECK: load a track, PLAY A: it starts on the bar, locked to Live's tempo.",
                  "Set KEY on SF CONTROL: BASS, LEAD and PAD transpose, KICK and RUMBLE retune. Hold PANIC with SF DELAY in feedback: silence within 5 ms, for as long as you hold it.",
                  "Record a minute on REC with a deep cut on ROOM EQ: the PA has the cut, the recording doesn't."])
story.append(P("Chapter 7 is a complete show set built on this layout."))
story.append(PageBreak())

# ---------------- 7: the show template
story.append(H1("7  THE SHOW TEMPLATE"))
story.append(Rule())
story.append(P("One Live set for every show, built on fixed roles. The OXI ONE plays the notes and patterns; the E16 plays the sound, the balance and the transitions. The E16's top half is the same on every page, so the whole arrangement is always under one hand while the other works deep in one sound.", "summary"))
story.append(P("The set is built: <b>controllers/live/SF SHOW.als</b> in the repository holds every track, group and return below, every SF plugin with its starting settings, the routing, and every E16 address mapped. Open it, do the five steps its README lists (MIDI From, the inputs and outputs, the two keys, two options, the save), and keep it as your template."))

story.append(H2("TRACKS"))
story.append(table([["TRACK", "DEVICES", "MIDI FROM"],
    ["00 CONTROL", "SF CONTROL", "ONE port B, ch 16"],
    ["01 KICK", "SF KICK", "ONE port A, ch 1"],
    ["10 LOW (group)", "SF ENERGY, PROFILE BASS", ""],
    ["11 BASS", "SF BASS", "port B, ch 1"],
    ["12 RUMBLE", "SF RUMBLE, SOURCE KICK BUS", "none: it follows the kick"],
    ["20 DRUMS (group)", "SF ENERGY, PROFILE DRUMS; SF REPEAT", ""],
    ["21 CLOSED", "SF PERC: closed hat", "port A, ch 2"],
    ["22 OPEN", "SF PERC: open hat or ride", "port A, ch 3"],
    ["23 CLAP", "SF PERC: clap or rim", "port A, ch 4"],
    ["24 STRIKE", "SF STRIKE", "port B, ch 6"],
    ["30 MUSIC (group)", "SF ENERGY, PROFILE MUSIC", ""],
    ["31 ACID", "SF LEAD: ACID 303-TYPE or another mono lead", "port B, ch 2"],
    ["32 STAB", "SF LEAD: chords and stabs", "port B, ch 3"],
    ["33 TEXTURE", "SF PAD, often GRANULAR", "port B, ch 4"],
    ["34 VOICE", "SF VOICE: phrases, radio fragments, choir", "port B, ch 5"],
    ["40 BRIDGE", "Live audio clips", ""],
    ["41 DECK", "SF DECK", "port B, ch 7 (hot cues; optional)"],
    ["A DUB (return)", "SF DELAY, DRY off", ""],
    ["B SPACE (return)", "SF REVERB, DRY off", ""],
    ["90 LIVE BUS", "SF ISOLATOR > SF BUILD > SF GLUE > SF TONE > SF LOWEND; SF ALLOC (off at first)", ""],
    ["Main", "SF LIMIT", ""],
    ["92 PA", "SF ROOM EQ", "audio: Resampling"],
    ["93 REC", "nothing", "audio: Resampling"],
    ["94 ROOM", "SF CAPTURE", "audio: Ext. In 1/2, the room mics"]], [1.35 * inch, 3.25 * inch, W - 4.6 * inch]))
story.append(Spacer(1, 6))
story += bullets(["The kick stays outside LOW, so bass and rumble fade together while the kick holds the floor. In each section decide whether BASS or RUMBLE carries the sustained low end: one of two knobs then steers it.",
                  "The PERCs are separate instruments: CHOKE works inside one track only, so a closed hat doesn't cut the open hat on its own track. Write the open hat's rhythm and DECAY for that.",
                  "STAB: if the ONE sends whole chords, set CHORD to NONE; if it sends single notes, CHORD (or the model's own chord) voices them. Not both.",
                  "ACID 303-TYPE slides when notes overlap, always over 60 ms like a 303: write legato notes for slides. GLIDE doesn't change it.",
                  "VOICE with a phrase loaded: C3 plays slice 1, C#3 slice 2 and so on; below C3, the whole phrase. Sequence it as phrase choreography, not as a pitched sampler.",
                  "Name the tracks as above: SF ALLOC lists its sources by Live track name (VST3)."])

story.append(H2("ROUTING"))
story.append(RoutingDiagram())
story.append(Spacer(1, 6))
story.append(table([["TRACK", "INPUT / MONITOR", "OUTPUT"],
    ["Instrument tracks", "MIDI From as above, Monitor In", "their group; KICK to 90 LIVE BUS"],
    ["Groups, returns A and B", "", "90 LIVE BUS"],
    ["90 LIVE BUS", "the tracks routed to it, Monitor In, every send disabled", "Main"],
    ["40 BRIDGE", "clips, Monitor Off (clips still play)", "Main"],
    ["41 DECK", "MIDI From port B ch 7, Monitor In", "Main"],
    ["Main", "", "an output pair that goes nowhere"],
    ["92 PA", "Audio From Resampling, Monitor In, sends disabled", "the PA outputs"],
    ["93 REC", "Audio From Resampling, Monitor Off, sends disabled", "Sends Only"],
    ["94 ROOM", "Ext. In 1/2 (the room mics, a stereo pair), Monitor In, sends disabled", "the headphone outputs: a speaker on them for a capture"],
    ["Cue", "Live's Cue Out; DECK's CUE track", "the headphone outputs"]], [1.75 * inch, 2.75 * inch, W - 4.5 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Main is the set. Exporting or resampling it gives the release mix; the venue's correction (ROOM EQ) exists only on 92 PA, so the recording never hears it.",
                  "<b>Disable every send on 90 LIVE BUS</b> (right-click a send, Disable Send). The returns feed LIVE BUS, and Live can't compensate a return's delay on a track whose matching send is active, even at zero (Ableton's Delay Compensation FAQ).",
                  "Pull 90 LIVE BUS down and the live instruments and their effects go while BRIDGE and DECK keep playing. Mute 92 PA and the speakers go silent while REC keeps recording. Put 92 PA's on/off on a computer key (Cmd-K, click its activator, press the key), not on an E16 press: a knob is easily pressed while it is turned.",
                  "PANIC silences every SF plugin, DECK included, but not Live's own clips and devices: for total silence, mute 92 PA.",
                  "BRIDGE is for transitions and regrouping. It still needs Live: it is not a backup for a computer failure.",
                  "ROOM EQ only cuts, so it needs no limiter after it; LIMIT's -1 dB ceiling leaves the margin.",
                  "94 ROOM is the room: armed, it records the mics with the set, for the mix. Its SF CAPTURE never passes them, so the set never hears the mics; for SF TERROIR it plays the sweep to a speaker on the headphone out (transport stopped) and records the place (SF CAPTURE)."])

story.append(H2("THE MASTER DEVICES"))
story += bullets(["Start GLUE, TONE and LOWEND gently: the set doesn't need every processor working all the time. Soundcheck the busiest passage of the night and set LIMIT's INPUT for 1-3 dB of limiting there.",
                  "LIMIT AUTO off while you set the gain staging, then on, AUTO MODE DOWN ONLY, for shows: it stops the set creeping above TARGET and holds through breakdowns.",
                  "SF ALLOC off at first. Turn it on band by band once the arrangement is known. It measures each source before its LEVEL and before Live's fader, so a source you've faded out still claims its bands: take a source out with its lane mute or its FILTER, or switch its ALLOC off."])

story.append(H2("THE OXI ONE"))
story.append(P("Port A carries the drums, port B everything pitched and SF CONTROL. Every sequencer has its own channel; in Live, set every track's MIDI From to its port and channel, never All Ins / All Channels."))
story.append(table([["SEQUENCER", "MODE", "OUTPUT", "TRACK"],
    ["1 DRUMS", "Multitrack: lane 1-4 on ch 1-4", "port A", "KICK, CLOSED, OPEN, CLAP (lanes 5-8 spare)"],
    ["2 BASS", "Mono", "port B ch 1", "BASS"],
    ["3 ACID", "Mono", "port B ch 2", "ACID"],
    ["4 STAB", "Chord or Poly", "port B ch 3", "STAB"],
    ["5 TEXTURE", "Poly", "port B ch 4", "TEXTURE"],
    ["6 VOICE", "Mono", "port B ch 5", "VOICE"],
    ["7 STRIKE", "Mono to start", "port B ch 6", "STRIKE"],
    ["8 CONTROL", "Program Change only", "port B ch 16", "CONTROL"]], [1.0 * inch, 1.9 * inch, 1.0 * inch, W - 3.9 * inch]))
story.append(Spacer(1, 6))
story += bullets(["KICK lane on C1: with KEY TRACK on, any other note transposes the kick (C1 = TUNE), so C1 is right either way. PERC plays any note.",
                  "RUMBLE needs no lane: SOURCE KICK BUS follows SF KICK.",
                  "KEY OFF on SF CONTROL to start with: the ONE's notes set the pitch, and KICK and RUMBLE are tuned per song with TUNE. Turn KEY on for songs that should change key without reprogramming the ONE (mind the low register, chapter 1).",
                  "One ONE project per track, with a pattern per section: SPARSE, MAIN, VARIATION, EXIT. Sequencer 8 sends that track's Program Change on port B ch 16, so loading the project recalls the matching SF song. Program Change stays off on the musical sequencers.",
                  "When patterns change is set on the ONE (its launch and arranger settings): Live's launch quantization doesn't reach it."])

story.append(H2("CLOCK, MIDI AND LATENCY"))
story.append(table([["LIVE MIDI PORT", "TRACK", "SYNC", "REMOTE"],
    ["ONE port A in", "on", "off", "off"], ["ONE port B in", "on", "off", "off"],
    ["ONE out (the one it takes clock from)", "off", "on", "off"], ["E16 Port 2 in", "off", "off", "on"], ["E16 Port 2 out", "off", "off", "on"],
    ["E16 Ports 1 and 3, in and out", "off", "off", "off"]],
    [2.6 * inch, 0.9 * inch, 0.9 * inch, W - 4.4 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Live is the clock master. On the ONE: USB device mode, sync from USB, Auto Sync on (it starts with Live), Start and Stop accepted, MIDI Thru off. Never send clock both ways.",
                  "Audio: 48 kHz, buffer 256 (Pocket needs 512 or less), Delay Compensation on, <b>Reduced Latency When Monitoring off</b> (it skips compensation on monitored tracks, so returns and Pocket drift), launch quantization 1 bar, record quantization off.",
                  "Calibrate once: record a plain quarter-note pattern from the ONE, compare it with Live's grid and set the ONE port's MIDI Clock Sync Delay in Live's preferences. Leave POCKET OFFSET at 0 unless the pump is still off after that.",
                  "Every Pocket stage in a chain adds its 512-sample lookahead to the total latency (compensated, but it adds up): no SF TAP or SF DUCK on tracks that already hold SF instruments."])

story.append(H2("THE E16"))
story.append(P("One E16 scene runs the whole rig: <b>SF SHOW</b>, in the repository under controllers/e16, with its address map (sf-show-map.csv) and loading steps (README); SF SHOW.als has every address mapped. The E16 is a 4 &times; 4 grid, numbered row by row from the top left. Rows 1 and 2 are the same on every page; rows 3 and 4 are the page."))
PRESS_DOES = {"SLOW ALL": "a quick press flips the knob to SLOW ALL on SF CONTROL (green ring): how long your next moves take, OFF to 128 BAR; another press flips it back",
              "DROP": "the drop lands on the next bar line (BUILD's GAP before it)", "PANIC": "every SF plugin silent while held",
              "THROW": "sends into the effect while held", "FREEZE": "holds the effect until pressed again",
              "ROLL": "the drums roll while held"}
def press_text(p, full=True):
    kind = p.mode.lower()
    return f"{p.name} ({kind})" + (f": {PRESS_DOES[p.name]}" if full else "")
def where_to(c):
    t = c.target.split(" > ")
    return f"{t[1]}, {t[0]}" if t[1].startswith("SF ") else t[0]
cells = {}
for i, c in enumerate(E16.TOP, 1):
    pr = E16.PRESSES_ALL.get(i)
    sub = f"PRESS: {pr.short()}" if pr else where_to(c)
    cells[i] = (c.label, sub, CYAN if c.colour == E16.LIGHT_BLUE else INK, False)
for i, (name, quiet) in zip(range(9, 17), [("SOUND", 1), ("SOUND", 1), ("SOUND", 1), ("GATE", 0), ("DENSITY", 0), ("FILTER", 0), ("SOUND", 1), ("LEVEL", 0)]):
    pr = E16.PRESSES_ALL.get(i)
    cells[i] = (name, f"PRESS: {pr.short()}" if pr else ("THE PLUGIN'S OWN" if quiet else ""),
                CYAN if name == "LEVEL" else INK, bool(quiet))
story.append(Spacer(1, 2)); story.append(E16Grid(cells)); story.append(Spacer(1, 8))
story += bullets([f"<b>Press {i}{', on every page' if i > 8 else ''}:</b> {esc(press_text(pr))}." for i, pr in sorted(E16.PRESSES_ALL.items())]
                 + ["<b>PANIC and 92 PA's on/off are on computer keys</b> (Key Map, Cmd-K), never on a press: a knob is easily pressed while it is turned, and neither may ever happen by accident. A key flips them: PANIC on, every SF plugin silent; again, back. Every press on the E16 is one the set plays through if it happens by accident."])
story += bullets(["The ENERGY knobs are the group DJ filters, plus drive, space, echo and chop: centre is neutral, left takes energy out, right puts it in.",
                  "Each instrument's LEVEL belongs to its song; the volumes here are your balance on the night. Set every LEVEL at soundcheck so a volume at 0 dB gives the intended mix, and give every volume mapping a maximum of 0 dB (Max in Live's MIDI Mappings).",
                  "Rings: light blue is a volume, white is everything else, green is knob 1 on SLOW ALL. A knob that is travelling (SLOW) turns green and walks with the sound until it arrives (GLIDE RINGS: SF CONTROL's E16 RINGS and the scene's script; controllers/e16 README)."])

story.append(H2("THE E16: PAGES"))
rows = [["PAGE", "ROW 3: ENCODERS 9-12", "ROW 4: ENCODERS 13-16", "PRESSES"]]
for n, (title, bottom) in enumerate(E16.PAGES, 1):
    pr = [f"{i} {press_text(E16.PRESSES_PAGE[(title, i)], False)}" for i in range(9, 17) if (title, i) in E16.PRESSES_PAGE]
    rows.append([f"{n} {title}", esc(" / ".join(c.label for c in bottom[:4])), esc(" / ".join(c.label for c in bottom[4:])), esc(", ".join(pr))])
story.append(table(rows, [0.95 * inch, 2.3 * inch, 2.3 * inch, W - 5.55 * inch]))
story.append(Spacer(1, 6))
story += bullets(["On every single-instrument page 13 is DENSITY, 14 FILTER and 16 LEVEL, and 12 is GATE wherever there is one: the hand learns one layout (HATS holds three PERCs). The rest is the plugin's own performance row, with a swap where a model's character lives elsewhere (GRANULAR: COLOR is grain size, DETUNE is scatter; STAB: SPACE is the ensemble).",
                  "On SPACE the presses sit under their knobs: row 3 is DUB, row 4 is SPACE. On MACROS, hold ROLL on 15 and turn LENGTH under the same finger.",
                  "The HATS page is the fill page: CLOSED DENSITY with ECHO SPACE 1/16 turns 1/8 hats into 1/16s, the send-delay trick built in."])

story.append(H2("THE E16: ADDRESSES AND SETTINGS"))
story += bullets(["<b>Channel = track:</b> 1 KICK, 2 BASS, 3 RUMBLE, 4 CLOSED, 5 OPEN, 6 CLAP, 7 STRIKE, 8 ACID, 9 STAB, 10 TEXTURE, 11 VOICE, 12 DUB, 13 SPACE, 14 the groups (LOW, DRUMS, MUSIC and DRUMS' REPEAT), 15 LIVE BUS and BRIDGE, 16 CONTROL.",
                  "<b>CC = block base + the parameter's place in its plugin's own order</b>, counting from 0. One plugin on a channel: base 20, so its performance row (its first eight parameters) is CC 20-27. Channel 14: LOW ENERGY 20, DRUMS ENERGY 30, MUSIC ENERGY 40, REPEAT 50. Channel 15: BUILD 20 (DUCK TIME, its eleventh, has no address), with 30-79 kept for the master devices. Channel 16: CONTROL 20 (MACRO 1-6 are 20-25). The plugins never reorder their parameters, so an address never moves. The appendix lists every address.",
                  "<b>Mixer:</b> CC 100 volume, 101 send A, 102 send B. Channel 14: 100-102 the LOW, DRUMS and MUSIC volumes; channel 15: 100 LIVE BUS, 101 BRIDGE (102 kept for DECK). A parameter on two pages sends the same address from both: two ways to one mapping.",
                  "<b>Presses:</b> a hold is a CC, 127 on press and 0 on release, so the parameter is on while the knob is down. A toggle is a note: Live flips a switch on every Note On, so the E16 can never think FREEZE is on while Live has it off. SF SHOW sets every turn; until the OXI App's press format is known, the presses are added in the App (the README lists them), and again after loading a new scene file.",
                  "<b>On the E16:</b> Auto Transmit off, for the scene and globally (stored values sent on a page change would pull Live's faders to them), MIDI Thru off. Stay in SF SHOW for the whole show.",
                  "<b>In Live:</b> E16 Port 2 Remote on, input and output; Ports 1 and 3 off (Port 1 is the OXI App's, and a second Remote port doubles every message: a toggle would flip twice). Takeover Mode: Value Scaling, so a knob that disagrees with Live after a recall moves the sound smoothly instead of jumping.",
                  "<b>Mapped once, kept:</b> MIDI mappings belong to the Live set. SF SHOW.als comes with every address mapped, every volume at Max 0 dB and DUB FEEDBACK at 0-70 %; save it as a template (File &gt; Save Live Set As Template) and new sets start mapped. In a set of your own, map once from the appendix and do the same.",
                  "<b>Map the E16 straight to the SF parameters, not through Instrument Rack macros.</b> A song recall changes the plugin's value underneath a macro: the macro doesn't follow, its ring shows the wrong value and the next turn jumps the sound back. Ranges go in Live's MIDI Mappings (Min and Max per mapping): give resonance and feedback the part of their travel that matters, and widen it when the music asks.",
                  "Check the shared knobs once: turn LOW ENERGY on page 2, go to page 1 and turn it a step. The sound must move a step, not jump back. If it jumps, the README's relative fallback fixes it.",
                  "Keep STORE ALL, MODEL, KEY and the calibration controls off the performance pages."])

story.append(H2("MACROS"))
story.append(P("SF CONTROL's six macros, on the MACROS page. Every SF plugin follows one macro (MACRO SRC) by an amount (MACRO AMT, negative turns it the other way); a macro moves the knobs marked M in the module tables, adds to them and is stored per song. A starting plan:"))
story.append(table([["MACRO", "FOLLOWED BY", "MOVES"],
    ["1 ENERGY", "the three ENERGY plugins, BUILD", "every group's energy at once (a negative amount on LOW darkens the bass while the rest lifts)"],
    ["2 MOOD", "TEXTURE, STAB, VOICE", "cutoff, envelope and drive; the voice's SIGNAL and ECHO"],
    ["3 DENSITY", "CLOSED, OPEN, CLAP, STRIKE", "DECAY and DENSITY: the fills"],
    ["4 PULSE", "KICK, RUMBLE", "kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY"],
    ["5 SPACE", "DUB, SPACE returns", "delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX"],
    ["6 FRACTURE", "ACID, BASS", "cutoff, envelope, density and drive: smooth to heavy"]], [1.0 * inch, 2.1 * inch, W - 3.1 * inch]))
story.append(P("Start the amounts at 30-50 %. One macro per plugin: a plugin can follow only one.", "smallgrey"))

story.append(H2("RETURNS AND POCKET"))
story += bullets(["A DUB: SF DELAY, DRY off, TIME synced, FEEDBACK mapped to 0-70 % at first, DUCK up so the repeats sit around the kick. INPUT THROW ONLY makes its THROW press the only way in: dub throws.",
                  "B SPACE: SF REVERB, DRY off, DUCK 30-60 % so the tail blooms in the gaps, LO CUT up.",
                  "Sends post-fader: turning a source down stops feeding its effect while the tail rings on. No return-to-return sends at first.",
                  "Pocket needs no wiring: SF KICK reports on the bus. SF CONTROL should show BUS SHARED and KICK REPORTING.",
                  "Pulling the KICK fader down leaves the kick playing on the bus: Pocket and RUMBLE keep pumping to a silent kick, which can be the point. For a clean withdrawal, mute the kick lane on the ONE."])

story.append(H2("SONGS, PATTERNS AND SCENES"))
story.append(table([["SYSTEM", "RESPONSIBLE FOR"],
    ["OXI ONE projects and patterns", "Notes, rhythms, gates, accents, the pattern progression."],
    ["SF songs (SF CONTROL)", "The sound of every SF plugin, recalled on the bar."],
    ["E16 pages", "Where your hands are."],
    ["Live Session scenes", "BRIDGE clips, reference material, rehearsal."],
    ["Live Arrangement", "Recording the performance."]], [2.0 * inch, W - 2.0 * inch], mono_first=False))
story.append(Spacer(1, 6))
story += bullets(["The roles stay put for the whole set: a new track changes what they play, not where they are.",
                  "Rehearse song recall, SF REPEAT and SF BUILD's drop (GAP, TAIL, DROP DUCK) in your Live before relying on them in a transition: all three changed on Nov 3."])

story.append(H2("RECORDING AND REHEARSAL"))
story += bullets(["Instrument tracks on Monitor In: they play from the ONE whether or not they're armed.",
                  "To record: RESET on SF LIMIT, arm the instrument tracks, 93 REC and 94 ROOM, Automation Arm on, Arrangement record, play. The stereo recording on REC is the set, the one on ROOM the room mics for the mix; the MIDI and automation are for editing and rehearsal.",
                  "To replay recorded MIDI: stop the ONE, set those tracks to Monitor Auto and disarm them (Monitor In ignores a track's own clips).",
                  "Collect All and Save. SF samples are inside the set; DECK tracks are referenced by path, so keep the files where they are."])

story.append(H2("BUILD ORDER"))
story += numbered(["SF SHOW.als (controllers/live): MIDI From, the inputs and outputs, PANIC and 92 PA on their keys, the two options; saved as a template.",
                   "CONTROL, KICK, BASS, RUMBLE, LIVE BUS, PA and REC playing. Clock and latency calibrated, a short recording made.",
                   "CLOSED, OPEN, CLAP and STRIKE.",
                   "ACID, STAB, TEXTURE and VOICE, one at a time.",
                   "The returns; check the sends are disabled on LIVE BUS.",
                   "BRIDGE and DECK.",
                   "SF SHOW on the E16 (controllers/e16): the scene loaded, the presses added, every page checked against the set's mappings; the macros.",
                   "Rehearse moving between BRIDGE, DECK and LIVE BUS.",
                   "Record a whole set before adding anything else."])
story.append(CondPageBreak(2.2 * inch))
story.append(H2("BEFORE THE FIRST SHOW"))
story += bullets(["CONTROL shows BUS SHARED and KICK REPORTING; the pump lands on the kick.",
                  "The ONE's Program Change recalls the song, every track on the same bar, and a recalled knob is yours the moment you turn it. (VST3 needs Live 12.2 or later; the AU works in any version.)",
                  "PANIC from its key: silence, and every header shows PANIC. Again: back without a click. 92 PA muted from its key: the speakers silent, REC still recording.",
                  "Every E16 hold press (DROP, THROW, ROLL) held for three seconds stays on for the whole hold and goes off on release.",
                  "The shared E16 knobs don't jump: LOW ENERGY turned on page 2, then a step on page 1, moves a step. Changing pages sends nothing (Live's MIDI input indicator stays dark).",
                  "A minute recorded on REC with a deep ROOM EQ cut: the cut isn't in the recording.",
                  "With SF TERROIR in the set: a capture from 94 ROOM into an empty slot ends SWEEP x3, 30 dB or more over the noise, and the PA stays silent through it."])
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
                            ("DETAIL", [p for p in sp if not played(p) and not p.get("setup") and p["id"] != "model"]),
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
                    ["SF CONTROL", "Program Change ch 16", "Song 01-16 (channel set by PC CHANNEL)."],
                    ["SF LEAD / BASS ACID", "velocity > ~108", "Accent. Overlapping notes slide."]], [1.5 * inch, 1.4 * inch, W - 2.9 * inch]))
story.append(H2("WHERE THINGS ARE SAVED"))
story += bullets(["Songs, samples, ALLOC choices, MOTION random seeds, DECK tracklist and venue EQs: inside the Live set (plugin state).",
                  "User presets: Music/SF Live Suite/Presets/&lt;PLUGIN&gt;, one file per preset.",
                  "DECK track log: Documents/SF Live Suite/tracklog.txt.",
                  "SF TERROIR: its places are inside the Live set; the sweep file is in Music/SF Live Suite/TERROIR, and every take captured in the set (SF CAPTURE) in Music/SF Live Suite/TERROIR/Captures, one WAV each.", "DECK tracks are referenced by file path, not embedded: keep the files where they are."])
story.append(H2("WHEN SOMETHING ISN'T RIGHT"))
story.append(table([["SYMPTOM", "CHECK"],
                    ["SF SHOW.als: a plugin is missing", "The suite isn't installed, or VST3 is off (Settings &gt; Plug-Ins). Install or turn VST3 on, then open the set again."],
                    ["An instrument doesn't play", "MIDI From: SF SHOW.als starts every MIDI track on No Input; set each to its ONE port and channel (chapter 7), never All Ins, where every instrument plays every part."],
                    ["Nothing ducks", "SF CONTROL: BUS SHARED and KICK REPORTING. The kick must be an SF KICK (or use SF DUCK with SOURCE SIDECHAIN)."],
                    ["Pump early or late", "Live buffer size 512 or less. Then trim POCKET OFFSET on SF CONTROL by ear."],
                    ["Songs don't change", "PC ENABLE on, PC CHANNEL matches the OXI. The VST3 needs Live 12.2 or later; otherwise use the AU."],
                    ["E16 rings don't follow", "Remote on for E16 Port 2's output in Live's MIDI preferences; map with Live's MIDI Map."],
                    ["An E16 press does nothing", "An SF SHOW file from before 3 Nov is loaded (it had no presses): load the current one. A toggle that flips back: Remote is on for two E16 ports, so the note arrives twice. Port 2 only."],
                    ["An E16 knob jumps when touched", "A shared knob holding an old value: controllers/e16 README, check 2. Or Auto Transmit is on: off in the scene and globally."],
                    ["ALLOC does nothing", "ENABLE on, DEPTH above 0 on the band, the instruments' ALLOC toggles on. In DYNAMIC, the favoured source must be playing in that band."],
                    ["DECK row says MISSING", "The file moved. Drop it again."],
                    ["A knob shows GLIDE", "SLOW ALL or the plugin's SLOW is on: the knob is travelling to where you set it."],
                    ["A knob moves on its own", "MOTION: check M1 / M2 TARGET (the plugin's MOTION group; its title line says where they go)."],
                    ["Header shows FAULT (red)", "The plugin caught an invalid value or a runaway and reset itself to silence. It carries on working. If it happens again, note what was playing: it is a bug."],
                    ["SF TERROIR won't capture", "Its status line says why. The transport must be stopped; NO SF CAPTURE: put it on the room mics' track (94 ROOM in SF SHOW), its device on; one take at a time."],
                    ["A capture says THE SWEEP DIDN'T COME BACK, or under 30 dB", "The speaker didn't play the sweep, or the mics didn't hear it: the speaker on 94 ROOM's output (3/4), the track on and up; the mics on 94 ROOM's input (Input Config: 1/2 a stereo pair), Monitor In. Then louder or closer. The slot stays as it was."],
                    ["A capture says NO SWEEP IN THE RIGHT CHANNEL", "That mic heard nothing (its cable, its gain, Input Config): the place came in mono from the other. Fix it and capture again."],
                    ["A capture says CAPTURE STOPPED: SF CAPTURE STOPPED ANSWERING", "94 ROOM's SF CAPTURE was removed or switched off, or its track's audio stood still. Put it back and capture again: nothing was stored."],
                    ["Something won't stop", "PANIC on SF CONTROL: every SF plugin fades out, clears and stays silent while held. Live's own clips and devices aren't SF: mute the PA track for those."],
                    ["Header shows PANIC (red)", "PANIC is on at SF CONTROL: held in its window, or flipped on by its key or button. A key flips it back."],
                    ["CONTROL shows LOCAL ONLY", "The plugins come from different builds. Rebuild and install the whole suite."],
                    ["A return doubles the dry signal", "DRY off on SF DELAY, SF REVERB or SF DENSITY on a return track."],
                    ["Pitch is wrong after a song change", "KEY on SF CONTROL is stored per song. KEY FOLLOW off on an instrument ignores it."],
                    ["Levels changed after the update", "Since Nov 3 every model is calibrated to its role's target (chapter 5). Re-balance the faders once."]], [1.7 * inch, W - 1.7 * inch], mono_first=False))

story.append(CondPageBreak(3 * inch))
story.append(H2("E16 ADDRESS MAP (SF SHOW)"))
story.append(P("Every address of the SF SHOW scene once, by channel: the Live target, the mapping's range, and where the knob is on the E16. SF SHOW.als has them all mapped; this list is for checking it, and for mapping a set of your own (Cmd-M). The same list is controllers/e16/sf-show-map.csv."))
rows = [["CH", "MESSAGE", "LIVE TARGET", "MAPPING", "ON THE E16"]]
for (msg, ch, n), (target, places, kind, rng, item) in sorted(E16.addresses().items(), key=lambda kv: (kv[0][1], kv[0][0], kv[0][2])):
    m = f"CC {n}" if msg == "CC" else f"NOTE {n} ({E16.note_name(n)})"
    how = {"TURN": "absolute", "HOLD": "press: hold", "TOGGLE": "press: toggle"}[kind] + (f", {rng}" if rng else "")
    rows.append([str(ch), m, esc(target), esc(how), esc(E16.where(places, getattr(item, "second", False)))])
story.append(table(rows, [0.35 * inch, 1.05 * inch, 2.55 * inch, 1.25 * inch, W - 5.2 * inch], mono_first=False, pad=2.3))

doc = Doc(os.path.join(ROOT, "docs", "SF-Live-Suite-Manual.pdf"))
doc.multiBuild(story)
print("ok")
