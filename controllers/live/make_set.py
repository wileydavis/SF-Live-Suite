#!/usr/bin/env python3
"""Builds the Ableton Live set "SF SHOW.als": the show template of the manual's chapter 7 with every
track, group, return and SF plugin in place, the routing set, and every E16 address of
controllers/e16/layout.py already MIDI-mapped.

    python3 make_set.py --base EMPTY.als [--out "SF SHOW.als"] [--sftests PATH]

--base   any Live 12 set with a MIDI track, an audio track and a return track: Live's own new set
         (File > New Live Set, then Save Live Set As) is right, and makes the result match your Live
         version exactly. Its tracks are patterns only: its tracks, devices, clips and tempo
         automation are not kept.
--sftests the SFTests tool (build-test): it writes each plugin's starting state and reports every
         parameter's value, from the plugins' own code.

A Live set is gzipped XML. Live is strict about it: an element it doesn't know, a wrong value in a
field it knows, a clip slot count that differs from the scene count, or a NextPointeeId not above every
Id in the file, and the set won't open or crashes. So nothing here is invented: every element is a
copy of the base set's own (a track, a mixer, a device header), and the only new structures are the
plugin device and its parameter list, whose layout follows sets Live 11 and 12 wrote themselves.
"""
import argparse
import copy
import gzip
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

sys.dont_write_bytecode = True
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "controllers", "e16"))
import layout as L  # noqa: E402

# ---------------------------------------------------------------------------------------------- show
# Live's 70-colour palette: 13 white, 27 light grey, 41 grey, 55 dark grey, 69 near black, 14 red.
WHITE, LIGHT, GREY, DARK, BLACK, RED, DARK_RED = 13, 27, 41, 55, 69, 14, 58

