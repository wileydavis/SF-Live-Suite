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
sys.path.insert(0, HERE)
import content as C

F = "/usr/share/fonts/truetype/dejavu/"
pdfmetrics.registerFont(TTFont("Sans", F + "DejaVuSans.ttf"))
pdfmetrics.registerFont(TTFont("SansB", F + "DejaVuSans-Bold.ttf"))
pdfmetrics.registerFont(TTFont("Mono", F + "DejaVuSansMono.ttf"))
pdfmetrics.registerFont(TTFont("MonoB", F + "DejaVuSansMono-Bold.ttf"))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily("Sans", normal="Sans", bold="SansB", italic="Sans", boldItalic="SansB")
registerFontFamily("Mono", normal="Mono", bold="MonoB", italic="Mono", boldItalic="MonoB")

INK = colors.HexColor("#000000")
GREY = colors.HexColor("#6b6b6b")
LIGHT = colors.HexColor("#d9d9d9")
FAINT = colors.HexColor("#f2f2f2")
RED, AMBER, GREEN, CYAN = colors.HexColor("#ff3b30"), colors.HexColor("#ffb000"), colors.HexColor("#30d158"), colors.HexColor("#00c8ff")

S = {
    "body": ParagraphStyle("body", fontName="Sans", fontSize=9.2, leading=13.2, textColor=INK, spaceAfter=6),
    "small": ParagraphStyle("small", fontName="Sans", fontSize=8, leading=10.6, textColor=INK),
    "smallgrey": ParagraphStyle("smallgrey", fontName="Sans", fontSize=8, leading=10.6, textColor=GREY),
    "cell": ParagraphStyle("cell", fontName="Sans", fontSize=8, leading=10.4, textColor=INK),
    "cellmono": ParagraphStyle("cellmono", fontName="MonoB", fontSize=7.8, leading=10.4, textColor=INK),
    "cellrange": ParagraphStyle("cellrange", fontName="Mono", fontSize=7, leading=9.4, textColor=GREY),
    "h1": ParagraphStyle("h1", fontName="MonoB", fontSize=22, leading=26, textColor=INK, spaceBefore=0, spaceAfter=4),
    "h2": ParagraphStyle("h2", fontName="MonoB", fontSize=11, leading=14, textColor=INK, spaceBefore=12, spaceAfter=5, keepWithNext=1),
    "label": ParagraphStyle("label", fontName="Mono", fontSize=8, leading=10, textColor=GREY, spaceAfter=2),
    "bullet": ParagraphStyle("bullet", fontName="Sans", fontSize=9.2, leading=13, leftIndent=12, bulletIndent=0, spaceAfter=3),
    "summary": ParagraphStyle("summary", fontName="Sans", fontSize=10.5, leading=15, textColor=INK, spaceAfter=8),
}

class Rule(Flowable):
    def __init__(self, w=None, thick=0.8, color=INK, space=4):
        super().__init__(); self.w = w; self.thick = thick; self.color = color; self.space = space
    def wrap(self, aw, ah): self.aw = self.w or aw; return self.aw, self.thick + self.space * 2
    def draw(self):
        self.canv.setStrokeColor(self.color); self.canv.setLineWidth(self.thick)
        self.canv.line(0, self.space, self.aw, self.space)

class Swatch(Flowable):
    def __init__(self, c): super().__init__(); self.c = c
    def wrap(self, aw, ah): return 10, 10
    def draw(self): self.canv.setFillColor(self.c); self.canv.rect(0, 0, 10, 10, stroke=0, fill=1)

