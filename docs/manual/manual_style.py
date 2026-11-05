"""The SF manuals' look: fonts, colours, paragraph styles, page chrome and the shared drawings.
Used by build_manual.py (the suite reference) and build_show_manual.py (the SF SHOW operator's manual)."""
import os
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
    PA and REC (both from Resampling); along the bottom, the room mics > 94 ROOM > the headphone pair."""
    H = 244
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
        # The room: its own path, never into the set (SF CAPTURE passes nothing but TERROIR's sweep).
        yr = 2
        self.box(x0, yr, w0, bh, "ROOM MICS", ("INPUTS 1/2",))
        self.box(x1, yr, w1, bh, "94 ROOM", ("SF CAPTURE",))
        self.box(x3, yr, w3, bh, "HEADPHONES 3/4", ("CUE; THE CAPTURE SPEAKER",))
        self.arrow(x0 + w0, yr + bh / 2, x1, yr + bh / 2)
        self.arrow(x1 + w1, yr + bh / 2, x3, yr + bh / 2, dashed=True)

class E16Grid(Flowable):
    """The E16 in SF SHOW: a 4 x 4 grid numbered row by row from the top left. Rows 1-2 are the same on
    every page; rows 3-4 are drawn as they are on an instrument page. A light blue ring is a volume."""
    CW, RH = 100, 38
    H = 4 * RH + 16
    def __init__(self, cells): super().__init__(); self.cells = cells
    def wrap(self, aw, ah): self.aw = aw; return aw, self.H
    def draw(self):
        c = self.canv
        top = self.H - 2
        for r in range(4):
            for col in range(4):
                i = r * 4 + col + 1
                name, sub, ring, quiet = self.cells[i]
                x, y = col * self.CW, top - (r + 1) * self.RH
                cx, cy = x + 13, y + self.RH / 2
                c.setLineWidth(2.4); c.setStrokeColor(ring); c.circle(cx, cy, 10.5, stroke=1, fill=0)
                c.setFillColor(INK); c.setFont("MonoB", 7.2); c.drawCentredString(cx, cy - 2.5, str(i))
                c.setFillColor(GREY if quiet else INK); c.setFont("MonoB", 7.2); c.drawString(x + 29, cy + 1.6, name)
                if sub: c.setFillColor(GREY); c.setFont("Mono", 5.9); c.drawString(x + 29, cy - 6.6, sub)
        yd = top - 2 * self.RH
        c.setStrokeColor(GREY); c.setLineWidth(0.6); c.setDash(2.2, 2.2); c.line(0, yd, 4 * self.CW, yd); c.setDash()
        xb = 4 * self.CW + 8
        for (y1, y0, text, sub) in ((top - 3, yd + 3, "EVERY PAGE", "ROWS 1 AND 2"), (yd - 3, top - 4 * self.RH + 3, "THE PAGE", "ROWS 3 AND 4")):
            c.setStrokeColor(INK); c.setLineWidth(0.8)
            c.line(xb, y1, xb + 4, y1); c.line(xb + 4, y1, xb + 4, y0); c.line(xb, y0, xb + 4, y0)
            c.setFillColor(INK); c.setFont("MonoB", 7.2); c.drawString(xb + 10, (y0 + y1) / 2 + 1.5, text)
            c.setFillColor(GREY); c.setFont("Mono", 5.9); c.drawString(xb + 10, (y0 + y1) / 2 - 6.5, sub)
        c.setFillColor(GREY); c.setFont("Mono", 5.9)
        c.drawString(0, 2, "ROWS 3 AND 4 AS ON AN INSTRUMENT PAGE.  LIGHT BLUE RING: A VOLUME.")

# ------------------------------------------------------------------ document
class Doc(BaseDocTemplate):
    def __init__(self, path, title="SF Live Suite Reference Manual", header="SF LIVE SUITE  REFERENCE", footer="V0.2  NOV 2026",
                 cover_title="SF LIVE SUITE", cover_sub="REFERENCE MANUAL",
                 cover_lines=("31 PLUGINS FOR PERFORMING HYPNOTIC / DUB TECHNO IN ABLETON LIVE",
                              "10 INSTRUMENTS   12 EFFECTS   9 MASTER AND MIX TOOLS",
                              "", "VERSION 0.2   NOVEMBER 2026", "STRANGE FREQUENCIES"),
                 cover_note="CONTROL RANGES AND DEFAULTS IN THIS MANUAL ARE GENERATED FROM THE PLUGIN CODE."):
        super().__init__(path, pagesize=letter, leftMargin=0.8 * inch, rightMargin=0.8 * inch, topMargin=0.85 * inch,
                         bottomMargin=0.75 * inch, title=title, author="Strange Frequencies")
        self.header, self.footer = header, footer
        self.cover_title, self.cover_sub, self.cover_lines, self.cover_note = cover_title, cover_sub, cover_lines, cover_note
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
        canv.drawString(self.leftMargin, h - 0.5 * inch, self.header)
        canv.setFont("Mono", 7.5); canv.setFillColor(GREY)
        canv.drawRightString(w - self.rightMargin, h - 0.5 * inch, self.section.upper())
        canv.setStrokeColor(LIGHT); canv.line(self.leftMargin, 0.5 * inch, w - self.rightMargin, 0.5 * inch)
        canv.setFillColor(GREY); canv.setFont("Mono", 7.5)
        canv.drawString(self.leftMargin, 0.36 * inch, self.footer)
        canv.drawRightString(w - self.rightMargin, 0.36 * inch, str(doc.page))
        canv.restoreState()
    def cover(self, canv, doc):
        w, h = letter
        canv.saveState()
        canv.setFillColor(INK); canv.rect(0, 0, w, h, stroke=0, fill=1)
        canv.setFillColor(colors.white)
        canv.setFont("MonoB", 40); canv.drawString(0.8 * inch, h - 2.6 * inch, self.cover_title)
        canv.setFont("Mono", 14); canv.drawString(0.8 * inch, h - 3.05 * inch, self.cover_sub)
        canv.setStrokeColor(colors.white); canv.setLineWidth(1.2)
        canv.line(0.8 * inch, h - 3.35 * inch, w - 0.8 * inch, h - 3.35 * inch)
        canv.setFont("Mono", 9); canv.setFillColor(colors.HexColor("#9a9a9a"))
        y = h - 3.7 * inch
        for line in self.cover_lines:
            canv.drawString(0.8 * inch, y, line); y -= 14
        canv.setFont("Mono", 7.5)
        canv.drawString(0.8 * inch, 0.8 * inch, self.cover_note)
        canv.restoreState()

def H1(text, level=0):
    p = Paragraph(text, S["h1"]); p.toc_level = level; return p
def H2(text):
    return Paragraph(text, S["h2"])
def P(text, st="body"): return Paragraph(text, S[st])
def bullets(items): return [Paragraph(t, S["bullet"], bulletText="—") for t in items]
def numbered(items): return [Paragraph(t, S["bullet"], bulletText=f"{i}.") for i, t in enumerate(items, 1)]

def table(rows, widths, header=True, mono_first=True, pad=3.5):
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
          ("TOPPADDING", (0, 0), (-1, -1), pad), ("BOTTOMPADDING", (0, 0), (-1, -1), pad),
          ("LINEBELOW", (0, 0), (-1, -1), 0.4, LIGHT)]
    if header: st += [("LINEBELOW", (0, 0), (-1, 0), 0.9, INK)]
    t.setStyle(TableStyle(st))
    return t

def esc(s): return (s or "").replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

W = letter[0] - 1.6 * inch      # the text column