# kind: midi | audio | group | return. out: "main", "group", "pa" (PA_OUT), "none" (Sends Only) or a track name
# (its Track In). monitor: in | auto | off. devices: (SF module, {parameter id: value}) in chain order.
# midi: the MIDI From, (ONE port, channel): set in the set, under the names Live gives the OXI One MkII's
# two USB ports on a Mac (ONE_PORTS). A MIDI track without one (RUMBLE) stays on No Input.
# One number per track [Nov 8]: an instrument track's number is its MIDI channel, on the OXI One (port A)
# and on the E16, and its page in the E16 scene PERF (controllers/e16/layout.py). Tracks are named by
# their role, not their instrument: a track keeps its name and number when another instrument goes on it.
# The kick sits outside DRUMS (the drum group's REPEAT and ENERGY never take it). A track without a number
# has no channel of its own: CONTROL listens to port A ch 16 (Program Change, for an AU of SF CONTROL;
# the set calls its songs from the E16: NEXT SONG, GO), DECK to port B ch 7 (hot cues).
SHOW = [
    dict(name="CONTROL", kind="midi", colour=BLACK, devices=[("CONTROL", {})], midi=("A", 16)),
    dict(name="1 KICK", kind="midi", colour=LIGHT, out="LIVE BUS", devices=[("KICK", {})], midi=("A", 1)),
    dict(name="DRUMS", kind="group", colour=WHITE, out="LIVE BUS",
         devices=[("ENERGY", {"profile": "DRUMS"}), ("REPEAT", {})]),
    dict(name="2 CLOSED HAT", kind="midi", group="DRUMS", colour=LIGHT, devices=[("PERC", {"model": "HAT 909-TYPE"})],
         midi=("A", 2)),
    dict(name="3 OPEN HAT", kind="midi", group="DRUMS", colour=LIGHT,
         devices=[("PERC", {"model": "HAT 909-TYPE", "decay": "0.8"})], midi=("A", 3)),
    dict(name="4 CLAP", kind="midi", group="DRUMS", colour=LIGHT, devices=[("PERC", {"model": "CLAP"})],
         midi=("A", 4)),
    dict(name="5 PERC", kind="midi", group="DRUMS", colour=LIGHT, devices=[("STRIKE", {})], midi=("A", 5)),
    dict(name="LOW", kind="group", colour=WHITE, out="LIVE BUS", devices=[("ENERGY", {"profile": "BASS"})]),
    dict(name="6 BASS", kind="midi", group="LOW", colour=LIGHT, devices=[("BASS", {})], midi=("A", 6)),
    dict(name="7 RUMBLE", kind="midi", group="LOW", colour=LIGHT, devices=[("RUMBLE", {"source": "KICK BUS"})],
         midi=None),                         # it follows the kick
    dict(name="MUSIC", kind="group", colour=WHITE, out="LIVE BUS", devices=[("ENERGY", {"profile": "MUSIC"})]),
    dict(name="8 LEAD", kind="midi", group="MUSIC", colour=LIGHT, devices=[("LEAD", {"model": "ACID 303-TYPE"})],
         midi=("A", 8)),
    dict(name="9 CHORD", kind="midi", group="MUSIC", colour=LIGHT, devices=[("LEAD", {"model": "STAB DUB"})],
         midi=("A", 9)),
    dict(name="10 PAD", kind="midi", group="MUSIC", colour=LIGHT, devices=[("PAD", {"model": "GRANULAR"})],
         midi=("A", 10)),
    dict(name="11 VOICE", kind="midi", group="MUSIC", colour=LIGHT, devices=[("VOICE", {})], midi=("A", 11)),
    dict(name="BRIDGE", kind="audio", colour=GREY, audio_in="none", monitor="off", devices=[]),
    dict(name="DECK", kind="midi", colour=GREY, devices=[("DECK", {})], midi=("B", 7)),
    # SF DRIVE [Nov 10]: NRG's DIST (its MIX: 0 clean, up the whole mix driven; tube into fuzz, 20 dB in,
    # the lows under 120 Hz kept clean, AUTO GAIN holding the level), before BUILD so the build's wash
    # stays clean on top.
    dict(name="LIVE BUS", kind="audio", colour=DARK, audio_in="none", monitor="in", sends=False,
         devices=[("ISOLATOR", {}), ("DRIVE", {"drive": "0.55", "character": "0.4", "low_keep": "120", "mix": "0"}),
                  ("BUILD", {}), ("GLUE", {}), ("TONE", {}), ("LOWEND", {}), ("ALLOC", {"enable": "off"})]),
    # The PA feed: the Main (the set, after SF LIMIT), the venue's correction, then a safety limiter that never
    # touches the set (its peaks are already at -1 dBTP) and only catches what the room EQ's phase pushes past.
    dict(name="PA", kind="audio", colour=RED, audio_in="main", monitor="in", sends=False, out="pa",
         devices=[("ROOMEQ", {}), ("LIMIT", {"ceiling": "-0.5", "clip": "0", "auto": "off"})]),
    dict(name="REC", kind="audio", colour=DARK_RED, audio_in="main", monitor="off", sends=False, out="none",
         devices=[]),
    # The room mics (a stereo pair on inputs 1/2): arm it to record the room with the set. SF CAPTURE
    # plays TERROIR's sweep out of the headphone pair (a speaker on it) and records the mics; it never
    # passes them, so its output is silent the rest of the time.
    dict(name="ROOM", kind="audio", colour=DARK_RED, audio_in="ext:S0", monitor="in", sends=False, out="ext:S1",
         devices=[("CAPTURE", {})], setup="inputs 1/2 as a stereo pair (Settings > Audio > Input Config)"),
    dict(name="A DUB", kind="return", colour=GREY, out="LIVE BUS", devices=[("DELAY", {"dry": "off"})]),
    dict(name="B SPACE", kind="return", colour=GREY, out="LIVE BUS", devices=[("REVERB", {"dry": "off"})]),
]
# The numbered tracks are the scene's, in the scene's order: one list (controllers/e16/layout.py).
assert [t["name"] for t in SHOW if t["name"].split(" ", 1)[0].isdigit()] == [L.PLUGINS[d][0] for d in L.TRACKS]
assert all(t["midi"] == ("A", int(t["name"].split(" ", 1)[0])) for t in SHOW if t["name"].split(" ", 1)[0].isdigit() and t["midi"])
# Every instrument track ends in SF ENERGY, with the profile for its part: the E16's ENERGY knobs
# (PERF's NRG page) [Nov 8]. One list for the scene and the set: controllers/e16/layout.py.
for _dest, _profile in L.ENERGY_TRACKS.items():
    next(t for t in SHOW if t["name"] == L.PLUGINS[_dest][0])["devices"].append(("ENERGY", {"profile": _profile}))
MAIN_DEVICES = [("LIMIT", {})]
MAIN_COLOUR = DARK
PA_OUT = "ext:S0"            # PA: outputs 1/2, the only signal on them
# The OXI One MkII's USB ports as Live names them on a Mac: port A is Port 1, port B is Port 2.
ONE_PORTS = {"A": "OXI ONE MKII (Port 1){28E9-1113-0-in-0}", "B": "OXI ONE MKII (Port 2){28E9-1113-0-in-1}"}
CUE_OUT = "ext:S1"           # Cue Out 3/4, the headphones (Live: never the Main's pair)
MAIN_OUT = "ext:S2"          # Main 5/6, a pair that goes nowhere: a missing pair stays silent
TEMPO = 120                  # Live's own default; the set's tempo is set per song

MONITOR = {"in": "0", "auto": "1", "off": "2"}


