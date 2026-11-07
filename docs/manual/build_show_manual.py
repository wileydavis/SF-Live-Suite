#!/usr/bin/env python3
"""Builds docs/SF-SHOW-Operators-Manual.pdf: the show template, the E16 scene SF SHOW and the OXI One
MkII, and how to play them. Same look as the suite reference (manual_style.py). Every E16 page,
press and address is drawn from controllers/e16/layout.py, the scene's one source."""
import math, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.dont_write_bytecode = True
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
from manual_style import *     # fonts, colours, styles, page chrome, shared drawings
from reportlab.pdfbase.pdfmetrics import stringWidth
import layout as E16
import content as C           # the plugins' own control descriptions (the suite reference uses them too)


# ------------------------------------------------------------------ drawing helpers
def box(c, x, y, w, h, title, subs=(), heavy=False, fill=colors.white, title_size=7.4, sub_size=6.0):
    """A box in the manual's style: bold mono name, grey mono lines under it, centred."""
    c.setLineWidth(1.6 if heavy else 0.8); c.setStrokeColor(INK); c.setFillColor(fill)
    c.rect(x, y, w, h, stroke=1, fill=1)
    n = len(subs)
    ty = y + h / 2 - title_size * 0.35 + (sub_size + 2.2) * n / 2
    c.setFillColor(INK); c.setFont("MonoB", title_size); c.drawCentredString(x + w / 2, ty, title)
    c.setFont("Mono", sub_size); c.setFillColor(GREY)
    for i, s in enumerate(subs): c.drawCentredString(x + w / 2, ty - (sub_size + 2.2) * (i + 1), s)


def arrow(c, x0, y0, x1, y1, dashed=False, col=None):
    col = col or (GREY if dashed else INK)
    a = math.atan2(y1 - y0, x1 - x0)
    c.setStrokeColor(col); c.setLineWidth(0.8)
    if dashed: c.setDash(2.2, 2.2)
    c.line(x0, y0, x1 - 4 * math.cos(a), y1 - 4 * math.sin(a))
    c.setDash()
    p = c.beginPath(); p.moveTo(x1, y1)
    p.lineTo(x1 - 6 * math.cos(a) + 2.6 * math.sin(a), y1 - 6 * math.sin(a) - 2.6 * math.cos(a))
    p.lineTo(x1 - 6 * math.cos(a) - 2.6 * math.sin(a), y1 - 6 * math.sin(a) + 2.6 * math.cos(a))
    p.close(); c.setFillColor(col); c.drawPath(p, stroke=0, fill=1)


def label(c, x, y, text, size=6.0, col=GREY, font="Mono", align="c"):
    c.setFont(font, size); c.setFillColor(col)
    {"c": c.drawCentredString, "l": c.drawString, "r": c.drawRightString}[align](x, y, text)


def knock(c, x, y, text, size=5.6, col=GREY, font="Mono", align="l"):
    """A label on a white patch, for words that sit over grid lines."""
    w = stringWidth(text, font, size)
    x0 = x if align == "l" else (x - w / 2 if align == "c" else x - w)
    c.setFillColor(colors.white); c.rect(x0 - 1.5, y - 1.8, w + 3, size + 2.4, stroke=0, fill=1)
    label(c, x, y, text, size, col, font, align)


# ------------------------------------------------------------------ 1: the rig
class RigDiagram(Flowable):
    """The three parts and what runs between them: notes and Program Change from the ONE, clock back
    to it; knobs and presses from the E16, values back to its rings; audio to the interface, and the
    room mics into it."""
    H = 266
    def wrap(self, aw, ah): self.aw = aw; return aw, self.H
    def draw(self):
        c, aw = self.canv, self.aw
        bw, lw = 112, 136
        xo, xl, xe = 0, (aw - lw) / 2, aw - bw
        yb, hb = 140, 60                      # OXI and E16 boxes
        yl, hl = 124, 92                      # Live
        box(c, xo, yb, bw, hb, "OXI ONE MKII", ("8 SEQUENCERS", "PLAYS THE NOTES", "AND THE PATTERNS"))
        box(c, xl, yl, lw, hl, "LIVE: SF SHOW.ALS", ("SF PLUGINS ON EVERY TRACK", "SF CONTROL: SONGS, KEY,", "MACROS, PANIC",
                                                    "THE CLOCK MASTER"), heavy=True)
        box(c, xe, yb, bw, hb, "OXI E16", ("SCENE SF SHOW, 12 PAGES", "PLAYS THE SOUND, THE", "BALANCE AND THE DROPS"))
        # ONE <-> Live
        gx0, gx1 = xo + bw, xl
        arrow(c, gx0, yb + 44, gx1, yb + 44); label(c, (gx0 + gx1) / 2, yb + 49, "NOTES, PC")
        arrow(c, gx1, yb + 18, gx0, yb + 18, dashed=True); label(c, (gx0 + gx1) / 2, yb + 8, "CLOCK")
        # E16 <-> Live
        gx2, gx3 = xl + lw, xe
        arrow(c, gx3, yb + 44, gx2, yb + 44); label(c, (gx2 + gx3) / 2, yb + 49, "KNOBS, PRESSES")
        arrow(c, gx2, yb + 18, gx3, yb + 18, dashed=True); label(c, (gx2 + gx3) / 2, yb + 8, "VALUES: RINGS")
        # Port notes under the side boxes
        for i, t in enumerate(("USB PORT A: DRUMS, CH 1-4", "USB PORT B: PITCHED, CH 1-7", "AND SF CONTROL, CH 16")):
            label(c, xo + bw / 2, yb - 12 - 8 * i, t, 5.8)
        for i, t in enumerate(("USB PORT 2: REMOTE,", "INPUT AND OUTPUT", "PORTS 1 AND 3: OFF")):
            label(c, xe + bw / 2, yb - 12 - 8 * i, t, 5.8)
        # Computer keys above Live
        yk = yl + hl + 18
        box(c, xl, yk, lw, 28, "COMPUTER KEYS", ("PANIC  /  92 PA ON-OFF",))
        arrow(c, xl + lw / 2, yk, xl + lw / 2, yl + hl)
        # Interface and outputs below
        yi, hi = 52, 30
        box(c, xl, yi, lw, hi, "AUDIO INTERFACE", ("48 KHZ, BUFFER 256",))
        arrow(c, xl + lw / 2, yl, xl + lw / 2, yi + hi)
        label(c, xl + lw / 2 + 6, yi + hi + 16, "PA 1/2   CUE 3/4   MAIN 5/6", 5.8, align="l")
        ow = 118
        xp, xh = xl + lw / 2 - ow - 22, xl + lw / 2 + 22
        box(c, xp, 0, ow, 28, "PA", ("FROM 92 PA (ROOM EQ)",))
        box(c, xh, 0, ow, 28, "HEADPHONES", ("CUE OUT; 94 ROOM'S SWEEP",))
        arrow(c, xl + lw / 2 - 10, yi, xp + ow / 2, 28)
        arrow(c, xl + lw / 2 + 10, yi, xh + ow / 2, 28)
        # The room mics: 94 ROOM records them with the set; SF CAPTURE takes SF TERROIR's places with them.
        box(c, xo, yi, bw, hi, "ROOM MICS", ("INPUTS 1/2: 94 ROOM",))
        arrow(c, xo + bw, yi + hi / 2, xl, yi + hi / 2)


# ------------------------------------------------------------------ 3: the ONE's sequencers
class OxiMap(Flowable):
    """The ONE's eight sequencers, the output (channel and port, as the ONE names it) and the Live
    track each one plays."""
    RH = 21
    ROWS = [("01 KICK", "SF KICK", "1A"), ("21 CLOSED", "SF PERC", "2A"), ("22 OPEN", "SF PERC", "3A"), ("23 CLAP", "SF PERC", "4A"),
            ("11 BASS", "SF BASS", "1B"), ("31 ACID", "SF LEAD", "2B"), ("32 STAB", "SF LEAD", "3B"), ("33 TEXTURE", "SF PAD", "4B"),
            ("34 VOICE", "SF VOICE", "5B"), ("24 STRIKE", "SF STRIKE", "6B"), ("00 CONTROL", "SF CONTROL", "16B")]
    SEQ = [("2  BASS", "MONO"), ("3  ACID", "MONO"), ("4  STAB", "CHORD OR POLY"), ("5  TEXTURE", "POLY"),
           ("6  VOICE", "MONO"), ("7  STRIKE", "MONO TO START"), ("8  CONTROL", "PROGRAM CHANGE ONLY")]
    def wrap(self, aw, ah): self.aw = aw; self.H = len(self.ROWS) * self.RH + 22; return aw, self.H
    def draw(self):
        c, aw, RH = self.canv, self.aw, self.RH
        top = self.H - 16
        sw, tw = 140, 150
        xs, xt = 0, 236
        label(c, xs, top + 6, "SEQUENCER", 6.2, align="l"); label(c, xt, top + 6, "LIVE TRACK", 6.2, align="l")
        label(c, (xs + sw + xt) / 2, top + 6, "OUTPUT", 6.2)
        yrow = lambda r: top - (r + 1) * RH + 2                  # bottom of row r's box
        bh = RH - 4
        box(c, xs, yrow(3), sw, 4 * RH - 4, "1  DRUMS", ("MULTITRACK", "LANES 1-4 PLAY THE KIT", "LANES 5-8 SPARE"))
        for i, (name, mode) in enumerate(self.SEQ):
            box(c, xs, yrow(4 + i), sw, bh, name, (mode,), title_size=7.0, sub_size=5.6)
        for r, (track, plugin, out) in enumerate(self.ROWS):
            y = yrow(r)
            c.setLineWidth(0.8); c.setStrokeColor(INK); c.setFillColor(colors.white); c.rect(xt, y, tw, bh, stroke=1, fill=1)
            label(c, xt + 7, y + bh / 2 - 2.6, track, 7.2, INK, "MonoB", "l")
            label(c, xt + tw - 7, y + bh / 2 - 2.4, plugin, 6.0, GREY, "Mono", "r")
            arrow(c, xs + sw, y + bh / 2, xt, y + bh / 2)
            label(c, (xs + sw + xt) / 2, y + bh / 2 + 3, out, 6.6, INK, "MonoB")
        # Port brackets
        xb = xt + tw + 8
        for (r0, r1, name, sub) in ((0, 3, "USB PORT A", "THE DRUMS"), (4, 10, "USB PORT B", "PITCHED + CONTROL")):
            y1, y0 = yrow(r0) + bh, yrow(r1)
            c.setStrokeColor(INK); c.setLineWidth(0.8)
            c.line(xb, y1, xb + 4, y1); c.line(xb + 4, y1, xb + 4, y0); c.line(xb, y0, xb + 4, y0)
            label(c, xb + 10, (y0 + y1) / 2 + 1.5, name, 7.0, INK, "MonoB", "l")
            label(c, xb + 10, (y0 + y1) / 2 - 7, sub, 5.8, GREY, "Mono", "l")


