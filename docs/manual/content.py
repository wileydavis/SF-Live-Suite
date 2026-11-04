# Text for the SF Live Suite reference manual. Control ranges and defaults come from the code
# (SFTests --dump); everything written here is the explanation.

# ------------------------------------------------------------------ shared control descriptions
COMMON = {
    "filter": "Bipolar DJ filter on the output. Centre is open. Left sweeps a 24 dB/oct low-pass down to 60 Hz; right sweeps a high-pass up to 12 kHz. Resonance is level-compensated, so a full sweep never jumps in volume.",
    "density": "Ghost notes. 0-50 %: one echo of every note fades in one ECHO SPACE later (1/8 hats become 1/16 hats with ECHO SPACE at 1/16). 50-100 %: two more ghosts at half that spacing (1/32 rolls). Each ghost is quieter, darker and shorter than the note before it. Ghosts are played softer and also faded in after the instrument's velocity response, so at the first notch they are 30 dB down: a fill really rises from silence. A quiet ghost never cuts the ringing kick or chokes the previous hit.",
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
    "key_follow": "On: follows the KEY set on SF CONTROL (parts written in C play in the song's key). Off: ignores KEY.",
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
        "Set the song's KEY on SF CONTROL and the kick tunes itself to the key root (KEY FOLLOW).",
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
        "layer": "Level of the LAYER MODEL. Main and layer always add up to the level of one kick.",
        "pitch_time": "How fast the pitch sweep falls. Short = snappy, long = laser.",
        "vel_sens": "How much velocity changes the level. 0 = every hit equal.",
        "key_track": "On: the note number sets the pitch (C1 = TUNE). Off: every note plays the same kick.",
        "res": "Resonance of the output FILTER.",
        "key_follow": "On: with a KEY set on SF CONTROL, the model's fundamental lands on the nearest key root and TUNE moves from there (TUNE 0 = on the root). Off: ignores KEY.",
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
        "key_follow": "On: with a KEY set on SF CONTROL, the thump lands on the nearest key root. Off: it follows the kick's pitch.",
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
        "key_follow": "On: playback and carriers follow the KEY set on SF CONTROL (phrases written in C play in the song's key). Off: ignores KEY.",
    },
)