def plugin_list():
    """Each module's plugin code, product name and instrument flag, from the build's own list (CMakeLists.txt)."""
    text = open(os.path.join(ROOT, "CMakeLists.txt")).read()
    manufacturer = re.search(r"PLUGIN_MANUFACTURER_CODE\s+(\w{4})", text).group(1)
    rows = re.findall(r'"([A-Z0-9]+)\|(\w{4})\|(TRUE|FALSE)\|([^"|]+)"', text)
    return manufacturer, {m: c for m, c, _, _ in rows}, {m: n for m, _, _, n in rows}, {m for m, _, s, _ in rows if s == "TRUE"}


MANUFACTURER, PLUGIN_CODE, PRODUCT, INSTRUMENTS = plugin_list()


# ---------------------------------------------------------------------------------------------- ids
def fourcc(s):
    return int.from_bytes(s.encode("ascii"), "big")


def signed32(u):
    return u - (1 << 32) if u >= 1 << 31 else u


def vst3_uid(module):
    """JUCE's VST3 processor class id: ABCDEF01 9182FAEB <manufacturer> <plugin> (no VST2 replacement)."""
    return [signed32(x) for x in (0xABCDEF01, 0x9182FAEB, fourcc(MANUFACTURER), fourcc(PLUGIN_CODE[module]))]


JUCE_PROGRAM_PARAM_ID = 0x70727374   # 'prst': the Program parameter JUCE's VST3 wrapper adds


def vst3_param_id(pid):
    """JUCE's VST3 parameter id: String::hashCode of the parameter id, top bit cleared (Studio One rule)."""
    h = 0
    for ch in pid:
        h = (h * 31 + ord(ch)) & 0xFFFFFFFF
    return h & 0x7FFFFFFF


# ---------------------------------------------------------------------------------------------- xml
def V(tag, value):
    e = ET.Element(tag)
    e.set("Value", str(value))
    return e


def setv(parent, path, value):
    e = parent.find(path)
    assert e is not None, (parent.tag, path)
    e.set("Value", str(value))


class Ids:
    """Pointee ids: every element with an Id and a LockEnvelope child, and every <Pointee>."""
    def __init__(self, root):
        self.next = max(int(e.get("Id")) for e in root.iter() if e.get("Id", "").lstrip("-").isdigit()) + 1

    def fresh(self, tree):
        """New ids for every pointee in a copied subtree (and its own references to them)."""
        remap = {}
        for e in tree.iter():
            if "Id" in e.attrib and (e.tag == "Pointee" or e.find("LockEnvelope") is not None):
                remap[e.get("Id")] = str(self.next)
                e.set("Id", str(self.next))
                self.next += 1
        for e in tree.iter("PointeeId"):
            if e.get("Value") in remap:
                e.set("Value", remap[e.get("Value")])

    def target(self, tag):
        e = ET.Element(tag)
        e.set("Id", str(self.next))
        self.next += 1
        e.append(V("LockEnvelope", 0))
        return e


# ---------------------------------------------------------------------------------------------- plugins
def sftests_state(sftests, module, overrides, seed):
    args = [sftests, "--state", module, f"seed={seed}"] + [f"{k}={v}" for k, v in overrides.items()]
    out = subprocess.run(args, capture_output=True, text=True)
    if out.returncode != 0:
        raise SystemExit(f"SFTests --state {module}: {out.stderr.strip()}")
    st = json.loads(out.stdout)
    if not st["roundtrip"]:
        raise SystemExit(f"SF {module}: the state doesn't load back into the plugin")
    for p in st["params"]:
        if vst3_param_id(p["id"]) != p["vst3_id"]:
            raise SystemExit(f"SF {module} {p['id']}: VST3 id {vst3_param_id(p['id'])} here, {p['vst3_id']} in JUCE")
    return st


def hex_block(data, indent):
    lines = [data[i:i + 80] for i in range(0, len(data), 80)]
    return "\n" + "".join(indent + x + "\n" for x in lines) + indent[:-1]


