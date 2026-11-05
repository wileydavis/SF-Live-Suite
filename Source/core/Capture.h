#pragma once
// In-set place capture. SF TERROIR (on a return) can't send a sweep to a speaker of its own or hear
// microphones: its sound goes where its track goes, and in comes what the sends bring. So the room
// mics' own track holds SF CAPTURE: asked over the SF BUS (v9, BusData::cap*), it plays the sweep
// file out of its track's output (the headphone out, a speaker on it), records its input (the mics)
// on the same clock, writes the take, and TERROIR analyses it into the slot it asked for.
#include <juce_core/juce_core.h>
#include <atomic>
#include <cstdint>
#include <string>

namespace sf::capture
{
enum State : int32_t { Idle = 0, Preparing = 1, Playing = 2, Writing = 3, Done = 4, Failed = 5 };
enum Error : int32_t { None = 0, Cancelled = 1, TransportStarted = 2, CantWrite = 3, NoMemory = 4 };
constexpr double kTailSec = 3.0;            // recorded after the sweep file ends: the last sweep's room, the round trip
inline std::string testFolder;              // tests only: takes go here, not to Music/SF Live Suite/TERROIR/Captures

inline bool busy (int32_t state) { return state == Preparing || state == Playing || state == Writing; }

// Where a take goes: one file per request, named by when it was asked for (the slot takes the name).
inline juce::File folder()
{
    if (! testFolder.empty()) return juce::File (testFolder);
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("SF Live Suite")
               .getChildFile ("TERROIR").getChildFile ("Captures");
}
inline juce::File fileFor (int64_t stampMs)
{
    return folder().getChildFile ("ROOM " + juce::Time (stampMs).formatted ("%Y-%m-%d %H.%M.%S") + ".wav");
}
} // namespace sf::capture