# ------------------------------------------------------------------ document
class Doc(BaseDocTemplate):
    def __init__(self, path):
        super().__init__(path, pagesize=letter, leftMargin=0.8 * inch, rightMargin=0.8 * inch, topMargin=0.85 * inch,
                         bottomMargin=0.75 * inch, title="SF Live Suite Reference Manual", author="Strange Frequencies")
        fr = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="f")
        self.addPageTemplates([PageTemplate(id="cover", frames=[fr], onPage=self.cover),
                               PageTemplate(id="body", frames=[fr], onPage=self.chrome)])
        self.section = ""
        self.sections = {}
        self.prevSections = {}
    def afterFlowable(self, f):
        if isinstance(f, Paragraph) and getattr(f, "toc_level", None) is not None:
            self.notify("TOCEntry", (f.toc_level, f.getPlainText(), self.page))
            if f.toc_level == 0: self.sections[self.page] = f.getPlainText()
    def handle_documentBegin(self):
        if self.sections: self.prevSections = dict(self.sections)
        self.sections = {}
        super().handle_documentBegin()
    def sectionFor(self, page):
        best = ""
        for p in sorted(self.prevSections):
            if p <= page: best = self.prevSections[p]
        return best
    def chrome(self, canv, doc):
        w, h = letter
        self.section = self.sectionFor (doc.page)
        canv.saveState()
        canv.setStrokeColor(INK); canv.setLineWidth(0.8)
        canv.line(self.leftMargin, h - 0.55 * inch, w - self.rightMargin, h - 0.55 * inch)
        canv.setFont("MonoB", 7.5); canv.setFillColor(INK)
        canv.drawString(self.leftMargin, h - 0.5 * inch, "SF LIVE SUITE  REFERENCE")
        canv.setFont("Mono", 7.5); canv.setFillColor(GREY)
        canv.drawRightString(w - self.rightMargin, h - 0.5 * inch, self.section.upper())
        canv.setStrokeColor(LIGHT); canv.line(self.leftMargin, 0.5 * inch, w - self.rightMargin, 0.5 * inch)
        canv.setFillColor(GREY); canv.setFont("Mono", 7.5)
        canv.drawString(self.leftMargin, 0.36 * inch, "V0.2  NOV 2026")
        canv.drawRightString(w - self.rightMargin, 0.36 * inch, str(doc.page))
        canv.restoreState()
    def cover(self, canv, doc):
        w, h = letter
        canv.saveState()
        canv.setFillColor(INK); canv.rect(0, 0, w, h, stroke=0, fill=1)
        canv.setFillColor(colors.white)
        canv.setFont("MonoB", 40); canv.drawString(0.8 * inch, h - 2.6 * inch, "SF LIVE SUITE")
        canv.setFont("Mono", 14); canv.drawString(0.8 * inch, h - 3.05 * inch, "REFERENCE MANUAL")
        canv.setStrokeColor(colors.white); canv.setLineWidth(1.2)
        canv.line(0.8 * inch, h - 3.35 * inch, w - 0.8 * inch, h - 3.35 * inch)
        canv.setFont("Mono", 9); canv.setFillColor(colors.HexColor("#9a9a9a"))
        y = h - 3.7 * inch
        for line in ["30 PLUGINS FOR PERFORMING HYPNOTIC / DUB TECHNO IN ABLETON LIVE",
                     "10 INSTRUMENTS   11 EFFECTS   9 MASTER AND MIX TOOLS",
                     "", "VERSION 0.2   NOVEMBER 2026", "STRANGE FREQUENCIES"]:
            canv.drawString(0.8 * inch, y, line); y -= 14
        canv.setFont("Mono", 7.5)
        canv.drawString(0.8 * inch, 0.8 * inch, "CONTROL RANGES AND DEFAULTS IN THIS MANUAL ARE GENERATED FROM THE PLUGIN CODE.")
        canv.restoreState()

def H1(text, level=0):
    p = Paragraph(text, S["h1"]); p.toc_level = level; return p
def H2(text):
    return Paragraph(text, S["h2"])
def P(text, st="body"): return Paragraph(text, S[st])
def bullets(items): return [Paragraph(t, S["bullet"], bulletText="—") for t in items]

