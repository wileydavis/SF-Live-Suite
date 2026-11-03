# Text for the SF Live Suite reference manual. Control ranges and defaults come from the code
# (SFTests --dump); everything written here is the explanation.

# ------------------------------------------------------------------ shared control descriptions
COMMON = {
    "filter": "Bipolar DJ filter on the output. Centre is open. Left sweeps a 24 dB/oct low-pass down to 60 Hz; right sweeps a high-pass up to 12 kHz. Resonance is level-compensated, so a full sweep never jumps in volume.",
    "density": "Ghost notes. 0-50 %: one echo of every note fades in one ECHO SPACE later (1/8 hats become 1/16 hats with ECHO SPACE at 1/16). 50-100 %: two more ghosts at half that spacing (1/32 rolls). Each ghost is quieter, darker and shorter than the note before it.",
    "level": "Output level. Default is set so the module sits sensibly in a mix.",
    "echo_space": "Spacing of the DENSITY ghost notes after each note: 1/8, 1/16, 1/16T, 1/32, 1/8T, 1/8D. 1/8 lands on the offbeat; 1/8D gives a dub gallop.",
    "echo_swing": "Pushes the first ghost note late by up to a third of ECHO SPACE, so fills sit looser.",
    "echo_prob": "Chance each ghost note plays. Below 100 % the fills become irregular, like a drummer.",
    "alloc": "Takes part in SF ALLOC (the mix assigner): this module reports its energy per band and accepts cuts where it collides with something more important. Off = never cut.",
    "res": "Resonance of the output FILTER. Higher gives a sharper, more vocal sweep.",
    "pocket_depth": "How deep this module ducks around SF KICK (Pocket). 0 = no ducking.",
    "pocket_shape": "One knob from transparent to pumping. Left: a narrow cut that follows the kick's pitch sweep (felt more than heard). Middle: the low band ducks for the kick's length. Right: full-band pump.",
    "model": "The sound engine. Choose from the header menu. All models share the same performance row.",
    "gate": "Note length. 0 = clipped staccato, 50 % = as played, 100 % = held long (release up to 8x).",
    "tune": "Pitch offset in semitones.",
}

SYNTH = {
    "cutoff": "Cutoff of the model's own filter (ladder, state-variable or MS-20-style). Responds to key position and the filter envelope.",
    "res": "Resonance of the model's filter. On ACID 303-TYPE this is the squelch.",
    "env_amt": "How far the filter envelope opens the filter on each note. Accented notes open further.",
    "drive": "Saturation before the filter. Adds weight at low settings, grit at high.",
    "decay": "Length of the filter envelope (and amp decay on most models). Short = plucky, long = sweeping. ACID: the classic decay knob.",
    "attack": "Adds attack time to amp and filter envelopes. Up for swells.",
    "accent": "How strongly accented notes (velocity above about 108) stand out: louder, brighter, shorter filter decay.",
    "glide": "Portamento between notes (non-acid models). ACID slides only when notes overlap, like a 303.",
    "tune": "Pitch offset in semitones (-24 to +24).",
    "detune": "Spread between oscillators and unison voices. Also sets grain scatter on GRANULAR.",
    "color": "Model-specific tone control. Mono subtractive: sub-oscillator level. ACID: below 50 % saw, above 50 % square. ORGAN: drawbar brightness. FM: modulation index. SYNC: sync sweep. WAVETABLE: table position (sine > saw > square > vocal). GRANULAR: grain size. NOISE BED: resonance of the noise.",
    "layer_model": "Second model played with every note, blended by LAYER.",
    "layer": "Blend of the LAYER MODEL under the main model. Use it to put a sub under a stab or an acid line on a bass.",
    "chord": "Notes played per key. MODEL uses the model's own chord (STAB DUB = minor 7, STAB DETROIT = minor 9, ORGAN = minor triad). NONE forces single notes. Or choose MIN7, MIN9, SUS4, MAJ7, MIN, 5TH, OCT.",
    "space": "Ensemble chorus amount, for the models that have one (strings, poly pad, stabs, hoover, organ).",
}

# ------------------------------------------------------------------ modules
MODULES = {}

