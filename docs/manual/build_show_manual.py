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
    """The three parts and what runs between them: notes from the ONE, clock back to it; knobs and
    presses from the E16, values back to its rings; audio to the interface, and the room mics into it."""
    H = 266
    def wrap(self, aw, ah): self.aw = aw; return aw, self.H
    def draw(self):
        c, aw = self.canv, self.aw
        bw, lw = 112, 136
        xo, xl, xe = 0, (aw - lw) / 2, aw - bw
        yb, hb = 140, 60                      # OXI and E16 boxes
        yl, hl = 124, 92                      # Live
        box(c, xo, yb, bw, hb, "OXI ONE MKII", ("8 SEQUENCERS", "PLAYS THE NOTES", "AND THE PATTERNS"))
        box(c, xl, yl, lw, hl, "LIVE: SF SHOW.ALS", ("SF PLUGINS ON EVERY TRACK", "SF CONTROL: SONGS, KEY,", "MACROS",
                                                    "THE CLOCK MASTER"), heavy=True)
        box(c, xe, yb, bw, hb, "OXI E16", ("SF SHOW: A PAGE A TRACK", "SF MIX: LEVELS, RETURNS", "THE SONG AND THE DROPS"))
        # ONE <-> Live
        gx0, gx1 = xo + bw, xl
        arrow(c, gx0, yb + 44, gx1, yb + 44); label(c, (gx0 + gx1) / 2, yb + 49, "NOTES")
        arrow(c, gx1, yb + 18, gx0, yb + 18, dashed=True); label(c, (gx0 + gx1) / 2, yb + 8, "CLOCK")
        # E16 <-> Live
        gx2, gx3 = xl + lw, xe
        arrow(c, gx3, yb + 44, gx2, yb + 44); label(c, (gx2 + gx3) / 2, yb + 49, "KNOBS, PRESSES")
        arrow(c, gx2, yb + 18, gx3, yb + 18, dashed=True); label(c, (gx2 + gx3) / 2, yb + 8, "VALUES: RINGS")
        # Port notes under the side boxes
        for i, t in enumerate(("USB PORT A: EVERY PART,", "CHANNEL = TRACK NUMBER", "PORT B: DECK'S CUES, CH 7")):
            label(c, xo + bw / 2, yb - 12 - 8 * i, t, 5.8)
        for i, t in enumerate(("USB PORT 1: REMOTE,", "INPUT AND OUTPUT", "PORTS 2 AND 3: OFF")):
            label(c, xe + bw / 2, yb - 12 - 8 * i, t, 5.8)
        # Interface and outputs below
        yi, hi = 52, 30
        box(c, xl, yi, lw, hi, "AUDIO INTERFACE", ("48 KHZ, BUFFER 256; ITS KNOB", "IS THE PA'S VOLUME AND STOP"))
        arrow(c, xl + lw / 2, yl, xl + lw / 2, yi + hi)
        label(c, xl + lw / 2 + 6, yi + hi + 16, "PA 1/2   CUE 3/4   MAIN 5/6", 5.8, align="l")
        ow = 118
        xp, xh = xl + lw / 2 - ow - 22, xl + lw / 2 + 22
        box(c, xp, 0, ow, 28, "PA", ("FROM PA (ROOM EQ)",))
        box(c, xh, 0, ow, 28, "HEADPHONES", ("CUE OUT; ROOM'S SWEEP",))
        arrow(c, xl + lw / 2 - 10, yi, xp + ow / 2, 28)
        arrow(c, xl + lw / 2 + 10, yi, xh + ow / 2, 28)
        # The room mics: ROOM records them with the set; SF CAPTURE takes SF TERROIR's places with them.
        box(c, xo, yi, bw, hi, "ROOM MICS", ("INPUTS 1/2: ROOM",))
        arrow(c, xo + bw, yi + hi / 2, xl, yi + hi / 2)


# ------------------------------------------------------------------ 3: the ONE's sequencers
class OxiMap(Flowable):
    """The ONE's eight sequencers, the output (channel and port, as the ONE names it) and the Live
    track each one plays: channel = the track's number, on port A."""
    RH = 21
    ROWS = [("1 KICK", "SF KICK", "1A"), ("2 CLOSED HAT", "SF PERC", "2A"), ("3 OPEN HAT", "SF PERC", "3A"), ("4 CLAP", "SF PERC", "4A"),
            ("5 PERC", "SF STRIKE", "5A"), ("6 BASS", "SF BASS", "6A"), ("8 LEAD", "SF LEAD", "8A"), ("9 CHORD", "SF LEAD", "9A"),
            ("10 PAD", "SF PAD", "10A"), ("11 VOICE", "SF VOICE", "11A"), ("DECK", "SF DECK", "7B"), ("CONTROL", "SF CONTROL", "16A")]
    SEQ = [("2  BASS", "MONO"), ("3  LEAD", "MONO"), ("4  CHORD", "MONO TO START"), ("5  PAD", "MONO TO START"),
           ("6  VOICE", "MONO"), ("7  SPARE", "MONO: DECK'S HOT CUES"), ("8  SPARE", "MONO: PC FOR AN AU")]
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
        box(c, xs, yrow(4), sw, 5 * RH - 4, "1  DRUMS", ("MULTITRACK", "A TRACK PER DRUM", "KICK ON THE BOTTOM ROW"))
        for i, (name, mode) in enumerate(self.SEQ):
            box(c, xs, yrow(5 + i), sw, bh, name, (mode,), title_size=7.0, sub_size=5.6)
        for r, (track, plugin, out) in enumerate(self.ROWS):
            y = yrow(r)
            c.setLineWidth(0.8); c.setStrokeColor(INK); c.setFillColor(colors.white); c.rect(xt, y, tw, bh, stroke=1, fill=1)
            label(c, xt + 7, y + bh / 2 - 2.6, track, 7.2, INK, "MonoB", "l")
            label(c, xt + tw - 7, y + bh / 2 - 2.4, plugin, 6.0, GREY, "Mono", "r")
            arrow(c, xs + sw, y + bh / 2, xt, y + bh / 2)
            label(c, (xs + sw + xt) / 2, y + bh / 2 + 3, out, 6.6, INK, "MonoB")
        # Port brackets
        xb = xt + tw + 8
        for (r0, r1, name, sub) in ((0, 9, "USB PORT A", "CHANNEL = TRACK NUMBER"), (10, 10, "PORT B", "HOT CUES"), (11, 11, "PORT A", "AN AU'S PC ONLY")):
            y1, y0 = yrow(r0) + bh, yrow(r1)
            c.setStrokeColor(INK); c.setLineWidth(0.8)
            c.line(xb, y1, xb + 4, y1); c.line(xb + 4, y1, xb + 4, y0); c.line(xb, y0, xb + 4, y0)
            label(c, xb + 10, (y0 + y1) / 2 + 1.5, name, 7.0, INK, "MonoB", "l")
            label(c, xb + 10, (y0 + y1) / 2 - 7, sub, 5.8, GREY, "Mono", "l")


# ------------------------------------------------------------------ 5: one E16 page
def short_target(ctl):
    """'LOW / SF ENERGY' or '9 CHORD / SEND A': where the knob lands, short enough for its cell."""
    parts = ctl.target.split(" > ")
    if parts[1] == "mixer": return f"{parts[0]} / {parts[2].upper()}"
    return f"{parts[0]} / {parts[1]}"


class E16Page(Flowable):
    """One page of the scenes, drawn from layout.py: the 4 x 4 grid, numbered row by row from the top
    left. Each knob: its ring (black: a parameter, light blue: a level), its number, the name on the
    E16's screen, the parameter, where it lands in Live, its address, and its press if it has one. An
    encoder with nothing on it is drawn empty."""
    RH, PAD = 52, 6
    def __init__(self, title, controls):
        super().__init__(); self.title, self.controls = title, controls
    def wrap(self, aw, ah):
        self.aw = aw; self.H = 4 * self.RH + 2 * self.PAD + 14; return aw, self.H
    def draw(self):
        c, aw, RH, PAD = self.canv, self.aw, self.RH, self.PAD
        cw = (aw - 2 * PAD) / 4
        gy0 = 14                                  # grid bottom (legend below)
        gtop = gy0 + 4 * RH + 2 * PAD
        c.setStrokeColor(INK); c.setLineWidth(1.0); c.roundRect(0.5, gy0, aw - 1, gtop - gy0, 5, stroke=1, fill=0)
        for r in range(4):
            for col in range(4):
                i = r * 4 + col + 1
                ctl = self.controls[i - 1]
                x = PAD + col * cw
                y1 = gtop - PAD - r * RH                       # top of the cell
                cx, cy = x + 15, y1 - 19
                if ctl is None:
                    c.setLineWidth(1.0); c.setStrokeColor(FAINT); c.setFillColor(colors.white); c.circle(cx, cy, 10.5, stroke=1, fill=1)
                    label(c, cx, cy - 2.5, str(i), 7.0, FAINT, "MonoB")
                    label(c, x + 31, y1 - 13, "-", 7.6, GREY, "MonoB", "l")
                    continue
                vol = ctl.colour == E16.LIGHT_BLUE
                ring = CYAN if vol else INK
                c.setLineWidth(2.4); c.setStrokeColor(ring); c.setFillColor(colors.white); c.circle(cx, cy, 10.5, stroke=1, fill=1)
                label(c, cx, cy - 2.5, str(i), 7.0, INK, "MonoB")
                tx = x + 31
                label(c, tx, y1 - 13, ctl.name, 7.6, INK, "MonoB", "l")
                yl = y1 - 21.5
                if ctl.label.upper() != ctl.name.upper():
                    label(c, tx, yl, ctl.label.upper(), 6.0, INK, "Mono", "l"); yl -= 8
                label(c, tx, yl, short_target(ctl), 5.7, GREY, "Mono", "l"); yl -= 7.6
                addr = f"CH {ctl.channel}  CC {ctl.number}"
                rng = ctl.live_range.replace("Max 0 dB", "MAX 0 dB").replace(" at first", "")
                label(c, tx, yl, addr + (f"  {rng}" if rng else ""), 5.7, GREY, "Mono", "l")
                p = E16.press_for(self.title, i)
                if p:
                    t = (f"PRESS: SWAP TO {p.name}" if p.mode == "SWAP" else "PRESS: TO THE CENTRE" if p.mode == "CENTRE"
                         else f"PRESS {p.name} / {p.mode}")
                    w = stringWidth(t, "MonoB", 5.6) + 6
                    by = y1 - RH + 4
                    c.setFillColor(INK); c.rect(tx, by, w, 8, stroke=0, fill=1)
                    label(c, tx + 3, by + 2.2, t, 5.6, colors.white, "MonoB", "l")
        label(c, 0, 3, "RING: BLACK = A PARAMETER, LIGHT BLUE = A LEVEL.   CH / CC: THE ADDRESS.", 5.8, GREY, "Mono", "l")