class Builder:
    def __init__(self, base, sftests):
        self.root = base
        self.live = base.find("LiveSet")
        self.ids = Ids(base)
        self.sftests = sftests
        self.seed = 1
        self.protect = self.live.find("OverwriteProtectionNumber").get("Value")
        tracks = self.live.find("Tracks")
        self.t_midi = self.pattern(tracks, "MidiTrack")
        self.t_audio = self.pattern(tracks, "AudioTrack")
        self.t_return = self.pattern(tracks, "ReturnTrack")
        # A group as this Live writes one, when the base has one (Live 12.4's differs from a MIDI
        # track in more than the slots: no sequencer of its own, the freeze state in another place).
        self.t_group = self.pattern(tracks, "GroupTrack", optional=True)
        mixer = self.t_audio.find("DeviceChain/Mixer")
        cut = [c.tag for c in mixer].index("Sends")
        self.device_header = [copy.deepcopy(c) for c in list(mixer)[:cut]]   # this Live version's device header
        self.routing_pattern = copy.deepcopy(self.t_audio.find("DeviceChain/AudioInputRouting"))
        self.scenes = len(self.live.find("Scenes"))
        self.devices = {}             # track name -> {module: PluginDevice element}
        self.params = {}              # id(device) -> {param id: (slot element, param json)}

    @staticmethod
    def pattern(tracks, tag, optional=False):
        t = next((x for x in tracks if x.tag == tag), None)
        if t is None:
            if optional:
                return None
            raise SystemExit(f"the base set needs a {tag}")
        t = copy.deepcopy(t)
        for tag_, value in (("SavedPlayingSlot", -1), ("SavedPlayingOffset", 0), ("LinkedTrackGroupId", -1)):
            if t.find(tag_) is not None:                # Live 12.4: what the base's owner left playing
                setv(t, tag_, value)
        for p in [p for p in t.iter() if p.find("KeyMidi") is not None]:   # the base owner's mappings
            for k in p.findall("KeyMidi"):
                p.remove(k)
        for path in ("DeviceChain/DeviceChain/Devices", "AutomationEnvelopes/Envelopes", "TakeLanes/TakeLanes"):
            e = t.find(path)
            if e is not None:
                for c in list(e):
                    e.remove(c)
        for seq in ("DeviceChain/MainSequencer", "DeviceChain/FreezeSequencer"):
            s = t.find(seq)
            if s is None:
                continue
            for slot in s.iter("ClipSlot"):
                value = slot.find("ClipSlot/Value") if slot.find("ClipSlot") is not None else None
                if value is not None:
                    for c in list(value):
                        value.remove(c)
            ev = s.find("ClipTimeable/ArrangerAutomation/Events")
            if ev is not None:
                for c in list(ev):
                    ev.remove(c)
        return t

    # -------------------------------------------------------------------------------------- devices
    def plugin_device(self, module, overrides, index):
        st = sftests_state(self.sftests, module, overrides, self.seed)
        self.seed += 1
        instrument = module in INSTRUMENTS
        d = ET.Element("PluginDevice")
        d.set("Id", str(index))
        for h in self.device_header:
            d.append(copy.deepcopy(h))
        setv(d, "IsExpanded", "true")                  # the parameter panel open: what the E16 maps is visible
        setv(d, "IsFolded", "false")
        setv(d, "UserName", "")
        uid = vst3_uid(module)
        desc = ET.SubElement(d, "PluginDesc")
        info = ET.SubElement(desc, "Vst3PluginInfo")
        info.set("Id", "0")
        for tag, v in (("WinPosX", 120 + 24 * index), ("WinPosY", 120 + 24 * index),
                       ("NumAudioInputs", 0 if instrument else 1), ("NumAudioOutputs", 1), ("IsPlaceholderDevice", "false")):
            info.append(V(tag, v))
        preset = ET.SubElement(ET.SubElement(info, "Preset"), "Vst3Preset")
        preset.set("Id", "1")
        preset.append(V("OverwriteProtectionNumber", self.protect))
        preset.append(V("MpeEnabled", 0))
        mpe = ET.SubElement(preset, "MpeSettings")
        for tag, v in (("ZoneType", 0), ("FirstNoteChannel", 1), ("LastNoteChannel", 15)):
            mpe.append(V(tag, v))
        ET.SubElement(preset, "ParameterSettings")
        preset.append(V("IsOn", "true"))
        preset.append(V("PowerMacroControlIndex", -1))
        pm = ET.SubElement(preset, "PowerMacroMappingRange")
        pm.append(V("Min", 64))
        pm.append(V("Max", 127))
        for tag, v in (("IsFolded", "false"), ("StoredAllParameters", "true"), ("DeviceLomId", 0), ("DeviceViewLomId", 0),
                       ("IsOnLomId", 0), ("ParametersListWrapperLomId", 0)):
            preset.append(V(tag, v))
        self.uid_element(preset, uid)
        preset.append(V("DeviceType", 1 if instrument else 2))
        ps = ET.SubElement(preset, "ProcessorState")
        ps.text = hex_block(st["state"], "\t" * 13)
        ET.SubElement(preset, "ControllerState")
        preset.append(V("Name", ""))
        ET.SubElement(preset, "PresetRef")
        info.append(V("Name", PRODUCT[module]))
        self.uid_element(info, uid)
        info.append(V("DeviceType", 1 if instrument else 2))
        d.append(V("MpeEnabled", "false"))
        mpe = ET.SubElement(d, "MpeSettings")
        for tag, v in (("ZoneType", 0), ("FirstNoteChannel", 1), ("LastNoteChannel", 15)):
            mpe.append(V(tag, v))
        plist = ET.SubElement(d, "ParameterList")
        slots = {}
        # Live stores a VST3 parameter in the plugin's plain units (IEditController::normalizedParamToPlain).
        # JUCE's plain value is the normalised value for every parameter, menus included (sets Live wrote:
        # Gatelab Quantization 0.636, SoftLimit Oversampling 0.333), so Manual and ranges are 0-1. The one
        # exception is the Program parameter JUCE adds when a plugin has programs (CONTROL's 16 songs): its
        # plain value is the program number, and Live lists it after the plugin's own parameters (SoftLimit:
        # Manual 9, range 0-9). JUCE's Bypass parameter Live doesn't list.
        entries = [(p["name"], vst3_param_id(p["id"]), f"{p['norm']:.9g}", 1, p) for p in st["params"]]
        if st["programs"] > 1:
            entries.append(("Program", JUCE_PROGRAM_PARAM_ID, st["program"], st["programs"] - 1, None))
        assert len(entries) <= 128, module
        for k in range(128):
            e = entries[k] if k < len(entries) else None
            slot = ET.SubElement(plist, "PluginFloatParameter")
            slot.set("Id", str(k))
            slot.append(V("ParameterName", e[0] if e else ""))
            slot.append(V("ParameterId", e[1] if e else -1))
            slot.append(V("ParameterIdFlankBool", "false"))
            slot.append(V("VisualIndex", k if e else 1073741823))
            pv = ET.SubElement(slot, "ParameterValue")
            pv.append(V("LomId", 0))
            pv.append(V("Manual", e[2] if e else "0.1234567687"))
            r = ET.SubElement(pv, "MidiControllerRange")
            r.append(V("Min", 0))
            r.append(V("Max", e[3] if e else 1))
            pv.append(self.ids.target("AutomationTarget"))
            pv.append(self.ids.target("ModulationTarget"))
            ur = ET.SubElement(slot, "LastUserRange")
            ur.append(V("First", 0 if e else "Invalid"))
            ur.append(V("Last", e[3] if e else "Invalid"))
            ir = ET.SubElement(slot, "LastInternalRange")
            ir.append(V("First", 0))
            ir.append(V("Last", 1))
            if e and e[4]:
                slots[e[4]["id"]] = (slot, e[4])
        d.append(V("AxisX", 0))
        d.append(V("AxisY", 0))
        sc = ET.SubElement(d, "SideChain")
        onoff = ET.SubElement(sc, "OnOff")
        onoff.append(V("LomId", 0))
        onoff.append(V("Manual", "true"))
        onoff.append(self.ids.target("AutomationTarget"))
        th = ET.SubElement(onoff, "MidiCCOnOffThresholds")
        th.append(V("Min", 64))
        th.append(V("Max", 127))
        routed = ET.SubElement(sc, "RoutedInput")
        routable = copy.deepcopy(self.routing_pattern)
        routable.tag = "Routable"
        setv(routable, "Target", "AudioIn/None")
        setv(routable, "UpperDisplayString", "No Output")
        setv(routable, "LowerDisplayString", "")
        routed.append(routable)
        vol = ET.SubElement(routed, "Volume")
        vol.append(V("LomId", 0))
        vol.append(V("Manual", 1))
        r = ET.SubElement(vol, "MidiControllerRange")
        r.append(V("Min", "0.0003162277571"))
        r.append(V("Max", "15.8489332"))
        vol.append(self.ids.target("AutomationTarget"))
        vol.append(self.ids.target("ModulationTarget"))
        dw = ET.SubElement(sc, "DryWet")
        dw.append(V("LomId", 0))
        dw.append(V("Manual", 1))
        r = ET.SubElement(dw, "MidiControllerRange")
        r.append(V("Min", 0))
        r.append(V("Max", 1))
        dw.append(self.ids.target("AutomationTarget"))
        dw.append(self.ids.target("ModulationTarget"))
        self.ids.fresh(d)                              # the copied header's pointees too
        self.params[id(d)] = slots
        return d

    @staticmethod
    def uid_element(parent, uid):
        u = ET.SubElement(parent, "Uid")
        for i, v in enumerate(uid):
            u.append(V(f"Fields.{i}", v))

    # -------------------------------------------------------------------------------------- tracks
    def make_track(self, spec, track_id):
        kind = spec["kind"]
        if kind == "group" and self.t_group is not None:
            t = copy.deepcopy(self.t_group)
            slots = t.find("Slots")
            for c in list(slots):
                slots.remove(c)
            for i in range(self.scenes):
                g = ET.SubElement(slots, "GroupTrackSlot")
                g.set("Id", str(i))
                g.append(V("LomId", 0))
            setv(t, "TrackUnfolded", "true")
        else:
            t = copy.deepcopy(self.t_midi if kind in ("midi", "group") else self.t_audio if kind == "audio" else self.t_return)
        if kind == "group" and self.t_group is None:
            t.tag = "GroupTrack"
            slots = ET.Element("Slots")
            for i in range(self.scenes):
                s = ET.SubElement(slots, "GroupTrackSlot")
                s.set("Id", str(i))
                s.append(V("LomId", 0))
            at = [c.tag for c in t].index("DeviceChain")
            t.insert(at, slots)
            # Live 12 groups carry the freeze state too (group freezing), after the slots.
            for i, tag in enumerate(("Freeze", "NeedArrangerRefreeze")):
                t.insert(at + 1 + i, copy.deepcopy(self.t_audio.find(tag)))
            setv(t, "TrackUnfolded", "true")
        t.set("Id", str(track_id))
        setv(t, "Name/EffectiveName", spec["name"])
        setv(t, "Name/UserName", spec["name"])
        setv(t, "Color", spec["colour"])
        setv(t, "TrackGroupId", -1)
        self.ids.fresh(t)
        if kind in ("midi", "audio"):
            setv(t, "DeviceChain/MainSequencer/MonitoringEnum", MONITOR[spec.get("monitor", "in" if kind == "midi" else "auto")])
        devs = t.find("DeviceChain/DeviceChain/Devices")
        self.devices[spec["name"]] = {}
        for i, (module, overrides) in enumerate(spec["devices"]):
            d = self.plugin_device(module, overrides, i)
            devs.append(d)
            self.devices[spec["name"]][module] = d
        return t

    def route(self, t, path, target, upper, lower=""):
        r = t.find("DeviceChain/" + path)
        setv(r, "Target", target)
        setv(r, "UpperDisplayString", upper)
        setv(r, "LowerDisplayString", lower)

    def out_target(self, out, ids):
        if out == "main":
            return "AudioOut/Main", "Main", ""
        if out == "group":
            return "AudioOut/GroupTrack", "Group", ""
        if out == "none":
            return "AudioOut/None", "Sends Only", ""
        if out == "pa":
            out = PA_OUT
        if out.startswith("ext:S"):
            n = int(out[5:])
            return f"AudioOut/External/S{n}", "Ext. Out", f"{2 * n + 1}/{2 * n + 2}"
        return f"AudioOut/Track.{ids[out]}/TrackIn", out, "Track In"

    def set_sends(self, t, returns, enabled):
        sends = t.find("DeviceChain/Mixer/Sends")
        pattern = copy.deepcopy(sends[0])
        for c in list(sends):
            sends.remove(c)
        for i in range(returns):
            h = copy.deepcopy(pattern)
            h.set("Id", str(i))
            setv(h, "Send/Manual", "0.0003162277571")
            setv(h, "EnabledByUser", "true" if enabled else "false")
            self.ids.fresh(h)
            sends.append(h)

    def build(self):
        tracks_el = self.live.find("Tracks")
        for c in list(tracks_el):
            tracks_el.remove(c)
        returns = [s for s in SHOW if s["kind"] == "return"]
        ids, built = {}, []
        next_track = 10
        for s in SHOW:
            if s["kind"] == "return":
                ids[s["name"]] = len([x for x in ids if x in [r["name"] for r in returns]]) + 2
            else:
                ids[s["name"]] = next_track
                next_track += 1
        for s in SHOW:
            t = self.make_track(s, ids[s["name"]])
            if s.get("group"):
                setv(t, "TrackGroupId", ids[s["group"]])
            out = "group" if s.get("group") else s.get("out", "main")
            self.route(t, "AudioOutputRouting", *self.out_target(out, ids))
            audio_in = s.get("audio_in")
            if audio_in == "none":
                self.route(t, "AudioInputRouting", "AudioIn/None", "No Input")
            elif audio_in == "main":                    # the Main's output after its fader: the finished set
                self.route(t, "AudioInputRouting", "AudioIn/Track.-1/TrackOut", "Main", "Post Mixer")
            elif audio_in and audio_in.startswith("ext:S"):           # a stereo pair of the interface's inputs
                n = int(audio_in[5:])
                self.route(t, "AudioInputRouting", f"AudioIn/External/S{n}", "Ext. In", f"{2 * n + 1}/{2 * n + 2}")
            if s["kind"] == "midi":                     # one port, one channel: never All Ins, every part at once
                if s.get("midi"):
                    port, ch = s["midi"]
                    dev = ONE_PORTS[port]
                    self.route(t, "MidiInputRouting", f"MidiIn/External.Dev:{dev}/{ch - 1}", dev.split("{")[0], f"Ch. {ch}")
                else:
                    self.route(t, "MidiInputRouting", "MidiIn/None", "No Input")
            self.set_sends(t, len(returns), s.get("sends", s["kind"] != "return"))
            built.append(t)
        for t in built:                                 # Live lists the returns after every other track
            tracks_el.append(t)
        # SendsPre: one entry per return
        sp = self.live.find("SendsPre")
        for c in list(sp):
            sp.remove(c)
        for i in range(len(returns)):
            e = V("SendPreBool", "false")
            e.set("Id", str(i))
            sp.append(e)
        # Main
        main = self.live.find("MainTrack")
        devs = main.find("DeviceChain/DeviceChain/Devices")
        for c in list(devs):
            devs.remove(c)
        self.devices["Main"] = {}
        for i, (module, overrides) in enumerate(MAIN_DEVICES):
            d = self.plugin_device(module, overrides, i)
            devs.append(d)
            self.devices["Main"][module] = d
        self.route(main, "AudioOutputRouting", *self.out_target(MAIN_OUT, ids))
        setv(main, "Color", MAIN_COLOUR)
        self.route(self.live.find("PreHearTrack"), "AudioOutputRouting", *self.out_target(CUE_OUT, ids))
        self.clean_base(main)
        self.tracks = {s["name"]: t for s, t in zip(SHOW, built)}
        self.track_ids = ids
        self.map_e16()
        setv(self.live, "NextPointeeId", max(int(e.get("Id")) for e in self.root.iter() if e.get("Id", "").lstrip("-").isdigit()) + 1)

    def clean_base(self, main):
        """What the base set's owner left behind: tempo automation, the playhead, the scroll position."""
        tempo = main.find("DeviceChain/Mixer/Tempo")
        setv(tempo, "Manual", TEMPO)
        tempo_target = tempo.find("AutomationTarget").get("Id")
        envelopes = main.find("AutomationEnvelopes/Envelopes")
        for env in list(envelopes):
            events = env.find("Automation/Events")
            for e in list(events):
                if e.get("Time") != "-63072000":      # the envelope's own start value, which Live always writes
                    events.remove(e)
            if len(events) == 0:
                envelopes.remove(env)
            elif env.find("EnvelopeTarget/PointeeId").get("Value") == tempo_target:
                for e in events:
                    e.set("Value", str(TEMPO))
        setv(self.live, "Transport/CurrentTime", 0)
        for tag in ("AnchorTime", "OtherTime"):
            setv(self.live, "TimeSelection/" + tag, 0)
        pos = self.live.find("SequencerNavigator/ScrollerPos")
        pos.set("X", "0")
        pos.set("Y", "0")
        setv(self.live, "HighlightedTrackIndex", 0)
        grooves = self.live.find("GroovePool/Grooves")         # the base owner's grooves
        if grooves is not None and not any(e.get("Value") not in (None, "-1") for e in self.root.iter("GrooveId")):
            for g in list(grooves):
                grooves.remove(g)

    # -------------------------------------------------------------------------------------- mappings
    @staticmethod
    def key_midi(note, channel, number):
        k = ET.Element("KeyMidi")
        for tag, v in (("PersistentKeyString", ""), ("IsNote", "true" if note else "false"), ("Channel", channel - 1),
                       ("NoteOrController", number), ("LowerRangeNote", -1), ("UpperRangeNote", -1), ("ControllerMapMode", 0)):
            k.append(V(tag, v))
        return k

    def map_e16(self):
        self.mapped = []
        for (msg, ch, n), (target, places, kind, rng, item) in L.addresses().items():
            if getattr(item, "mixer", None):
                t = self.tracks[item.track]
                if item.mixer == "volume":
                    value = t.find("DeviceChain/Mixer/Volume")
                elif item.mixer in ("send A", "send B"):
                    value = t.find("DeviceChain/Mixer/Sends")[" AB".index(item.mixer[-1]) - 1].find("Send")
                else:
                    raise SystemExit(f"no mixer control {item.mixer}")
                if item.max_value is not None:
                    setv(value, "MidiControllerRange/Max", f"{item.max_value:.9g}")
            else:
                dev = self.devices[item.track][item.module]
                slot, p = self.params[id(dev)][item.pid]
                value = slot.find("ParameterValue")
                if getattr(item, "max_value", None) is not None:
                    top = sftests_state(self.sftests, item.module, {item.pid: item.max_value}, 1)
                    norm = next(q for q in top["params"] if q["id"] == item.pid)["norm"]
                    setv(value, "MidiControllerRange/Max", f"{norm:.9g}")
            assert value.find("KeyMidi") is None, target
            value.insert(1, self.key_midi(msg == "NOTE", ch, n))   # right after LomId, as Live writes it
            self.mapped.append((msg, ch, n, target))


