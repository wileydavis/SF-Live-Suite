"""Render the course narration with Kokoro (an open speech model, run locally).

    python3 -I tts.py MODEL.onnx VOICES.bin JOBS.json OUTDIR [--voice af_heart] [--speed 0.95] [--shard 0/2]

JOBS.json is a list of {"key": ..., "text": ...}. Each job becomes OUTDIR/<key>.wav at 24 kHz.
A job whose WAV exists and whose text is unchanged (OUTDIR/<key>.txt) is skipped, so a rebuild
renders only what changed. The model files are data: they are passed in, never imported, and this
script is run with -I so nothing is loaded from the working directory.
"""
import argparse
import json
import os
import sys
import time

import onnxruntime as rt
import soundfile as sf
from kokoro_onnx import Kokoro


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("model")
    ap.add_argument("voices")
    ap.add_argument("jobs")
    ap.add_argument("outdir")
    ap.add_argument("--voice", default="af_heart")
    ap.add_argument("--speed", type=float, default=0.95)
    ap.add_argument("--shard", default="0/1", help="i/n: render every n-th job from the i-th (one process per core)")
    ap.add_argument("--threads", type=int, default=1)
    a = ap.parse_args()
    shard, shards = (int(x) for x in a.shard.split("/"))
    jobs = json.load(open(a.jobs))[shard::shards]
    os.makedirs(a.outdir, exist_ok=True)
    todo = []
    for j in jobs:
        sig = f"{a.voice}|{a.speed}|{j['text']}"
        wav, txt = (os.path.join(a.outdir, j["key"] + e) for e in (".wav", ".txt"))
        if os.path.exists(wav) and os.path.exists(txt) and open(txt).read() == sig:
            continue
        todo.append((j, wav, txt, sig))
    print(f"{len(todo)} of {len(jobs)} to render", file=sys.stderr)
    if not todo:
        return
    opts = rt.SessionOptions()
    opts.intra_op_num_threads = a.threads
    opts.inter_op_num_threads = 1
    k = Kokoro.from_session(rt.InferenceSession(a.model, sess_options=opts, providers=["CPUExecutionProvider"]), a.voices)
    t0 = time.time()
    total = 0.0
    for n, (j, wav, txt, sig) in enumerate(todo, 1):
        samples, sr = k.create(j["text"], voice=a.voice, speed=a.speed, lang="en-us")
        sf.write(wav + ".part.wav", samples, sr)
        os.replace(wav + ".part.wav", wav)          # never a half-written WAV for the encoder
        open(txt, "w").write(sig)
        total += len(samples) / sr
        print(f"[{a.shard} {n}/{len(todo)}] {j['key']}: {len(samples) / sr:.1f}s  (elapsed {time.time() - t0:.0f}s)", file=sys.stderr, flush=True)
    print(f"rendered {total:.0f}s of speech in {time.time() - t0:.0f}s", file=sys.stderr)


if __name__ == "__main__":
    main()