# ------------------------------------------------------------------ 6: a song change
class SongFlow(Flowable):
    H = 52
    STEPS = [("NEXT SONG", ("E16 KNOB 15 PICKS", "NOTHING CHANGES YET")), ("GO", ("KNOB 15'S PRESS", "CALLS THE SONG")),
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
    "1 KICK": ("SF KICK on 1 KICK. TUNE and DECAY are the body, PUNCH and CLICK the attack.",
               ["Set TUNE and DECAY per song and store them: KICK and RUMBLE tune themselves to the song's KEY when KEY is on.",
                "FILTER right thins the kick, and Pocket closes the low pockets as it thins: the bass and rumble take the sub. Bring the kick's low end back on the drop."]),
    "2 CLOSED HT": ("SF PERC (HAT 909-TYPE) on 2 CLOSED HAT. The fill page.",
                    ["DENSITY with ECHO SPACE at 1/16 turns 1/8 hats into 1/16 hats: the send-delay trick, built in. Past 50 % it rolls in 1/32s.",
                     "ECHO SWING and ECHO PROB make the ghosts loose and sparse: a groove that breathes without a new pattern."]),
    "3 OPEN HAT": ("SF PERC (HAT 909-TYPE, DECAY 0.8) on 3 OPEN HAT.",
                   ["DECAY from closed to open across a build. CHOKE works inside one track only: the closed hat never cuts this one.",
                    "TONE and ACCENT set how the off-beat speaks; SPREAD widens it."]),
    "4 CLAP": ("SF PERC (CLAP) on 4 CLAP.",
               ["DENSITY for rolls into the drop; DECAY short for a snap, long for a room.",
                "SEND B on the clap's last hit before a breakdown: the room carries it."]),
    "5 PERC": ("SF STRIKE on 5 PERC: blast-excited bells, plates, bars, tubes and gongs.",
               ["SIZE sets the pitch; MATERIAL goes from wood to glass; DISTANCE darkens and lengthens the hit.",
                "POWER above about 70 % fractures the object: save it for the peak."]),
    "6 BASS": ("SF BASS on 6 BASS. CUTOFF, RES and ENV AMT move the line; GATE shortens it.",
               ["GATE down with DENSITY up is a tight, nervous build; GATE open with DENSITY down is spacious dub.",
                "DRIVE adds weight at low settings and grit at high ones. GLIDE and ACCENT shape the line's slides and pushes."]),
    "7 RUMBLE": ("SF RUMBLE on 7 RUMBLE. It plays no notes: it follows SF KICK.",
                 ["SIZE is the bed's reverb, DARK its low-pass (600 Hz down to 70 Hz), GRIT its distortion; PUMP is how far it ducks under each kick.",
                  "In each section decide whether BASS or RUMBLE carries the sustained low end, then steer that one."]),
    "8 LEAD": ("SF LEAD (ACID 303-TYPE) on 8 LEAD. CUTOFF, RES and ENV AMT are the squelch.",
               ["Overlapping notes slide like a 303 (always 60 ms): write legato notes on the ONE for slides.",
                "SLOW ALL at 8 BAR and a quarter turn of CUTOFF: an eight-bar rise that plays itself."]),
    "9 CHORD": ("SF LEAD (STAB DUB) on 9 CHORD. SPACE is the stab's ensemble.",
                ["If the ONE sends whole chords, set CHORD to NONE on the plugin; if it sends single notes, CHORD voices them. Not both.",
                 "Short GATE plus SEND A (13): the dub-chord move. Ride the DUB return on SF MIX's LEVELS page."]),
    "10 PAD": ("SF PAD (GRANULAR) on 10 PAD. On GRANULAR, COLOR is grain size and DETUNE is scatter.",
               ["ATTACK up for swells; SPACE is the pad's own room.",
                "Field recordings work well in GRANULAR: drop one on the plugin before the show."]),
    "11 VOICE": ("SF VOICE on 11 VOICE: phrases, radio fragments, choir.",
                 ["SIGNAL goes from lost to clear (RADIO model); ECHO feeds the voice's own dub echo, ECHO TME and ECHO FBK shape it.",
                  "C3 plays slice 1, C#3 slice 2 and so on; below C3 the whole phrase. Sequence it as choreography."]),
    "PERFORM": ("Every ENERGY knob: knob n is track n's SF ENERGY, 12-14 the groups'. The arrangement under one hand.",
                ["Centre is neutral: left takes energy out (filter, space), right puts it in (drive, echo, chop). Small moves, often.",
                 "Press an ENERGY knob and it goes back to the centre; with SLOW ALL on it glides there.",
                 "The group knobs (12-14) move a whole section; a track's own knob shapes one part inside it."]),
    "LEVELS": ("SF MIX. The volumes in PERFORM's places: knob n is track n's, 12-14 the groups', 15 and 16 the returns'.",
               ["Every volume is mapped up to 0 dB, so a full turn never runs past the soundcheck balance.",
                "Set each instrument's LEVEL (its song) so the volumes at 0 dB give the intended mix; the volumes are your balance on the night."]),
    "SPACE": ("SF MIX. The returns: 1-4 DUB (SF DELAY on A DUB), 5-8 SPACE (SF REVERB on B SPACE). The presses sit under their knobs.",
              ["THROW (hold) sends into the effect for as long as you hold it. FREEZE (toggle) loops what is in it until pressed again.",
               "Turn DUB TIME while frozen: the loop bends like tape speed and locks again when you let go.",
               "DUB FEEDBACK is mapped 0-70 % at first; widen the mapping when the music asks.",
               "SF TERROIR (your captured places) can replace SF REVERB on B SPACE: the same four knobs and two presses, mapped once (Cmd-M)."]),
    "MACROS": ("SF MIX. SF CONTROL's six macros and SLOW ALL, and SF REPEAT on DRUMS.",
               ["Hold ROLL on 9 and turn LENGTH under the same finger: the drums roll at the length you dial.",
                "10 PITCH makes the roll fall as it repeats. The macro plan is in chapter 7. SLOW ALL is a plain knob here (7)."]),
}

SHORT = {
    "density": "Ghost notes: an echo of every note one ECHO SPACE later, up to 50 %; above it, two more at half that spacing (1/32 rolls). Each ghost quieter, darker, shorter.",
    "filter": "Bipolar DJ filter on the output: centre open, left a low-pass down to 60 Hz, right a high-pass up to 12 kHz. A full sweep never jumps in volume.",
    "level": "The plugin's output level, stored per song. Set at soundcheck so the volumes at 0 dB give the intended mix.",
}
MACRO_PLAN = {"macro1": "ENERGY in the starting plan: the group ENERGY plugins and BUILD together.",
              "macro2": "MOOD: PAD, CHORD and VOICE (cutoff, envelope, drive; the voice's SIGNAL and ECHO).",
              "macro3": "DENSITY: CLOSED HAT, OPEN HAT, CLAP and PERC (DECAY and DENSITY: the fills).",
              "macro4": "PULSE: KICK and RUMBLE (kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY).",
              "macro5": "SPACE: the DUB and SPACE returns (delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX).",
              "macro6": "FRACTURE: LEAD and BASS (cutoff, envelope, density and drive: smooth to heavy)."}
MIXER_DESC = {"A DUB": "The dub return's volume (SF DELAY on A DUB), up to 0 dB.", "B SPACE": "The reverb return's volume (SF REVERB on B SPACE), up to 0 dB.",
              "DRUMS": "The DRUMS group's volume: 2-5, the hats, the clap and the perc (not the kick), up to 0 dB.",
              "LOW": "The LOW group's volume: 6 BASS and 7 RUMBLE, up to 0 dB.", "MUSIC": "The MUSIC group's volume: 8-11, up to 0 dB."}