# ---------------------------------------------------------------------------------------------- checks
def check(root, scenes, track_ids):
    """The rules Live enforces without saying so (and a few of our own)."""
    problems = []
    all_ids = [int(e.get("Id")) for e in root.iter() if e.get("Id", "").lstrip("-").isdigit()]
    nxt = int(root.find("LiveSet/NextPointeeId").get("Value"))
    if nxt <= max(all_ids):
        problems.append(f"NextPointeeId {nxt} not above the highest Id {max(all_ids)}")
    pointees = [e.get("Id") for e in root.iter() if "Id" in e.attrib and (e.tag == "Pointee" or e.find("LockEnvelope") is not None)]
    dup = {x for x in pointees if pointees.count(x) > 1}
    if dup:
        problems.append(f"pointee ids used twice: {sorted(dup)[:8]}")
    for lst in root.iter("ClipSlotList"):
        if len(lst) not in (0, scenes):
            problems.append(f"a ClipSlotList holds {len(lst)} slots for {scenes} scenes")
    for g in root.iter("GroupTrack"):
        if len(g.find("Slots")) != scenes:
            problems.append("a group's Slots don't match the scenes")
    tids = {t.get("Id") for t in root.find("LiveSet/Tracks")}
    if len(tids) != len(root.find("LiveSet/Tracks")):
        problems.append("two tracks share an Id")
    groups = {t.get("Id") for t in root.find("LiveSet/Tracks") if t.tag == "GroupTrack"}
    for t in root.find("LiveSet/Tracks"):
        g = t.find("TrackGroupId").get("Value")
        if g != "-1" and g not in groups:
            problems.append(f"{t.find('Name/EffectiveName').get('Value')} is in a group that doesn't exist")
    for e in root.iter("Target"):
        v = e.get("Value", "")
        if v.startswith(("AudioOut/Track.", "AudioIn/Track.")) and v.split("/")[1].split(".")[1] not in tids | {"-1"}:
            problems.append(f"routing to a track that doesn't exist: {v}")
    keys = [(k.find("IsNote").get("Value"), k.find("Channel").get("Value"), k.find("NoteOrController").get("Value"))
            for k in root.iter("KeyMidi")]
    dupk = {x for x in keys if keys.count(x) > 1}
    if dupk:
        problems.append(f"one MIDI address mapped twice: {sorted(dupk)[:5]}")
    return problems


