"""The lesson toolkit: steps, E16 and ONE diagrams, and the checks that tie the lessons to the rig.

Every E16 knob a lesson names is looked up in controllers/e16/layout.py (the scene), every plugin
parameter in docs/manual/specs.json and every factory preset in Source/modules/Presets.cpp, so a
lesson can't point at a knob, a parameter or a preset that isn't there. lessons.py holds the content.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(REPO, "controllers", "e16"))
import layout as E16  # noqa: E402

SPECS = E16.SPECS
WHERE = ("START", "LAPTOP", "ONE", "E16", "LISTEN", "PLAY")
PAGES = [t for t, _ in E16.PAGES]


def _presets():
    src = open(os.path.join(REPO, "Source", "modules", "Presets.cpp")).read()
    out = {}
    for m in re.finditer(r"std::vector<Preset>\s+(\w+)\s*\(\)\s*\{(.*?)\n\}", src, re.S):
        out[m.group(1).upper()] = re.findall(r'\{\s*"([^"]+)"\s*,', m.group(2))
    return out


PRESETS = _presets()


def preset(module, name):
    """'GROUP › NAME' for a factory preset, checked against the plugin's list."""
    assert name in PRESETS.get(module, []), f"{module} has no factory preset {name!r}"
    return name.replace("/", " › ", 1)


def param(module, pid):
    """A plugin parameter's name as its window shows it, checked against the plugin."""
    return E16.param_name(module, pid)


def choice(module, pid, value):
    """A menu value, checked against the parameter's choices."""
    p = next(x for x in SPECS[module]["params"] if x["id"] == pid)
    assert value in p["choices"].split("|"), f"{module} {pid} has no choice {value!r}"
    return value


# ------------------------------------------------------------------ the E16
def page_no(title):
    return PAGES.index(title) + 1


def _controls(page):
    return E16.encoders(page)          # 16 turns, knob 1 first


def knob(page, which):
    """The knob number (1-16) on `page` for `which`: a number, or a control's label as the manual
    names it ('KICK VOLUME', 'CLOSED DENSITY', 'DECAY'), or a parameter id on the page's plugin."""
    if isinstance(which, int):
        assert 1 <= which <= 16
        return which
    hits = [i for i, c in enumerate(_controls(page), 1) if which in (c.label, c.name, c.pid)]
    assert len(hits) == 1, f"page {page}: {which!r} matches knobs {hits}"
    return hits[0]


def screen_name(page, n):
    return _controls(page)[n - 1].name


def K(which, act=None, page=None, colour=None, name=None):
    """One knob in a diagram: act is 'left', 'right', 'turn', 'press' or 'hold'."""
    assert act in (None, "left", "right", "turn", "press", "hold"), act
    return dict(which=which, act=act, page=page, colour=colour, name=name)


def e16(page, *knobs, foot=None, top_only=False):
    """An E16 diagram: the page as the E16 shows it, the named knobs lit."""
    assert page in PAGES, page
    out = []
    for k in knobs:
        n = knob(k["page"] or page, k["which"])
        d = {"n": n}
        if k["act"]:
            d["act"] = k["act"]
        if k["colour"]:
            d["colour"] = k["colour"]
        if k["name"]:
            d["name"] = k["name"]
        out.append(d)
    d = {"page": page, "knobs": out}
    if foot is not None:
        d["foot"] = foot
    if top_only:
        d["topOnly"] = True
    return d


SLOW_ALL = K(1, "press", colour="green", name="SLOW ALL")


def e16_json():
    """The scene for the player: rows 1-2 and every page's rows 3-4, as the E16's screen names them."""
    col = lambda c: "blue" if c.colour == E16.LIGHT_BLUE else "white"
    return {"top": [{"n": i, "name": c.name, "colour": col(c)} for i, c in enumerate(E16.TOP, 1)],
            "pages": [{"title": t, "knobs": [{"n": i, "name": c.name, "colour": col(c)} for i, c in enumerate(b, 9)]} for t, b in E16.PAGES]}


# ------------------------------------------------------------------ the ONE
def one(title, rows, right="", foot=""):
    """A ONE grid: rows = [(row 1-8 from the bottom, label, steps 1-16, ties)]."""
    rr = []
    for r in rows:
        row, label, steps = r[0], r[1], r[2]
        ties = r[3] if len(r) > 3 else []
        assert 1 <= row <= 8 and all(1 <= s <= 16 for s in list(steps) + list(ties)), r
        rr.append({"row": row, "label": label, "steps": list(steps), "ties": list(ties)})
    return {"title": title, "rows": rr, "right": right, "foot": foot}


QUARTERS = [1, 5, 9, 13]
EIGHTHS = [1, 3, 5, 7, 9, 11, 13, 15]
OFFBEATS = [3, 7, 11, 15]
BACKBEAT = [5, 13]


# ------------------------------------------------------------------ steps and lessons
def S(where, title, lines, say, **kw):
    assert where in WHERE, where
    assert len(title) <= 44, f"title too long: {title}"
    allowed = {"path", "table", "e16", "one", "timer", "listen", "why", "done", "sub", "wait"}
    assert set(kw) <= allowed, set(kw) - allowed
    if isinstance(lines, str):
        lines = [lines]
    d = {"where": where, "title": title, "lines": lines, "say": say}
    d.update({k: v for k, v in kw.items() if v is not None})
    return d


def L(num, title, summary, time, steps, checks, end_text):
    return {"num": num, "id": f"l{num}", "title": title, "summary": summary, "time": time,
            "steps": steps, "checks": checks, "endText": end_text}


# ------------------------------------------------------------------ narration
_WORDS = [
    (r"\bSF\b", "S F"), (r"\bE16\b", "E sixteen"), (r"\bOXI\b", "Oxy"), (r"\bONE\b", "One"),
    (r"\bPA\b", "P.A."), (r"\bkHz\b", "kilohertz"), (r"\bdB\b", "decibels"), (r"(\d) ?%", r"\1 percent"),
    (r"\bms\b", "milliseconds"), (r"\bCmd\b", "Command"), (r"›", ","), (r"\bLFO\b", "L F O"),
    (r"\b303\b", "three-oh-three"), (r"\b909\b", "nine-oh-nine"), (r"\b808\b", "eight-oh-eight"),
]


def speak(text):
    """Narration as the voice should read it."""
    t = " ".join(text.split())
    for a, b in _WORDS:
        t = re.sub(a, b, t)
    # Readings that come out wrong: a slash, a zero-led number, 'of A' (said as the article).
    assert "/" not in t, f"say it in words: {t[:80]}"
    assert not re.search(r"\b0\d", t), f"say the number in words: {t[:80]}"
    assert not re.search(r"\bof A\b|\bA (minor|major)\b", t), f"'A' reads as the article: {t[:80]}"
    # The letter A before a lowercase word reads as the article: give it a comma ('port A, channel one').
    for m in re.finditer(r"(\b(?:port|ports|in|to|key|Key|track|output|one|two|three|four|sixteen)[ ,]+)A (?=[a-z])", t):
        raise AssertionError(f"'A' will read as the article, add a comma: ...{t[max(0, m.start() - 20):m.end() + 20]}...")
    return t