def knob_desc(ctl, many):
    """What a knob does, from the plugin's own description (shortened where every page repeats it)."""
    if ctl.mixer == "volume": return MIXER_DESC.get(ctl.track, f"The volume of {ctl.track}, up to 0 dB.")
    if ctl.mixer in ("send A", "send B"):
        return f"{ctl.track}'s send into " + ("A DUB, the dub delay." if ctl.mixer == "send A" else "B SPACE, the reverb.") + " Post-fader, up to 0 dB."
    if ctl.module == "CONTROL" and ctl.pid in MACRO_PLAN:
        return f"Macro {ctl.pid[-1]}: every SF plugin with MACRO SRC = MACRO {ctl.pid[-1]} follows it. " + MACRO_PLAN[ctl.pid]
    if ctl.module == "CONTROL" and ctl.pid == "next_song":
        return "Picks the next song; nothing changes until you press it (GO): the song lands on the next bar line. The same knob on every SF SHOW page."
    if ctl.module == "BUILD" and ctl.pid == "build":
        return "SF BUILD on LIVE BUS: turn it up over a build. Press it: DROP, on the next bar line (chapter 7). The same knob on every SF SHOW page."
    if ctl.module == "ENERGY" and ctl.pid == "energy":
        return f"{ctl.track}'s SF ENERGY: centre neutral, left takes energy out, right puts it in."
    if ctl.module == "PAD" and ctl.pid == "color":
        return (f"{ctl.track}: " if many else "") + "Model-specific tone; on GRANULAR, the template's PAD model, it is the grain size."
    d = dict(C.COMMON); d.update(C.MODULES[ctl.module]["desc"])
    text = d.get(ctl.pid, "")
    if ctl.pid in SHORT and text == C.COMMON.get(ctl.pid): text = SHORT[ctl.pid]
    return (f"{ctl.track}: " if many else "") + text


def page_sub(scene, title, controls):
    """The line under a page's heading: its scene, what it works on and on which channel."""
    if title in [t for t, _ in E16.SHOW_PAGES[:-1]]:
        c = controls[1]
        return f"SF SHOW: SF {c.module} ON {c.track}, CHANNEL {c.channel}"
    return {"PERFORM": "SF SHOW: EVERY SF ENERGY, NEXT SONG AND BUILD", "LEVELS": "SF MIX: THE VOLUMES, IN PERFORM'S PLACES",
            "SPACE": "SF MIX: SF DELAY ON A DUB, SF REVERB ON B SPACE", "MACROS": "SF MIX: SF CONTROL'S MACROS, SF REPEAT ON DRUMS"}[title]

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
    ["OXI ONE MKII", "Every note, rhythm and pattern change."],
    ["LIVE: SF SHOW.ALS", "Every sound (one SF plugin per role), the songs, the master chain, the recording, the clock."],
    ["OXI E16", "A page per track, every ENERGY on one page, the song call and the drop (SF SHOW); the levels, the returns and the macros (SF MIX)."],
    ["AUDIO INTERFACE", "Its output knob is the PA's volume and the emergency stop: the room goes quiet, the recording carries on."]],
    [1.35 * inch, W - 1.35 * inch]))
story.append(H2T("ONE NUMBER PER TRACK"))
story += bullets(["<b>A track's number is everything:</b> 3 OPEN HAT is the third track in Live, plays on channel 3 of the ONE's port A, and is page 3 of SF SHOW, its knobs on channel 3 of the E16. Live, the ONE and the E16 read the same way.",
                  "<b>The order:</b> 1 KICK, the drums (2 CLOSED HAT, 3 OPEN HAT, 4 CLAP, 5 PERC), the low end (6 BASS, 7 RUMBLE), the music (8 LEAD, 9 CHORD, 10 PAD, 11 VOICE).",
                  "<b>A track is a role, not an instrument.</b> Put another instrument on 9 CHORD and it keeps its name, its channel and its page: map the page to the new instrument (Cmd-M) and nothing else moves."])
story.append(H2T("THE TWO HANDS"))
story += bullets(["<b>PERFORM, page 12:</b> every track's ENERGY and every group's, the song call and the build. The whole arrangement under one hand.",
                  "<b>A track's page:</b> its instrument, deep. Knobs 15 and 16 are NEXT SONG and BUILD on every page, so the song and the drop are always under the same finger."])
story.append(H2T("WHAT NEVER CHANGES IN A SHOW"))
story += bullets(["The tracks, the routing, the E16 scenes and the mappings. A song is a stored set of values in every SF plugin, recalled on the bar: nothing is loaded between tracks."])
story.append(PageBreak())

# ---------------- 2
chapter("2  THE SHOW TEMPLATE", "SF SHOW.als is built: every track, group and return, every SF plugin with its starting settings, the routing, every instrument's MIDI From on its OXI One port and channel, and every address of both E16 scenes mapped. Set Live's own settings once (below), open it, and keep it as the template for every show.")
story.append(H2T("TRACKS"))
story.append(table([["TRACK", "DEVICES", "MIDI FROM", "OUT"],
    ["CONTROL", "SF CONTROL", "ONE port A, ch 16", "Main"],
    ["1 KICK", "SF KICK; SF ENERGY", "ONE port A, ch 1", "LIVE BUS"],
    ["DRUMS (group)", "SF ENERGY, PROFILE DRUMS; SF REPEAT", "", "LIVE BUS"],
    ["2 CLOSED HAT", "SF PERC: closed hat; SF ENERGY", "port A, ch 2", "DRUMS"],
    ["3 OPEN HAT", "SF PERC: open hat or ride; SF ENERGY", "port A, ch 3", "DRUMS"],
    ["4 CLAP", "SF PERC: clap or rim; SF ENERGY", "port A, ch 4", "DRUMS"],
    ["5 PERC", "SF STRIKE; SF ENERGY", "port A, ch 5", "DRUMS"],
    ["LOW (group)", "SF ENERGY, PROFILE BASS", "", "LIVE BUS"],
    ["6 BASS", "SF BASS; SF ENERGY", "port A, ch 6", "LOW"],
    ["7 RUMBLE", "SF RUMBLE, SOURCE KICK BUS; SF ENERGY", "none: it follows the kick", "LOW"],
    ["MUSIC (group)", "SF ENERGY, PROFILE MUSIC", "", "LIVE BUS"],
    ["8 LEAD", "SF LEAD: ACID 303-TYPE; SF ENERGY", "port A, ch 8", "MUSIC"],
    ["9 CHORD", "SF LEAD: STAB DUB; SF ENERGY", "port A, ch 9", "MUSIC"],
    ["10 PAD", "SF PAD: GRANULAR; SF ENERGY", "port A, ch 10", "MUSIC"],
    ["11 VOICE", "SF VOICE; SF ENERGY", "port A, ch 11", "MUSIC"],
    ["BRIDGE", "Live audio clips", "", "Main"],
    ["DECK", "SF DECK", "port B, ch 7 (hot cues; optional)", "Main"],
    ["LIVE BUS", "SF ISOLATOR &gt; BUILD &gt; GLUE &gt; TONE &gt; LOWEND; SF ALLOC (off at first)", "sends disabled", "Main"],
    ["PA", "SF ROOM EQ &gt; SF LIMIT (safety)", "audio: Main, Post Mixer", "the PA outputs (1/2)"],
    ["REC", "nothing", "audio: Main, Post Mixer; Monitor Off", "Sends Only"],
    ["ROOM", "SF CAPTURE", "audio: Ext. In 1/2, the room mics; Monitor In", "the headphones (3/4)"],
    ["A DUB (return)", "SF DELAY, DRY off", "", "LIVE BUS"],
    ["B SPACE (return)", "SF REVERB, DRY off", "", "LIVE BUS"],
    ["Main", "SF LIMIT", "", "5/6: a pair that goes nowhere"]],
    [1.25 * inch, 2.35 * inch, 1.75 * inch, W - 5.35 * inch], pad=2.6))
story.append(Spacer(1, 4))
story.append(P("Every instrument track is on Monitor In: it plays from the ONE whether or not it is armed, and ends in SF ENERGY (its knob on PERFORM). MIDI From is set on every instrument: in Live the ONE's port A is OXI ONE MKII (Port 1), port B is (Port 2). The kick stays outside DRUMS and LOW, so the drum group's REPEAT never takes it and the bass and the rumble fade together while the kick holds the floor.", "smallgrey"))
story.append(H2T("ROUTING"))
story.append(RoutingDiagram())
story.append(Spacer(1, 6))
story += bullets(["Main is the set, after SF LIMIT: exporting it gives the release mix, and its own output goes nowhere. PA and REC both take the Main after its fader. The venue's correction (ROOM EQ) exists only on PA, so the recording never hears it.",
                  "PA's SF LIMIT is a safety: CEILING -0.5 dB, no clip, AUTO off. The set's peaks are already at -1 dBTP, so it only catches what the room EQ's phase pushes past. Keep PA's fader at 0 dB: the PA's level is the interface's.",
                  "Never send LIVE BUS to PA: BRIDGE, DECK and SF LIMIT would be left out of the PA, and REC would record nothing of the set.",
                  "<b>Every send on LIVE BUS is disabled.</b> The returns feed LIVE BUS, and Live can't compensate a return's delay on a track whose matching send is active, even at zero.",
                  "Pull LIVE BUS down and the live instruments and their effects go while BRIDGE and DECK play on. Turn the interface's output down and the room goes quiet while REC keeps recording.",
                  "ROOM is the room mics, a stereo pair. Armed, it records them with the set. Its SF CAPTURE never passes them (the set never hears the mics): it only plays SF TERROIR's sweep, to a speaker on the headphone out, when you capture the room (chapter 8)."])
