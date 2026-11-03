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

class RoutingDiagram(Flowable):
    """The show template's signal flow, drawn in the manual's own type: sources > LIVE BUS > Main >
    PA and REC (both from Resampling)."""
    H = 214
    def wrap(self, aw, ah): self.aw = aw; return aw, self.H
    def box(self, x, y, w, h, text, subs=(), heavy=False):
        c = self.canv
        c.setLineWidth(1.6 if heavy else 0.8); c.setStrokeColor(INK); c.setFillColor(colors.white)
        c.rect(x, y, w, h, stroke=1, fill=1)
        lines = len(subs)
        ty = y + h / 2 - 2.6 + 3.6 * lines
        c.setFillColor(INK); c.setFont("MonoB", 7.4); c.drawCentredString(x + w / 2, ty, text)
        c.setFont("Mono", 6.0); c.setFillColor(GREY)
        for i, sub in enumerate(subs): c.drawCentredString(x + w / 2, ty - 8.0 * (i + 1), sub)
    def arrow(self, x0, y0, x1, y1, dashed=False):
        import math
        c = self.canv
        col = GREY if dashed else INK
        a = math.atan2(y1 - y0, x1 - x0)
        c.setStrokeColor(col); c.setLineWidth(0.8)
        if dashed: c.setDash(2.2, 2.2)
        c.line(x0, y0, x1 - 4 * math.cos(a), y1 - 4 * math.sin(a))
        c.setDash()
        p = c.beginPath(); p.moveTo(x1, y1)
        p.lineTo(x1 - 6 * math.cos(a) + 2.6 * math.sin(a), y1 - 6 * math.sin(a) - 2.6 * math.cos(a))
        p.lineTo(x1 - 6 * math.cos(a) - 2.6 * math.sin(a), y1 - 6 * math.sin(a) + 2.6 * math.cos(a))
        p.close(); c.setFillColor(col); c.drawPath(p, stroke=0, fill=1)
    def draw(self):
        c = self.canv
        aw, H = self.aw, self.H
        bh, gap = 22, 6
        x0, w0 = 26, 112
        top = H - 4
        ys = [top - bh - i * (bh + gap) for i in range(5)]                 # KICK, LOW, DRUMS, MUSIC, RETURNS
        src = [("01 KICK", ("SF KICK",)), ("10 LOW", ("SF ENERGY",)), ("20 DRUMS", ("SF ENERGY, SF REPEAT",)),
               ("30 MUSIC", ("SF ENERGY",)), ("A DUB  B SPACE", ("SF DELAY, SF REVERB",))]
        for (t, sub), y in zip(src, ys): self.box(x0, y, w0, bh, t, sub)
        yb = [ys[4] - 18 - bh, ys[4] - 18 - 2 * bh - gap]                 # BRIDGE, DECK
        self.box(x0, yb[0], w0, bh, "40 BRIDGE", ("LIVE AUDIO CLIPS",))
        self.box(x0, yb[1], w0, bh, "41 DECK", ("SF DECK",))
        # Sends: the groups feed the returns (left of the column, dashed).
        xs = 12
        c.setStrokeColor(GREY); c.setLineWidth(0.8); c.setDash(2.2, 2.2)
        for y in (ys[2], ys[3]): c.line(x0, y + bh / 2, xs, y + bh / 2)
        c.line(xs, ys[2] + bh / 2, xs, ys[4] + bh / 2)
        c.setDash()
        self.arrow(xs, ys[4] + bh / 2, x0, ys[4] + bh / 2, dashed=True)
        c.saveState(); c.translate(6, (ys[2] + ys[4] + bh) / 2); c.rotate(90)
        c.setFont("Mono", 6.0); c.setFillColor(GREY); c.drawCentredString(0, 0, "SENDS"); c.restoreState()
        # LIVE BUS
        x1, w1, h1 = 160, 96, 44
        yl = (ys[0] + ys[4] + bh) / 2 - h1 / 2
        self.box(x1, yl, w1, h1, "90 LIVE BUS", ("ISOLATOR BUILD GLUE", "TONE LOWEND ALLOC"))
        for y in ys: self.arrow(x0 + w0, y + bh / 2, x1, yl + h1 / 2)
        # MAIN
        x2, w2, h2 = 280, 56, 40
        ym = (yl + h1 / 2 + (yb[0] + yb[1] + bh) / 2) / 2 - h2 / 2
        self.box(x2, ym, w2, h2, "MAIN", ("SF LIMIT",), heavy=True)
        self.arrow(x1 + w1, yl + h1 / 2, x2, ym + h2 / 2 + 8)
        for y in yb: self.arrow(x0 + w0, y + bh / 2, x2, ym + h2 / 2 - 8)
        # Outputs
        w3 = 122; x3 = aw - w3
        yo = [ym + h2 / 2 + 40, ym + h2 / 2 - bh / 2, ym + h2 / 2 - 40 - bh]
        self.box(x3, yo[0], w3, bh, "UNUSED OUTPUT", ("MAIN'S OWN OUTPUT",))
        self.box(x3, yo[1], w3, bh, "92 PA", ("RESAMPLING > ROOM EQ > PA",))
        self.box(x3, yo[2], w3, bh, "93 REC", ("RESAMPLING > RECORDING",))
        self.arrow(x2 + w2, ym + h2 / 2 + 10, x3, yo[0] + bh / 2, dashed=True)
        self.arrow(x2 + w2, ym + h2 / 2, x3, yo[1] + bh / 2)
        self.arrow(x2 + w2, ym + h2 / 2 - 10, x3, yo[2] + bh / 2)

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
def numbered(items): return [Paragraph(t, S["bullet"], bulletText=f"{i}.") for i, t in enumerate(items, 1)]

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
story.append(P("Chapter 7 is a complete show set built on this layout."))
story.append(PageBreak())

