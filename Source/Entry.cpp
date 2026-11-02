// One file per plugin target: SF_MODULE_ID (set by CMake) chooses the module.
#include "core/Processor.h"
#include "core/Registry.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SFProcessor (sf::makeModule (SF_MODULE_ID));
}