story.append(H2T("LIVE'S OWN SETTINGS, ONCE PER COMPUTER"))
story.append(table([["PORT (SETTINGS &gt; TEMPO &amp; MIDI)", "TRACK", "SYNC", "REMOTE"],
    ["OXI ONE MKII (Port 1) and (Port 2), input", "on", "off", "off"],
    ["OXI ONE MKII (Port 1), output", "off", "on", "off"],
    ["OXI E16 (Port 1), input and output", "off", "off", "on"],
    ["every other port", "off", "off", "off"]],
    [3.2 * inch, 0.9 * inch, 0.9 * inch, W - 5.0 * inch], mono_first=False))
story.append(Spacer(1, 4))
story += bullets(["Takeover Mode: Value Scaling. Audio: 48 kHz, buffer 256. These live in Live's preferences, not in any set: nothing in SF SHOW.als can set them.",
                  "<b>MIDI Clock Sync Delay</b> on the ONE's Port 1 output (its row unfolded with the triangle at its left): <b>0</b>. The ONE's notes come back to Live a little late (the clock goes out, the notes come back, buffers both ways: on Nov 7, 40 ms), and nothing needs them on the beat. BUILD lands its drop on the kick it hears (LAND ON KICK; its KICK readout shows how late the kick reaches it), and Pocket and the effects that cut on the beat (GATE, STRIP, ENERGY's chop, DUCK's GRID, REPEAT, DECK) follow where SF KICK's kicks land. On Nov 7, -70 ms there moved the kick not at all.",
                  "Before Live 12.4 the tab is called Link, Tempo &amp; MIDI."])
story.append(H2T("OPEN IT, CHECK, SAVE"))
story += numbered(["<b>MIDI From</b> is set on every instrument, as in the table above (chapter 3 shows the ONE's side). If your Live names the ONE differently, a track shows its port as missing: set it by the table. Never All Ins: every instrument would play every part.",
                   "<b>Inputs and outputs</b>, set for the PA on 1/2 and headphones on 3/4. PA to the pair that feeds the PA (1/2 in the set). Cue Out, on the Main track, to the headphones (3/4), and ROOM there too. Main to a pair that goes nowhere (5/6). Check all three before any sound reaches a PA: the PA pair carries PA and nothing else. The room mics on inputs 1 and 2 (1: the mic on your left as you face the speaker), and in Live's Settings &gt; Audio &gt; Input Config, 1/2 as a stereo pair: ROOM's input.",
                   "<b>Two options:</b> Delay Compensation on; Reduced Latency When Monitoring off.",
                   "<b>Save</b> as a template (File &gt; Save Live Set As Template) or into your show folder. MIDI From and the mappings belong to the set, so every set made from it starts ready."])
story.append(H2T("THE MASTER CHAIN"))
story += bullets(["Start GLUE, TONE and LOWEND gently: the set doesn't need every processor working all the time.",
                  "Soundcheck the busiest passage of the night and set SF LIMIT's INPUT for 1-3 dB of limiting there. LIMIT AUTO on, AUTO MODE DOWN ONLY, for shows: it holds the set at TARGET and holds through breakdowns.",
                  "SF ALLOC starts off. Turn it on band by band once the arrangement is known, and judge it by ear."])
story.append(PageBreak())

# ---------------- 3
chapter("3  THE OXI ONE MKII", "The ONE plays every part. Eight sequencers, one per role; every part plays on port A, on its Live track's number. Live is the clock: the ONE follows it.")
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
story.append(P("On the ONE an output is a channel and a port: 1A is channel 1 on port A, 7B channel 7 on port B. Every part plays on port A on its track's number (3 OPEN HAT on 3A); each Live track's MIDI From listens to the same port and channel. SF START has every sequencer set so (controllers/oxi-one)."))
story.append(OxiMap())
story.append(Spacer(1, 6))
story += bullets(["<b>The multitrack's rows,</b> bottom up: 8 KICK 1A, 7 PERC 5A, 6 CLAP 4A, 5 OPEN HAT 3A, 4 CLOSED HAT 2A; 3-1 off. The kick stays on the bottom row.",
                  "<b>KICK lane on C1.</b> With KEY TRACK on, any other note transposes the kick (C1 = TUNE), so C1 is right either way. PERC plays any note.",
                  "<b>7 RUMBLE needs no lane:</b> SOURCE KICK BUS follows SF KICK. Channel 7 on port A stays free.",
                  "<b>9 CHORD:</b> if the ONE sends whole chords (Chord or Poly mode), set CHORD on the plugin to NONE; if it sends single notes, CHORD voices them. Not both.",
                  "<b>8 LEAD</b> slides when notes overlap, over 60 ms like a 303: write legato notes for slides.",
                  "<b>11 VOICE:</b> C3 plays slice 1 of the loaded phrase, C#3 slice 2, and so on; below C3 the whole phrase.",
                  "<b>DECK</b> takes hot cues on port B, channel 7 (C1-D#1 deck A, C2-D#2 deck B): spare sequencer 7 is set to 7B in SF START."])
story.append(CondPageBreak(2.6 * inch))
story.append(H2T("CLOCK AND PORTS"))
story += bullets(["<b>Live is the clock master.</b> On the ONE: sync USB (stopped, Shift + Stop until the header shows USB, not INT); in its config (Shift + Back), Start Stop Msgs Ignore No, MIDI Transport Msgs Send Off, MIDI Thru off, Auto Sync on (it starts with Live). Never send clock both ways.",
                  "<b>Live sends its clock only while it plays.</b> Start and stop from Live, never with the ONE's Play: started on its own, the ONE runs at its own tempo and ignores Live's.",
                  "<b>Audio:</b> 48 kHz, buffer 256 (Pocket needs 512 or less), launch quantization 1 bar, record quantization off.",
                  "<b>The ONE plays a little after Live's beat,</b> and nothing needs it on the beat: BUILD lands its drop on the kick it hears (its KICK readout shows how late), Pocket and the effects that cut on the beat follow SF KICK. MIDI Clock Sync Delay stays at 0. Leave POCKET OFFSET on SF CONTROL at 0 unless the pump sounds off."])
story.append(table([["LIVE MIDI PORT", "TRACK", "SYNC", "REMOTE"],
    ["OXI ONE MKII (Port 1), in: the ONE's port A", "on", "off", "off"], ["OXI ONE MKII (Port 2), in: its port B", "on", "off", "off"],
    ["OXI ONE MKII (Port 1), out: the clock", "off", "on", "off"],
    ["OXI E16 (Port 1), in and out", "off", "off", "on"],
    ["Every other port, E16 Ports 2 and 3 included", "off", "off", "off"]],
    [3.0 * inch, 0.9 * inch, 0.9 * inch, W - 4.8 * inch]))
story.append(H2T("PROJECTS AND PATTERNS"))
story += bullets(["<b>One ONE project per track</b> (per piece of music), with a pattern per section: SPARSE, MAIN, VARIATION, EXIT.",
                  "<b>Every project starts as a copy of SF START</b>, the startup project: every sequencer set as above, and four scenes of simple patterns in columns 1-4, SPARSE, MAIN, VARIATION, EXIT (controllers/oxi-one: SFSTART.oxipro and its card). Save, an empty slot, Duplic proj here. check_project.py checks the copy.",
                  "<b>The song comes from the E16, not the project</b> (chapter 6): any song with any project's patterns, and changing projects never changes the sound. Program Change stays off on every sequencer (it reaches only an AU of SF CONTROL).",
                  "<b>When patterns change is set on the ONE</b> (its launch and arranger settings): Live's launch quantization doesn't reach it.",
                  "<b>KEY on SF CONTROL starts OFF:</b> the ONE's notes set the pitch, and KICK and RUMBLE are tuned per song with TUNE. Turn KEY on for songs that should change key without reprogramming the ONE.",
                  "<b>Taking the kick out:</b> mute its lane on the ONE. Pulling the kick's volume down leaves it playing on the bus, so Pocket and RUMBLE keep pumping to a silent kick (which can be the point)."])
story.append(PageBreak())

# ---------------- 4
chapter("4  THE E16 SCENES", "Two E16 scenes run the show. SF SHOW: a page per track (page n is track n) and PERFORM, every ENERGY knob with the song call and the build. SF MIX: the levels, the returns and the macros. The set comes with every address of both mapped.")
story.append(H2T("THE GRID"))
story.append(P("The E16 is a 4 &times; 4 grid of endless encoders, numbered row by row from the top left. Its screen shows each knob's short name. On SF SHOW, a track's page:"))
cells = {i: ("SOUND", "ITS OWN" if i > 1 else "PRESS: SLOW ALL", INK, True) for i in range(1, 9)}
cells.update({9: ("GATE", "OR DECAY", INK, False), 10: ("DENSITY", "", INK, False), 11: ("FILTER", "", INK, False),
              12: ("LEVEL", "", CYAN, False), 13: ("SEND A", "INTO A DUB", INK, False), 14: ("SEND B", "INTO B SPACE", INK, False),
              15: ("NEXT SONG", "PRESS: GO", INK, False), 16: ("BUILD", "PRESS: DROP", INK, False)})