# ---------------- 7: the show template
story.append(H1("7  THE SHOW TEMPLATE"))
story.append(Rule())
story.append(P("One Live set for every show, built on fixed roles. The OXI ONE plays the notes and patterns; the E16 plays the sound, the balance and the transitions. The E16's top half is the same on every page, so the whole arrangement is always under one hand while the other works deep in one sound.", "summary"))

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
    ["93 REC", "nothing", "audio: Resampling"]], [1.35 * inch, 3.25 * inch, W - 4.6 * inch]))
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
    ["Cue", "Live's Cue Out; DECK's CUE track", "the headphone outputs"]], [1.75 * inch, 2.75 * inch, W - 4.5 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Main is the set. Exporting or resampling it gives the release mix; the venue's correction (ROOM EQ) exists only on 92 PA, so the recording never hears it.",
                  "<b>Disable every send on 90 LIVE BUS</b> (right-click a send, Disable Send). The returns feed LIVE BUS, and Live can't compensate a return's delay on a track whose matching send is active, even at zero (Ableton's Delay Compensation FAQ).",
                  "Pull 90 LIVE BUS down and the live instruments and their effects go while BRIDGE and DECK keep playing. Mute 92 PA and the speakers go silent while REC keeps recording.",
                  "PANIC silences every SF plugin, DECK included, but not Live's own clips and devices: for total silence, mute 92 PA.",
                  "BRIDGE is for transitions and regrouping. It still needs Live: it is not a backup for a computer failure.",
                  "ROOM EQ only cuts, so it needs no limiter after it; LIMIT's -1 dB ceiling leaves the margin."])

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
    ["ONE out (the one it takes clock from)", "off", "on", "off"], ["E16 in", "off", "off", "on"], ["E16 out", "off", "off", "on"]],
    [2.6 * inch, 0.9 * inch, 0.9 * inch, W - 4.4 * inch]))
story.append(Spacer(1, 6))
story += bullets(["Live is the clock master. On the ONE: USB device mode, sync from USB, Auto Sync on (it starts with Live), Start and Stop accepted, MIDI Thru off. Never send clock both ways.",
                  "Audio: 48 kHz, buffer 256 (Pocket needs 512 or less), Delay Compensation on, <b>Reduced Latency When Monitoring off</b> (it skips compensation on monitored tracks, so returns and Pocket drift), launch quantization 1 bar, record quantization off.",
                  "Calibrate once: record a plain quarter-note pattern from the ONE, compare it with Live's grid and set the ONE port's MIDI Clock Sync Delay in Live's preferences. Leave POCKET OFFSET at 0 unless the pump is still off after that.",
                  "Every Pocket stage in a chain adds its 512-sample lookahead to the total latency (compensated, but it adds up): no SF TAP or SF DUCK on tracks that already hold SF instruments."])

story.append(H2("THE E16: TOP HALF"))
story.append(P("One E16 scene for the whole rig. Encoders 1-8 are the same on every page; 9-16 change with the page."))
story.append(table([["ENCODER", "TURN", "PRESS"],
    ["1", "LOW ENERGY (SF ENERGY on 10 LOW)", ""],
    ["2", "DRUMS ENERGY", ""],
    ["3", "MUSIC ENERGY", ""],
    ["4", "BUILD (SF BUILD on 90 LIVE BUS)", "DROP (hold): the build snaps off on the next bar"],
    ["5", "KICK volume", ""],
    ["6", "LOW group volume", ""],
    ["7", "DRUMS group volume", ""],
    ["8", "MUSIC group volume", ""],
    ["13 (every page)", "", "92 PA on / off (toggle): the speakers' kill switch"],
    ["16 (every page)", "", "PANIC (hold): every SF plugin silent while held"]], [1.3 * inch, 2.6 * inch, W - 3.9 * inch]))
