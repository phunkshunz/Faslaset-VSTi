cmake_minimum_required(VERSION 3.22)

project(MOTSTAND VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE hämtas automatiskt vid bygget (eller från JUCE_DIR om den finns lokalt)
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# MOTSTÅND gör allt ljud själv (909-syntes). MIDI in triggar instrumenten (GM 909-map),
# MIDI ut skickar varje steg som not, med drift och jitter inbakat.
set(PHZ_EXTRA_FORMATS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PHZ_EXTRA_FORMATS LV2)
endif()
juce_add_plugin(Motstand
    COMPANY_NAME "Phunkshunz AUDIO"
    PRODUCT_NAME "MOTSTAND"
    BUNDLE_ID "com.phunkshunz.motstand"
    PLUGIN_MANUFACTURER_CODE Phnk
    PLUGIN_CODE Mtsd
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "https://phunkshunz.github.io/lv2/Motstand"
    FORMATS VST3 AU Standalone ${PHZ_EXTRA_FORMATS})

set(MOTSTAND_SOURCES src/PluginProcessor.cpp)
set(MOTSTAND_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(Motstand PRIVATE ${MOTSTAND_SOURCES})
target_compile_definitions(Motstand PUBLIC ${MOTSTAND_DEFS})
target_link_libraries(Motstand
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Windows: källkoden är UTF-8 (å, ä, ö i texterna)
if(MSVC)
    target_compile_options(Motstand PRIVATE /utf-8)
endif()

# Linux-bygget behöver inget ljudkort och inga X-tillägg
if(UNIX AND NOT APPLE AND TARGET Motstand)
    target_compile_definitions(Motstand PUBLIC JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0)
endif()

# Tester (byggs bara när man ber om det): cmake -B build -DMOTSTAND_TESTS=ON
option(MOTSTAND_TESTS "Bygg MOTSTÅND-pluginetestet" OFF)
if(MOTSTAND_TESTS)
    juce_add_console_app(MotstandTest PRODUCT_NAME "MotstandTest")
    target_sources(MotstandTest PRIVATE tests/plugin_test.cpp ${MOTSTAND_SOURCES})
    target_compile_definitions(MotstandTest PRIVATE ${MOTSTAND_DEFS} JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(MotstandTest PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