def table(rows, widths, header=True, mono_first=True):
    data = []
    for r_i, row in enumerate(rows):
        cells = []
        for c_i, v in enumerate(row):
            if isinstance(v, Flowable): cells.append(v); continue
            if r_i == 0 and header: cells.append(Paragraph(v, ParagraphStyle("th", fontName="Mono", fontSize=7, leading=9, textColor=GREY)))
            elif c_i == 0 and mono_first: cells.append(Paragraph(v, S["cellmono"]))
            else: cells.append(Paragraph(v, S["cell"]))
        data.append(cells)
    t = Table(data, colWidths=widths, repeatRows=1 if header else 0, hAlign="LEFT")
    st = [("VALIGN", (0, 0), (-1, -1), "TOP"), ("LEFTPADDING", (0, 0), (-1, -1), 4), ("RIGHTPADDING", (0, 0), (-1, -1), 6),
          ("TOPPADDING", (0, 0), (-1, -1), 3.5), ("BOTTOMPADDING", (0, 0), (-1, -1), 3.5),
          ("LINEBELOW", (0, 0), (-1, -1), 0.4, LIGHT)]
    if header: st += [("LINEBELOW", (0, 0), (-1, 0), 0.9, INK)]
    t.setStyle(TableStyle(st))
    return t

def esc(s): return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

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

W = letter[0] - 1.6 * inch

# ---------------- PART 1
story.append(H1("1  HOW THE SUITE WORKS"))
story.append(Rule())
story.append(P("The suite is built for one kind of performance: very simple note content, often one note in a pattern that never changes, with all the variation played on knobs. Everything follows from that. Every plugin is designed to be played, not programmed, and every plugin talks to the others.", "summary"))

story.append(H2("THE RIG"))
story.append(P("One Live set that never changes. The track layout is fixed: one track per role (kick, percussion, rumble, bass, lead, voice, pad, deck). The OXI One MkII sequences everything; the OXI E16 plays the knobs. Songs are not projects: a song is a stored set of values in every plugin, recalled on the bar. You switch songs without loading anything."))
story.append(H2("THE PERFORMANCE ROW"))
story.append(P("The first eight controls of every plugin are its performance row: the large knobs at the top of its window, and the first eight parameters Live shows for the device, so one E16 row maps one plugin. Everything below the display is set-and-forget: models, ranges, routing, behaviour."))
story.append(H2("DENSITY, GATE, FILTER"))
story.append(P("Three controls cover most of what changes in a track, and they appear wherever they make sense. <b>DENSITY</b> is how many events happen (ghost notes fill the gaps). <b>GATE</b> is how long each event lasts. <b>FILTER</b> is what each event sounds like: a bipolar DJ filter, centre open, left low-pass, right high-pass. Play two against each other: density up with gate down is a tight, nervous build; density down with gate open is spacious dub; density up with the filter closed is tension waiting to be released."))
story.append(H2("SYSTEM CONTROLS ON EVERY PLUGIN"))
story.append(P("Below every plugin's controls: the MOTION row (two modulators) and the SYSTEM row (SLOW, MACRO). They are the same on every plugin and are not repeated in the module tables."))
story.append(table([["CONTROL", "RANGE", "WHAT IT DOES"]] + [[a, b, c] for a, b, c in C.SYSTEM], [1.1 * inch, 1.75 * inch, W - 2.85 * inch]))
story.append(H2("MOTION"))
story.append(P("Two modulators in every plugin, M1 and M2. Pick a SOURCE, a RATE, the TARGET knob and a DEPTH. Everything is locked to Live's song position: every plugin's modulators move together, a 64-bar ramp lands on the 64th bar, and a set replays the same way every time, RANDOM and DRIFT included (each plugin has its own random seed, saved with the set). Modulation adds to the knob, the macro and SLOW, and the knob shows a bar for where the modulation has taken it. Steps (SQUARE, RANDOM, a ramp restarting) are smoothed over 3 ms, so they never click. MOTION is part of songs and presets."))
story += bullets(["<b>Hypnotic movement:</b> DRIFT on a filter at 8 or 16 BAR, DEPTH 10-20 %. It never repeats, so the loop never sounds like a loop.",
                  "<b>Long builds:</b> RAMP UP at 32 or 64 BAR on ENERGY or a filter; turn DEPTH to set how far it goes.",
                  "<b>Kick-driven:</b> KICK on a bass's DRIVE or a pad's CUTOFF: every kick pushes it, RATE sets how fast it falls back.",
                  "<b>Played:</b> NOTES on an instrument (velocity) or INPUT on an effect (level) makes the sound respond to how hard it is played."])
