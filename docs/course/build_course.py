"""Build the lesson player.

    python3 build_course.py                       # the page only (uses audio already rendered)
    python3 build_course.py --audio --model M.onnx --voices V.bin

Writes site/index.html, and with --audio site/audio/: one MP3 per step (the voice on that step) and
one per lesson (the whole lesson, with room between steps). Narration is rendered by tts.py (Kokoro,
run locally) into a cache, so a rebuild renders only the steps whose words changed.
"""
import argparse
import json
import os
import subprocess
import sys

import soundfile as sf
import numpy as np

import course
from lessons import COURSE

HERE = course.HERE
SITE = os.path.join(HERE, "site")
AUDIO = os.path.join(SITE, "audio")
CACHE = os.path.join(HERE, ".voice-cache")
SR = 24000


def keys():
    for L in COURSE["lessons"]:
        for i, st in enumerate(L["steps"], 1):
            yield L, st, f"{L['id']}-{i:02d}"


def render(model, voices, voice, speed):
    os.makedirs(CACHE, exist_ok=True)
    jobs = [{"key": k, "text": course.speak(st["say"])} for _, st, k in keys()]
    path = os.path.join(CACHE, "jobs.json")
    json.dump(jobs, open(path, "w"), indent=1)
    shards = max(1, os.cpu_count() or 1)
    procs = [subprocess.Popen([sys.executable, "-I", os.path.join(HERE, "tts.py"), model, voices, path, CACHE,
                               "--voice", voice, "--speed", str(speed), "--shard", f"{i}/{shards}"], cwd=HERE)
             for i in range(shards)]
    assert all(p.wait() == 0 for p in procs), "narration render failed"


LEAD, TAIL = 0.25, 0.35     # silence around each clip: phones clip the first moments of playback


def mp3(wav, out, kbps=56):
    """Encode, 4 dB louder (the voice has to carry over a rig playing), peaks held at -1 dB."""
    subprocess.run(["ffmpeg", "-loglevel", "error", "-y", "-i", wav, "-ac", "1", "-ar", str(SR),
                    "-af", "volume=4dB,alimiter=limit=0.89:level=false", "-codec:a", "libmp3lame", "-b:a", f"{kbps}k", out], check=True)


def padded(wav):
    a, sr = sf.read(wav, dtype="float32")
    assert sr == SR, (wav, sr)
    return np.concatenate([np.zeros(int(SR * LEAD), np.float32), a, np.zeros(int(SR * TAIL), np.float32)])


def encode():
    """Step MP3s and lesson tracks from the cached WAVs. Returns {key: seconds}."""
    os.makedirs(AUDIO, exist_ok=True)
    durs = {}
    lessons = {}
    for L, st, k in keys():
        wav = os.path.join(CACHE, k + ".wav")
        if not os.path.exists(wav):
            continue
        out = os.path.join(AUDIO, k + ".mp3")
        if not os.path.exists(out) or os.path.getmtime(out) < os.path.getmtime(wav):
            tmp = os.path.join(CACHE, k + ".pad.wav")
            sf.write(tmp, padded(wav), SR)
            mp3(tmp, out)
            os.remove(tmp)
        info = sf.info(wav)
        durs[k] = info.frames / info.samplerate + LEAD + TAIL
        lessons.setdefault(L["id"], []).append((wav, st.get("wait", 4.0)))
    for L in COURSE["lessons"]:
        parts = lessons.get(L["id"])
        if not parts or len(parts) != len(L["steps"]):
            continue
        chunks = [np.zeros(int(SR * 0.6), dtype=np.float32)]
        marks = []
        for wav, gap in parts:
            marks.append(round(sum(len(c) for c in chunks) / SR, 2))     # where this step starts in the track
            chunks += [padded(wav), np.zeros(int(SR * gap), dtype=np.float32)]
        durs[L["id"] + ":marks"] = marks
        whole = os.path.join(CACHE, f"lesson-{L['num']}.wav")
        sf.write(whole, np.concatenate(chunks), SR)
        mp3(whole, os.path.join(AUDIO, f"lesson-{L['num']}.mp3"), kbps=48)
        durs[L["id"]] = sum(len(c) for c in chunks) / SR
    return durs


def page(durs):
    data = json.loads(json.dumps(COURSE))
    data["e16"] = course.e16_json()
    for L in data["lessons"]:
        for i, st in enumerate(L["steps"], 1):
            k = f"{L['id']}-{i:02d}"
            st.pop("say", None)
            st.pop("wait", None)
            if k in durs:
                st["audio"] = f"audio/{k}.mp3"
                st["dur"] = round(durs[k], 1)
        if L["id"] in durs:
            L["audio"] = f"audio/lesson-{L['num']}.mp3"
            L["duration"] = round(durs[L["id"]])
            L["marks"] = durs[L["id"] + ":marks"]
    html = open(os.path.join(HERE, "player.html")).read()
    blob = json.dumps(data, ensure_ascii=False, separators=(",", ":")).replace("</", "<\\/")
    assert "/*COURSE_JSON*/null" in html
    os.makedirs(SITE, exist_ok=True)
    open(os.path.join(SITE, "index.html"), "w").write(html.replace("/*COURSE_JSON*/null", blob))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--audio", action="store_true")
    ap.add_argument("--model")
    ap.add_argument("--voices")
    ap.add_argument("--voice", default="af_heart")
    ap.add_argument("--speed", type=float, default=0.95)
    a = ap.parse_args()
    # Every narration must read cleanly before anything is built.
    for _, st, k in keys():
        course.speak(st["say"])
    if a.audio:
        render(a.model, a.voices, a.voice, a.speed)
    durs = encode() if os.path.isdir(CACHE) else {}
    page(durs)
    n = sum(len(L["steps"]) for L in COURSE["lessons"])
    talk = sum(v for k, v in durs.items() if "-" in k and ":" not in k)
    print(f"{len(COURSE['lessons'])} lessons, {n} steps, {talk / 60:.1f} min of voice -> {os.path.relpath(SITE, os.getcwd())}/index.html")


if __name__ == "__main__":
    main()