MODULES["KICK"] = dict(
    group="INSTRUMENTS",
    summary="Kick drum with six synthesised models and a sampler. Every hit is reported to the other SF plugins, which duck around it (Pocket).",
    use=[
        "Put it on a MIDI track. Any note plays the kick (KEY TRACK off).",
        "DECAY is your GATE: short and tight for driving sections, long and boomy for breakdowns. RUMBLE and the bass pockets follow the change automatically.",
        "Ride DENSITY with ECHO SPACE at 1/16 or 1/8D to thicken the kick into a drop. Ghost kicks get smaller pockets.",
        "High-passing the kick with FILTER hands the sub to the bass and rumble: their pockets close as the kick thins.",
    ],
    models=[
        ("KICK 909-TYPE", "Fast pitch drop, noise click, short body. The default techno kick."),
        ("KICK 808-TYPE", "Low, long, tuned boom with little click."),
        ("KICK SINE", "Clean sine with a moderate sweep. Deep and minimal (dub techno)."),
        ("KICK INDUSTRIAL", "Steep sweep, heavy drive and clip. Hard and compressed."),
        ("KICK MODULAR", "Triangle core with noise FM and a resonant low-pass sweep (DFAM-like)."),
        ("KICK DUB", "Soft, low, slow sweep, filtered dark."),
        ("SAMPLE", "Plays the dropped one-shot. Its pitch and decay are analysed and reported for Pocket."),
    ],
    desc={
        "tune": "Kick pitch in semitones around the model's fundamental (about 44-55 Hz).",
        "decay": "Body length, x0.25 to x4 of the model's decay. This is the kick's GATE. On SAMPLE it gates the sample: 0 = 10 ms, 90 % and up = the whole sample.",
        "punch": "Depth of the pitch sweep. More = harder transient, less = rounder.",
        "click": "Level of the noise/impulse click on the attack.",
        "drive": "Saturation on the kick, added to the model's own.",
        "layer_model": "Second model hit with every note (e.g. 909 click on an 808 body).",
        "layer": "Level of the LAYER MODEL. Total level is compensated.",
        "pitch_time": "How fast the pitch sweep falls. Short = snappy, long = laser.",
        "vel_sens": "How much velocity changes the level. 0 = every hit equal.",
        "key_track": "On: the note number sets the pitch (C1 = TUNE). Off: every note plays the same kick.",
        "res": "Resonance of the output FILTER.",
    },
)

MODULES["PERC"] = dict(
    group="INSTRUMENTS",
    summary="Hats, cymbals, claps and percussion. Thirteen models including a sampler. Run two or three instances for a full kit.",
    use=[
        "Program fixed 1/8 hats on the OXI. Bring DENSITY up with ECHO SPACE 1/16 to turn them into 1/16s without changing the pattern; above 50 % they start rolling.",
        "DECAY opens closed hats into open hats.",
        "ECHO PROB under 100 % makes the fills irregular; SPREAD throws the ghosts left and right.",
        "For the real 909 hat sound, load your own samples into the SAMPLE model.",
    ],
    models=[
        ("HAT 909-TYPE", "Six square oscillators plus noise, bright band-pass. Synthesised approximation of the sampled original."),
        ("HAT 808-TYPE", "Six-oscillator metallic hat, darker, less noise."),
        ("HAT 606-TYPE", "Thin, noisy, ticky."),
        ("RIDE 909-TYPE", "Long metallic ride."),
        ("CRASH", "Noise-heavy long crash."),
        ("CLAP", "Three quick noise bursts and a tail."),
        ("RIM", "Two tuned sines, short."),
        ("COWBELL", "Two square oscillators through a band-pass, two-stage decay."),
        ("CLAVE", "Short tuned sine."),
        ("SHAKER", "Band-passed noise with a soft attack."),
        ("METAL FM", "Inharmonic two-operator FM hit. Industrial."),
        ("MODAL", "Six tuned resonators struck by a noise burst. Wood, glass or metal depending on TONE."),
        ("SAMPLE", "Plays the dropped one-shot, pitched by TUNE."),
    ],
    desc={
        "tune": "Pitch in semitones (-24 to +24).",
        "decay": "Length of each hit, x0.25 to x4. Closed to open. On SAMPLE it gates the sample: 0 = 10 ms, 90 % and up = the whole sample.",
        "tone": "Model-specific brightness: band-pass position on hats, oscillator spread on FM, resonator ratios on MODAL.",
        "accent": "How much velocity matters. 0 = all hits equal.",
        "drive": "Saturation on each hit.",
        "choke": "On: a new hit cuts the previous one (like a closed hat choking an open hat).",
        "spread": "Pans the DENSITY ghosts alternately left and right.",
    },
)