# ------------------------------------------------------------------ 5: one E16 page
def short_target(ctl):
    """'10 LOW / SF ENERGY' or '32 STAB / SEND A': where the knob lands, short enough for its cell."""
    parts = ctl.target.split(" > ")
    if parts[1] == "mixer": return f"{parts[0]} / {parts[2].upper()}"
    return f"{parts[0]} / {parts[1]}"


class E16Page(Flowable):
    """One page of the scene SF SHOW, drawn from layout.py: the 4 x 4 grid, numbered row by row from
    the top left. Rows 1-2 (grey band) are the same on every page; rows 3-4 are this page. Each knob:
    its ring (black: a parameter, light blue: a volume), its number, the name on the E16's screen, the
    parameter, where it lands in Live, its address, and its press if it has one."""
    RH, PAD = 52, 6
    def __init__(self, title, bottom):
        super().__init__(); self.title, self.bottom = title, bottom
    def wrap(self, aw, ah):
        self.aw = aw; self.H = 4 * self.RH + 2 * self.PAD + 14; return aw, self.H
    def press_for(self, i):
        if i in E16.PRESSES_ALL: return E16.PRESSES_ALL[i]
        return E16.PRESSES_PAGE.get((self.title, i))
    def draw(self):
        c, aw, RH, PAD = self.canv, self.aw, self.RH, self.PAD
        cw = (aw - 2 * PAD) / 4
        gy0 = 14                                  # grid bottom (legend below)
        gtop = gy0 + 4 * RH + 2 * PAD
        # The controller: outline, the fixed half shaded, the line between the halves.
        c.setFillColor(FAINT); c.setStrokeColor(FAINT)
        c.rect(1, gtop - PAD - 2 * RH, aw - 2, 2 * RH + PAD - 1, stroke=0, fill=1)
        c.setStrokeColor(INK); c.setLineWidth(1.0); c.roundRect(0.5, gy0, aw - 1, gtop - gy0, 5, stroke=1, fill=0)
        yd = gtop - PAD - 2 * RH
        c.setStrokeColor(GREY); c.setLineWidth(0.6); c.setDash(2.2, 2.2); c.line(PAD, yd, aw - PAD, yd); c.setDash()
        for r in range(4):
            for col in range(4):
                i = r * 4 + col + 1
                ctl = E16.TOP[i - 1] if i <= 8 else self.bottom[i - 9]
                fixed = i <= 8
                x = PAD + col * cw
                y1 = gtop - PAD - r * RH                       # top of the cell
                cx, cy = x + 15, y1 - 19
                vol = ctl.colour == E16.LIGHT_BLUE
                ring = CYAN if vol else (GREY if fixed else INK)
                c.setLineWidth(2.4); c.setStrokeColor(ring); c.setFillColor(colors.white); c.circle(cx, cy, 10.5, stroke=1, fill=1)
                label(c, cx, cy - 2.5, str(i), 7.0, GREY if fixed else INK, "MonoB")
                tx = x + 31
                ink = GREY if fixed else INK
                label(c, tx, y1 - 13, ctl.name, 7.6, ink, "MonoB", "l")
                yl = y1 - 21.5
                if ctl.label.upper() != ctl.name.upper():
                    label(c, tx, yl, ctl.label.upper(), 6.0, ink, "Mono", "l"); yl -= 8
                label(c, tx, yl, short_target(ctl), 5.7, GREY, "Mono", "l"); yl -= 7.6
                addr = f"CH {ctl.channel}  CC {ctl.number}"
                rng = ctl.live_range.replace("Max 0 dB", "MAX 0 dB").replace(" at first", "")
                label(c, tx, yl, addr + (f"  {rng}" if rng else ""), 5.7, GREY, "Mono", "l")
                p = self.press_for(i)
                if p:
                    t = f"PRESS: SWAP TO {p.name}" if p.mode == "SWAP" else f"PRESS {p.name} / {p.mode}"
                    w = stringWidth(t, "MonoB", 5.6) + 6
                    by = y1 - RH + 4
                    c.setFillColor(GREY if fixed else INK); c.rect(tx, by, w, 8, stroke=0, fill=1)
                    label(c, tx + 3, by + 2.2, t, 5.6, colors.white, "MonoB", "l")
        label(c, 0, 3, "GREY BAND: ROWS 1-2, THE SAME ON EVERY PAGE.   RING: BLACK = A PARAMETER, LIGHT BLUE = A VOLUME.   CH / CC: THE ADDRESS.", 5.8, GREY, "Mono", "l")


# ------------------------------------------------------------------ 6: a song change
class SongFlow(Flowable):
    H = 52
    STEPS = [("OXI ONE", ("A PATTERN LOADS", "ON SEQUENCER 8")), ("PROGRAM CHANGE", ("PORT B, CHANNEL 16", "PROGRAM 1 = SONG 01")),
             ("SF CONTROL", ("SETS THE BAR LINE", "AND THE SONG'S KEY")), ("EVERY SF PLUGIN", ("RECALLS ON THAT BAR", "HEADERS: WAIT BAR")),
             ("E16 RINGS", ("FOLLOW THE RECALL", "TURN ONE: IT'S YOURS"))]
    def wrap(self, aw, ah): self.aw = aw; return aw, self.H
    def draw(self):
        c, aw = self.canv, self.aw
        n, gap = len(self.STEPS), 16
        w = (aw - gap * (n - 1)) / n
        for i, (t, subs) in enumerate(self.STEPS):
            x = i * (w + gap)
            box(c, x, 4, w, 44, t, subs, heavy=(i == 3), title_size=7.0, sub_size=5.8)
            if i: arrow(c, x - gap + 1, 26, x, 26)