story.append(H2("KEY"))
story.append(P("Write parts in C. SF CONTROL's KEY (stored per song) then plays them in the song's key on every SF instrument: BASS, LEAD, PAD and VOICE transpose by the nearest interval (-6 to +5 semitones); KICK, RUMBLE and STRIKE land their own pitch on the key root, so the low end is always in tune with the track. Notes already sounding follow a KEY change. KEY FOLLOW on each instrument opts out. OFF changes nothing, and without SF CONTROL running every instrument plays in C."))
story.append(P("Mind the low register. KEY moves parts by up to 6 semitones either way, and KICK, RUMBLE and STRIKE snap to the nearest root, so the low end can land high or very low: a kick body at 50 Hz becomes 69 Hz in C# and 37 Hz in D. Check every song's KEY on a sub. TUNE moves the kick from the snapped root and is stored per song: -12 takes the 69 Hz kick to the root an octave down, +7 or -5 puts it on the fifth. Where a bass goes too deep, move the part up an octave on the OXI."))
story.append(H2("NOTHING CAN HURT THE PA"))
story.append(P("Every knob is smoothed, so 7-bit MIDI never steps audibly. Resonance is level-compensated. Feedback above 100 % saturates instead of exploding. Every output is clamped at +12 dBFS. If a plugin ever produces an invalid value, or sits at the clamp for 250 ms (a runaway), it outputs silence, resets itself and shows <b>FAULT</b> in red in its header; it carries on working. <b>PANIC</b> on SF CONTROL fades every SF plugin out over 5 ms and clears every voice, tail and delay line: for a runaway feedback loop, a stuck note or a wrong recall in front of a crowd. Everything stays silent while you hold it (no input, no new notes; DECK keeps its place in time; every header shows PANIC in red) and fades back in over 5 ms when you let go. A set never reopens with PANIC held. Grab any knob in the dark."))
_cc_head = [H2("THE COLOUR CODE"), P("The plugins are black, white and grey. Colour only carries information:")]
cc = [["", "COLOUR", "MEANS"], [Swatch(RED), "RED", "Clip, FAULT (the plugin reset itself), missing file, true peak at the ceiling."], [Swatch(AMBER), "AMBER", "Conflict or warning: an ALLOC conflict, a deck armed and waiting, RETURN KNOB on BUILD."],
      [Swatch(GREEN), "GREEN", "Active or locked: bus connected, kick reporting, a song recall waiting for the bar, a value gliding (SLOW)."],
      [Swatch(CYAN), "CYAN", "The value of the control you are touching."]]
story.append(KeepTogether(_cc_head + [table(cc, [0.3 * inch, 0.8 * inch, W - 1.1 * inch])]))
story.append(PageBreak())

story.append(H1("2  SONGS AND PRESETS"))
story.append(Rule())
story.append(P("Every SF plugin stores 16 song slots of its own settings, saved with the Live set. SF CONTROL chooses the song; every plugin recalls its slot on the same bar line while Live plays (at once when stopped).", "summary"))
story.append(H2("SAVING A SONG"))
story += bullets(["Set every plugin how the song should start.", "Choose the song number on SF CONTROL (SONG).", "Press <b>STORE ALL</b> on SF CONTROL. Every SF plugin stores its current values into that song.",
                  "STORE in any plugin's header stores just that plugin into the selected song."])
