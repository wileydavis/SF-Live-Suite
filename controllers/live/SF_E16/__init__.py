"""SF E16: Live's side of the E16 scenes' TRACK FOCUS [Nov 8].

When a PERF track page comes up, the scene's script sends CC 119 on channel 16 with the page's track
number (controllers/e16/layout.py FOCUS). This control surface selects the track whose name starts
with that number: page 6 (BAS) selects 6 BASS. NRG and MIX's pages send nothing, so the selection
stays where it was. Selecting is what a click on the track's name does; nothing else changes.

Install (once per computer):
  1. Copy this folder (SF_E16) into ~/Music/Ableton/User Library/Remote Scripts/ (make the Remote
     Scripts folder if it isn't there). Restart Live.
  2. Settings > Tempo & MIDI (Link, Tempo & MIDI before 12.4): a free Control Surface slot: SF E16,
     Input: OXI E16 (Port 1), Output: None. Keep Remote on for the E16's Port 1 input and output, as
     the scenes need it.
  3. Optional: Settings > Plug-Ins > Auto-Hide Plug-In Windows on: the selected track's open plugin
     windows come up with the page, the others hide.

Python 3, Live 11 and later (Live's own _Framework). No Live API but track selection and, for a
track inside a folded group that Live won't select, unfolding its groups.
"""
from .surface import SFE16


def create_instance(c_instance):
    return SFE16(c_instance)