# ------------------------------------------------------------------ 7: the drop
class DropTimeline(Flowable):
    """A drop with GAP 1/2, TAIL 1 BAR, DROP DUCK 6 dB, DUCK TIME 1 BAR: two bars either side of the
    drop's bar line. What BUILD, the mix, the tail, the kick and every Pocket part do."""
    LANE, GAPY = 24, 7
    LANES = [("BUILD", "WHAT THE KNOB DOES"), ("DISPLAY", "SF BUILD'S SCREEN"), ("THE MIX", "ALL OF LIVE BUS"),
             ("TAIL", "NOISE AND WASH"), ("KICK", "SF KICK"), ("PARTS", "EVERY POCKET STAGE")]
    def wrap(self, aw, ah):
        self.aw = aw; self.H = len(self.LANES) * (self.LANE + self.GAPY) + 30; return aw, self.H
    def draw(self):
        c, aw, L, G = self.canv, self.aw, self.LANE, self.GAPY
        x0, x1 = 92, aw - 6
        X = lambda b: x0 + (b + 8) / 16 * (x1 - x0)
        top = self.H - 4
        lane_y = lambda k: top - (k + 1) * L - k * G           # lane k bottom
        # Bars and the drop line
        for b in range(-8, 9):
            c.setStrokeColor(LIGHT if b % 4 else GREY); c.setLineWidth(0.4 if b % 4 else 0.6)
            c.line(X(b), lane_y(len(self.LANES) - 1) - 4, X(b), top)
        c.setStrokeColor(INK); c.setLineWidth(1.4); c.line(X(0), lane_y(len(self.LANES) - 1) - 4, X(0), top + 2)
        for b, t in ((-8, "-2 BARS"), (-4, "-1 BAR"), (0, "THE DROP"), (4, "+1 BAR"), (8, "+2 BARS")):
            label(c, X(b), lane_y(len(self.LANES) - 1) - 13, t, 6.0, INK if b == 0 else GREY, "MonoB" if b == 0 else "Mono")
        for k, (name, sub) in enumerate(self.LANES):
            y = lane_y(k)
            label(c, 0, y + L / 2 + 1, name, 7.2, INK, "MonoB", "l")
            label(c, 0, y + L / 2 - 7, sub, 5.6, GREY, "Mono", "l")
        # BUILD: the knob up over the build; the effect stops where the gap starts.
        y = lane_y(0)
        c.setStrokeColor(INK); c.setLineWidth(1.2)
        c.line(X(-8), y + 3, X(-2), y + L - 2); c.line(X(-2), y + L - 2, X(-2), y + 1); c.line(X(-2), y + 1, X(8), y + 1)
        knock(c, X(-2) + 4, y + L - 8, "STOPS WHERE THE GAP STARTS")
        knock(c, X(0) + 6, y + 6, "OFF UNTIL KNOB 4 IS BACK BELOW 5 %")
        # DISPLAY: DROP pressed, DROP ON BAR, GAP, RETURN KNOB.
        y = lane_y(1)
        xp = X(-5.5)
        c.setFillColor(INK); p = c.beginPath(); p.moveTo(xp, y + L - 2); p.lineTo(xp - 4, y + L + 4); p.lineTo(xp + 4, y + L + 4); p.close(); c.drawPath(p, stroke=0, fill=1)
        label(c, xp, y + L + 6, "DROP PRESSED", 5.6, INK, "MonoB")
        for (b0, b1, t, fill) in ((-5.5, -2, "DROP ON BAR", GREEN), (-2, 0, "GAP", GREEN), (0, 8, "RETURN KNOB", AMBER)):
            c.setFillColor(fill); c.setStrokeColor(INK); c.setLineWidth(0.5); c.rect(X(b0), y + 4, X(b1) - X(b0), L - 8, stroke=1, fill=1)
            label(c, (X(b0) + X(b1)) / 2, y + L / 2 - 2, t, 6.0, INK, "MonoB")
        # THE MIX: silent in the gap, back on the bar line.
        y = lane_y(2)
        c.setFillColor(INK); c.rect(X(-8), y + 5, X(-2) - X(-8), L - 10, stroke=0, fill=1); c.rect(X(0), y + 5, X(8) - X(0), L - 10, stroke=0, fill=1)
        c.setStrokeColor(GREY); c.setDash(2, 2); c.setLineWidth(0.6); c.rect(X(-2), y + 5, X(0) - X(-2), L - 10, stroke=1, fill=0); c.setDash()
        label(c, (X(-2) + X(0)) / 2, y + L / 2 - 2, "SILENT", 6.0, GREY, "MonoB")
        # TAIL: from the gap, fading over one bar, DROP DUCK lower past the bar line.
        y = lane_y(3)
        pts = []
        for k in range(0, 81):
            b = -2 + 4 * k / 80
            g = 0.5 + 0.5 * math.cos(math.pi * (b + 2) / 4)
            if b >= 0: g *= 0.5
            pts.append((X(b), y + 2 + g * (L - 4)))
        p = c.beginPath(); p.moveTo(X(-2), y + 2)
        for (px, py) in pts: p.lineTo(px, py)
        p.lineTo(X(2), y + 2); p.close()
        c.setFillColor(LIGHT); c.setStrokeColor(INK); c.setLineWidth(0.8); c.drawPath(p, stroke=1, fill=1)
        knock(c, X(0) + 5, y + L - 6, "DROP DUCK: 6 dB LOWER")
        knock(c, X(-8) + 3, y + L / 2 - 2, "UNTIL THE GAP: PART OF THE MIX")
        # KICK: every beat, none in the gap.
        y = lane_y(4)
        c.setStrokeColor(INK); c.setLineWidth(1.6)
        for b in list(range(-8, -2)) + list(range(0, 8)):
            c.line(X(b), y + 3, X(b), y + L - 3)
        # PARTS: dip under each kick for one bar after the drop: full for half of it, easing out.
        y = lane_y(5)
        base, full = y + L - 3, (L - 6) * 0.5
        c.setStrokeColor(INK); c.setLineWidth(1.0)
        p = c.beginPath(); p.moveTo(X(-8), base); p.lineTo(X(0), base)
        for b in range(0, 8):
            xstep = (X(b + 1) - X(b))
            w = 1.0 if b < 2 else (0.5 + 0.5 * math.cos(2 * math.pi * (b / 4 - 0.5)) if b < 4 else 0.0)
            depth = full * (1 - 10 ** (-6 * w / 20)) / 0.5 if w > 0 else 0
            p.lineTo(X(b), base - depth)
            for k in range(1, 21):
                t = k / 20
                p.lineTo(X(b) + t * xstep, base - depth * math.exp(-t * 6))
        p.lineTo(X(8), base)
        c.drawPath(p, stroke=1, fill=0)
        c.setStrokeColor(GREY); c.setLineWidth(0.5); c.setDash(1.5, 1.5)
        c.line(X(0), y + 1, X(4), y + 1); c.setDash()
        knock(c, X(4) + 5, y + 1, "DUCK TIME: 1 BAR")


# ------------------------------------------------------------------ the manual
PAGE_NOTES = {
    "PERFORM": ("The home page: the character of the four music parts, both returns, and the two big faders.",
                ["15 LIVE BUS and 16 BRIDGE are the escape hatch: bring BRIDGE up, pull LIVE BUS down, and the live set is gone while a BRIDGE clip plays. Regroup, then cross back.",
                 "10 sends STAB into DUB: the classic dub stab. Ride it with 13 DUB RETURN under the other hand."]),
    "KICK": ("SF KICK on 01 KICK. TUNE and DECAY are the body, PUNCH and CLICK the attack.",
             ["Set TUNE and DECAY per song and store them: KICK and RUMBLE tune themselves to the song's KEY when KEY is on.",
              "FILTER right thins the kick, and Pocket closes the low pockets as it thins: the bass and rumble take the sub. Bring the kick's low end back on the drop."]),
    "BASS": ("SF BASS on 11 BASS. CUTOFF, RES and ENV AMT move the line; GATE shortens it.",
             ["GATE down with DENSITY up is a tight, nervous build; GATE open with DENSITY down is spacious dub.",
              "DRIVE adds weight at low settings and grit at high ones."]),
    "RUMBLE": ("SF RUMBLE on 12 RUMBLE. It plays no notes: it follows SF KICK.",
               ["SIZE is the bed's reverb, DARK its low-pass (600 Hz down to 70 Hz), GRIT its distortion; PUMP is how far it ducks under each kick.",
                "In each section decide whether BASS or RUMBLE carries the sustained low end, then steer that one."]),
    "HATS": ("Three SF PERCs on one page: CLOSED (9-11), OPEN (12-14), CLAP (15-16). The fill page.",
             ["CLOSED DENSITY with ECHO SPACE at 1/16 turns 1/8 hats into 1/16 hats: the send-delay trick, built in.",
              "OPEN DECAY from closed to open across a build; CLAP DENSITY for rolls into the drop."]),
    "STRIKE": ("SF STRIKE on 24 STRIKE: blast-excited bells, plates, bars, tubes and gongs.",
               ["SIZE sets the pitch; MATERIAL goes from wood to glass; DISTANCE darkens and lengthens the hit.",
                "POWER above about 70 % fractures the object: save it for the peak."]),
    "ACID": ("SF LEAD (ACID 303-TYPE) on 31 ACID. CUTOFF, RES and ENV AMT are the squelch.",
             ["Overlapping notes slide like a 303 (always 60 ms): write legato notes on the ONE for slides.",
              "ACID CUTOFF also sits on PERFORM (9), so the acid line is never more than one hand away."]),
    "STAB": ("SF LEAD (STAB DUB) on 32 STAB. 15 SPACE is the stab's ensemble.",
             ["If the ONE sends whole chords, set CHORD to NONE on the plugin; if it sends single notes, CHORD voices them. Not both.",
              "Short GATE plus DUB send (PERFORM 10) is the dub-chord move."]),
    "TEXTURE": ("SF PAD (GRANULAR) on 33 TEXTURE. On GRANULAR, COLOR is grain size and DETUNE is scatter.",
                ["ATTACK up for swells; TEXTURE COLOR also sits on PERFORM (11).",
                 "Field recordings work well in GRANULAR: drop one on the plugin before the show."]),
    "VOICE": ("SF VOICE on 34 VOICE: phrases, radio fragments, choir.",
              ["SIGNAL goes from lost to clear (RADIO model); ECHO feeds the voice's own dub echo.",
               "C3 plays slice 1, C#3 slice 2 and so on; below C3 the whole phrase. Sequence it as choreography."]),
    "SPACE": ("The returns. Row 3 is DUB (SF DELAY on A DUB), row 4 is SPACE (SF REVERB on B SPACE). The presses sit under their knobs.",
              ["THROW (hold) sends into the effect for as long as you hold it. FREEZE (toggle) loops what is in it until pressed again.",
               "Turn DUB TIME while frozen: the loop bends like tape speed and locks again when you let go.",
               "DUB FEEDBACK is mapped 0-70 % at first; widen the mapping when the music asks.",
               "SF TERROIR (your captured places) can replace SF REVERB on B SPACE: the same four knobs and two presses, mapped once (Cmd-M)."]),
    "MACROS": ("SF CONTROL's six macros, and SF REPEAT on 20 DRUMS.",
               ["Hold ROLL on 15 and turn LENGTH under the same finger: the drums roll at the length you dial.",
                "16 PITCH makes the roll fall as it repeats. The macro plan is in chapter 7."]),
}

SHORT = {
    "density": "Ghost notes: an echo of every note one ECHO SPACE later, up to 50 %; above it, two more at half that spacing (1/32 rolls). Each ghost quieter, darker, shorter.",
    "filter": "Bipolar DJ filter on the output: centre open, left a low-pass down to 60 Hz, right a high-pass up to 12 kHz. A full sweep never jumps in volume.",
    "level": "The plugin's output level, stored per song. Set at soundcheck so the volumes at 0 dB give the intended mix.",
}
MACRO_PLAN = {"macro1": "ENERGY in the starting plan: the three ENERGY plugins and BUILD together.",
              "macro2": "MOOD: TEXTURE, STAB and VOICE (cutoff, envelope, drive; the voice's SIGNAL and ECHO).",
              "macro3": "DENSITY: CLOSED, OPEN, CLAP and STRIKE (DECAY and DENSITY: the fills).",
              "macro4": "PULSE: KICK and RUMBLE (kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY).",
              "macro5": "SPACE: the DUB and SPACE returns (delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX).",
              "macro6": "FRACTURE: ACID and BASS (cutoff, envelope, density and drive: smooth to heavy)."}
MIXER_DESC = {"A DUB": "The dub return's volume (SF DELAY on A DUB), up to 0 dB.", "B SPACE": "The reverb return's volume (SF REVERB on B SPACE), up to 0 dB.",
              "90 LIVE BUS": "The whole live set: every instrument and both returns. Pull it down to hand the room to BRIDGE.",
              "40 BRIDGE": "BRIDGE: your Live audio clips, for transitions and regrouping."}