story.append(H2("RECALLING A SONG"))
story += bullets(["Send Program Change on MIDI channel 16 to SF CONTROL (program 1 = song 01). Or turn SF CONTROL's SONG.",
                  "SF CONTROL decides the bar line and sends it with the song (and the song's KEY), so every plugin changes on the same bar whatever order Live processes the tracks in. A change sent within 30 ms after a bar line (on the downbeat) counts for that bar; later in the bar, for the next one. Inside an Arrangement loop that never reaches that bar, every plugin changes on the next bar line after the loop jumps back.",
                  "The header of every plugin shows SONG nn, and WAIT BAR in green while the recall is waiting for the bar line.",
                  "Recalled values are reported to Live, so the E16 rings jump to the new song's settings. A recalled knob is yours again the moment you turn it.",
                  "A plugin with nothing stored in that song stays as it is.",
                  "With SLOW on, the recall glides instead of jumping: a song change becomes an 8- or 16-bar transition.",
                  "Arrangement automation wins over recalled values, as on any device."])
story.append(H2("VST3 AND PROGRAM CHANGE"))
story.append(P("VST3 plugins don't receive MIDI Program Change as MIDI. SF CONTROL therefore lists 16 programs (SONG 01-16); Live 12.2 and later turn a Program Change on the track into a program change for the VST3, older versions don't. The AU version reads Program Change directly from MIDI in any version. If song changes don't arrive in the VST3, use the AU."))
story.append(H2("PRESETS"))
story += bullets(["The <b>PRESET</b> button in every header opens the menu: factory presets grouped by style (DUB, ACID, HYPNOTIC ...), your own presets, SAVE USER PRESET and SHOW PRESET FOLDER. The button shows the preset that is loaded.",
                  "A preset sets every sound control of the plugin, MOTION included, and Live is told about every change, so the E16 rings follow. It never touches SLOW, MACRO, ALLOC, ROLE, a source setting or KEY FOLLOW.",
                  "181 factory presets across 25 plugins. Use them as starting points, then store the result into a song.",
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
       ["Live instruments / hardware", "SF STRIP last on the track: the suite's performance row, Pocket and ALLOC for any sound."],
       ["Stems", "SF TAP with ROLE set (no sound of its own), or SF STRIP to play them."]]
story.append(table(lay, [1.3 * inch, W - 1.3 * inch]))
story.append(H2("THE E16"))
story += bullets(["Map with Live's MIDI Map (Cmd-M): click a knob in the plugin (or in Live's device panel), turn the E16 knob.",
                  "In Live's MIDI preferences turn <b>Remote</b> on for the E16 <i>output</i>. Live then sends values back, so the rings follow song recalls and ALLOC slot changes and the knobs hold the new values.",
                  "The E16's default absolute 0-127 output is right; the plugins smooth every step."])
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
story.append(PageBreak())

# ---------------- PART 2: modules
order = [("INSTRUMENTS", ["KICK", "PERC", "RUMBLE", "BASS", "LEAD", "PAD", "VOICE", "STRIKE", "DECK", "CONTROL"]),
         ("EFFECTS", ["STRIP", "FILTER", "GATE", "DENSITY", "DELAY", "REVERB", "DRIVE", "RADIO", "REPEAT", "DUCK", "ENERGY"]),
         ("MASTER AND MIX", ["ISOLATOR", "BUILD", "GLUE", "TONE", "LOWEND", "LIMIT", "ROOMEQ", "ALLOC", "TAP"])]
SYSTEM_IDS = ("slow", "macro_src", "macro_amt")
def is_system(pid): return pid in SYSTEM_IDS or pid[:3] in ("m1_", "m2_")
chapter = 7
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
        story.append(CondPageBreak(4.5 * inch))
        story.append(h)
        story.append(Paragraph(("INSTRUMENT, MIDI TRACK" if specs[mid]["instrument"] else "AUDIO EFFECT") + (f"   LATENCY {specs[mid]['latency']} SAMPLES AT 48 KHZ" if specs[mid]["latency"] else ""), S["label"]))
        story.append(Rule(thick=0.5, color=LIGHT, space=2))
        story.append(P(esc(m["summary"]), "summary"))
        img = os.path.join(ROOT, "docs", "snapshots", f"{mid}.png")
        if os.path.exists(img):
            from PIL import Image as PI
            iw, ih = PI.open(img).size
            w = W; hgt = w * ih / iw
            if hgt > 4.2 * inch: hgt = 4.2 * inch; w = hgt * iw / ih
            im = Image(img, width=w, height=hgt); im.hAlign = "LEFT"
            story.append(im); story.append(Spacer(1, 8))
        story.append(H2("USING IT"))
        story += bullets([esc(u) for u in m["use"]])
        if m["models"]:
            story.append(H2("OBJECTS" if mid == "STRIKE" else "MODELS" if mid not in ("GATE", "ENERGY") else "REFERENCE"))
            story.append(table([["NAME", "SOUND"]] + [[esc(a), esc(b)] for a, b in m["models"]], [1.55 * inch, W - 1.55 * inch]))
        desc = dict(C.COMMON); desc.update(m["desc"])
        def rng(p):
            if p["type"] == "knob": return f"{esc(p['min'])} to {esc(p['max'])}<br/>default {esc(p['def'])}"
            if p["type"] == "menu":
                ch = p["choices"].split("|")
                listing = ", ".join(ch) if len(ch) <= 8 else ", ".join(ch[:6]) + f" ... ({len(ch)})"
                return f"{esc(listing)}<br/>default {esc(p['def'])}"
            if p["type"] == "toggle": return f"ON / OFF<br/>default {esc(p['def'])}"
            return "HOLD"
        for label, rows in (("PERFORMANCE ROW", [p for p in sp if p["primary"]]), ("SETUP", [p for p in sp if not p["primary"] and p["id"] != "model"])):
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
                  "DECK track log: Documents/SF Live Suite/tracklog.txt.", "DECK tracks are referenced by file path, not embedded: keep the files where they are."])