story.append(E16Grid(cells, bands=[(1, 2, "ITS SOUND", "ROWS 1 AND 2"), (3, 4, "EVERY TRACK", "ROWS 3 AND 4")],
                     note="LIGHT BLUE RING: A LEVEL.  15 AND 16 ARE THE SAME KNOBS ON EVERY SF SHOW PAGE."))
story.append(Spacer(1, 8))
story += bullets(["<b>1-8, its sound:</b> the instrument's own knobs (chapter 5 has every page).",
                  "<b>9-12, on every track:</b> GATE (DECAY where the instrument has none), DENSITY, FILTER, LEVEL. The hand learns one layout.",
                  "<b>13-14, its sends</b> into A DUB and B SPACE, post-fader, mapped up to 0 dB.",
                  "<b>15, NEXT SONG:</b> turn it to pick the next song: nothing changes. <b>Press it: GO</b>, the song lands on the next bar line (chapter 6).",
                  "<b>16, BUILD:</b> turn it up over a build. <b>Press it: DROP</b>, on every page. The drop lands on the next bar line (chapter 7).",
                  "<b>Press 1: SLOW ALL.</b> A quick press flips knob 1 to SLOW ALL on SF CONTROL (the ring turns green): turn it to set how long your next moves take, OFF (at once) to 128 BAR. Press again: the page's own knob 1. Knobs already travelling keep going."])
story.append(CondPageBreak(E16Grid.H + 30))
story.append(P("PERFORM, page 12:"))
cells = {}
for i, c in enumerate(E16.encoders("PERFORM"), 1):
    pr = E16.press_for("PERFORM", i)
    sub = "PRESS: SLOW ALL" if pr.mode == "SWAP" else "PRESS: CENTRE" if pr.mode == "CENTRE" else f"PRESS: {pr.name}"
    cells[i] = (c.label.replace(" ENERGY", "") if c.pid == "energy" else c.label, sub, INK, False)
story.append(E16Grid(cells, note="KNOB N IS TRACK N'S SF ENERGY; 12-14 THE GROUPS. A PRESS SETS IT BACK TO THE CENTRE (KNOB 1: SLOW ALL)."))
story.append(Spacer(1, 8))
story += bullets(["<b>The ENERGY knobs:</b> centre is neutral, left takes energy out, right puts it in (filter, drive, space, echo and chop together). Each track's own knob shapes that part; 12-14 move a whole group.",
                  "<b>SF MIX:</b> LEVELS has the volumes in PERFORM's places (knob n is track n's, 12-14 the groups', 15 DUB and 16 SPACE the returns'); SPACE the two returns, their throws and freezes; MACROS SF CONTROL's six macros, SLOW ALL and the drum roll. Its other nine pages are empty: their knobs send nothing."])
story.append(H2T("RINGS, PAGES, PRESSES"))
story += bullets(["<b>Rings:</b> light blue is a level (a LEVEL, a volume), white is everything else, green is knob 1 on SLOW ALL or a knob travelling (knob 1 itself never shows a glide: it swaps, so it always shows the value of the side it is on). Live sends every value back, so the rings follow song recalls and mouse moves.",
                  "<b>A recalled knob is yours the moment you turn it.</b> With Takeover Mode Value Scaling, a knob that disagrees with Live (after a song recall) moves the sound smoothly from where it is, never jumps.",
                  "<b>Changing pages or scenes sends nothing.</b> A knob on several pages (NEXT SONG, BUILD, SLOW ALL) sends the same address from all: several ways to one mapping.",
                  "<b>A hold press</b> is a CC, 127 on press and 0 on release: the parameter is on for as long as the knob is down (GO, DROP, THROW, ROLL). <b>A toggle press</b> is a note: Live flips the switch on every Note On (FREEZE), so the E16 can never think FREEZE is on while Live has it off. <b>A centre press</b> sends 64 on its knob's own address: an ENERGY back to neutral.",
                  "Every press is one the set plays through if it happens by accident (a throw, a roll, a freeze, a drop, an ENERGY back to neutral, a GO with no new song picked). Nothing that would stop the set is on the E16: that is the interface's output knob.",
                  "<b>On screen, the same grid.</b> Every SF plugin's window starts with its page of the scenes: its own encoders lit where they sit on the E16, in these colours, the rest dark, its presses under their knobs. Several windows side by side read like the E16 itself. LEAD, PERC and ENERGY are on several tracks: their windows find their encoders by the track's name (VST3)."])
story.append(CondPageBreak(1.7 * inch))
def press_row(where, i, p):
    kind = {"HOLD": "hold", "TOGGLE": "toggle", "SWAP": "swap", "CENTRE": "centre"}[p.mode]
    msg = {"HOLD": f"CC {p.number} ch {p.channel}", "SWAP": f"turn 2: CC {p.number} ch {p.channel}",
           "CENTRE": f"CC {p.number} ch {p.channel}, value 64"}.get(p.mode, f"note {p.number} ({E16.note_name(p.number)}) ch {p.channel}")
    return [where, str(i), f"{p.name}, {kind}", msg, esc(p.target)]
first_show = E16.SHOW_PAGES[0][0]
rows = [press_row("every SF SHOW page", i, E16.press_for(first_show, i)) for i in (1, 15, 16)]
for title, _ in E16.PAGES:
    for i in range(1, 17):
        p = E16.press_for(title, i)
        if p is None or (title in dict(E16.SHOW_PAGES) and i in (1, 15, 16)):
            continue
        rows.append(press_row(title, i, p))
story.append(table([["WHERE", "KNOB", "PRESS", "MESSAGE", "LIVE TARGET"]] + rows,
    [0.95 * inch, 0.45 * inch, 1.15 * inch, 1.45 * inch, W - 4.0 * inch], mono_first=False))
story.append(H2T("ADDRESSES"))
story += bullets(["<b>Channel = track:</b> 1-11 the instrument tracks, 12 A DUB, 13 B SPACE, 14 the groups (DRUMS, LOW, MUSIC and DRUMS' REPEAT), 15 LIVE BUS, 16 CONTROL. Nothing else uses a track's channel.",
                  "<b>CC = the knob's place + 11 in SF SHOW</b> (a track's page: CC 12-23), <b>+ 43 in SF MIX</b>; a press its place + 27 in SF SHOW, + 59 in SF MIX. A track's ENERGY: CC 28 (the groups on 14: 28, 29, 30).",
                  "<b>Mixer:</b> CC 102 volume, 103 send A, 104 send B on the track's own channel; channel 14: 102-104 the DRUMS, LOW and MUSIC volumes. CC 6, 38 and 96-101 (data entry and RPN/NRPN) are never used.",
                  "Every address is in the appendix and in controllers/e16/sf-show-map.csv."])
story.append(H2T("LOAD THE SCENES"))
story += numbered(["OXI App &gt; Scenes. If no computer folder is assigned yet, assign one.",
                   "Copy <b>SF SHOW.oxie16</b> and <b>SF MIX.oxie16</b> (controllers/e16 in the repository) into the Scenes folder inside it and click Refresh Views.",
                   "Drag SF SHOW, then SF MIX, from On Computer onto two free slots under On Device. The presses come with them.",
                   "<b>Auto Transmit off</b>, for both scenes and in the global settings: sending stored values on a page change would pull Live's faders to whatever the E16 last held. MIDI Thru off.",
                   "In Live: E16 Port 1 Remote on, input and output; Ports 2 and 3 off (Port 1 is the one the E16 sends on; a second Remote port could take a message twice, and a FREEZE would flip twice). Takeover Mode: Value Scaling."])
story.append(H2T("CHECK IT ONCE"))
story += numbered(["Every knob: turn it, the right parameter moves. Every press: the right switch. A track's page moves that track only.",
                   "Hold DROP, a THROW and ROLL for three seconds each: on for the whole hold, off on release. (A long press also opens the E16's own functions; if one interferes, make that press a toggle.)",
                   "Shared knobs: on page 1 turn NEXT SONG fully left; on page 6 turn it one step right. The pick moves one step, not jumps. If it jumps, the E16 keeps a value per page: controllers/e16 README, check 4, has the relative fallback.",
                   "Change pages, change to SF MIX and back: Live's MIDI input indicator stays dark.",
                   "Press knob 1 (green) and turn: SLOW ALL moves, the page's knob 1 doesn't. Press again: the page's own.",
                   "Turn NEXT SONG and press it: the song lands on the bar, the rings follow, and a recalled knob moves smoothly from the new value."])
story.append(PageBreak())