def knob_desc(ctl, many):
    """What a knob does, from the plugin's own description (shortened where every page repeats it)."""
    if ctl.mixer == "volume": return MIXER_DESC.get(ctl.track, f"Volume of {ctl.track}, up to 0 dB.")
    if ctl.mixer == "send A": return f"Send from {ctl.track} into A DUB, the dub delay. Sends are post-fader."
    if ctl.module == "CONTROL" and ctl.pid in MACRO_PLAN:
        return f"Macro {ctl.pid[-1]}: every SF plugin with MACRO SRC = MACRO {ctl.pid[-1]} follows it. " + MACRO_PLAN[ctl.pid]
    if ctl.module == "PAD" and ctl.pid == "color":
        return (f"{ctl.track}: " if many else "") + "Model-specific tone; on GRANULAR, the template's TEXTURE model, it is the grain size."
    d = dict(C.COMMON); d.update(C.MODULES[ctl.module]["desc"])
    text = d.get(ctl.pid, "")
    if ctl.pid in SHORT and text == C.COMMON.get(ctl.pid): text = SHORT[ctl.pid]
    return (f"{ctl.track}: " if many else "") + text


PAGE_SUB = {"PERFORM": "ROWS 3-4: THE MUSIC PARTS, THE RETURNS, LIVE BUS AND BRIDGE", "KICK": "ROWS 3-4: SF KICK ON 01 KICK, CHANNEL 1",
            "BASS": "ROWS 3-4: SF BASS ON 11 BASS, CHANNEL 2", "RUMBLE": "ROWS 3-4: SF RUMBLE ON 12 RUMBLE, CHANNEL 3",
            "HATS": "ROWS 3-4: SF PERC ON 21 CLOSED, 22 OPEN, 23 CLAP", "STRIKE": "ROWS 3-4: SF STRIKE ON 24 STRIKE, CHANNEL 7",
            "ACID": "ROWS 3-4: SF LEAD ON 31 ACID, CHANNEL 8", "STAB": "ROWS 3-4: SF LEAD ON 32 STAB, CHANNEL 9",
            "TEXTURE": "ROWS 3-4: SF PAD ON 33 TEXTURE, CHANNEL 10", "VOICE": "ROWS 3-4: SF VOICE ON 34 VOICE, CHANNEL 11",
            "SPACE": "ROWS 3-4: SF DELAY ON A DUB, SF REVERB ON B SPACE", "MACROS": "ROWS 3-4: SF CONTROL MACROS, SF REPEAT ON 20 DRUMS"}

story = []
story += [NextPageTemplate("body"), PageBreak()]
story.append(Paragraph("CONTENTS", S["h1"]))
story.append(Rule())
toc = TableOfContents()
toc.levelStyles = [ParagraphStyle("t0", fontName="MonoB", fontSize=9.5, leading=15, leftIndent=0, spaceBefore=6),
                   ParagraphStyle("t1", fontName="Mono", fontSize=8.5, leading=12.5, leftIndent=16, textColor=INK)]
story += [toc, PageBreak()]

def chapter(title, summary):
    story.append(H1(title)); story.append(Rule()); story.append(P(summary, "summary"))

def H2T(text):
    p = H2(text); p.toc_level = 1; return p

# ---------------- 1
chapter("1  THE RIG", "Three devices, three jobs. The OXI One MkII plays the notes and the patterns. Live, running SF SHOW.als, makes the sound. The E16 plays the sound, the balance and the drops. Nothing on stage is loaded, routed or mapped: everything below is set once.")
story.append(RigDiagram())
story.append(Spacer(1, 8))
story.append(table([["DEVICE", "ITS JOB"],
    ["OXI ONE MKII", "Every note, rhythm and pattern change, and each song's Program Change."],
    ["LIVE: SF SHOW.ALS", "Every sound (one SF plugin per role), the songs, the master chain, the recording, the clock."],
    ["OXI E16", "Energy, build and drop, the volumes; then one instrument, the returns or the macros per page."],
    ["COMPUTER KEYS", "PANIC and 92 PA on/off: the two things that may never happen by accident."]],
    [1.35 * inch, W - 1.35 * inch]))
story.append(H2T("THE TWO HANDS"))
story += bullets(["<b>Left hand: rows 1 and 2.</b> The three group ENERGY knobs, BUILD (press it: DROP) and the four main volumes. They are the same on every page, so the arrangement is always under one hand.",
                  "<b>Right hand: rows 3 and 4.</b> The page: one instrument, the returns, or the macros. Change pages to go deep into one sound while the left hand keeps the set moving."])
story.append(H2T("WHAT NEVER CHANGES IN A SHOW"))
story += bullets(["The tracks, the routing, the E16 scene and the mappings. A song is a stored set of values in every SF plugin, recalled on the bar: nothing is loaded between tracks."])
story.append(PageBreak())

# ---------------- 2
chapter("2  THE SHOW TEMPLATE", "SF SHOW.als is built: every track, group and return, every SF plugin with its starting settings, the routing, and every E16 address mapped. Open it, do the five steps below, and keep it as the template for every show.")
story.append(H2T("TRACKS"))
story.append(table([["TRACK", "DEVICES", "MIDI FROM", "OUT"],
    ["00 CONTROL", "SF CONTROL", "ONE port B, ch 16", "Main"],
    ["01 KICK", "SF KICK", "ONE port A, ch 1", "90 LIVE BUS"],
    ["10 LOW (group)", "SF ENERGY, PROFILE BASS", "", "90 LIVE BUS"],
    ["11 BASS", "SF BASS", "port B, ch 1", "LOW"],
    ["12 RUMBLE", "SF RUMBLE, SOURCE KICK BUS", "none: it follows the kick", "LOW"],
    ["20 DRUMS (group)", "SF ENERGY, PROFILE DRUMS; SF REPEAT", "", "90 LIVE BUS"],
    ["21 CLOSED", "SF PERC: closed hat", "port A, ch 2", "DRUMS"],
    ["22 OPEN", "SF PERC: open hat or ride", "port A, ch 3", "DRUMS"],
    ["23 CLAP", "SF PERC: clap or rim", "port A, ch 4", "DRUMS"],
    ["24 STRIKE", "SF STRIKE", "port B, ch 6", "DRUMS"],
    ["30 MUSIC (group)", "SF ENERGY, PROFILE MUSIC", "", "90 LIVE BUS"],
    ["31 ACID", "SF LEAD: ACID 303-TYPE", "port B, ch 2", "MUSIC"],
    ["32 STAB", "SF LEAD: STAB DUB", "port B, ch 3", "MUSIC"],
    ["33 TEXTURE", "SF PAD: GRANULAR", "port B, ch 4", "MUSIC"],
    ["34 VOICE", "SF VOICE", "port B, ch 5", "MUSIC"],
    ["40 BRIDGE", "Live audio clips", "", "Main"],
    ["41 DECK", "SF DECK", "port B, ch 7 (hot cues; optional)", "Main"],
    ["A DUB (return)", "SF DELAY, DRY off", "", "90 LIVE BUS"],
    ["B SPACE (return)", "SF REVERB, DRY off", "", "90 LIVE BUS"],
    ["90 LIVE BUS", "SF ISOLATOR &gt; BUILD &gt; GLUE &gt; TONE &gt; LOWEND; SF ALLOC (off at first)", "sends disabled", "Main"],
    ["Main", "SF LIMIT", "", "5/6: a pair that goes nowhere"],
    ["92 PA", "SF ROOM EQ", "audio: Resampling", "the PA outputs (1/2)"],
    ["93 REC", "nothing", "audio: Resampling, Monitor Off", "Sends Only"],
    ["94 ROOM", "SF CAPTURE", "audio: Ext. In 1/2, the room mics; Monitor In", "the headphones (3/4)"]],
    [1.25 * inch, 2.35 * inch, 1.75 * inch, W - 5.35 * inch], pad=2.6))
story.append(Spacer(1, 4))
story.append(P("Every instrument track is on Monitor In: it plays from the ONE whether or not it is armed. The kick stays outside LOW, so the bass and the rumble fade together while the kick holds the floor.", "smallgrey"))
story.append(H2T("ROUTING"))
story.append(RoutingDiagram())
story.append(Spacer(1, 6))
story += bullets(["Main is the set: exporting or resampling it gives the release mix. The venue's correction (ROOM EQ) exists only on 92 PA, so the recording never hears it.",
                  "<b>Every send on 90 LIVE BUS is disabled.</b> The returns feed LIVE BUS, and Live can't compensate a return's delay on a track whose matching send is active, even at zero.",
                  "Pull 90 LIVE BUS down and the live instruments and their effects go while BRIDGE and DECK play on. Mute 92 PA and the speakers go silent while REC keeps recording.",
                  "94 ROOM is the room mics, a stereo pair. Armed, it records them with the set. Its SF CAPTURE never passes them (the set never hears the mics): it only plays SF TERROIR's sweep, to a speaker on the headphone out, when you capture the room (chapter 8)."])
story.append(H2T("FINISH THE SET: FIVE STEPS"))
story += numbered(["<b>MIDI From</b> on the twelve MIDI tracks, as in the table above (chapter 3 shows the ONE's side). Every track opens on No Input: an instrument plays nothing until it is set. Never All Ins: every instrument would play every part.",
                   "<b>Inputs and outputs.</b> 92 PA to the pair that feeds the PA (1/2 in the set). Cue Out, on the Main track, to the headphones (3/4), and 94 ROOM there too. Main to a pair that goes nowhere (5/6). Check all three before any sound reaches a PA: the PA pair carries 92 PA and nothing else. The room mics on inputs 1 and 2 (1: the mic on your left as you face the speaker), and in Live's Settings &gt; Audio &gt; Input Config, 1/2 as a stereo pair: 94 ROOM's input.",
                   "<b>Two computer keys</b> (Cmd-K), never E16 presses: PANIC on SF CONTROL, and 92 PA's activator. Pick keys you can find in the dark.",
                   "<b>Two options:</b> Delay Compensation on; Reduced Latency When Monitoring off.",
                   "<b>Save</b> as a template (File &gt; Save Live Set As Template) or into your show folder. MIDI and key mappings belong to the set, so every set made from it starts mapped."])