story.append(Spacer(1, 6))
story += bullets(["The ENERGY knobs are the group DJ filters, plus drive, space, echo and chop: centre is neutral, left takes energy out, right puts it in.",
                  "Each instrument's LEVEL belongs to its song; the faders here are your balance on the night. Set every LEVEL at soundcheck so a fader at 0 dB gives the intended mix, and give the fader mappings a maximum of 0 dB (Max in Live's MIDI Mappings)."])

story.append(H2("THE E16: PAGES"))
story.append(table([["PAGE", "ENCODERS 9-12", "ENCODERS 13-16"],
    ["1 PERFORM", "ACID CUTOFF / STAB send A / TEXTURE COLOR / VOICE FILTER", "DUB return / SPACE return / LIVE BUS volume / BRIDGE volume"],
    ["2 KICK", "TUNE / DECAY / PUNCH / CLICK", "DENSITY / FILTER / DRIVE / LEVEL"],
    ["3 BASS", "CUTOFF / RES / ENV AMT / GATE", "DENSITY / FILTER / DRIVE / LEVEL"],
    ["4 RUMBLE", "SIZE / DARK / GRIT / GATE", "DENSITY / FILTER / PUMP / LEVEL"],
    ["5 HATS", "CLOSED: DENSITY / DECAY / TONE; OPEN: DENSITY", "OPEN: DECAY / TONE; CLAP: DENSITY / DECAY"],
    ["6 STRIKE", "POWER / DISTANCE / SIZE / DECAY", "DENSITY / FILTER / MATERIAL / LEVEL"],
    ["7 ACID", "CUTOFF / RES / ENV AMT / GATE", "DENSITY / FILTER / DRIVE / LEVEL"],
    ["8 STAB", "CUTOFF / RES / ENV AMT / GATE", "DENSITY / FILTER / SPACE / LEVEL"],
    ["9 TEXTURE", "CUTOFF / COLOR / DETUNE / GATE", "DENSITY / FILTER / ATTACK / LEVEL"],
    ["10 VOICE", "SIGNAL / TONE / ECHO / GATE", "DENSITY / FILTER / DRIVE / LEVEL"],
    ["11 SPACE", "DUB: TIME / FEEDBACK / TONE / DRIVE", "SPACE: SIZE / DECAY / DARK / DUCK"],
    ["12 MACROS", "MACRO 1-4", "MACRO 5 / MACRO 6 / REPEAT LENGTH / REPEAT PITCH"]], [0.95 * inch, 2.95 * inch, W - 3.9 * inch]))
story.append(Spacer(1, 6))
story += bullets(["On every instrument page 13 is DENSITY, 14 FILTER and 16 LEVEL, and 12 is GATE wherever there is one: the hand learns one layout. The rest is the plugin's own performance row, with a swap where a model's character lives elsewhere (GRANULAR: COLOR is grain size, DETUNE is scatter; STAB: SPACE is the ensemble).",
                  "Presses on 11 SPACE: 9 DUB THROW (hold), 10 DUB FREEZE (toggle), 13 SPACE THROW (hold), 14 SPACE FREEZE (toggle). On 12 MACROS: 15 REPEAT ROLL (hold).",
                  "The HATS page is the fill page: CLOSED DENSITY with ECHO SPACE 1/16 turns 1/8 hats into 1/16s, the send-delay trick built in."])

story.append(H2("THE E16: ADDRESSES AND SETTINGS"))
story += bullets(["One address per parameter. A parameter on two pages (ACID CUTOFF on PERFORM and on ACID) sends the same channel and CC from both encoders: two ways to reach one mapping.",
                  "A clean scheme: one E16 channel per destination (1 KICK, 2 BASS, 3 RUMBLE, 4 CLOSED, 5 OPEN, 6 CLAP, 7 STRIKE, 8 ACID, 9 STAB, 10 TEXTURE, 11 VOICE, 12 DUB, 13 SPACE, 14 groups, 15 LIVE BUS / BRIDGE / PA / REPEAT, 16 CONTROL). Inside a channel: CC 20-27 the eight controls of its page in order, CC 28 its volume, CC 29 send A, CC 30 send B. Mixed pages use each encoder's own channel.",
                  "Absolute CC 0-127, one destination per encoder, no value sent on page change, USB Thru off. Live's Takeover Mode: Pick-Up.",
                  "<b>Map the E16 straight to the SF parameters, not through Instrument Rack macros.</b> A song recall changes the plugin's value underneath a macro: the macro doesn't follow, its ring shows the wrong value and the next turn jumps the sound back. Ranges go in Live's MIDI Mappings (Min and Max per mapping): give resonance and feedback the part of their travel that matters, and widen it when the music asks.",
                  "Check the feedback: move a parameter in Live and watch the ring, change page and come back. A ring shows the stored value; SLOW and MOTION can take the sound elsewhere for a while.",
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
                  "Rehearse song recall and SF REPEAT in your Live before relying on them in a transition: both changed on Nov 3."])

