cmake_minimum_required(VERSION 3.22)

project(MOTSTAND VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: bygg in C++-runtime i pluginet, så att köparen inte behöver installera något extra
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
# Filnamnet är MOTSTAND (utan Å) så att det funkar överallt. I pluginet heter den MOTSTÅND.
set(PHZ_EXTRA_FORMATS "")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PHZ_EXTRA_FORMATS LV2)
endif()

# DEMO: cmake -DMOTSTAND_DEMO=ON ger MOTSTAND-DEMO, ett eget plugin som kan ligga bredvid fullversionen
option(MOTSTAND_DEMO "Bygg demoversionen" OFF)
if(MOTSTAND_DEMO)
    set(MOTSTAND_PRODUCT "MOTSTAND-DEMO")
    set(MOTSTAND_BUNDLE "com.phunkshunz.motstanddemo")
    set(MOTSTAND_CODE Mtdm)
    set(MOTSTAND_LV2 "https://phunkshunz.github.io/lv2/MotstandDemo")
else()
    set(MOTSTAND_PRODUCT "MOTSTAND")
    set(MOTSTAND_BUNDLE "com.phunkshunz.motstand")
    set(MOTSTAND_CODE Mtsd)
    set(MOTSTAND_LV2 "https://phunkshunz.github.io/lv2/Motstand")
endif()

juce_add_plugin(Motstand
    COMPANY_NAME "phunkshunz"
    PRODUCT_NAME "${MOTSTAND_PRODUCT}"
    BUNDLE_ID "${MOTSTAND_BUNDLE}"
    PLUGIN_MANUFACTURER_CODE Phnk
    PLUGIN_CODE ${MOTSTAND_CODE}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Drum
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    LV2URI "${MOTSTAND_LV2}"
    FORMATS VST3 AU Standalone ${PHZ_EXTRA_FORMATS})

set(MOTSTAND_SOURCES src/PluginProcessor.cpp src/PluginEditor.cpp)
set(MOTSTAND_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

target_sources(Motstand PRIVATE ${MOTSTAND_SOURCES})
target_compile_definitions(Motstand PUBLIC ${MOTSTAND_DEFS})
if(MOTSTAND_DEMO)
    target_compile_definitions(Motstand PUBLIC MOTSTAND_DEMO=1)
endif()
target_link_libraries(Motstand
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# MSVC: läs källkoden som UTF-8 (å, ä, ö), och stora filer (nyckelpoolen) får plats
if(MSVC)
    target_compile_options(Motstand PRIVATE /utf-8 /bigobj)
endif()

# Lokalt Linux-bygge utan ljudkort och X-tillägg (används bara i testmiljön, aldrig i Actions)
option(MOTSTAND_MINIMAL_LINUX "Bygg utan ALSA/JACK och X-tillägg" OFF)
set(MOTSTAND_MINIMAL_DEFS "")
if(MOTSTAND_MINIMAL_LINUX)
    set(MOTSTAND_MINIMAL_DEFS JUCE_ALSA=0 JUCE_JACK=0 JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0
        JUCE_USE_XCURSOR=0 JUCE_USE_XRENDER=0 JUCE_USE_XSHM=0)
    target_compile_definitions(Motstand PUBLIC ${MOTSTAND_MINIMAL_DEFS})
endif()

# Tester (byggs bara när man ber om det): cmake -B build -DMOTSTAND_TESTS=ON
# Testverktygen körs upplåsta (MOTSTAND_TEST_UNLOCKED). Det som säljs byggs aldrig så.
option(MOTSTAND_TESTS "Bygg MOTSTÅND-testverktygen" OFF)
if(MOTSTAND_TESTS)
    set(MOTSTAND_TEST_DEFS ${MOTSTAND_DEFS} ${MOTSTAND_MINIMAL_DEFS} MOTSTAND_TEST_UNLOCKED=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    if(MOTSTAND_DEMO)
        list(APPEND MOTSTAND_TEST_DEFS MOTSTAND_DEMO=1)
    endif()
    function(motstand_tool name src)
        juce_add_console_app(${name} PRODUCT_NAME "${name}")
        target_sources(${name} PRIVATE ${src} ${MOTSTAND_SOURCES})
        target_compile_definitions(${name} PRIVATE ${MOTSTAND_TEST_DEFS})
        target_link_libraries(${name} PRIVATE juce::juce_audio_utils juce::juce_dsp
            PUBLIC juce::juce_recommended_config_flags)
        if(MSVC)
            target_compile_options(${name} PRIVATE /utf-8 /bigobj)
        endif()
    endfunction()
    motstand_tool(MotstandTest tests/plugin_test.cpp)       # plugintestet
    motstand_tool(MotstandTour tests/preset_tour.cpp)       # rundtur genom presets till WAV
    motstand_tool(MotstandSnapshot tests/gui_snapshot.cpp)  # panelen till PNG utan fönster
endif()