story.append(H2T("THE MASTER CHAIN"))
story += bullets(["Start GLUE, TONE and LOWEND gently: the set doesn't need every processor working all the time.",
                  "Soundcheck the busiest passage of the night and set SF LIMIT's INPUT for 1-3 dB of limiting there. LIMIT AUTO on, AUTO MODE DOWN ONLY, for shows: it holds the set at TARGET and holds through breakdowns.",
                  "SF ALLOC starts off. Turn it on band by band once the arrangement is known, and judge it by ear."])
story.append(PageBreak())

# ---------------- 3
chapter("3  THE OXI ONE MKII", "The ONE plays every part. Eight sequencers, one per role; port A carries the drums, port B everything pitched and SF CONTROL. Live is the clock: the ONE follows it.")
story.append(H2T("WHAT THE ONE BRINGS"))
story.append(table([["", "OXI ONE MKII"],
    ["SEQUENCERS", "8, each in one of six modes: Mono, Poly (up to 7 voices), Chord (up to 8 voices), Multitrack (8 tracks), Stochastic, Matriceal."],
    ["PATTERNS", "Up to 128 steps; 128 patterns per project."],
    ["USB", "USB-C for MIDI and power: 32 MIDI channels, as two ports in Live (banks A and B, 16 channels each)."],
    ["UNUSED HERE", "TRS MIDI in and out, Bluetooth MIDI, 8 CV and 8 gate outputs, clock in and out. The rig runs on USB alone."]],
    [1.25 * inch, W - 1.25 * inch]))
story.append(Spacer(1, 3))
story.append(P("Sources: <link href='https://www.thomann.co.uk/oxi_instruments_one_mkii_black_edition.htm'>Thomann, OXI One MkII</link>; <link href='https://www.milkaudiostore.com/fi/shop/oxi-instruments-oxi-one-black-edition-mk2'>Milk Audio, OXI ONE MkII specifications</link>; <link href='https://www.elektronauts.com/t/oxi-one-hardware-sequencer/141839?page=48'>Elektronauts: banks A and B appear as two USB MIDI devices</link>. Check menus and limits against your firmware's manual.", "smallgrey"))
story.append(H2T("SEQUENCERS, OUTPUTS, TRACKS"))
story.append(P("On the ONE an output is a channel and a port: 1A is channel 1 on port A, 16B channel 16 on port B. Set each sequencer's output as below, and each Live track's MIDI From to the same port and channel."))
story.append(OxiMap())
story.append(Spacer(1, 6))
story += bullets(["<b>KICK lane on C1.</b> With KEY TRACK on, any other note transposes the kick (C1 = TUNE), so C1 is right either way. PERC plays any note.",
                  "<b>RUMBLE needs no lane:</b> SOURCE KICK BUS follows SF KICK.",
                  "<b>STAB:</b> if the ONE sends whole chords (Chord or Poly mode), set CHORD on the plugin to NONE; if it sends single notes, CHORD voices them. Not both.",
                  "<b>ACID</b> slides when notes overlap, over 60 ms like a 303: write legato notes for slides.",
                  "<b>VOICE:</b> C3 plays slice 1 of the loaded phrase, C#3 slice 2, and so on; below C3 the whole phrase.",
                  "<b>DECK</b> takes hot cues on port B, channel 7 (C1-D#1 deck A, C2-D#2 deck B): any spare lane set to 7B can cue it."])
story.append(CondPageBreak(2.6 * inch))
story.append(H2T("CLOCK AND PORTS"))
story += bullets(["<b>Live is the clock master.</b> On the ONE: USB device mode, sync from USB, Auto Sync on (it starts with Live), Start and Stop accepted, MIDI Thru off. Never send clock both ways.",
                  "<b>Audio:</b> 48 kHz, buffer 256 (Pocket needs 512 or less), launch quantization 1 bar, record quantization off.",
                  "<b>Calibrate once:</b> record a plain quarter-note pattern from the ONE, compare it with Live's grid and set the ONE port's MIDI Clock Sync Delay in Live's preferences. Leave POCKET OFFSET on SF CONTROL at 0 unless the pump is still off after that."])
story.append(table([["LIVE MIDI PORT", "TRACK", "SYNC", "REMOTE"],
    ["ONE port A, in", "on", "off", "off"], ["ONE port B, in", "on", "off", "off"],
    ["ONE, out (the port it takes clock from)", "off", "on", "off"],
    ["E16 Port 2, in", "off", "off", "on"], ["E16 Port 2, out", "off", "off", "on"],
    ["E16 Ports 1 and 3, in and out", "off", "off", "off"]],
    [3.0 * inch, 0.9 * inch, 0.9 * inch, W - 4.8 * inch]))
story.append(H2T("PROJECTS AND PATTERNS"))
story += bullets(["<b>One ONE project per track</b> (per piece of music), with a pattern per section: SPARSE, MAIN, VARIATION, EXIT.",
                  "<b>Sequencer 8 carries the song.</b> The ONE sends a Program Change (and bank) per pattern: give sequencer 8's patterns the track's song number, output 16B. Loading the project recalls the matching SF song on the bar (chapter 6). Program Change stays off on the musical sequencers.",
                  "<b>When patterns change is set on the ONE</b> (its launch and arranger settings): Live's launch quantization doesn't reach it.",
                  "<b>KEY on SF CONTROL starts OFF:</b> the ONE's notes set the pitch, and KICK and RUMBLE are tuned per song with TUNE. Turn KEY on for songs that should change key without reprogramming the ONE.",
                  "<b>Taking the kick out:</b> mute its lane on the ONE. Pulling the KICK fader down, or pressing it (MUTE), leaves it playing on the bus: Live mutes after the plugin, so Pocket and RUMBLE keep pumping to a silent kick (which can be the point)."])
story.append(PageBreak())

# ---------------- 4
chapter("4  THE E16 SCENE", "One E16 scene runs the whole show: SF SHOW, twelve pages of sixteen knobs. Rows 1 and 2 are the same on every page; rows 3 and 4 are the page. The set comes with every address mapped.")
story.append(H2T("THE GRID"))
story.append(P("The E16 is a 4 &times; 4 grid of endless encoders, numbered row by row from the top left. Its screen shows each knob's short name (below: LOW ENGY, DRM ENGY ...)."))
cells = {}
for i, ctl in enumerate(E16.TOP, 1):
    pr = E16.PRESSES_ALL.get(i)
    sub = f"PRESS: {pr.short()}" if pr else ctl.label.upper()
    cells[i] = (ctl.name, sub, CYAN if ctl.colour == E16.LIGHT_BLUE else INK, False)
for i, (name, quiet) in zip(range(9, 17), [("SOUND", 1), ("SOUND", 1), ("SOUND", 1), ("GATE", 0), ("DENSITY", 0), ("FILTER", 0), ("SOUND", 1), ("LEVEL", 0)]):
    cells[i] = (name, "THE PLUGIN'S OWN" if quiet else "", CYAN if name == "LEVEL" else INK, bool(quiet))
story.append(E16Grid(cells))
story.append(Spacer(1, 8))
story += bullets(["<b>1-3, the group ENERGY knobs:</b> centre is neutral, left takes energy out, right puts it in (filter, drive, space, echo and chop together).",
                  "<b>Press 1: SLOW ALL.</b> A quick press flips knob 1 to SLOW ALL on SF CONTROL (the ring turns green): turn it to set how long your next moves take, OFF (at once) to 128 BAR. Press again: LOW ENERGY. Knobs already travelling keep going.",
                  "<b>4, BUILD:</b> turn it up over a build. <b>Press it: DROP</b>, on every page. The drop lands on the next bar line (chapter 7).",
                  "<b>5-8, the volumes:</b> KICK, LOW, DRUMS, MUSIC. Each is mapped with a maximum of 0 dB, so a full turn never runs past the soundcheck balance. <b>Press one: MUTE</b>, on every page: its track goes silent (Live's track activator); press again, it is back.",
                  "<b>On an instrument page</b> 13 is DENSITY, 14 FILTER and 16 LEVEL, and 12 is GATE wherever there is one: the hand learns one layout. The rest is the plugin's own performance row."])
story.append(H2T("RINGS, PAGES, PRESSES"))
story += bullets(["<b>Rings:</b> light blue is a volume, white is everything else, green is knob 1 on SLOW ALL or a knob travelling (knob 1 itself never shows a glide: it swaps, so it always shows the value of the side it is on). Live sends every value back, so the rings follow song recalls and mouse moves.",
                  "<b>A recalled knob is yours the moment you turn it.</b> With Takeover Mode Value Scaling, a knob that disagrees with Live (after a song recall) moves the sound smoothly from where it is, never jumps.",
                  "<b>Changing pages sends nothing.</b> A knob on two pages (LOW ENERGY is on all twelve) sends the same address from both: two ways to one mapping.",
                  "<b>A hold press</b> is a CC, 127 on press and 0 on release: the parameter is on for as long as the knob is down (DROP, THROW, ROLL). <b>A toggle press</b> is a note: Live flips the switch on every Note On (FREEZE, MUTE), so the E16 can never think FREEZE is on, or a track muted, while Live has it otherwise.",
                  "Every press is one the set plays through if it happens by accident (a throw, a roll, a freeze, a drop, one track muted). What would stop the set is on a computer key: LIVE BUS has no MUTE.",
                  "<b>On screen, the same grid.</b> Every SF plugin's window starts with its page of this scene: its own encoders lit where they sit on the E16, in these colours, the rest dark, its presses under their knobs. Several windows side by side read like the E16 itself. LEAD, PERC and ENERGY are on several tracks: their windows find their encoders by the track's name (VST3)."])