def load(path):
    raw = open(path, "rb").read()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    root = ET.fromstring(raw)
    minor = root.get("MinorVersion", "")
    if root.tag != "Ableton" or not minor.startswith("12."):
        raise SystemExit(f"{path}: not a Live 12 set (MinorVersion {minor})")
    return root


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--base", required=True, help="an empty Live 12 set saved from your Live")
    ap.add_argument("--out", default=os.path.join(HERE, "SF SHOW.als"))
    ap.add_argument("--sftests", default=os.path.join(ROOT, "build-test", "SFTests_artefacts", "Release", "SFTests"))
    a = ap.parse_args()
    if not os.path.exists(a.sftests):
        raise SystemExit(f"no SFTests at {a.sftests}: cmake -B build-test -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF && cmake --build build-test")
    base = load(a.base)
    b = Builder(base, a.sftests)
    b.build()
    problems = check(base, b.scenes, b.track_ids)
    if problems:
        raise SystemExit("not written:\n  " + "\n  ".join(problems))
    xml = ET.tostring(base, encoding="unicode")
    data = ('<?xml version="1.0" encoding="UTF-8"?>\n' + xml + "\n").encode("utf-8")
    with open(a.out, "wb") as raw, gzip.GzipFile(fileobj=raw, mode="wb", filename="", mtime=0) as f:
        f.write(data)                                   # no name or time in the header: the same input, the same file
    n_dev = sum(len(v) for v in b.devices.values())
    print(f"{a.out}: {len(SHOW)} tracks, {n_dev} SF plugins, {len(b.mapped)} E16 mappings "
          f"(Live {base.get('Creator', '?').replace('Ableton Live ', '')} format)")
    print("MIDI From (set):")
    for s in SHOW:
        if s.get("midi"):
            print(f"  {s['name']:12s} ONE port {s['midi'][0]}, ch {s['midi'][1]}")
    print("Audio setup in Live's Settings:")
    for s in SHOW:
        if s.get("setup"):
            print(f"  {s['name']:12s} {s['setup']}")


if __name__ == "__main__":
    main()