MODULES["STRIKE"] = dict(
    group="INSTRUMENTS",
    summary="Blast-excited resonant objects. A pressure wave from a charge of POWER at DISTANCE strikes a bell, plate, bar, tube, membrane or gong, modelled as up to 16 resonant modes. Industrial impacts, distant detonations, metal that rings for bars.",
    use=[
        "Put it on a MIDI track. Any note strikes the object (KEY TRACK on: the note sets the pitch, C3 = TUNE).",
        "POWER is the charge: low is a tap, high a blast. Above about 70 % the object FRACTURES: its modes split and detune, debris rattles and it distorts. The display shows FRACTURE in amber.",
        "DISTANCE moves the blast away: quieter, longer and darker, exciting the low modes. Close and small is a sharp crack; far and big is a deep push.",
        "SIZE sets the pitch (bigger = lower). MATERIAL sets how long the high modes ring (wood to glass). DECAY sets the overall ring.",
        "DENSITY adds ghost strikes like on the other instruments; each ghost is a smaller charge.",
        "With a KEY set on SF CONTROL the object's main partial lands on the key root; SIZE then picks the octave.",
    ],
    models=[
        ("BELL", "Church-bell partials (hum, prime, tierce, quint, nominal). Long ring; the nominal is the pitch you hear."),
        ("PLATE", "Sheet metal: dense pairs of close modes that beat. Thunder sheet, industrial clang."),
        ("BAR", "Free bar (anvil, xylophone ratios). Short and pure."),
        ("TUBE", "Pipe or tubular bell. Bright and long."),
        ("MEMBRANE", "Drum head (circular membrane ratios). Short, thuddy."),
        ("GONG", "Sixteen dense modes, the longest ring, and the strongest pitch glide after a hard hit (TENSION)."),
    ],
    desc={
        "power": "Size of the charge. Velocity scales it (VEL SENS). Above about 70 % the object fractures.",
        "distance": "Stand-off. Further = quieter, longer, darker blast that excites fewer high modes.",
        "size": "Object size, which sets the pitch: the fundamental goes from about 1.4 kHz (small) to 45 Hz (large).",
        "material": "Wood (0) to glass (100 %): how long the high modes ring compared with the low ones.",
        "decay": "Ring time, 0.15 to 9 s, times the object's own (a gong rings longer than a bar). Four strikes ring at once; a fifth fades the quietest out over 5 ms.",
        "position": "Strike point. Striking near a node mutes the modes that have one there, so it changes the colour.",
        "spread": "Spreads the modes across the stereo field.",
        "tension": "How sharp a hard hit sounds before it settles (the pitch glides down after the blast).",
        "tune": "Pitch in semitones (-24 to +24).",
        "key_track": "On: the note number sets the pitch (C3 = TUNE). Off: every note strikes the same pitch.",
        "res": "Resonance of the output FILTER.",
        "vel_sens": "How much velocity scales the charge. 0 = every strike the same.",
        "key_follow": "On: with a KEY set on SF CONTROL, the main partial lands on the nearest key root and TUNE moves from there. Off: ignores KEY.",
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
    summary="The suite's brain. Six macros every SF plugin can follow, song select (Program Change), STORE ALL and STORE TO. Makes no sound.",
    use=[
        "Put it on its own MIDI track receiving the OXI One.",
        "Program Change on channel 16 selects the song. AU: in any Live version. VST3: Live 12.2 and later deliver it as a program change (SF CONTROL lists SONG 01-16); older versions don't.",
        "Every plugin changes on the same bar line, whatever order Live processes the tracks in. A change sent within 30 ms after a bar line (on the downbeat) counts for that bar; later in the bar, for the next one.",
        "To save a song: set every plugin and press STORE ALL. It goes into the song in force (SONG).",
        "To save into another song, choose it in STORE TO first, then press STORE ALL. That song isn't recalled, so nothing you hear changes; it becomes the song in force (SONG follows) and STORE TO goes back to ACTIVE SONG. If a song change is waiting for its bar, that change still lands and stays the song in force.",
        "The display shows macro levels, the song, the key, and whether the bus is shared, SF KICK is reporting and SF ALLOC is running.",
        "KEY: write parts in C and set each song's key here. Every pitched SF instrument follows; KICK, RUMBLE and STRIKE tune themselves to the key root.",
        "PANIC: map it to a button you can find in the dark. Every SF plugin fades out over 5 ms, clears every voice, tail and delay line, and stays silent while you hold it.",
    ],
    models=[],
    desc={
        "macro1": "Macro 1. Any SF plugin with MACRO SRC = MACRO 1 follows it.",
        "macro2": "Macro 2.", "macro3": "Macro 3.", "macro4": "Macro 4.", "macro5": "Macro 5.", "macro6": "Macro 6.",
        "song": "Selects the song (01-16). Every SF plugin recalls its stored values on the next bar. A recalled knob is yours again the moment you turn it.",
        "store_all": "Stores every SF plugin's current values into the song STORE TO names: ACTIVE SONG, the song in force, unless you chose another.",
        "store_to": "Where STORE ALL stores: ACTIVE SONG (the song in force) or SONG 01-16. Storing into another song doesn't recall it: what plays now goes in, that song becomes the song in force (SONG follows) and STORE TO goes back to ACTIVE SONG. (Choosing the song on SONG first would recall it and lose what you set.)",
        "pc_channel": "MIDI channel that Program Change is read from (default 16).",
        "pc_enable": "Turns Program Change song select on or off.",
        "pocket_offset": "Timing calibration for Pocket and RUMBLE, -20 to +20 ms (+ = later). Set once, by ear, if the pump lands early or late in your Live setup; leave at 0 otherwise.",
        "key": "Key of the song: OFF, C ... B. Parts written in C play in this key on every SF instrument with KEY FOLLOW on (nearest transposition, -6 to +5 semitones). Stored per song; a song's key changes on the song's bar in every plugin. Notes already sounding follow. Only while SF CONTROL runs: remove it and every instrument is back in C.",
        "panic": "Hold: every SF plugin fades out over 5 ms, clears every voice, tail and delay line, and stays silent (no input, no new notes) until you let go; every SF plugin's header shows PANIC in red meanwhile. Release: input and output fade back in over 5 ms; DECK has kept its place in time. A set never reopens with PANIC held.",
    },
)

# --- effects
MODULES["FILTER"] = dict(group="EFFECTS", summary="Bipolar performance filter with drive, envelope follow and a throw.",
    use=["One knob from low-pass through open to high-pass.", "Hold THROW for a timed sweep to the extreme (THROW TIME); release and it returns."],
    models=[], desc={
        "filter": "Bipolar 24 dB/oct filter. Centre open, left low-pass, right high-pass.",
        "res": "Resonance, level-compensated.",
        "drive": "Saturation before the filter, 2x oversampled. Fades in from exactly clean, so the first notch never jumps the level.",
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
    use=["Put it on hats or percussion (insert, or a return with DRY off).", "DENSITY 50 % with SPACE 1/16 turns 1/8 hats into 1/16s; above 50 % adds 1/32 ghosts.", "PROB decides, per incoming hit, whether its echoes play."],
    models=[], desc={
        "density": "0-50 %: one echo one SPACE later. 50-100 %: two more ghosts at half spacing.",
        "space": "Echo spacing: 1/8, 1/16, 1/16T, 1/32, 1/8T, 1/8D.",
        "darken": "How much darker each generation of echoes is.",
        "swing": "Delays the first echo.",
        "prob": "Chance each incoming hit gets its echoes.",
        "mix": "Echo level.",
        "shorten": "Echoes carry only the start of each hit (tighter ghosts).",
        "dry": "Passes the input. Turn it off on a return track, so the return gives only the echoes. Not stored in songs or presets.",
    })
MODULES["DELAY"] = dict(group="EFFECTS", summary="Tape-style dub delay. Changing TIME bends the pitch as the tape head moves; feedback above 100 % saturates instead of exploding; THROW and FREEZE for live moves. Ducks its repeats around the kick.",
    use=["As a return (DRY off): INPUT THROW ONLY and THROW mapped to a button gives dub throws.", "Turn TIME during a tail for the tape-bend. SLEW sets how long the bend takes.", "FEEDBACK past 100 % with TONE swept = the runaway dub effect, safe."],
    models=[], desc={
        "time": "Delay time, synced: 1 BAR to 1/32. Changing it slews the head (pitch bend).",
        "feedback": "Repeats, up to 115 %. Above 100 % the loop saturates instead of growing.",
        "tone": "Bipolar filter inside the feedback loop: left darker, right thinner. Each repeat goes further than the last.",
        "drive": "Saturation inside the feedback loop (2x oversampled): each repeat dirtier than the last.",
        "throw": "Hold to send into the delay (louder send in INPUT ALWAYS, the only send in THROW ONLY).",
        "freeze": "Loops the current repeats forever, losslessly: no feedback loss, no filtering, the tape wow stops (10 ms crossfade in and out); input muted. Turning TIME while frozen bends the loop (a tape-speed move) and it locks again when the head settles.",
        "mix": "Wet level.",
        "width": "Stereo width of the repeats.",
        "slew": "How long a TIME change takes (the length of the pitch bend).",
        "mode": "STEREO or PING-PONG.",
        "input": "ALWAYS, or THROW ONLY (nothing enters except while THROW is held).",
        "duck": "Pocket on the repeats: they duck under the kick.",
        "duck_shape": "Pocket shape for the repeats.",
        "wow": "Tape wow on the repeats.",
        "lo_cut": "Low cut inside the loop.",
        "dry": "Passes the input at unity (MIX sets only the repeats). Turn it off on a return track. Not stored in songs or presets.",
    })
MODULES["REVERB"] = dict(group="EFFECTS", summary="Dark feedback-network reverb with freeze, throw, gated mode and ducking (kick pocket plus dry-follow, so tails bloom in the gaps).",
    use=["DUCK at 30-60 % keeps big spaces from washing out the groove.", "FREEZE holds the space as a drone; THROW sends a single hit."],
    models=[], desc={
        "size": "Room size.", "decay": "Decay time of the lows, 0.3 to 30 s (measured: DECAY is the real RT60).", "dark": "Damping: shortens the decay of the highs only, so the tail gets darker without getting shorter.",
        "predelay": "Synced predelay: 0, 1/64, 1/32, 1/16, 1/8, 1/4.",
        "duck": "Ducks the tail under the kick and while the dry signal plays.",
        "freeze": "Infinite hold: the space keeps its level and brightness (measured over 14 s). The input fades into the frozen space over 10 ms, then is muted.", "throw": "Hold to send (or the only send in INPUT THROW ONLY).",
        "mix": "Wet level.", "mod": "Modulation of the tail (chorus-like movement).", "width": "Stereo width.",
        "gated": "Gated reverb: the tail is cut GATE TIME after each hit.", "gate_time": "1/16, 1/8, 1/4, 1/2.",
        "lo_cut": "Low cut on the input.", "duck_shape": "Pocket shape for the tail.", "input": "ALWAYS or THROW ONLY.",
        "dry": "Passes the input at unity (MIX sets only the tail). Turn it off on a return track. Not stored in songs or presets.",
    })
MODULES["DRIVE"] = dict(group="EFFECTS", summary="Saturation with one CHARACTER knob from smooth to fuzz: TAPE, TUBE, FUZZ, INDUSTRIAL. 4x oversampled, auto gain.",
    use=["Your smooth-then-heavy arc on a single knob: automate CHARACTER across a track.", "LOW KEEP keeps the sub clean while the rest distorts."],
    models=[], desc={
        "drive": "Amount, 0 to +36 dB into the shaper.", "character": "TAPE > TUBE > FUZZ > INDUSTRIAL (wavefold).",
        "tone": "Tilt EQ around 800 Hz.", "low_keep": "Below this frequency the signal stays clean. 0 = off.",
        "mix": "Dry/wet. The dry signal runs through an identical oversampler, so the blend is phase-aligned (no comb filtering) and MIX 0 is the clean input.", "auto_gain": "Holds the driven signal's loudness near the input's, before MIX.",
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
        "roll": "Hold to roll. The slice is captured from the QUANT point (up to 4 s) and kept for the whole hold. Release: the loop keeps playing while it crossfades back to the input.", "length": "Loop length: 1/4 to 1/32 and triplets.",
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
        "res": "Resonant edge (stronger on the up side).", "drive_amt": "Drive added on the up side (2x oversampled, from clean at centre).",
        "space_amt": "Reverb added on the down side. It rings out when the knob comes back past centre.", "echo_amt": "1/16 echo added on the up side.",
        "gate_amt": "1/16 chop added on the up side.",
    })

# --- master
MODULES["ISOLATOR"] = dict(group="MASTER AND MIX", summary="3-band DJ kill EQ (Linkwitz-Riley, phase-aligned).", use=["Kill the lows before a drop, bring them back on the one."], models=[], desc={
    "low": "0 = kill, 75 % = 0 dB, 100 % = +6 dB.", "mid": "Same for mids.", "high": "Same for highs.",
    "low_kill": "Kill the low band.", "mid_kill": "Kill the mid band.", "high_kill": "Kill the high band.",
    "xover_lo": "Low/mid crossover.", "xover_hi": "Mid/high crossover."})
MODULES["BUILD"] = dict(group="MASTER AND MIX", summary="One knob for a whole-mix build: high-pass sweep, noise riser, reverb wash and width. DROP lands on the next bar line, with an optional silent GAP before it, the riser's TAIL ringing on, and DROP DUCK clearing the way for the first kicks.",
    use=["Turn BUILD up over the build. Hold DROP: the drop lands exactly on the next bar line. The display shows DROP ON BAR, then GAP during the gap.",
         "GAP silences everything through BUILD for that long before the bar line: in the show template the whole LIVE BUS, returns included (BRIDGE and DECK play on). Pressed later than that, the gap starts on the next 1/16.",
         "TAIL: the riser's noise and the wash ring on for that long after the build stops (where the gap starts, or on the bar with GAP OFF), fading out. Shorter than the GAP, it dies away in the silence; longer, it carries into the drop, DROP DUCK lower. CUT: gone in 5 ms.",
         "DROP DUCK: for DUCK TIME after the drop, every SF part with Pocket dips this far under each kick, whatever its POCKET DEPTH, so the first kicks land on top of everything that comes back. It starts with the first kick on the bar line (a roll into the drop doesn't duck): full depth for the first half of DUCK TIME, easing out over the second. The parts: BASS, LEAD, PAD, VOICE, RUMBLE, the wet signal of DELAY and REVERB, DUCK, and TAP and STRIP unless their ROLE is KICK, PERC or DECK. Not KICK, PERC, STRIKE, DECK or tracks without an SF plugin. It needs SF KICK reporting (KICK REPORTING on SF CONTROL). 0 dB: off.",
         "After a drop the build stays off until BUILD is back below 5 % (display: RETURN KNOB), so you can bring the knob down without re-building. An Arrangement loop that wraps while a drop waits lands it on the wrap; looping back over a drop doesn't duck again."],
    models=[], desc={
        "build": "The build amount.", "drop": "Arms the drop for the next bar line.", "noise": "Noise riser level.",
        "space": "Reverb wash.", "width": "Stereo widening.", "hp_max": "How high the high-pass goes at full build.",
        "gap": "Silence before the drop's bar line: OFF, 1/16, 1/8, 1/4, 1/2 or 1 BAR.",
        "tail": "How long the noise and the wash ring on after the build stops, fading out: CUT (5 ms), 1/16 to 2 BARS.",
        "drop_duck": "How far every SF part with Pocket dips under each kick after the drop, and how much lower the tail runs into it. 0 = off.",
        "duck_time": "How long the drop duck lasts after the bar line: full depth for the first half, easing out over the second."})
MODULES["GLUE"] = dict(group="MASTER AND MIX", summary="Bus compressor with one PUSH knob (threshold and makeup together). 6 dB soft knee.", use=["PUSH 20-40 % for glue; more for audible pump."], models=[], desc={
    "push": "Compression and makeup together.", "attack": "0.1 to 30 ms.", "release": "0.1 to 1.2 s or AUTO (dual-stage).",
    "ratio": "2:1, 4:1, 10:1.", "sc_hpf": "High-pass on the detector so the kick doesn't drive it.", "mix": "Parallel compression."})
MODULES["TONE"] = dict(group="MASTER AND MIX", summary="Master voicing EQ: five broad moves on one row (low shelf, mud cut, presence, air, tilt). The ranges stop before damage. Runs at 2x, so the air shelf keeps its shape up to 20 kHz.",
    use=["After GLUE, before LOWEND and LIMIT.", "MUD 1-3 dB cleans a dense mix without thinning it. LOW and AIR at +1-2 dB open up a big system. TILT darkens or brightens everything in one move.",
         "For the room or PA itself use SF ROOM EQ (stored per venue, after the recording point); TONE is the sound of the set."],
    models=[], desc={
        "low": "Low shelf at 70 Hz, +/-6 dB.", "mud": "Broad cut at 280 Hz, 0 to 6 dB.", "presence": "Bell at 3.2 kHz, +/-4 dB.",
        "air": "High shelf at 12 kHz, +/-6 dB.", "tilt": "Tilts the whole spectrum around 900 Hz: right brighter, left darker (+/-3 dB at the ends)."})
MODULES["LOWEND"] = dict(group="MASTER AND MIX", summary="Low-end safety: mono below a frequency, sub clip, rumble cut, width above.", use=["Last-but-one on the master, before LIMIT."], models=[], desc={
    "mono_freq": "Everything below this is mono.", "sub_clip": "Soft clip on the mono low band (louder sub, same peak), 2x oversampled.",
    "rumble_cut": "24 dB/oct high-pass for DC and rumble.", "width": "Stereo width above MONO FREQ.", "low_trim": "Level of the low band."})
MODULES["LIMIT"] = dict(group="MASTER AND MIX", summary="Clipper and true-peak lookahead limiter with a BS.1770 loudness meter. The clipper runs at 4x through linear-phase oversampling; the limiter detects true peaks at 8x and never lets the output past CEILING. AUTO trims the input so short-term loudness sits at TARGET.",
    use=["Last on the master. CEILING -1 dB for venues and releases.",
         "AUTO keeps the loudest moments of the last minute at TARGET, moving the input by at most 1 dB per second, so a whole set sits at TARGET without anyone riding the master. A breakdown doesn't count as the set getting quieter: AUTO holds through it, and the drop comes back where it was. AUTO MODE DOWN ONLY (the default) never lifts above unity; UP AND DOWN also lifts a quieter set (0.5 dB per second), but never a breakdown.",
         "CLIP takes the first few dB off the peaks transparently (kick transients) so the limiter works less.",
         "Recording a set: RESET at the start; INTEGRATED then reads the loudness of the whole set.",
         "Latency 176 samples at 48 kHz, compensated by Live."],
    models=[], desc={
        "target": "Loudness AUTO aims for (LUFS short-term).", "ceiling": "True-peak ceiling (dBTP). The output never exceeds it, including between samples.", "clip": "Soft clip before the limiter, 4x oversampled, linear phase. The knee starts up to 6 dB below CEILING.",
        "input": "Input gain.", "release": "Limiter release. Under sustained limiting a slower envelope holds the gain, so dense passages don't pump.", "auto": "Automatic input trim (+/-12 dB) that keeps the loudest short-term loudness of the last minute at TARGET. It moves up only after 3 s of music within 6 dB of TARGET, so it never lifts a breakdown. Measured on the raw input, so it settles without overshoot.", "auto_mode": "DOWN ONLY (default): AUTO never lifts above unity, it only stops the set running above TARGET. UP AND DOWN: it also lifts a quieter set.",
        "reset": "Resets the integrated loudness and true-peak hold."})
MODULES["ROOMEQ"] = dict(group="MASTER AND MIX", summary="Cut-only correction for the room or PA, stored per venue (16 venues). Not part of songs.",
    use=["Put it after the recording point: on an audio track PA with Audio From = Resampling (the Main output), routed to the PA outputs (see Setting up Live). The set is recorded on its own track from the same signal, without the venue's correction.",
         "Find the boomy and harsh frequencies at soundcheck, cut them, STORE VENUE.", "Next time at that venue, choose it from VENUE."],
    models=[], desc=dict({f"cut{i}": f"Cut at band {i} (0 to 18 dB)." for i in range(1, 7)}, **{f"freq{i}": f"Frequency of band {i}." for i in range(1, 7)},
        q="Width of all six bands.", venue="Recalls a stored venue.", store_venue="Stores the current bands into the selected venue."))
MODULES["ALLOC"] = dict(group="MASTER AND MIX", summary="The mix assigner. You put the sources in order; each of the ten bands has a priority curve over that order, set with three knobs, and the display shows exactly what each source keeps and loses in that band. Pass-through audio: put it on the master.",
    use=["Every SF instrument, SF STRIP, SF TAP and SF DECK appears in the SOURCES list by its Live track name (VST3) or its role (PERC, PERC 2 ...).",
         "Drag the list into order, top = most important. This is the ORDER: ALL BANDS. It is saved with the set and never changes on its own; new sources are added at the bottom. The order follows each plugin, not its name: reopening the set or renaming a track keeps every source's place, and a duplicated track joins at the bottom.",
         "Any band can have its own order: click the band (its frame turns white, the list shows ORDER: 180 HZ), drag, and the band is marked OWN. LINK TO ALL puts it back on the global order. Click the band again or ALL BANDS to return to the global list.",
         "In each band: PRIORITY moves the curve's peak up and down the list, DEPTH sets how hard everything away from the peak is cut (0 = nothing), Q sets the width (narrow: one source owns the band; wide: neighbours share).",
         "Each band shows its sources in its own order, named in the lanes: grey = what plays, amber = what ALLOC takes away. The name above the knobs is the source the peak sits on.",
         "MODE DYNAMIC cuts a source only while a more favoured source is actually playing in that band, so when the bass stops the pad gets 180 Hz back within a fraction of a second. STATIC cuts all the time, like a fixed EQ.",
         "Knobs are drawn in the window; the 30 band knobs are not meant for hardware, but all are automatable in Live."],
    models=[], desc={
        "enable": "Off = no cuts anywhere.", "mode": "DYNAMIC (cut only while a more favoured source plays in the band) or STATIC (always).",
        "max_cut": "The largest cut any source can get (DEPTH 100 %, far from the peak).",
        "sensitivity": "DYNAMIC: how quiet a favoured source can be and still count as playing.",
        **{f"pri{i}": f"Band {i}: where the priority peak sits in the list (0 = top, 100 = bottom)." for i in range(1, 11)},
        **{f"depth{i}": f"Band {i}: how hard sources away from the peak are cut." for i in range(1, 11)},
        **{f"q{i}": f"Band {i}: width of the peak (low = wide, sources share; high = narrow, one owns the band)." for i in range(1, 11)}})
MODULES["TAP"] = dict(group="MASTER AND MIX", summary="Puts any track (stems, hardware, other plugins) into Pocket and ALLOC. Adds no sound of its own.",
    use=["One on each stem track from Live's stem separation, with ROLE set (stem drums = PERC, stem bass = BASS ...)."],
    models=[], desc={"role": "Priority in ALLOC (KICK highest ... OTHER lowest) and the name shown.", "alloc": "Take part in ALLOC.",
        "pocket_depth": "Duck this track around SF KICK.", "pocket_shape": "Pocket shape."})

SYSTEM = [
    ("SLOW", "OFF / 1 / 2 / 4 / 8 / 16 / 32 / 64 BAR", "Slow mode. Every performance-row knob travels to where you set it over exactly this many bars, however far you moved it. Song recalls glide too. While travelling the knob shows GLIDE now>target in green."),
    ("MACRO SRC", "OFF, MACRO 1-6", "Which SF CONTROL macro this plugin follows."),
    ("MACRO AMT", "-100 to +100 %", "How strongly and in which direction. Each plugin decides which of its knobs respond (the ones marked M in the tables)."),
    ("M1 / M2 SOURCE", "OFF, SINE, TRIANGLE, SQUARE, RAMP UP, RAMP DOWN, RANDOM, DRIFT, KICK, NOTES or INPUT", "Two modulators (MOTION). Waves, RANDOM (stepped) and DRIFT (smooth) move around the knob; ramps rise or fall from it and restart every RATE; KICK follows SF KICK's hits; NOTES (instruments) follows velocity; INPUT (effects) follows the input level."),
    ("M1 / M2 RATE", "1/16 to 64 BAR", "Cycle length, locked to Live's song position. For KICK, NOTES and INPUT: the release time."),
    ("M1 / M2 TARGET", "OFF or any knob", "The control the modulator moves."),
    ("M1 / M2 DEPTH", "-100 to +100 %", "How far, as a share of the target's whole range. Negative turns it upside down."),
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
        "drive": "Saturation, 0 to +30 dB into the shaper (CHARACTER sets tape to fuzz), 4x oversampled, fading in from clean.",
        "pocket": "Pocket depth: ducks the track around SF KICK.",
        "space": "Send into the strip's own SPACE (dub echo or reverb). The reverb rings out when the send closes.",
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
