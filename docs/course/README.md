# Lessons: BUILD THE FIRST SET

A course for learning the suite by building with it. Nine lessons take you from an empty ONE project to
three songs and a recorded thirty-minute set. They are made to be read on a phone propped by the
laptop while your hands are on Live, the OXI One and the E16, with a spoken guide for every step.

| LESSON | WHAT YOU DO |
|---|---|
| 1 SET UP | Live's settings, SF SHOW saved as your set (MIDI From already in it), the E16 scene, the ONE on Live's clock and tempo. Once only. |
| 2 THE KICK | An empty ONE project, four kicks on lane 1, the KICK page; SONG 01 stored. |
| 3 THE LOW END | RUMBLE under the kick; LOW ENERGY and the volumes on rows 1-2. |
| 4 THE DRUMS | Closed, open and clap lanes; DENSITY turns eighths into sixteenths. |
| 5 THE MUSIC | One bass note, one stab, KEY A, the dub delay and a throw. |
| 6 THE SECTIONS | SPARSE, MAIN, VARIATION, EXIT as pattern slots 1-4; sequencer 8's Program Change calls the song. |
| 7 A SECOND SONG | SONG 02 with STORE TO, a duplicated project, an acid line in F, SLOW ALL. |
| 8 A THIRD SONG | SONG 03 in G: a field recording as a granular texture, a spoken phrase out of the static. |
| 9 PLAY THE SET | Song changes with SLOW ALL, the kick lane muted, thirty minutes recorded. |

## Files

| FILE | WHAT |
|---|---|
| `lessons.py` | The lessons: every step's screen text and narration. |
| `course.py` | Steps, E16 and ONE diagrams, and the checks: every E16 knob a step names is looked up in `controllers/e16/layout.py`, every parameter in `docs/manual/specs.json`, every preset in `Source/modules/Presets.cpp`. A lesson that names a knob, parameter or preset that isn't there doesn't build. |
| `player.html` | The phone player: one step per screen, the voice on each step, E16 and ONE diagrams, timers for the drills, progress kept on the phone. |
| `tts.py` | Renders the narration with Kokoro, an open speech model run locally. |
| `build_course.py` | Builds `site/index.html` and, with `--audio`, `site/audio/`: a clip per step and one track per lesson. |

## Build

    python3 build_course.py                      # the page, with any audio already rendered
    pip install kokoro-onnx soundfile            # once, for the voice
    python3 build_course.py --audio --model kokoro-v1.0.int8.onnx --voices voices-v1.0.bin

The model files are the `model-files-v1.0` release of github.com/thewh1teagle/kokoro-onnx. Keep them
in their own folder and pass their paths. Narration is cached in `.voice-cache/` by its text, so a
rebuild renders only the steps whose words changed. The voice is `af_heart` at 0.95 speed
(`--voice`, `--speed`). `site/audio/` and `.voice-cache/` are not committed.

OXI One procedures follow OXI's MkII manual v2.0a (firmware 2.2.10 and later). Where the manual is
silent (whether a project load sends its Program Change, whether the ONE counts programs from 0 or
1, the Global Quantization options), the lessons test it on the hardware and say what to do either way.