story.append(CondPageBreak(1.7 * inch))
def press_row(where, i, p):
    kind = {"HOLD": "hold", "TOGGLE": "toggle", "SWAP": "swap"}[p.mode]
    msg = {"HOLD": f"CC {p.number} ch {p.channel}", "SWAP": f"turn 2: CC {p.number} ch {p.channel}"}.get(
        p.mode, f"note {p.number} ({E16.note_name(p.number)}) ch {p.channel}")
    return [where, str(i), f"{p.name}, {kind}", msg, esc(p.target)]
story.append(table([["WHERE", "KNOB", "PRESS", "MESSAGE", "LIVE TARGET"]] +
    [press_row("every page", i, p) for i, p in sorted(E16.PRESSES_ALL.items())] +
    [press_row(t, i, p) for (t, i), p in sorted(E16.PRESSES_PAGE.items(), key=lambda kv: ([pg for pg, _ in E16.PAGES].index(kv[0][0]), kv[0][1]))],
    [0.85 * inch, 0.45 * inch, 1.15 * inch, 1.35 * inch, W - 3.8 * inch], mono_first=False))
story.append(H2T("ADDRESSES"))
story += bullets(["<b>Channel = track:</b> 1 KICK, 2 BASS, 3 RUMBLE, 4 CLOSED, 5 OPEN, 6 CLAP, 7 STRIKE, 8 ACID, 9 STAB, 10 TEXTURE, 11 VOICE, 12 DUB, 13 SPACE, 14 the groups (LOW, DRUMS, MUSIC and DRUMS' REPEAT), 15 LIVE BUS and BRIDGE, 16 CONTROL.",
                  "<b>CC = block base + the parameter's place in its plugin.</b> One plugin on a channel: base 20, so its performance row is CC 20-27. Channel 14: LOW ENERGY 20, DRUMS ENERGY 30, MUSIC ENERGY 40, REPEAT 50. Channel 15: BUILD 20. Channel 16: CONTROL 20.",
                  "<b>Mixer:</b> CC 100 volume, 101 send A, 102 send B. Channel 14: 100-102 the LOW, DRUMS and MUSIC volumes; channel 15: 100 LIVE BUS, 101 BRIDGE. A volume knob's MUTE press is the note with its number: note 100 on channel 1 (KICK), notes 100-102 on channel 14 (LOW, DRUMS, MUSIC).",
                  "The plugins never reorder their parameters, so an address never moves. Every address is in the appendix."])
story.append(H2T("LOAD THE SCENE"))
story += numbered(["OXI App &gt; Scenes. If no computer folder is assigned yet, assign one.",
                   "Copy <b>SF SHOW.oxie16</b> (controllers/e16 in the repository) into the Scenes folder inside it and click Refresh Views.",
                   "Drag SF SHOW from On Computer onto a free slot under On Device. The presses come with it.",
                   "<b>Auto Transmit off</b>, for the scene and in the global settings: sending stored values on a page change would pull Live's faders to whatever the E16 last held. MIDI Thru off.",
                   "In Live: E16 Port 2 Remote on, input and output; Ports 1 and 3 off (Port 1 is the OXI App's; a second Remote port would double every message, and a FREEZE would flip twice). Takeover Mode: Value Scaling."])
story.append(H2T("CHECK IT ONCE"))
story += numbered(["Every knob: turn it, the right parameter moves. Every press: the right switch.",
                   "Hold DROP, a THROW and ROLL for three seconds each: on for the whole hold, off on release. (A long press also opens the E16's own functions; if one interferes, make that press a toggle.)",
                   "Shared knobs: on page 2 turn LOW ENERGY fully left; on page 1 turn it one step right. The sound moves one step, not jumps. If it jumps, the E16 keeps a value per page: controllers/e16 README, check 4, has the relative fallback.",
                   "Change pages, leave SF SHOW and come back: Live's MIDI input indicator stays dark.",
                   "Press knob 1 (green) and turn: SLOW ALL moves, LOW ENERGY doesn't. Press again: LOW ENERGY.",
                   "Switch songs from the ONE: the rings follow, and a recalled knob moves smoothly from the new value."])
story.append(PageBreak())

# ---------------- 5
chapter("5  THE PAGES", "All twelve pages of SF SHOW. Rows 1 and 2 (grey) never change; each page is rows 3 and 4. Under each knob: the name on the E16's screen, the parameter, where it lands in Live, its address.")
rows = [["PAGE", "ROW 3: KNOBS 9-12", "ROW 4: KNOBS 13-16", "PRESSES"]]
for n, (title, bottom) in enumerate(E16.PAGES, 1):
    pr = [f"{i} {E16.PRESSES_PAGE[(title, i)].name}" for i in range(9, 17) if (title, i) in E16.PRESSES_PAGE]
    rows.append([f"{n} {title}", esc(" / ".join(c.name for c in bottom[:4])), esc(" / ".join(c.name for c in bottom[4:])), esc(", ".join(pr))])
story.append(table(rows, [0.95 * inch, 2.25 * inch, 2.25 * inch, W - 5.45 * inch], pad=2.6))
story.append(P("Knobs 1-8 are in chapter 4. On each page below: the drawing, what each of the page's eight knobs does (the plugin's own description), and how to play it.", "smallgrey"))
story.append(PageBreak())
for n, (title, bottom) in enumerate(E16.PAGES, 1):
    lead, tips = PAGE_NOTES[title]
    many = len({ctl.track for ctl in bottom}) > 1
    knobs = [["KNOB", "SCREEN", "WHAT IT DOES"]] + [[str(i), ctl.name, esc(knob_desc(ctl, many))] for i, ctl in enumerate(bottom, 9)]
    story += [H2T(f"PAGE {n}  {title}"), P(PAGE_SUB[title], "label"), E16Page(title, bottom), Spacer(1, 6), P(lead),
              table(knobs, [0.45 * inch, 0.85 * inch, W - 1.3 * inch], pad=2.4)] + [Spacer(1, 4)] + bullets(tips) + [PageBreak()]

# ---------------- 6
chapter("6  SONGS, PATTERNS AND SCENES", "Each system owns one thing. The ONE owns the notes, SF songs own the sound, the E16 owns where your hands are. A song change is one Program Change: every SF plugin lands on the same bar.")
story.append(table([["SYSTEM", "RESPONSIBLE FOR"],
    ["OXI ONE projects and patterns", "Notes, rhythms, gates, accents, the pattern progression, and the Program Change that names the song."],
    ["SF songs (SF CONTROL)", "The sound of every SF plugin and the song's KEY, recalled on the bar. Sixteen songs, saved with the set."],
    ["E16 pages", "Where your hands are. Pages hold no sound: changing pages changes nothing you hear."],
    ["Live Session scenes", "BRIDGE clips, reference material, rehearsal."],
    ["Live Arrangement", "Recording the performance."]], [2.0 * inch, W - 2.0 * inch], mono_first=False))
story.append(H2T("A SONG CHANGE"))
story.append(SongFlow())
story.append(Spacer(1, 6))
story += bullets(["SF CONTROL decides the bar line once and sends it with the song, so every plugin lands on the same bar whatever order Live processes the tracks in.",
                  "A change sent within 30 ms after a bar line (a Program Change on the downbeat) counts for that bar; later in the bar, for the next one. Every plugin's header shows WAIT BAR in green until it lands.",
                  "Turning SONG on SF CONTROL does the same as a Program Change. A plugin with nothing stored in that song stays as it is.",
                  "<b>SLOW ALL</b> on SF CONTROL makes a recall glide: at 8 or 16 BAR every SF knob the song stores travels to it over 8 or 16 bars; at OFF it lands on the bar. Menus and switches (a MODEL, an ECHO SPACE) land on the bar either way (GLIDES, chapter 7, has the list)."])
story.append(H2T("SAVING A SONG"))
story += bullets(["Set every plugin how the song should start, then press <b>STORE ALL</b> on SF CONTROL: every SF plugin stores its values into the song in force (SONG).",
                  "<b>Into another song:</b> choose it in <b>STORE TO</b> first, then STORE ALL. That song isn't recalled, so nothing you hear changes; it becomes the song in force (SONG follows) and STORE TO goes back to ACTIVE SONG. Choosing it on SONG instead would recall it and lose what you set.",
                  "If a song change is under way when you STORE TO (WAIT BAR), the change still lands and stays the song in force.",
                  "STORE in a plugin's header stores just that plugin. Nothing that rewrites a song is on the E16."])
story.append(H2T("A TRACK IN THE SET"))
story += numbered(["Load the track's ONE project: sequencer 8 sends its Program Change, and the song lands on the bar.",
                   "Play its patterns in order (SPARSE, MAIN, VARIATION, EXIT) from the ONE; play the sound from the E16.",
                   "Before the next track: BUILD and a drop, a dub throw or a BRIDGE clip covers the change (chapter 7)."])
story.append(PageBreak())

# ---------------- 7
chapter("7  PERFORMANCE", "The top half is the arrangement, the page is the detail. Very simple note content, all the variation played on knobs: every move below is a knob or a press, on the bar.")
story.append(H2T("THE SET ARC"))
story.append(table([["PHASE", "WHAT CARRIES IT", "WHERE"],
    ["OPENING", "Kick and rumble alone; TEXTURE or VOICE far back. ENERGY knobs left of centre.", "Rows 1-2; RUMBLE, TEXTURE"],
    ["LOCK-IN", "Hats come in by DENSITY, not by level. One part at a time, eight or sixteen bars apart.", "HATS, KICK"],
    ["BUILD", "BUILD up over 8-16 bars; hats and clap DENSITY up; SPACE DUCK down so the tail fills the bar.", "Knob 4, HATS, SPACE"],
    ["PEAK", "The drop; ENERGY knobs right of centre; acid or stab carries the hook; dub throws on the offbeats.", "Press 4, PERFORM, ACID, SPACE"],
    ["BREAKDOWN", "Kick lane muted on the ONE; LOW down; FREEZE a return; VOICE up.", "The ONE, rows 1-2, SPACE, VOICE"],
    ["RETURN", "Kick back on the one with DROP DUCK clearing the way; a song change lands here.", "Press 4, the ONE"],
    ["CLOSE", "ENERGY knobs left; parts out one by one; the last bars on a frozen reverb or BRIDGE.", "Rows 1-2, SPACE, PERFORM 16"]],
    [0.9 * inch, 3.75 * inch, W - 4.65 * inch], mono_first=True))