# ---------------- 5
chapter("5  THE PAGES", "All fifteen pages: SF SHOW's twelve (a page per track, then PERFORM) and SF MIX's three. Under each knob: the name on the E16's screen, the parameter, where it lands in Live, its address.")
rows = [["PAGE", "KNOBS 1-8", "KNOBS 9-16"]]
for scene, pages in E16.SCENES:
    for n, (title, controls) in enumerate(pages, 1):
        names = [c.name if c is not None else "-" for c in controls]
        rows.append([f"{scene.replace('SF ', '')} {n}: {title}", esc(" / ".join(names[:8])), esc(" / ".join(names[8:]))])
story.append(table(rows, [1.3 * inch, 2.9 * inch, W - 4.2 * inch], pad=2.6))
story.append(P("On each page below: the drawing, what each knob does (the plugin's own description), and how to play it. On every track page 15 is NEXT SONG (press: GO) and 16 BUILD (press: DROP), and knob 1's press swaps it to SLOW ALL.", "smallgrey"))
story.append(PageBreak())
for scene, pages in E16.SCENES:
    for n, (title, controls) in enumerate(pages, 1):
        lead, tips = PAGE_NOTES[title]
        live = [c for c in controls if c is not None]
        many = len({ctl.track for ctl in live if ctl.module not in ("CONTROL", "BUILD")}) > 1 and title not in ("PERFORM", "LEVELS")
        knobs = [["KNOB", "SCREEN", "WHAT IT DOES"]] + [[str(i), ctl.name, esc(knob_desc(ctl, many))] for i, ctl in enumerate(controls, 1) if ctl is not None]
        story += [H2T(f"{scene} {n}  {title}"), P(page_sub(scene, title, controls), "label"), E16Page(title, controls), Spacer(1, 6), P(lead),
                  table(knobs, [0.45 * inch, 0.85 * inch, W - 1.3 * inch], pad=2.4)] + [Spacer(1, 4)] + bullets(tips) + [PageBreak()]

# ---------------- 6
chapter("6  SONGS, PATTERNS AND SCENES", "Each system owns one thing. The ONE owns the notes, SF songs own the sound, the E16 owns where your hands are. A song change is one knob and one press: every SF plugin lands on the same bar.")
story.append(table([["SYSTEM", "RESPONSIBLE FOR"],
    ["OXI ONE projects and patterns", "Notes, rhythms, gates, accents, the pattern progression."],
    ["SF songs (SF CONTROL)", "The sound of every SF plugin and the song's KEY, called from the E16 and recalled on the bar. Sixteen songs, saved with the set."],
    ["E16 pages", "Where your hands are. Pages hold no sound: changing pages changes nothing you hear."],
    ["Live Session scenes", "BRIDGE clips, reference material, rehearsal."],
    ["Live Arrangement", "Recording the performance."]], [2.0 * inch, W - 2.0 * inch], mono_first=False))
story.append(H2T("A SONG CHANGE"))
story.append(SongFlow())
story.append(Spacer(1, 6))
story += bullets(["<b>NEXT SONG</b> (knob 15 of any SF SHOW page) picks the song and does nothing else: a knob knocked in passing never changes the song. SF CONTROL shows NEXT in amber, PRESS GO, until you call it.",
                  "<b>GO</b> (its press) calls it: SF CONTROL decides the bar line once and sends it with the song, so every plugin lands on the same bar whatever order Live processes the tracks in. Pressed up to 30 ms after a bar line, it counts for that bar; later, for the next one. With the transport stopped it lands at once. Every plugin's header shows WAIT BAR in green until it lands.",
                  "GO again, or GO on the song already in force, does nothing. NEXT SONG follows any song change (a turn of SONG on SF CONTROL, STORE TO), so a pick you didn't call gives way.",
                  "A song and a ONE project are independent: any song with any project's patterns. A plugin with nothing stored in that song stays as it is.",
                  "<b>SLOW ALL</b> on SF CONTROL makes a recall glide: at 8 or 16 BAR every SF knob the song stores travels to it over 8 or 16 bars; at OFF it lands on the bar. Menus and switches (a MODEL, an ECHO SPACE) land on the bar either way (GLIDES, chapter 7, has the list)."])
story.append(H2T("SAVING A SONG"))
story += bullets(["Set every plugin how the song should start, then press <b>STORE ALL</b> on SF CONTROL: every SF plugin stores its values into the song in force (SONG).",
                  "<b>Into another song:</b> choose it in <b>STORE TO</b> first, then STORE ALL. That song isn't recalled, so nothing you hear changes; it becomes the song in force (SONG follows) and STORE TO goes back to ACTIVE SONG. Choosing it on SONG instead would recall it and lose what you set.",
                  "If a song change is under way when you STORE TO (WAIT BAR), the change still lands and stays the song in force.",
                  "STORE in a plugin's header stores just that plugin. Nothing that rewrites a song is on the E16."])
story.append(H2T("A TRACK IN THE SET"))
story += numbered(["Turn NEXT SONG (15) to the track's song and press it on the bar before the change: the song lands on the bar.",
                   "Load the track's ONE project and play its patterns in order (SPARSE, MAIN, VARIATION, EXIT); play the sound from the E16.",
                   "Before the next track: BUILD and a drop, a dub throw or a BRIDGE clip covers the change (chapter 7)."])
story.append(PageBreak())

# ---------------- 7
chapter("7  PERFORMANCE", "PERFORM is the arrangement, a track's page is the detail. Very simple note content, all the variation played on knobs: every move below is a knob or a press, on the bar.")
story.append(H2T("THE SET ARC"))
story.append(table([["PHASE", "WHAT CARRIES IT", "WHERE"],
    ["OPENING", "Kick and rumble alone; PAD or VOICE far back. ENERGY knobs left of centre.", "PERFORM; 7 RUMBLE, 10 PAD"],
    ["LOCK-IN", "Hats come in by DENSITY, not by level. One part at a time, eight or sixteen bars apart.", "2 CLOSED HT, 1 KICK"],
    ["BUILD", "BUILD up over 8-16 bars; hats and clap DENSITY up; SPACE DUCK down so the tail fills the bar.", "Knob 16, 2-4, SF MIX SPACE"],
    ["PEAK", "The drop; ENERGY knobs right of centre; the lead or the chord carries the hook; dub throws on the offbeats.", "Press 16, PERFORM, 8 LEAD, SPACE"],
    ["BREAKDOWN", "Kick lane muted on the ONE; LOW ENERGY down; FREEZE a return; VOICE up.", "The ONE, PERFORM, SPACE, 11 VOICE"],
    ["RETURN", "Kick back on the one; a song change lands here (NEXT SONG, then GO).", "Press 16, knob 15, the ONE"],
    ["CLOSE", "ENERGY knobs left; parts out one by one; the last bars on a frozen reverb or BRIDGE.", "PERFORM, SPACE, Live's BRIDGE"]],
    [0.9 * inch, 3.75 * inch, W - 4.65 * inch], mono_first=True))
story.append(P("A starting plan, not a rule: hypnotic sets hold a phase far longer than feels safe.", "smallgrey"))
story.append(H2T("BUILDS AND DROPS"))
story.append(P("BUILD is one knob for a whole-mix build (16, on every SF SHOW page): a rising high-pass, a noise riser, a reverb wash and width. Press it during the build (DROP): the drop lands on the next bar line. Three settings on SF BUILD shape the landing; the drawing shows GAP 1/2, TAIL 1 BAR, DROP DUCK 6 dB, DUCK TIME 1 BAR."))
story.append(DropTimeline())
story.append(Spacer(1, 6))
story += bullets(["<b>GAP</b> (OFF to 1 BAR): everything through BUILD goes silent for that long before the bar line, the whole LIVE BUS with its returns (BRIDGE and DECK play on). Press DROP later than that and the gap starts on the next 1/16.",
                  "<b>TAIL</b> (CUT to 2 BARS): the riser's noise and the wash ring on after the build stops, fading out. Shorter than the GAP, it dies away in the silence; longer, it carries into the drop, DROP DUCK lower.",
                  "<b>DROP DUCK</b> (0-12 dB, off by default) and <b>DUCK TIME</b> (1/4 to 4 BARS): after the drop every SF part with Pocket dips under each kick, whatever its POCKET DEPTH: bass, lead, pad, voice, rumble, both returns. The first kicks stand alone on top of everything that comes back, with less weight than a kick brought back by hand: the bass and rumble around them are gone. A roll into the drop doesn't duck; the duck starts with the first kick on the bar.",
                  "<b>LAND ON</b> (KICK or BAR LINE): KICK, the default, lands the drop where BUILD hears the kick. BUILD measures how late each kick reaches it against Live's beat (the readout under its bar: KICK +80 MS) and moves the gap and the drop by that much: the silence breaks 10 ms before the kick. On Nov 7 the kick reached BUILD 80 ms after Live's beat: the drop opened that early, and the bass, the pads and the last kick's tail came back before the kick, like any other beat. It needs four kicks that agree (KICK --: Live's bar line until then) and keeps the last value through a breakdown with no kick. BAR LINE: 10 ms before Live's bar line, whatever the kick does.",
                  "After a drop the build stays off until BUILD (16) is back below 5 % (display: RETURN KNOB). Bring it down in your own time: nothing re-builds."])