story.append(H2("RECORDING AND REHEARSAL"))
story += bullets(["Instrument tracks on Monitor In: they play from the ONE whether or not they're armed.",
                  "To record: RESET on SF LIMIT, arm the instrument tracks and 93 REC, Automation Arm on, Arrangement record, play. The stereo recording on REC is the set; the MIDI and automation are for editing and rehearsal.",
                  "To replay recorded MIDI: stop the ONE, set those tracks to Monitor Auto and disarm them (Monitor In ignores a track's own clips).",
                  "Collect All and Save. SF samples are inside the set; DECK tracks are referenced by path, so keep the files where they are."])

story.append(H2("BUILD ORDER"))
story += numbered(["CONTROL, KICK, BASS, LIVE BUS, PA and REC. Clock and latency calibrated, a short recording made.",
                   "CLOSED, OPEN, CLAP and STRIKE.",
                   "The E16 top half.",
                   "ACID, STAB, TEXTURE and VOICE, one at a time.",
                   "The returns; check the sends are disabled on LIVE BUS.",
                   "The E16 pages and the macros.",
                   "BRIDGE and DECK; rehearse moving between them and LIVE BUS.",
                   "Record a whole set before adding anything else."])
story.append(H2("BEFORE THE FIRST SHOW"))
story += bullets(["CONTROL shows BUS SHARED and KICK REPORTING; the pump lands on the kick.",
                  "The ONE's Program Change recalls the song, every track on the same bar, and a recalled knob is yours the moment you turn it. (VST3 needs Live 12.2 or later; the AU works in any version.)",
                  "PANIC held on the E16: silence, and every header shows PANIC. Released: back without a click. 92 PA muted: the speakers silent, REC still recording.",
                  "A minute recorded on REC with a deep ROOM EQ cut: the cut isn't in the recording."])
story.append(PageBreak())

# ---------------- PART 2: modules
order = [("INSTRUMENTS", ["KICK", "PERC", "RUMBLE", "BASS", "LEAD", "PAD", "VOICE", "STRIKE", "DECK", "CONTROL"]),
         ("EFFECTS", ["STRIP", "FILTER", "GATE", "DENSITY", "DELAY", "REVERB", "DRIVE", "RADIO", "REPEAT", "DUCK", "ENERGY"]),
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
                    ["Songs don't change", "PC ENABLE on, PC CHANNEL matches the OXI. The VST3 needs Live 12.2 or later; otherwise use the AU."],
                    ["E16 rings don't follow", "Remote on for the E16 output in Live's MIDI preferences; map with Live's MIDI Map."],
                    ["ALLOC does nothing", "ENABLE on, DEPTH above 0 on the band, the instruments' ALLOC toggles on. In DYNAMIC, the favoured source must be playing in that band."],
                    ["DECK row says MISSING", "The file moved. Drop it again."],
                    ["A knob shows GLIDE", "SLOW is on: the knob is travelling to where you set it."],
                    ["A knob moves on its own", "MOTION: check M1 / M2 TARGET in the plugin's MOTION row."],
                    ["Header shows FAULT (red)", "The plugin caught an invalid value or a runaway and reset itself to silence. It carries on working. If it happens again, note what was playing: it is a bug."],
                    ["Something won't stop", "PANIC on SF CONTROL: every SF plugin fades out, clears and stays silent while held. Live's own clips and devices aren't SF: mute the PA track for those."],
                    ["Header shows PANIC (red)", "PANIC is held on SF CONTROL. Check that the button mapped to it isn't latched (it must be momentary)."],
                    ["CONTROL shows LOCAL ONLY", "The plugins come from different builds. Rebuild and install the whole suite."],
                    ["A return doubles the dry signal", "DRY off on SF DELAY, SF REVERB or SF DENSITY on a return track."],
                    ["Pitch is wrong after a song change", "KEY on SF CONTROL is stored per song. KEY FOLLOW off on an instrument ignores it."],
                    ["Levels changed after the update", "Since Nov 3 every model is calibrated to its role's target (chapter 5). Re-balance the faders once."]], [1.7 * inch, W - 1.7 * inch], mono_first=False))

doc = Doc(os.path.join(ROOT, "docs", "SF-Live-Suite-Manual.pdf"))
doc.multiBuild(story)
print("ok")