story.append(P("A starting plan, not a rule: hypnotic sets hold a phase far longer than feels safe.", "smallgrey"))
story.append(H2T("BUILDS AND DROPS"))
story.append(P("BUILD is one knob for a whole-mix build: a rising high-pass, a noise riser, a reverb wash and width. Press DROP (knob 4) during the build: the drop lands on the next bar line. Three settings on SF BUILD shape the landing; the drawing shows GAP 1/2, TAIL 1 BAR, DROP DUCK 6 dB, DUCK TIME 1 BAR."))
story.append(DropTimeline())
story.append(Spacer(1, 6))
story += bullets(["<b>GAP</b> (OFF to 1 BAR): everything through BUILD goes silent for that long before the bar line, the whole LIVE BUS with its returns (BRIDGE and DECK play on). Press DROP later than that and the gap starts on the next 1/16.",
                  "<b>TAIL</b> (CUT to 2 BARS): the riser's noise and the wash ring on after the build stops, fading out. Shorter than the GAP, it dies away in the silence; longer, it carries into the drop, DROP DUCK lower.",
                  "<b>DROP DUCK</b> (0-12 dB) and <b>DUCK TIME</b> (1/4 to 4 BARS): after the drop every SF part with Pocket dips under each kick, whatever its POCKET DEPTH: bass, lead, pad, voice, rumble, both returns. The first kicks land on top of everything that comes back. A roll into the drop doesn't duck; the duck starts with the first kick on the bar.",
                  "After a drop the build stays off until knob 4 is back below 5 % (display: RETURN KNOB). Bring it down in your own time: nothing re-builds."])
story.append(table([["RECIPE", "GAP", "TAIL", "DROP DUCK", "DUCK TIME", "WHAT IT DOES"],
    ["HARD STOP", "1/4", "CUT", "6 dB", "1 BAR", "A beat of true silence, then everything on the one."],
    ["BREATH", "1/2", "1/4", "6 dB", "1 BAR", "The riser rings into the silence and dies before the kick."],
    ["CARRY", "OFF", "1 BAR", "6 dB", "1 BAR", "No silence: the wash runs under the first bar, ducked."],
    ["LONG FALL", "1 BAR", "1/2", "9 dB", "2 BARS", "A full bar of air; the new section breathes for two bars."],
    ["SNAP", "OFF", "CUT", "0 dB", "-", "The build snaps off on the bar line; nothing else changes."]],
    [0.95 * inch, 0.45 * inch, 0.5 * inch, 0.75 * inch, 0.75 * inch, W - 3.4 * inch]))
story.append(P("Defaults: GAP OFF, TAIL 1/8, DROP DUCK 6 dB, DUCK TIME 1 BAR. The drop duck needs KICK REPORTING on SF CONTROL. TAP and STRIP set to ROLE KICK, PERC or DECK don't duck.", "smallgrey"))
story.append(H2T("ENERGY AND MACROS"))
story += bullets(["The three ENERGY knobs are the fastest way to move a whole set: small moves, often. A negative MACRO 1 amount on LOW darkens the bass while the rest lifts.",
                  "Six macros on the MACROS page, each followed by the plugins that subscribe to it (MACRO SRC, MACRO AMT on each plugin). A starting plan:"])
story.append(table([["MACRO", "FOLLOWED BY", "MOVES"],
    ["1 ENERGY", "the three ENERGY plugins, BUILD", "every group's energy at once"],
    ["2 MOOD", "TEXTURE, STAB, VOICE", "cutoff, envelope and drive; the voice's SIGNAL and ECHO"],
    ["3 DENSITY", "CLOSED, OPEN, CLAP, STRIKE", "DECAY and DENSITY: the fills"],
    ["4 PULSE", "KICK, RUMBLE", "kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY"],
    ["5 SPACE", "DUB, SPACE returns", "delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX"],
    ["6 FRACTURE", "ACID, BASS", "cutoff, envelope, density and drive: smooth to heavy"]], [1.0 * inch, 2.1 * inch, W - 3.1 * inch]))
story.append(P("Start the amounts at 30-50 %. One macro per plugin.", "smallgrey"))
story.append(H2T("DUB"))
story += bullets(["<b>Throws:</b> SPACE page, hold 9 (DUB THROW) on an offbeat stab or a voice fragment and let go: the repeats carry on. With INPUT THROW ONLY on SF DELAY, the press is the only way in.",
                  "<b>Tape moves:</b> turn DUB TIME while repeats are running: the head slews and the pitch bends. FREEZE (10) then TIME: the frozen loop bends like tape speed.",
                  "<b>Repeats that grow:</b> DUB FEEDBACK past 100 % saturates instead of exploding; the mapping stops at 70 % until you widen it.",
                  "<b>Space that breathes:</b> SPACE DUCK 30-60 % lets the reverb bloom in the gaps between kicks. FREEZE (14) holds a chord in the room for a breakdown."])
story.append(H2T("FILLS AND TRANSITIONS"))
story += bullets(["<b>Hats:</b> HATS page, CLOSED DENSITY up with ECHO SPACE at 1/16 on SF PERC: 1/8 hats become 1/16s, then 1/32 rolls past 50 %.",
                  "<b>Drum roll:</b> MACROS page, hold ROLL (15) and turn LENGTH under the same finger: 1/4 down to 1/32. PITCH (16) makes it fall.",
                  "<b>Between tracks:</b> load the next ONE project during a build or a breakdown; the song lands on a bar line. With SLOW ALL set, the knobs glide into the new song; a MODEL change in it still lands on the bar, so cover it with the build or the drop.",
                  "<b>BRIDGE:</b> bring 16 BRIDGE up on PERFORM and 15 LIVE BUS down to hand the room to a clip while you regroup. BRIDGE still needs Live: it is not a backup for a computer failure."])
story.append(H2T("GLIDES"))
story += bullets(["<b>A travelling knob's ring turns green</b> and walks with the sound to where you set it, then turns white. Turn it while it walks: the ring shows your hand, then green again. (SF CONTROL's E16 RINGS and the scene's script; nothing else depends on them.)",
                  "<b>SLOW ALL</b> (press knob 1, turn, press again) sets how long your next moves take, in every SF plugin. OFF: they land at once. 8 BAR: a quarter turn of ACID CUTOFF becomes an eight-bar rise that plays itself.",
                  "<b>A move keeps its length.</b> Start a 16-bar rise on the pad, set SLOW ALL to OFF, and make quick moves elsewhere: the pad keeps rising. Turn a travelling knob and it sets off again from where it has got to.",
                  "<b>SLOW ALL changed in the same instant as a move</b> (one audio buffer, a few ms): the move takes the length from before the change, in every plugin alike. Set SLOW ALL, then move.",
                  "<b>Song changes follow it, knob by knob.</b> SLOW ALL at 16 BAR before the Program Change: every SF knob the song stores travels to the new song over 16 bars; OFF: it lands on the bar. What does not glide (the table) changes on the bar. It is a morph of the knobs, not a crossfade between two whole sounds.",
                  "A plugin's own SLOW, set in its window, wins over SLOW ALL; leave it at CONTROL."])
# What SLOW moves, for every knob and press of the scene: drawn from layout.py and the plugins'
# parameters (specs.json), so it can't drift from the set. Every place on the scene lands in one row.
def spec_of(c):
    return next(q for q in E16.SPECS[c.module]["params"] if q["id"] == c.pid)
glides, menus, presses, live = [], [], [], []
for title, _ in E16.PAGES:
    for i, c in enumerate(E16.encoders(title), 1):
        if not c.module:
            live.append(c.label)
        elif spec_of(c)["slow"]:
            glides.append(c.target)
        else:
            assert spec_of(c)["type"] == "menu", f"{c.target}: a knob on the scene that SLOW doesn't move"
            menus.append(c.label)
    for i in range(1, 17):
        p = E16.press_for(title, i)
        if p is not None and p.mode == "SWAP":
            assert spec_of(p.ctl)["type"] == "menu", p.ctl.target
            menus.append(p.ctl.label)
        elif p is not None:
            presses.append(p.name)
names = lambda rows: ", ".join(dict.fromkeys(rows))
detail_names = sorted({f"{c.track.split(' ', 1)[1]} {E16.param_name(c.module, c.pid)}" for t, _ in E16.PAGES for c in E16.encoders(t)
                       if c.module and spec_of(c)["slow"] and not spec_of(c)["primary"]})
story.append(CondPageBreak(2.4 * inch))
story.append(P("<b>WHAT SLOW MOVES</b> (every knob and press of SF SHOW; drawn from the scene and the plugins)", "smallgrey"))
story.append(table([["ON THE E16", "WITH SLOW / SLOW ALL"],
    [f"Every SF knob, {len(set(glides))} of them: each plugin's top row, and the details on the scene too ({esc(', '.join(detail_names))})", "Travels over the bars set"],
    [esc(names(menus)), "Menus: at once, on the next buffer"],
    [esc(names(presses)), "Presses: at once, as long as held or until pressed again"],
    [esc(names(live)), "Live's own faders and send: never glide"],
    ["Not on the E16: MODEL (an instrument's sound)", "At once. What sounds releases on the old model; the keys still down play again on the new one (a held chord goes on)"],
    ["Not on the E16: CHORD, a dropped sample, any other menu or switch", "At once: CHORD from the next note; a sample replaces the old one, notes sounding included"]],
    [3.6 * inch, W - 3.6 * inch], mono_first=False))