story.append(table([["RECIPE", "GAP", "TAIL", "DROP DUCK", "DUCK TIME", "WHAT IT DOES"],
    ["HARD STOP", "1/4", "CUT", "OFF", "-", "A beat of true silence, then everything on the one."],
    ["BREATH", "1/2", "1/4", "OFF", "-", "The riser rings into the silence and dies before the kick."],
    ["CARRY", "OFF", "1 BAR", "6 dB", "1 BAR", "No silence: the wash runs under the first bar, ducked."],
    ["LONG FALL", "1 BAR", "1/2", "9 dB", "2 BARS", "A full bar of air; the new section breathes for two bars."],
    ["SNAP", "OFF", "CUT", "OFF", "-", "The build snaps off on the bar line; nothing else changes."]],
    [0.95 * inch, 0.45 * inch, 0.5 * inch, 0.75 * inch, 0.75 * inch, W - 3.4 * inch]))
story.append(P("Defaults: GAP OFF, TAIL 1/8, DROP DUCK OFF, DUCK TIME 1 BAR, LAND ON KICK. The drop duck needs KICK REPORTING on SF CONTROL. TAP and STRIP set to ROLE KICK, PERC or DECK don't duck.", "smallgrey"))
story.append(H2T("ENERGY AND MACROS"))
story += bullets(["The ENERGY knobs on PERFORM are the fastest way to move a whole set: small moves, often. The group knobs (12-14) move a section, a track's own knob one part. A negative MACRO 1 amount on LOW darkens the bass while the rest lifts.",
                  "Six macros on SF MIX's MACROS page, each followed by the plugins that subscribe to it (MACRO SRC, MACRO AMT on each plugin). A starting plan:"])
story.append(table([["MACRO", "FOLLOWED BY", "MOVES"],
    ["1 ENERGY", "the group ENERGY plugins, BUILD", "every group's energy at once"],
    ["2 MOOD", "PAD, CHORD, VOICE", "cutoff, envelope and drive; the voice's SIGNAL and ECHO"],
    ["3 DENSITY", "CLOSED HAT, OPEN HAT, CLAP, PERC", "DECAY and DENSITY: the fills"],
    ["4 PULSE", "KICK, RUMBLE", "kick DECAY, PUNCH, DRIVE; rumble SIZE, GRIT, DENSITY"],
    ["5 SPACE", "DUB, SPACE returns", "delay FEEDBACK and MIX; reverb SIZE, DECAY and MIX"],
    ["6 FRACTURE", "LEAD, BASS", "cutoff, envelope, density and drive: smooth to heavy"]], [1.0 * inch, 2.1 * inch, W - 3.1 * inch]))
story.append(P("Start the amounts at 30-50 %. One macro per plugin.", "smallgrey"))
story.append(H2T("DUB"))
story += bullets(["<b>Sends:</b> every track page has the track's two sends (13 into A DUB, 14 into B SPACE). A quarter turn of 9 CHORD's SEND A on the last stab of a phrase is the dub-chord move.",
                  "<b>Throws:</b> SF MIX's SPACE page, hold 1 (DUB THROW) on an offbeat stab or a voice fragment and let go: the repeats carry on. With INPUT THROW ONLY on SF DELAY, the press is the only way in.",
                  "<b>Tape moves:</b> turn DUB TIME while repeats are running: the head slews and the pitch bends. FREEZE (2) then TIME: the frozen loop bends like tape speed.",
                  "<b>Repeats that grow:</b> DUB FEEDBACK past 100 % saturates instead of exploding; the mapping stops at 70 % until you widen it.",
                  "<b>Space that breathes:</b> SPACE DUCK (8) 30-60 % lets the reverb bloom in the gaps between kicks. FREEZE (6) holds a chord in the room for a breakdown."])
story.append(H2T("FILLS AND TRANSITIONS"))
story += bullets(["<b>Hats:</b> 2 CLOSED HT page, DENSITY (10) up with ECHO SPACE (5) at 1/16: 1/8 hats become 1/16s, then 1/32 rolls past 50 %.",
                  "<b>Drum roll:</b> SF MIX's MACROS page, hold ROLL (9) and turn LENGTH under the same finger: 1/4 down to 1/32. PITCH (10) makes it fall.",
                  "<b>Between tracks:</b> turn NEXT SONG (15) during a build or a breakdown and press it on the bar before the change; load the next ONE project when you like, the two are independent. With SLOW ALL set, the knobs glide into the new song; a MODEL change in it still lands on the bar, so cover it with the build or the drop.",
                  "<b>BRIDGE:</b> bring BRIDGE up and LIVE BUS down in Live to hand the room to a clip while you regroup. BRIDGE still needs Live: it is not a backup for a computer failure."])
story.append(H2T("GLIDES"))
story += bullets(["<b>A travelling knob's ring turns green</b> and walks with the sound to where you set it, then turns white. Turn it while it walks: the ring shows your hand, then green again. (SF CONTROL's E16 RINGS and the scene's script; nothing else depends on them.)",
                  "<b>SLOW ALL</b> (press knob 1 of any SF SHOW page, turn, press again; or MACROS 7 on SF MIX) sets how long your next moves take, in every SF plugin. OFF: they land at once. 8 BAR: a quarter turn of LEAD CUTOFF becomes an eight-bar rise that plays itself.",
                  "<b>A move keeps its length.</b> Start a 16-bar rise on the pad, set SLOW ALL to OFF, and make quick moves elsewhere: the pad keeps rising. Turn a travelling knob and it sets off again from where it has got to.",
                  "<b>SLOW ALL changed in the same instant as a move</b> (one audio buffer, a few ms): the move takes the length from before the change, in every plugin alike. Set SLOW ALL, then move.",
                  "<b>Song changes follow it, knob by knob.</b> SLOW ALL at 16 BAR before GO: every SF knob the song stores travels to the new song over 16 bars; OFF: it lands on the bar. What does not glide (the table) changes on the bar. It is a morph of the knobs, not a crossfade between two whole sounds.",
                  "A plugin's own SLOW, set in its window, wins over SLOW ALL; leave it at CONTROL."])
# What SLOW moves, for every knob and press of the scenes: drawn from layout.py and the plugins'
# parameters (specs.json), so it can't drift from the set. Every place on the scenes lands in one row.
def spec_of(c):
    return next(q for q in E16.SPECS[c.module]["params"] if q["id"] == c.pid)
glides, menus, presses, live = [], [], [], []
for title, _ in E16.PAGES:
    for i, c in enumerate(E16.encoders(title), 1):
        if c is None:
            continue
        if not c.module:
            live.append({"volume": "VOLUMES", "send A": "SEND A", "send B": "SEND B"}[c.mixer])
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
            presses.append("ENERGY TO THE CENTRE" if p.mode == "CENTRE" else p.name)
names = lambda rows: ", ".join(dict.fromkeys(rows))
detail_names = sorted({f"{c.track.split(' ', 1)[-1]} {E16.param_name(c.module, c.pid)}" for t, _ in E16.PAGES for c in E16.encoders(t)
                       if c is not None and c.module and spec_of(c)["slow"] and not spec_of(c)["primary"]})
story.append(CondPageBreak(2.4 * inch))
story.append(P("<b>WHAT SLOW MOVES</b> (every knob and press of SF SHOW and SF MIX; drawn from the scenes and the plugins)", "smallgrey"))
story.append(table([["ON THE E16", "WITH SLOW / SLOW ALL"],
    [f"Every SF knob, {len(set(glides))} of them: each plugin's top row, and the details on the scenes too ({esc(', '.join(detail_names))})", "Travels over the bars set"],
    [esc(names(menus)), "Menus: at once, on the next buffer"],
    [esc(names(presses)), "Presses: at once, as long as held or until pressed again"],
    [esc(names(live)), "Live's own volumes and sends: never glide"],
    ["Not on the E16: MODEL (an instrument's sound)", "At once. What sounds releases on the old model; the keys still down play again on the new one (a held chord goes on)"],
    ["Not on the E16: CHORD, a dropped sample, any other menu or switch", "At once: CHORD from the next note; a sample replaces the old one, notes sounding included"]],
    [3.6 * inch, W - 3.6 * inch], mono_first=False))
story.append(H2T("WHEN SOMETHING GOES WRONG"))
story.append(table([["WHAT HAPPENED", "DO THIS"],
    ["A runaway: feedback, a stuck note, a wrong sound in front of the crowd", "The interface's output down first: the room goes quiet, REC keeps recording. Then PANIC, held in SF CONTROL's window: every SF plugin fades out over 5 ms and clears; let go, it comes back without a click."],
    ["A plugin header shows FAULT in red", "It caught an invalid value or a runaway and reset itself; it carries on. Note what was playing."],
    ["The wrong song landed", "Turn NEXT SONG (15) to the right one and press it: it lands on the next bar."],
    ["A knob seems dead", "Another page or scene is up. Or its range in Live's MIDI Mappings is narrow."],
    ["The ONE drifts or stops", "Live is the clock: stop and start Live, the ONE follows. Check its USB cable before the show, not during it."],
    ["The ONE doesn't follow Live's tempo", "Start from Live, not the ONE: Live sends clock only while it plays. The ONE's header shows USB (stopped: Shift + Stop until it does). Sync on OXI ONE MKII (Port 1)'s output and on no other port. Hold Shift + Back for 2 s: the ONE's MIDI monitor shows the clock arriving while Live plays. Still nothing: Sync on Port 2's output instead, never both."]],
    [2.3 * inch, W - 2.3 * inch], mono_first=False))