MODULES["RUMBLE"] = dict(
    group="INSTRUMENTS",
    summary="The kick-driven low-end bed of hypnotic techno. Needs no notes: it listens to SF KICK, excites a thump with the kick's own pitch sweep, and runs it through a reverb, delay or drone model, then darkens, dirties and ducks it.",
    use=[
        "Put it on a MIDI track (no clips needed) with SOURCE on KICK BUS.",
        "SIZE and DARK set the room; GRIT adds the dirt; PUMP decides how hard it ducks out of the kick's way.",
        "GATE below 100 % cuts the tail before the next kick: tight, rhythmic rumble.",
        "DENSITY adds extra thumps between the kicks for a rolling bed.",
    ],
    models=[
        ("REVERB", "Thump into a dark reverb. The classic."),
        ("DELAY", "Thump into a 3/16 feedback delay."),
        ("SUB DRONE", "Continuous sine at the kick's pitch, ducked by the kick: swells between hits."),
        ("DISTORTED", "Reverb with heavy drive."),
        ("DUB BOOM", "Delay and reverb together, softer."),
        ("SAMPLE", "Plays a dropped rumble one-shot on every kick."),
    ],
    desc={
        "size": "Reverb size / delay feedback.",
        "dark": "Low-pass on the bed: 600 Hz down to 70 Hz.",
        "grit": "Distortion on the bed.",
        "pump": "Pocket depth: how far the bed ducks under each kick.",
        "gate": "Cuts the tail after each kick. 100 % = no gate; lower = shorter.",
        "density": "Extra thumps between kicks (offbeat, then 16ths).",
        "tune": "Pitch of the thump relative to the kick.",
        "source": "KICK BUS: triggered by SF KICK. MIDI: by notes on this track. BOTH.",
        "predelay": "Delay before the reverb: 0, 1/32, 1/16, 1/8.",
        "width": "Stereo width of the bed.",
        "pocket_shape": "Pocket SHAPE (see Pocket chapter).",
        "echo_space": "Spacing of DENSITY thumps.",
    },
)

for name, role_text, models in [
    ("BASS", "Mono bass synth: ladder, 101, sub-oscillator, acid, FM, sine and Berlin off-beat models.", [
        ("BASS LADDER", "Two detuned saws and a sub through a Moog-style ladder."),
        ("MONO 101-TYPE", "Saw + pulse + sub, 4-pole filter."),
        ("SUB OSC", "Square plus heavy sub-oscillator, soft filter. Deep and round."),
        ("ACID 303-TYPE", "Mono, resonant ladder, accent and slide (see LEAD)."),
        ("FM", "Two-operator FM bass; COLOR sets the index."),
        ("SINE SUB", "Pure sine with slight drive. No filter."),
        ("OFFBEAT", "Square + saw pluck, high resonance. The rubbery Berlin off-beat bass."),
        ("SAMPLE", "Chromatic sampler from the analysed root note."),
    ]),
    ("LEAD", "Leads, acid lines and chord stabs.", [
        ("ACID 303-TYPE", "Mono, resonant 4-pole ladder. Velocity above ~108 is an accent (louder, brighter, snappier). Overlapping notes slide without retriggering. COLOR: saw or square."),
        ("MONO 101-TYPE", "Saw + pulse + sub. Classic mono sequence lead."),
        ("MS20-TYPE", "Driven high-pass + low-pass pair, screaming resonance."),
        ("STAB DUB", "Minor 7 chord stab, short filter envelope, light ensemble. Send it into SF DELAY."),
        ("STAB DETROIT", "Minor 9 string/brass stab with unison and ensemble."),
        ("ORGAN M1-TYPE", "Additive organ with key click and percussion, minor triad."),
        ("HOOVER", "Detuned pulse-width-modulated stack with a pitch dive and heavy chorus."),
        ("FM BELL", "Two-operator bell, long release."),
        ("SYNC", "Hard-sync lead; COLOR and ENV AMT sweep the sync."),
        ("SAMPLE", "Chromatic sampler from the analysed root note."),
    ]),
    ("PAD", "Pads, drones and textures. Polyphonic, slow envelopes.", [
        ("STRING MACHINE", "Four-voice unison saws plus an octave, deep ensemble."),
        ("POLY PAD", "Saw + pulse + sub, slow filter envelope, chorus (Juno-like)."),
        ("WAVETABLE", "Morphs sine > saw > square > vocal with COLOR and a slow drift."),
        ("GRANULAR", "Grains from the loaded sample (or a built-in chord bed). COLOR = grain size, DETUNE = scatter."),
        ("DUB WASH", "Minor 7 chord, slow filter, chorus. The dub chord bed."),
        ("TAPE LOOP", "Soft triangle/saw with wow, flutter, saturation and hiss."),
        ("NOISE BED", "Noise through a resonant band-pass at the note: wind, air."),
        ("SAMPLE", "Chromatic sampler from the analysed root note."),
    ]),
]:
    use = {
        "BASS": ["CUTOFF, RES and ENV AMT are the performance; one note can carry a whole track.",
                 "POCKET DEPTH (default 60 %) keeps the bass out of the kick's way; it follows the kick's pitch and length.",
                 "DENSITY ghost notes fill the gaps between notes but never interrupt a note that is held."],
        "LEAD": ["For acid: ACID 303-TYPE, RES high, ENV AMT and DECAY on knobs, accents and slides from the OXI.",
                 "For dub stabs: STAB DUB with GATE short and SF DELAY on a return.",
                 "CHORD overrides the model's chord; NONE gives single notes."],
        "PAD": ["GATE high keeps chords ringing; SLOW mode on CUTOFF gives long, even swells.",
                "GRANULAR and SAMPLE use a dropped file. Field recordings work well in GRANULAR.",
                "POCKET DEPTH lets the pad breathe with the kick without a sidechain."],
    }[name]
    MODULES[name] = dict(group="INSTRUMENTS", summary=role_text + " Shared engine with BASS / LEAD / PAD: the same controls on every model.",
                         use=use, models=models, desc=dict(SYNTH))

