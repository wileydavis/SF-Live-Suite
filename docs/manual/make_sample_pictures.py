#!/usr/bin/env python3
"""The manual's two pictures of a sample's part (SAMPLES, chapter 2): SF KICK's display with a one-shot
(END at 62 %, FADE OUT 150 ms) and SF VOICE's phrase (START on the second word, END in the last, the
fades), written to docs/snapshots/SAMPLE_KICK.png and SAMPLE_VOICE.png at twice the strip's scale.
The one-shot and the phrase are made here (synthetic), so the pictures come out the same every time.

    python3 docs/manual/make_sample_pictures.py [path to SFSnapshot]

SFSnapshot shares the SF BUS with SFTests: don't run the two at once.
"""
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SNAP = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "build-test", "SFSnapshot_artefacts", "Release", "SFSnapshot")
SR = 48000


def write(path, xs):
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(struct.pack("<hh", int(max(-1.0, min(1.0, x)) * 32000), int(max(-1.0, min(1.0, x * 0.95)) * 32000)) for x in xs))


def kick():
    """0.9 s: a sweep from 160 to 48 Hz, a click, a long tail with a little noise."""
    random.seed(3)
    ph, xs = 0.0, []
    for i in range(int(0.9 * SR)):
        t = i / SR
        ph += 2 * math.pi * (48 + 112 * math.exp(-t / 0.03)) / SR
        x = 0.9 * math.sin(ph) * math.exp(-t / 0.28) + (0.5 * (random.random() * 2 - 1) * math.exp(-t / 0.004) if t < 0.02 else 0.0)
        xs.append(x + 0.02 * (random.random() * 2 - 1) * math.exp(-t / 0.5))
    return xs


def phrase():
    """3 s: five words (a few harmonics under a syllable's attack and fall), gaps between them."""
    random.seed(5)
    xs = [0.0] * int(3.0 * SR)
    for st, ln in [(0.15, 0.32), (0.62, 0.30), (1.05, 0.45), (1.75, 0.28), (2.25, 0.5)]:
        f0 = 120 + 40 * random.random()
        for i in range(int(ln * SR)):
            t, k = i / SR, int(st * SR) + i
            if k >= len(xs):
                break
            a = min(1.0, t / 0.012) * math.exp(-1.6 * t / ln)
            v = sum(math.sin(2 * math.pi * f0 * h * t) / h for h in (1, 2, 3, 5))
            xs[k] += 0.4 * a * v + 0.03 * a * (random.random() * 2 - 1)
    return xs


def main():
    out = os.path.join(ROOT, "docs", "snapshots")
    with tempfile.TemporaryDirectory() as tmp:
        for name, module, file, xs, settings in [
            ("SAMPLE_KICK.png", "KICK", "one shot.wav", kick(), "model=6,smp_end=0.62,smp_fade_out=150"),
            ("SAMPLE_VOICE.png", "VOICE", "phrase.wav", phrase(), "model=0,smp_start=0.205,smp_end=0.82,smp_fade_in=40,smp_fade_out=120"),
        ]:
            wav = os.path.join(tmp, file)
            write(wav, xs)
            pics = os.path.join(tmp, module)
            os.makedirs(pics)
            env = dict(os.environ, SFSNAP_DISPLAY="1", SFSNAP_SAMPLE=wav, SFSNAP_SET=settings)
            subprocess.run([SNAP, pics, "2", module], env=env, check=True, stdout=subprocess.DEVNULL)
            shutil.copyfile(os.path.join(pics, module + ".png"), os.path.join(out, name))
            print(os.path.join("docs", "snapshots", name))


if __name__ == "__main__":
    main()