story.append(PageBreak())

# ---------------- 8
chapter("8  BEFORE THE SHOW", "Run through this list at every soundcheck. Each line is a thing that once went wrong somewhere.")
story.append(H2T("CHECKLIST"))
story += numbered(["Every SF plugin from the same build: SF CONTROL shows BUS SHARED and KICK REPORTING, and the pump lands on the kick.",
                   "NEXT SONG and GO recall the song: every track on the same bar, and a recalled knob is yours the moment you turn it.",
                   "The interface's output down: the room silent, REC still recording.",
                   "The PA pair carries PA and nothing else; Cue reaches the headphones and not the PA.",
                   "DROP, a THROW and ROLL each held for three seconds: on for the whole hold, off on release.",
                   "The shared E16 knobs don't jump (chapter 4); changing pages or scenes sends nothing.",
                   "SLOW ALL where you want it for the opening (knob 1 pressed; OFF for direct moves), then back to the page's own knob.",
                   "A drop rehearsed with the night's GAP, TAIL and DROP DUCK settings.",
                   "A minute recorded on REC with a deep ROOM EQ cut: the cut isn't in the recording.",
                   "Soundcheck the busiest passage: SF LIMIT INPUT for 1-3 dB of limiting there, AUTO on, DOWN ONLY.",
                   "The room mics on ROOM: IN L and IN R move on its SF CAPTURE, the same gain on both, well under the top at the loudest."])
story.append(H2T("CAPTURING THE ROOM"))
story.append(P("SF TERROIR can take the place you play in as a PLACE, without leaving Live: SF CAPTURE on ROOM plays the sweep to a speaker on the headphone out and records the room mics, and TERROIR turns the take into a place. SF TERROIR has to be in the set (on B SPACE in place of SF REVERB: controllers/e16 README, SF TERROIR ON B SPACE). At soundcheck or after the set: the transport stopped."))
story += numbered(["A speaker on the headphone out (a MiniRig on its line in), where the sound should come from, aimed at a wall or into the room. In TotalMix the mics must not reach the headphone out, or the speaker plays them and feeds back.",
                   "The mics where the audience will be, or where the place should be heard from (SF TERROIR's IN THE FIELD, MICS). The speaker as loud as it goes without buzzing; the headphone level sets it, SWEEP LEVEL on SF CAPTURE is only for headroom.",
                   "Stop the transport. In SF TERROIR click <b>CAPTURE</b> (amber), then the slot to fill. 46 s: keep the room quiet while the status line counts.",
                   "TERROIR analyses the take: the slot takes the name ROOM and the date and time; the status line should read SWEEP x3 and 30 dB or more over the noise. Under 30 dB: the speaker louder or the mics closer; THE MICS CLIPPED: the speaker or the gain down. Then again. THE SWEEP DIDN'T COME BACK: nothing reached the mics (TROUBLESHOOTING); the slot stays as it was.",
                   "PLACE is stored per song, so a recall goes back to the slot the song was stored with: STORE the songs that should play the new place. The take stays in Music/SF Live Suite/TERROIR/Captures."])
story.append(P("CAPTURE again (it reads STOP) ends a take early; so do starting the transport, PANIC and Live's audio restarting, even before the sweep has started or while the take is being written. The sweep fades out in 10 ms and nothing is stored. The PA stays silent: ROOM goes to the headphone out only.", "smallgrey"))
story.append(H2T("RECORDING THE SET"))
story += bullets(["Press RESET on SF LIMIT, arm the instrument tracks, REC and ROOM, Automation Arm on, Arrangement record, play. The stereo recording on REC is the set, the one on ROOM the room mics for the mix; the MIDI and automation are for editing and rehearsal.",
                  "Collect All and Save after the show. SF samples are inside the set; DECK tracks are referenced by path, so keep the files where they are."])
story.append(H2T("TROUBLESHOOTING"))
story.append(table([["SYMPTOM", "CHECK"],
    ["An instrument doesn't play", "MIDI From: its ONE port and channel, never All Ins. The ONE's sequencer output matches: port A, the track's number (3 OPEN HAT on 3A)."],
    ["Every instrument plays every part", "A track's MIDI From is All Ins or All Channels."],
    ["Nothing ducks", "SF CONTROL: BUS SHARED and KICK REPORTING. The kick must be SF KICK."],
    ["Pump early or late", "Live buffer 512 or less; then POCKET OFFSET by ear. (The ONE's clock moves every part alike: not this.)"],
    ["The drop opens before the kick", "SF BUILD: LAND ON KICK, and its readout shows a value (KICK --: four kicks haven't agreed yet; GAP silences the beat before)."],
    ["Songs don't change", "Turn NEXT SONG (15), then press it (GO): SF CONTROL shows NEXT in amber, PRESS GO, until you do. The song must be stored (SONG nn EMPTY otherwise). SF CONTROL from this build: an older one has no NEXT SONG."],
    ["E16 rings don't follow", "Remote on for E16 Port 1's output."],
    ["An E16 press does nothing", "A scene file from before Nov 8 is loaded: load the current SF SHOW and SF MIX. A toggle that flips back: Remote is on for two E16 ports."],
    ["No green on a travelling knob", "SF CONTROL shows E16 RINGS PORT 3 in green? If not, the E16 isn't found: try PORT 1. Load the scenes from the repository again (the script is in them). Use the VST3s."],
    ["Knob 1 swapped (green) changes nothing", "SLOW ALL isn't mapped in this set: Cmd-M, click SLOW ALL on SF CONTROL, press knob 1 and turn it. Cmd-M off, save."],
    ["Knob 1 moves its own parameter and SLOW ALL together", "Its push was changed in the App: load SF SHOW.oxie16 again (knob 1's push must be Swap Destination)."],
    ["Every move glides (or none does)", "SLOW ALL is saved with the set: press knob 1 and set it. A plugin whose own SLOW isn't CONTROL keeps its own."],
    ["An E16 knob jumps when touched", "Auto Transmit is on, or the E16 keeps a value per page: chapter 4, check 3."],
    ["CONTROL shows LOCAL ONLY", "The plugins come from different builds: rebuild and install the whole suite, restart Live."],
    ["A return doubles the dry signal", "DRY off on SF DELAY or SF REVERB on the returns."],
    ["SF TERROIR won't capture", "Its status line says why: the transport playing; NO SF CAPTURE (ROOM's device off); a take already running."],
    ["A capture says THE SWEEP DIDN'T COME BACK, or under 30 dB", "The sweep didn't reach the speaker or the mics: the speaker on the headphone out, ROOM on and up; Input Config 1/2 a stereo pair, Monitor In. Then louder or closer. The slot stays as it was."],
    ["A capture says NO SWEEP IN THE RIGHT CHANNEL", "That mic heard nothing (its cable, its gain): the place is mono. Fix it, capture again."],
    ["A capture says SF CAPTURE STOPPED ANSWERING", "ROOM's SF CAPTURE was removed or switched off. Put it back, capture again: nothing was stored."]], [1.9 * inch, W - 1.9 * inch], mono_first=False))
story.append(PageBreak())

# ---------------- appendix
story.append(H1("APPENDIX"))
story.append(Rule())
story.append(H2T("E16 ADDRESS MAP (SF SHOW, SF MIX)"))
story.append(P("Every address of the two scenes once, by channel: the Live target, the mapping, and where the knob is on the E16 (an SF SHOW page is named for its track; LEVELS, SPACE and MACROS are SF MIX's). The same list is controllers/e16/sf-show-map.csv."))
rows = [["CH", "MESSAGE", "LIVE TARGET", "MAPPING", "ON THE E16"]]
for (msg, ch, n), (target, places, kind, rng, item) in sorted(E16.addresses().items(), key=lambda kv: (kv[0][1], kv[0][0], kv[0][2])):
    m = f"CC {n}" if msg == "CC" else f"NOTE {n} ({E16.note_name(n)})"
    how = {"TURN": "absolute", "HOLD": "press: hold", "TOGGLE": "press: toggle"}[kind] + (f", {rng}" if rng else "")
    rows.append([str(ch), m, esc(target), esc(how), esc(E16.where(places, getattr(item, "second", False)))])
story.append(table(rows, [0.35 * inch, 1.05 * inch, 2.55 * inch, 1.25 * inch, W - 5.2 * inch], mono_first=False, pad=2.3))

doc = Doc(os.path.join(ROOT, "docs", "SF-SHOW-Operators-Manual.pdf"), title="SF SHOW Operator's Manual",
          header="SF SHOW  OPERATOR'S MANUAL", footer="V0.2  NOV 2026", cover_title="SF SHOW", cover_sub="OPERATOR'S MANUAL",
          cover_lines=("THE SHOW TEMPLATE, THE E16 SCENES AND THE OXI ONE MKII",
                       "ONE LIVE SET   TWO E16 SCENES   ONE SEQUENCER", "", "VERSION 0.2   NOVEMBER 2026", "STRANGE FREQUENCIES"),
          cover_note="E16 PAGES, PRESSES AND ADDRESSES IN THIS MANUAL ARE GENERATED FROM CONTROLLERS/E16/LAYOUT.PY.")
doc.multiBuild(story)
print("ok")