MODULES["VOICE"] = dict(
    group="INSTRUMENTS",
    summary="Spoken word and vocal phrases. Drop a phrase: it is sliced, and C3 plays slice 1, C#3 slice 2 and so on; keys below C3 play the whole phrase. Models treat it as radio, vocoder, robot or choir. Built-in dub echo. drosaka Voice stays separate.",
    use=[
        "Drop a phrase on the window (or LOAD). Markers show the slices and their keys.",
        "RADIO with SIGNAL low is a lost transmission; ride SIGNAL up to bring the voice in clear.",
        "ECHO is a send into a long dub echo (ECHO TIME, ECHO FDBK).",
        "With no phrase loaded, notes play a formant choir (TONE morphs the vowel).",
    ],
    models=[
        ("SAMPLE", "Plays the slices clean."),
        ("RADIO", "Band-limited, driven, noisy: CB / transmission. SIGNAL = clarity."),
        ("VOCODER", "16-band vocoder, saw carrier at the TONE pitch."),
        ("ROBOT", "Vocoder with a pulse carrier at 8 kHz / 8-bit."),
        ("CHOIR", "Vocoder with a minor-chord carrier."),
        ("STUTTER", "Loops the start of each slice; TONE sets the loop length."),
    ],
    desc={
        "signal": "Clarity. RADIO: from lost to clear. Vocoders: sibilance and high-band brightness.",
        "tone": "Model-specific: carrier pitch C1-C4 (vocoder, robot, choir), loop length (STUTTER), vowel (choir synth).",
        "gate": "Slice length. Low values clip each slice short.",
        "density": "Re-triggers the slice as ghost repeats (stutter fills).",
        "echo": "Send into the built-in dub echo.",
        "drive": "Saturation on the voice.",
        "slice_mode": "AUTO (detected onsets), EQUAL 8, EQUAL 16, WHOLE (every key plays the whole phrase).",
        "tune": "Playback pitch (varispeed) in semitones.",
        "echo_time": "Echo time, synced (1 BAR to 1/16). Default 1/4 dotted.",
        "echo_fb": "Echo feedback.",
    },
)

MODULES["DECK"] = dict(
    group="INSTRUMENTS",
    summary="Two DJ decks inside the live set. Tracklist with background analysis (tempo, beat grid, loudness), tempo sync to Live with key lock, starts on phrase boundaries, hot cues from MIDI, slip loop roll, per-deck isolator and filter, crossfader, CUE output.",
    use=[
        "Drop tracks on the window. Each is analysed in the background (ANALYSING, then BPM and LUFS).",
        "Click A or B on a row to load it into a deck. X removes it from the list.",
        "PLAY A starts the deck on the next QUANT boundary (default 4 bars), locked to Live's tempo. PLAY off fades it out.",
        "Hot cues: C1-D#1 jump deck A to cue 1-4 on the next beat, C2-D#2 deck B. Default cues at beats 0, 32, 64, 96.",
        "Headphones: make an audio track with Audio From = the DECK track, choose CUE, route it to your headphone outputs, and turn CUE A or CUE B on.",
        "Every deck start is written to Documents/SF Live Suite/tracklog.txt.",
        "Decks follow Live's tempo. A plugin cannot change Live's tempo: use Live's tempo automation for tempo moves.",
    ],
    models=[],
    desc={
        "xfade": "Crossfader, constant power. 0 = deck A, 100 % = deck B.",
        "filter_a": "Deck A bipolar DJ filter.",
        "filter_b": "Deck B bipolar DJ filter.",
        "low_a": "Deck A low band: 0 = kill, 75 % = 0 dB, 100 % = +6 dB.",
        "low_b": "Deck B low band.",
        "roll": "Slip loop roll on the playing decks: 1/4 to 1/32. The track keeps running underneath; OFF returns to where it would have been.",
        "gate": "1/16 trance gate depth on the deck mix.",
        "play_a": "Start (on the next QUANT boundary) / stop deck A.",
        "play_b": "Start / stop deck B.",
        "mid_a": "Deck A mid band.", "high_a": "Deck A high band.",
        "mid_b": "Deck B mid band.", "high_b": "Deck B high band.",
        "quant": "Where PLAY starts: next beat, bar, 4, 8 or 16 bars.",
        "key_lock": "On: time-stretch keeps the track's pitch when its tempo changes. Off: varispeed.",
        "cue_a": "Send deck A to the CUE output (pre-crossfader).",
        "cue_b": "Send deck B to the CUE output.",
        "target": "Every track is trimmed to this loudness (LUFS) so decks match.",
        "res": "Resonance of both deck filters.",
    },
)

