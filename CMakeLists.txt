cmake_minimum_required(VERSION 3.22)

project(SFLiveSuite VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE lives in ./JUCE (build.sh clones it on first run, pinned to a tag).
add_subdirectory(JUCE)

if(APPLE)
    set(SF_FORMATS AU VST3)
else()
    set(SF_FORMATS VST3)
endif()

# Every plugin compiles the whole core and every module; the entry file picks one module.
file(GLOB SF_CORE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Source/core/*.cpp)
file(GLOB SF_MODULE_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/Source/modules/*.cpp)
set(SF_ALL_SOURCES ${SF_CORE_SOURCES} ${SF_MODULE_SOURCES})

set(SF_DEFINES
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    # Off: otherwise JUCE adds 2,080 hidden MIDI CC parameters and Live stops listing the real ones.
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

set(SF_LIBS
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_dsp)

# sf_plugin(<module id> <4-char plugin code> <instrument TRUE/FALSE> <product name>)
function(sf_plugin MODULE_ID CODE IS_SYNTH PRODUCT)
    string(REPLACE " " "" TARGET "SF${MODULE_ID}")
    string(TOLOWER "${TARGET}" BUNDLE_SUFFIX)
    if(IS_SYNTH)
        set(CATS Instrument)
        set(AUTYPE kAudioUnitType_MusicDevice)
    else()
        set(CATS Fx)
        set(AUTYPE kAudioUnitType_Effect)
    endif()
    juce_add_plugin(${TARGET}
        COMPANY_NAME                "Strange Frequencies"
        COMPANY_WEBSITE             "https://strangefrequencies.fm"
        BUNDLE_ID                   "fm.strangefrequencies.${BUNDLE_SUFFIX}"
        PLUGIN_MANUFACTURER_CODE    Sfrq
        PLUGIN_CODE                 ${CODE}
        PRODUCT_NAME                "${PRODUCT}"
        FORMATS                     ${SF_FORMATS}
        IS_SYNTH                    ${IS_SYNTH}
        NEEDS_MIDI_INPUT            ${IS_SYNTH}
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES             ${CATS}
        AU_MAIN_TYPE                ${AUTYPE}
        COPY_PLUGIN_AFTER_BUILD     TRUE)
    target_sources(${TARGET} PRIVATE ${SF_ALL_SOURCES} ${CMAKE_SOURCE_DIR}/Source/Entry.cpp)
    target_include_directories(${TARGET} PRIVATE ${CMAKE_SOURCE_DIR}/Source ${CMAKE_SOURCE_DIR}/third_party)
    target_compile_definitions(${TARGET} PUBLIC ${SF_DEFINES} SF_MODULE_ID="${MODULE_ID}")
    target_link_libraries(${TARGET}
        PRIVATE ${SF_LIBS}
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
endfunction()

option(SF_BUILD_PLUGINS "Build the plugins" ON)
set(SF_ONLY "" CACHE STRING "Build only this module id (empty = all)")

if(SF_BUILD_PLUGINS)
    # Instruments (MIDI tracks)
    set(SF_LIST
        "KICK|SfKk|TRUE|SF KICK"
        "PERC|SfPc|TRUE|SF PERC"
        "RUMBLE|SfRm|TRUE|SF RUMBLE"
        "BASS|SfBs|TRUE|SF BASS"
        "LEAD|SfLd|TRUE|SF LEAD"
        "PAD|SfPd|TRUE|SF PAD"
        "VOICE|SfVc|TRUE|SF VOICE"
        "DECK|SfDk|TRUE|SF DECK"
        "CONTROL|SfCt|TRUE|SF CONTROL"
        # Effects
        "FILTER|SfFl|FALSE|SF FILTER"
        "GATE|SfGt|FALSE|SF GATE"
        "DENSITY|SfDn|FALSE|SF DENSITY"
        "DELAY|SfDl|FALSE|SF DELAY"
        "REVERB|SfRv|FALSE|SF REVERB"
        "DRIVE|SfDr|FALSE|SF DRIVE"
        "RADIO|SfRd|FALSE|SF RADIO"
        "REPEAT|SfRp|FALSE|SF REPEAT"
        "DUCK|SfDu|FALSE|SF DUCK"
        "ENERGY|SfEn|FALSE|SF ENERGY"
        # Master / mix
        "ISOLATOR|SfIs|FALSE|SF ISOLATOR"
        "BUILD|SfBu|FALSE|SF BUILD"
        "GLUE|SfGl|FALSE|SF GLUE"
        "LOWEND|SfLe|FALSE|SF LOWEND"
        "LIMIT|SfLm|FALSE|SF LIMIT"
        "ROOMEQ|SfRq|FALSE|SF ROOM EQ"
        "ALLOC|SfAl|FALSE|SF ALLOC"
        "TAP|SfTp|FALSE|SF TAP")
    foreach(ENTRY ${SF_LIST})
        string(REPLACE "|" ";" PARTS "${ENTRY}")
        list(GET PARTS 0 MID)
        list(GET PARTS 1 MCODE)
        list(GET PARTS 2 MSYNTH)
        list(GET PARTS 3 MNAME)
        if(SF_ONLY STREQUAL "" OR SF_ONLY STREQUAL MID)
            sf_plugin(${MID} ${MCODE} ${MSYNTH} "${MNAME}")
        endif()
    endforeach()
endif()

# Offline tools:  cmake -B build -DSF_BUILD_TESTS=ON -DSF_BUILD_PLUGINS=OFF
#   SFTests     - runs every module through audio, MIDI, parameter sweeps and bus checks
#   SFSnapshot  - renders every editor to PNG (docs/snapshots)
option(SF_BUILD_TESTS "Build offline test tools" OFF)
if(SF_BUILD_TESTS)
    function(sf_tool name src)
        juce_add_console_app(${name} PRODUCT_NAME "${name}")
        target_sources(${name} PRIVATE ${src} ${SF_ALL_SOURCES})
        target_include_directories(${name} PRIVATE ${CMAKE_SOURCE_DIR}/Source ${CMAKE_SOURCE_DIR}/third_party)
        target_compile_definitions(${name} PRIVATE ${SF_DEFINES} JUCE_MODAL_LOOPS_PERMITTED=1 SF_MODULE_ID="TEST")
        target_link_libraries(${name} PRIVATE ${SF_LIBS} juce::juce_recommended_config_flags)
    endfunction()
    sf_tool(SFTests tests/SuiteTests.cpp)
    sf_tool(SFSnapshot tests/Snapshot.cpp)
endif()