story.append(H2("WHEN SOMETHING ISN'T RIGHT"))
story.append(table([["SYMPTOM", "CHECK"],
                    ["Nothing ducks", "SF CONTROL: BUS SHARED and KICK REPORTING. The kick must be an SF KICK (or use SF DUCK with SOURCE SIDECHAIN)."],
                    ["Pump early or late", "Live buffer size 512 or less. Then trim POCKET OFFSET on SF CONTROL by ear."],
                    ["Songs don't change", "PC ENABLE on, PC CHANNEL matches the OXI. With VST3, try the AU."],
                    ["E16 rings don't follow", "Remote on for the E16 output in Live's MIDI preferences; map with Live's MIDI Map."],
                    ["ALLOC does nothing", "ENABLE on, DEPTH above 0 on the band, the instruments' ALLOC toggles on. In DYNAMIC, the favoured source must be playing in that band."],
                    ["DECK row says MISSING", "The file moved. Drop it again."],
                    ["A knob shows GLIDE", "SLOW is on: the knob is travelling to where you set it."],
                    ["A knob moves on its own", "MOTION: check M1 / M2 TARGET in the plugin's MOTION row."],
                    ["Header shows FAULT (red)", "The plugin caught an invalid value or a runaway and reset itself to silence. It carries on working. If it happens again, note what was playing: it is a bug."],
                    ["Something won't stop", "PANIC on SF CONTROL: every SF plugin fades out and clears."],
                    ["Pitch is wrong after a song change", "KEY on SF CONTROL is stored per song. KEY FOLLOW off on an instrument ignores it."],
                    ["Levels changed after the update", "Since Nov 3 every model is calibrated to its role's target (chapter 5). Re-balance the faders once."]], [1.7 * inch, W - 1.7 * inch], mono_first=False))

doc = Doc(os.path.join(ROOT, "docs", "SF-Live-Suite-Manual.pdf"))
doc.multiBuild(story)
print("ok")