MODULES["CONTROL"] = dict(
    group="INSTRUMENTS",
    summary="The suite's brain. Six macros every SF plugin can follow, song select (Program Change), STORE ALL. Makes no sound.",
    use=[
        "Put it on its own MIDI track receiving the OXI One.",
        "Program Change on channel 16 selects the song (VST3: Live delivers it as a program change; SF CONTROL lists SONG 01-16).",
        "To save a song: set every plugin, choose the song number, press STORE ALL.",
        "The display shows macro levels, the song, and whether the bus is shared, SF KICK is reporting and SF ALLOC is running.",
    ],
    models=[],
    desc={
        "macro1": "Macro 1. Any SF plugin with MACRO SRC = MACRO 1 follows it.",
        "macro2": "Macro 2.", "macro3": "Macro 3.", "macro4": "Macro 4.", "macro5": "Macro 5.", "macro6": "Macro 6.",
        "song": "Selects the song (01-16). Every SF plugin recalls its stored values on the next bar.",
        "store_all": "Stores every SF plugin's current values into the selected song.",
        "pc_channel": "MIDI channel that Program Change is read from (default 16).",
        "pc_enable": "Turns Program Change song select on or off.",
        "pocket_offset": "Timing calibration for Pocket and RUMBLE, -20 to +20 ms (+ = later). Set once, by ear, if the pump lands early or late in your Live setup; leave at 0 otherwise.",
    },
)

# --- effects
MODULES["FILTER"] = dict(group="EFFECTS", summary="Bipolar performance filter with drive, envelope follow and a throw.",
    use=["One knob from low-pass through open to high-pass.", "Hold THROW for a timed sweep to the extreme (THROW TIME); release and it returns."],
    models=[], desc={
        "filter": "Bipolar 24 dB/oct filter. Centre open, left low-pass, right high-pass.",
        "res": "Resonance, level-compensated.",
        "drive": "Saturation before the filter.",
        "env_amt": "Envelope follow: the input's level pushes the cutoff up (+) or down (-).",
        "env_speed": "Speed of the envelope follower.",
        "throw": "Hold: the filter travels to the extreme over THROW TIME. Release: back in a quarter of that.",
        "throw_time": "Length of the throw: 4 BAR to 1/16.",
        "throw_to": "Direction of the throw: HP or LP.",
    })
MODULES["GATE"] = dict(group="EFFECTS", summary="Rhythmic gate with 16 patterns, morphing between two of them.",
    use=["The tool that turns a held note into a rhythm.", "Set PATTERN A and B, then ride MORPH between them; DEPTH sets how hard the gate cuts."],
    models=[("Patterns", "FULL, 8THS, OFFBEAT, OFF 8THS, TRESILLO, GALLOP, SKIP 1, SKIP 2, STAB, PUMP, BROKEN, CLAVE, 3-3-2, HALF, SPARSE, DOTTED.")],
    desc={
        "depth": "How far the gate closes. 0 = no effect.",
        "length": "How long each step stays open.",
        "shape": "Edges: hard (clicky) to soft (swelling).",
        "swing": "Delays every second step.",
        "pattern_a": "First pattern.", "pattern_b": "Second pattern.",
        "morph": "Blend from PATTERN A to PATTERN B. In between, steps only in one pattern play partly open.",
        "rate": "Step size: 1/8, 1/16, 1/32, 1/16T.",
    })
MODULES["DENSITY"] = dict(group="EFFECTS", summary="The send-delay hat trick as a plugin: fills sparse rhythms with ghost echoes, each generation darker and quieter.",
    use=["Put it on hats or percussion (insert or return).", "DENSITY 50 % with SPACE 1/16 turns 1/8 hats into 1/16s; above 50 % adds 1/32 ghosts.", "PROB decides, per incoming hit, whether its echoes play."],
    models=[], desc={
        "density": "0-50 %: one echo one SPACE later. 50-100 %: two more ghosts at half spacing.",
        "space": "Echo spacing: 1/8, 1/16, 1/16T, 1/32, 1/8T, 1/8D.",
        "darken": "How much darker each generation of echoes is.",
        "swing": "Delays the first echo.",
        "prob": "Chance each incoming hit gets its echoes.",
        "mix": "Echo level.",
        "shorten": "Echoes carry only the start of each hit (tighter ghosts).",
    })
