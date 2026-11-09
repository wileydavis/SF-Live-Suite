"""The control surface: one CC from the E16 (FOCUS), which Live forwards to this script. The E16's port
has Track off, so no track sees it either way."""
import Live
from _Framework.ControlSurface import ControlSurface
from _Framework.EncoderElement import EncoderElement
from _Framework.InputControlElement import MIDI_CC_TYPE

from .focus import FOCUS_CC, FOCUS_CHANNEL, select


class SFE16(ControlSurface):

    def __init__(self, c_instance):
        super(SFE16, self).__init__(c_instance)
        with self.component_guard():
            self._focus = EncoderElement(MIDI_CC_TYPE, FOCUS_CHANNEL, FOCUS_CC, Live.MidiMap.MapMode.absolute)
            self._focus.add_value_listener(self._on_focus)
        self.log_message("SF E16: E16 pages select their tracks (CC %d, channel %d)" % (FOCUS_CC, FOCUS_CHANNEL + 1))

    def _on_focus(self, value):
        select(self.song(), value)

    def disconnect(self):
        if self._focus is not None:
            self._focus.remove_value_listener(self._on_focus)
            self._focus = None
        super(SFE16, self).disconnect()