story.append(H2T("WHEN SOMETHING GOES WRONG"))
story.append(table([["WHAT HAPPENED", "DO THIS"],
    ["A runaway: feedback, a stuck note, a wrong sound in front of the crowd", "PANIC key: every SF plugin fades out over 5 ms, clears and stays silent. Same key: back, without a click."],
    ["Something non-SF won't stop (a Live clip, a device)", "92 PA key: the speakers go silent, REC keeps recording. Same key: back on."],
    ["A plugin header shows FAULT in red", "It caught an invalid value or a runaway and reset itself; it carries on. Note what was playing."],
    ["The wrong song landed", "Send the right Program Change (or turn SONG): it lands on the next bar."],
    ["A knob seems dead", "Another page is up. Or its range in Live's MIDI Mappings is narrow."],
    ["The ONE drifts or stops", "Live is the clock: stop and start Live, the ONE follows. Check its USB cable before the show, not during it."]],
    [2.3 * inch, W - 2.3 * inch], mono_first=False))
story.append(PageBreak())

# ---------------- 8
chapter("8  BEFORE THE SHOW", "Run through this list at every soundcheck. Each line is a thing that once went wrong somewhere.")
story.append(H2T("CHECKLIST"))
story += numbered(["Every SF plugin from the same build: SF CONTROL shows BUS SHARED and KICK REPORTING, and the pump lands on the kick.",
                   "The ONE's Program Change recalls the song: every track on the same bar, and a recalled knob is yours the moment you turn it.",
                   "PANIC from its key: silence, and every header shows PANIC. Again: back without a click.",
                   "92 PA muted from its key: the speakers silent, REC still recording.",
                   "The PA pair carries 92 PA and nothing else; Cue reaches the headphones and not the PA.",
                   "DROP, a THROW and ROLL each held for three seconds: on for the whole hold, off on release.",
                   "The shared E16 knobs don't jump (chapter 4); changing pages sends nothing.",
                   "SLOW ALL where you want it for the opening (knob 1 pressed; OFF for direct moves), then back to LOW ENERGY.",
                   "A drop rehearsed with the night's GAP, TAIL and DROP DUCK settings.",
                   "A minute recorded on REC with a deep ROOM EQ cut: the cut isn't in the recording.",
                   "Soundcheck the busiest passage: SF LIMIT INPUT for 1-3 dB of limiting there, AUTO on, DOWN ONLY.",
                   "The room mics on 94 ROOM: IN L and IN R move on its SF CAPTURE, the same gain on both, well under the top at the loudest."])
story.append(H2T("CAPTURING THE ROOM"))
story.append(P("SF TERROIR can take the place you play in as a PLACE, without leaving Live: SF CAPTURE on 94 ROOM plays the sweep to a speaker on the headphone out and records the room mics, and TERROIR turns the take into a place. SF TERROIR has to be in the set (on B SPACE in place of SF REVERB: controllers/e16 README, SF TERROIR ON B SPACE). At soundcheck or after the set: the transport stopped."))
story += numbered(["A speaker on the headphone out (a MiniRig on its line in), where the sound should come from, aimed at a wall or into the room. In TotalMix the mics must not reach the headphone out, or the speaker plays them and feeds back.",
                   "The mics where the audience will be, or where the place should be heard from (SF TERROIR's IN THE FIELD, MICS). The speaker as loud as it goes without buzzing; the headphone level sets it, SWEEP LEVEL on SF CAPTURE is only for headroom.",
                   "Stop the transport. In SF TERROIR click <b>CAPTURE</b> (amber), then the slot to fill. 46 s: keep the room quiet while the status line counts.",
                   "TERROIR analyses the take: the slot takes the name ROOM and the date and time; the status line should read SWEEP x3 and 30 dB or more over the noise. Under 30 dB: the speaker louder or the mics closer; THE MICS CLIPPED: the speaker or the gain down. Then again. THE SWEEP DIDN'T COME BACK: nothing reached the mics (TROUBLESHOOTING); the slot stays as it was.",
                   "PLACE is stored per song, so a recall goes back to the slot the song was stored with: STORE the songs that should play the new place. The take stays in Music/SF Live Suite/TERROIR/Captures."])
story.append(P("CAPTURE again (it reads STOP) ends a take early; so do starting the transport, PANIC and Live's audio restarting, even before the sweep has started or while the take is being written. The sweep fades out in 10 ms and nothing is stored. The PA stays silent: 94 ROOM goes to the headphone out only.", "smallgrey"))
story.append(H2T("RECORDING THE SET"))
story += bullets(["Press RESET on SF LIMIT, arm the instrument tracks, 93 REC and 94 ROOM, Automation Arm on, Arrangement record, play. The stereo recording on REC is the set, the one on ROOM the room mics for the mix; the MIDI and automation are for editing and rehearsal.",
                  "Collect All and Save after the show. SF samples are inside the set; DECK tracks are referenced by path, so keep the files where they are."])
story.append(H2T("TROUBLESHOOTING"))
story.append(table([["SYMPTOM", "CHECK"],
    ["An instrument doesn't play", "MIDI From: its ONE port and channel, never All Ins. The ONE's sequencer output (1A, 2B ...) matches."],
    ["Every instrument plays every part", "A track's MIDI From is All Ins or All Channels."],
    ["Nothing ducks", "SF CONTROL: BUS SHARED and KICK REPORTING. The kick must be SF KICK."],
    ["Pump early or late", "Live buffer 512 or less; the ONE's MIDI Clock Sync Delay; then POCKET OFFSET by ear."],
    ["Songs don't change", "PC ENABLE on, PC CHANNEL 16, sequencer 8 on 16B. The VST3 needs Live 12.2 or later; otherwise use the AU."],
    ["E16 rings don't follow", "Remote on for E16 Port 2's output."],
    ["An E16 press does nothing", "An SF SHOW file from before 3 Nov is loaded (it had no presses): load the current one. A toggle that flips back: Remote is on for two E16 ports."],
    ["No green on a travelling knob", "SF CONTROL shows E16 RINGS PORT 3 in green? If not, the E16 isn't found: try PORT 2. Load SF SHOW from the repository again (the script is in it). Use the VST3s."],
    ["Knob 1 swapped (green) changes nothing", "SLOW ALL isn't mapped in this set: Cmd-M, click SLOW ALL on SF CONTROL, press knob 1 and turn it. Cmd-M off, save."],
    ["Knob 1 moves LOW ENERGY and SLOW ALL together", "Its push was changed in the App: load SF SHOW.oxie16 again (knob 1's push must be Swap Destination)."],
    ["Every move glides (or none does)", "SLOW ALL is saved with the set: press knob 1 and set it. A plugin whose own SLOW isn't CONTROL keeps its own."],
    ["An E16 knob jumps when touched", "Auto Transmit is on, or the E16 keeps a value per page: chapter 4, check 3."],
    ["CONTROL shows LOCAL ONLY", "The plugins come from different builds: rebuild and install the whole suite, restart Live."],
    ["A return doubles the dry signal", "DRY off on SF DELAY or SF REVERB on the returns."],
    ["SF TERROIR won't capture", "Its status line says why: the transport playing; NO SF CAPTURE (94 ROOM's device off); a take already running."],
    ["A capture says THE SWEEP DIDN'T COME BACK, or under 30 dB", "The sweep didn't reach the speaker or the mics: the speaker on the headphone out, 94 ROOM on and up; Input Config 1/2 a stereo pair, Monitor In. Then louder or closer. The slot stays as it was."],
    ["A capture says NO SWEEP IN THE RIGHT CHANNEL", "That mic heard nothing (its cable, its gain): the place is mono. Fix it, capture again."],
    ["A capture says SF CAPTURE STOPPED ANSWERING", "94 ROOM's SF CAPTURE was removed or switched off. Put it back, capture again: nothing was stored."]], [1.9 * inch, W - 1.9 * inch], mono_first=False))
story.append(PageBreak())

# ---------------- appendix
story.append(H1("APPENDIX"))
story.append(Rule())
story.append(H2T("E16 ADDRESS MAP (SF SHOW)"))
story.append(P("Every address of the scene once, by channel: the Live target, the mapping, and where the knob is on the E16. The same list is controllers/e16/sf-show-map.csv."))
rows = [["CH", "MESSAGE", "LIVE TARGET", "MAPPING", "ON THE E16"]]
for (msg, ch, n), (target, places, kind, rng, item) in sorted(E16.addresses().items(), key=lambda kv: (kv[0][1], kv[0][0], kv[0][2])):
    m = f"CC {n}" if msg == "CC" else f"NOTE {n} ({E16.note_name(n)})"
    how = {"TURN": "absolute", "HOLD": "press: hold", "TOGGLE": "press: toggle"}[kind] + (f", {rng}" if rng else "")
    rows.append([str(ch), m, esc(target), esc(how), esc(E16.where(places, getattr(item, "second", False)))])
story.append(table(rows, [0.35 * inch, 1.05 * inch, 2.55 * inch, 1.25 * inch, W - 5.2 * inch], mono_first=False, pad=2.3))

doc = Doc(os.path.join(ROOT, "docs", "SF-SHOW-Operators-Manual.pdf"), title="SF SHOW Operator's Manual",
          header="SF SHOW  OPERATOR'S MANUAL", footer="V0.2  NOV 2026", cover_title="SF SHOW", cover_sub="OPERATOR'S MANUAL",
          cover_lines=("THE SHOW TEMPLATE, THE E16 SCENE AND THE OXI ONE MKII",
                       "ONE LIVE SET   ONE E16 SCENE   ONE SEQUENCER", "", "VERSION 0.2   NOVEMBER 2026", "STRANGE FREQUENCIES"),
          cover_note="E16 PAGES, PRESSES AND ADDRESSES IN THIS MANUAL ARE GENERATED FROM CONTROLLERS/E16/LAYOUT.PY.")
doc.multiBuild(story)
print("ok")