MODULES["DELAY"] = dict(group="EFFECTS", summary="Tape-style dub delay. Changing TIME bends the pitch as the tape head moves; feedback above 100 % saturates instead of exploding; THROW and FREEZE for live moves. Ducks its repeats around the kick.",
    use=["As a return: INPUT THROW ONLY and THROW mapped to a button gives dub throws.", "Turn TIME during a tail for the tape-bend. SLEW sets how long the bend takes.", "FEEDBACK past 100 % with TONE swept = the runaway dub effect, safe."],
    models=[], desc={
        "time": "Delay time, synced: 1 BAR to 1/32. Changing it slews the head (pitch bend).",
        "feedback": "Repeats, up to 115 %. Above 100 % the loop saturates instead of growing.",
        "tone": "Bipolar filter on the repeats: left darker, right thinner.",
        "drive": "Saturation inside the feedback loop.",
        "throw": "Hold to send into the delay (louder send in INPUT ALWAYS, the only send in THROW ONLY).",
        "freeze": "Loops the current repeats forever; input muted.",
        "mix": "Wet level.",
        "width": "Stereo width of the repeats.",
        "slew": "How long a TIME change takes (the length of the pitch bend).",
        "mode": "STEREO or PING-PONG.",
        "input": "ALWAYS, or THROW ONLY (nothing enters except while THROW is held).",
        "duck": "Pocket on the repeats: they duck under the kick.",
        "duck_shape": "Pocket shape for the repeats.",
        "wow": "Tape wow on the repeats.",
        "lo_cut": "Low cut inside the loop.",
    })
MODULES["REVERB"] = dict(group="EFFECTS", summary="Dark feedback-network reverb with freeze, throw, gated mode and ducking (kick pocket plus dry-follow, so tails bloom in the gaps).",
    use=["DUCK at 30-60 % keeps big spaces from washing out the groove.", "FREEZE holds the space as a drone; THROW sends a single hit."],
    models=[], desc={
        "size": "Room size.", "decay": "Decay time, 0.3 to 30 s.", "dark": "Damping: more = darker tail.",
        "predelay": "Synced predelay: 0, 1/64, 1/32, 1/16, 1/8, 1/4.",
        "duck": "Ducks the tail under the kick and while the dry signal plays.",
        "freeze": "Infinite hold; input muted.", "throw": "Hold to send (or the only send in INPUT THROW ONLY).",
        "mix": "Wet level.", "mod": "Modulation of the tail (chorus-like movement).", "width": "Stereo width.",
        "gated": "Gated reverb: the tail is cut GATE TIME after each hit.", "gate_time": "1/16, 1/8, 1/4, 1/2.",
        "lo_cut": "Low cut on the input.", "duck_shape": "Pocket shape for the tail.", "input": "ALWAYS or THROW ONLY.",
    })
MODULES["DRIVE"] = dict(group="EFFECTS", summary="Saturation with one CHARACTER knob from smooth to fuzz: TAPE, TUBE, FUZZ, INDUSTRIAL. 4x oversampled, auto gain.",
    use=["Your smooth-then-heavy arc on a single knob: automate CHARACTER across a track.", "LOW KEEP keeps the sub clean while the rest distorts."],
    models=[], desc={
        "drive": "Amount, 0 to +36 dB into the shaper.", "character": "TAPE > TUBE > FUZZ > INDUSTRIAL (wavefold).",
        "tone": "Tilt EQ around 800 Hz.", "low_keep": "Below this frequency the signal stays clean. 0 = off.",
        "mix": "Dry/wet.", "auto_gain": "Holds output loudness near input loudness.",
    })
MODULES["RADIO"] = dict(group="EFFECTS", summary="Transmission processor: band-limit, dirt, noise, dropouts and squelch. SIGNAL is one knob from lost to clear.",
    use=["On a voice, ride SIGNAL from 0 to 100 % to bring a ghost transmission into focus."],
    models=[], desc={
        "signal": "Clarity: band width, noise, dropouts and distortion all follow it.",
        "band": "Centre of the pass band.", "width": "Width of the pass band.", "noise": "Noise floor.",
        "dropout": "How often the signal drops out (more at low SIGNAL).", "squelch": "Squelch burst after each dropout.",
        "ring": "Ring modulation (47 Hz) for a metallic voice.", "mix": "Dry/wet.",
        "crush": "Bit depth (16 = clean).", "rate": "Sample rate reduction.",
    })
MODULES["REPEAT"] = dict(group="EFFECTS", summary="Beat repeat for fills and builds. Hold ROLL: it captures LENGTH of audio from the next QUANT point and loops it, each pass quieter, lower, darker.",
    use=["Map ROLL to a button; LENGTH 1/16 then 1/32 into a drop.", "PITCH negative = the classic falling roll."],
    models=[], desc={
        "roll": "Hold to roll; release returns to the input.", "length": "Loop length: 1/4 to 1/32 and triplets.",
        "decay": "Level loss per pass.", "pitch": "Pitch drop over the repeats: falls by this amount every four passes.",
        "gate": "How much of each pass plays.", "filter": "Darkens over the repeats.", "mix": "Roll level over the input.",
        "quant": "Start point: OFF (at once), 1/16, 1/8, 1/4.",
    })
MODULES["DUCK"] = dict(group="EFFECTS", summary="Pocket as an effect for any audio: follows SF KICK on the bus, a sidechain input, or a tempo-synced volume shape.",
    use=["Put it on any track that should pump with the kick: no sidechain routing needed.", "SOURCE SIDECHAIN for an external kick; GRID for a classic volume-shaper pump."],
    models=[], desc={
        "depth": "How far it ducks.", "shape": "Transparent (pitch-tracking) to full pump.",
        "source": "KICK BUS (SF KICK), SIDECHAIN (Live's sidechain input), GRID (volume shape).",
        "rate": "GRID: 1/2, 1/4, 1/8.", "curve": "GRID: shape of the recovery.", "mix": "Amount.",
    })
MODULES["ENERGY"] = dict(group="EFFECTS", summary="Group energy. Replaces the DJ filter on groups: one bipolar knob, centre neutral, that adds energy as well as taking it away.",
    use=["One per group (drums, bass, music, voice) with PROFILE to match.", "Down: low-pass, space and softness. Up: high-pass with a resonant edge, drive, 1/16 echo and chop.", "With SLOW on, an 8-bar high-pass build happens by itself."],
    models=[("PROFILE", "DRUMS (high-pass side thins the kick moderately), BASS (strong low-pass side, gentle high-pass), MUSIC (both sides full), VOICE, MASTER (gentle both ways).")],
    desc={
        "energy": "Bipolar. Centre = neutral. Left = down, right = up.", "profile": "How far each side travels for this kind of group.",
        "res": "Resonant edge (stronger on the up side).", "drive_amt": "Drive added on the up side.",
        "space_amt": "Reverb added on the down side.", "echo_amt": "1/16 echo added on the up side.",
        "gate_amt": "1/16 chop added on the up side.",
    })

# --- master
MODULES["ISOLATOR"] = dict(group="MASTER AND MIX", summary="3-band DJ kill EQ (Linkwitz-Riley, phase-aligned).", use=["Kill the lows before a drop, bring them back on the one."], models=[], desc={
    "low": "0 = kill, 75 % = 0 dB, 100 % = +6 dB.", "mid": "Same for mids.", "high": "Same for highs.",
    "low_kill": "Kill the low band.", "mid_kill": "Kill the mid band.", "high_kill": "Kill the high band.",
    "xover_lo": "Low/mid crossover.", "xover_hi": "Mid/high crossover."})
MODULES["BUILD"] = dict(group="MASTER AND MIX", summary="One knob for a whole-mix build: high-pass sweep, noise riser, reverb wash and width. DROP resets it on the next bar line.",
    use=["Turn BUILD up over the build. Hold DROP: the build snaps off exactly on the next bar.", "After a drop the build stays off until BUILD is back below 5 % (display: RETURN KNOB), so you can bring the knob down without re-building."],
    models=[], desc={
        "build": "The build amount.", "drop": "Arms the reset for the next bar line.", "noise": "Noise riser level.",
        "space": "Reverb wash.", "width": "Stereo widening.", "hp_max": "How high the high-pass goes at full build."})
MODULES["GLUE"] = dict(group="MASTER AND MIX", summary="Bus compressor with one PUSH knob (threshold and makeup together).", use=["PUSH 20-40 % for glue; more for audible pump."], models=[], desc={
    "push": "Compression and makeup together.", "attack": "0.1 to 30 ms.", "release": "0.1 to 1.2 s or AUTO (dual-stage).",
    "ratio": "2:1, 4:1, 10:1.", "sc_hpf": "High-pass on the detector so the kick doesn't drive it.", "mix": "Parallel compression."})
MODULES["LOWEND"] = dict(group="MASTER AND MIX", summary="Low-end safety: mono below a frequency, sub clip, rumble cut, width above.", use=["Last-but-one on the master, before LIMIT."], models=[], desc={
    "mono_freq": "Everything below this is mono.", "sub_clip": "Soft clip on the mono low band (louder sub, same peak).",
    "rumble_cut": "24 dB/oct high-pass for DC and rumble.", "width": "Stereo width above MONO FREQ.", "low_trim": "Level of the low band."})
MODULES["LIMIT"] = dict(group="MASTER AND MIX", summary="Clipper and lookahead limiter with true-peak ceiling and a LUFS meter. AUTO trims the input so short-term loudness sits at TARGET.",
    use=["Last on the master. CEILING -1 dB for venues.", "AUTO moves the input by at most 1 dB per second, so a whole set sits at TARGET without anyone riding the master."],
    models=[], desc={
        "target": "Loudness AUTO aims for (LUFS short-term).", "ceiling": "True-peak ceiling (dBTP).", "clip": "Soft clip before the limiter.",
        "input": "Input gain.", "release": "Limiter release.", "auto": "Automatic input trim toward TARGET (+/-12 dB).",
        "reset": "Resets the integrated loudness and true-peak hold."})
MODULES["ROOMEQ"] = dict(group="MASTER AND MIX", summary="Cut-only correction for the room or PA, stored per venue (16 venues). Not part of songs.",
    use=["Find the boomy and harsh frequencies at soundcheck, cut them, STORE VENUE.", "Next time at that venue, choose it from VENUE."],
    models=[], desc=dict({f"cut{i}": f"Cut at band {i} (0 to 18 dB)." for i in range(1, 7)}, **{f"freq{i}": f"Frequency of band {i}." for i in range(1, 7)},
        q="Width of all six bands.", venue="Recalls a stored venue.", store_venue="Stores the current bands into the selected venue."))
MODULES["ALLOC"] = dict(group="MASTER AND MIX", summary="The mix assigner. Automatic where sources don't overlap; where they do, the conflict gets one of three SLOT knobs. Pass-through audio: put it on the master.",
    use=["Every SF instrument (and SF TAP / SF DECK) takes part automatically.", "When a SLOT lights amber it has a conflict (named in the display, e.g. BASS / PAD 150-320 HZ).", "Centre = AUTO. Right (HAND) moves ownership to the other source(s). Left (SHARE) fades the cuts out."],
    models=[], desc={
        "slot1": "First conflict knob. Centre AUTO, right HAND, left SHARE.", "slot2": "Second conflict knob.", "slot3": "Third conflict knob.",
        "depth": "Largest cut ALLOC will make.", "sensitivity": "How quiet a source can be and still count as playing in a band.",
        "hold": "How long an overlap must last before it is a conflict (1, 2 or 4 beats).", "enable": "Off = no cuts anywhere."})
MODULES["TAP"] = dict(group="MASTER AND MIX", summary="Puts any track (stems, hardware, other plugins) into Pocket and ALLOC. Adds no sound of its own.",
    use=["One on each stem track from Live's stem separation, with ROLE set (stem drums = PERC, stem bass = BASS ...)."],
    models=[], desc={"role": "Priority in ALLOC (KICK highest ... OTHER lowest) and the name shown.", "alloc": "Take part in ALLOC.",
        "pocket_depth": "Duck this track around SF KICK.", "pocket_shape": "Pocket shape."})

SYSTEM = [
    ("SLOW", "OFF / 1 / 2 / 4 / 8 / 16 / 32 / 64 BAR", "Slow mode. Every performance-row knob travels to where you set it over exactly this many bars, however far you moved it. Song recalls glide too. While travelling the knob shows GLIDE now>target in green."),
    ("MACRO SRC", "OFF, MACRO 1-6", "Which SF CONTROL macro this plugin follows."),
    ("MACRO AMT", "-100 to +100 %", "How strongly and in which direction. Each plugin decides which of its knobs respond (the ones marked M in the tables)."),
]

MODULES["STRIP"] = dict(group="EFFECTS", summary="The suite's performance row for any track. Put it after any Live instrument, sampler, drum rack or hardware input: the track gets DENSITY, GATE, FILTER, DRIVE, POCKET and a SPACE send, joins ALLOC, and follows SLOW, macros and songs like every SF plugin.",
    use=["Last device on the track (after the instrument and anything else).",
         "Live sends no MIDI to audio effects, so STRIP works on the hits it hears. GATE closes each detected hit after a set length (staccato to held); DENSITY adds darker, quieter echo ghosts between hits.",
         "Set ROLE so ALLOC knows the track's priority (drum rack = PERC, a bass synth = BASS).",
         "THROW sends the track into its own SPACE (dub echo or reverb) while held.",
         "On a long, sustained sound GATE and DENSITY act on its attacks only; SENSITIVITY sets how small an attack counts as a hit."],
    models=[("Chain", "GATE > DENSITY > DRIVE > FILTER > SPACE > POCKET > ALLOC > LEVEL (fixed order).")],
    desc={
        "density": "Ghost echoes of each hit: 0-50 % one echo one ECHO SPACE later, 50-100 % two more at half spacing. Each darker and quieter.",
        "gate": "GATE MODE HITS: how long each detected hit stays open, from 1/64 beat at 0 % to 2 beats; 97 % and up = never closes. PATTERN: how long each pattern step stays open.",
        "drive": "Saturation, 0 to +30 dB into the shaper (CHARACTER sets tape to fuzz).",
        "pocket": "Pocket depth: ducks the track around SF KICK.",
        "space": "Send into the strip's own SPACE (dub echo or reverb).",
        "throw": "Hold: full send into SPACE.",
        "role": "Priority in ALLOC and the name shown (STRIP BASS, STRIP PERC ...).",
        "echo_prob": "Chance each hit gets its echoes.",
        "darken": "How much darker each generation of echoes is.",
        "gate_mode": "HITS (closes each detected hit) or PATTERN (rhythmic gate).",
        "pattern": "Gate pattern for PATTERN mode.",
        "rate": "Step size for PATTERN mode: 1/8, 1/16, 1/32.",
        "character": "Drive character: tape (0) to fuzz (100 %).",
        "space_type": "REVERB or DUB ECHO (ping-pong, 1/8 to 3/16).",
        "space_size": "Reverb size and decay / echo time and feedback.",
        "sensitivity": "How small an attack still counts as a hit (GATE HITS and ECHO PROB).",
    })
