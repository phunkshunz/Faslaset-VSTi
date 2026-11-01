cmake_minimum_required(VERSION 3.22)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minsta macOS-version")
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal Binary")
endif()
project(VEVHUS VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

set(VEVHUS_FORMATS VST3)
if(APPLE)
    list(APPEND VEVHUS_FORMATS AU)
endif()

juce_add_plugin(Vevhus
    COMPANY_NAME                "Phunkshunz AUDIO"
    COMPANY_WEBSITE             "https://phunkshunz.github.io/maskinhallen"
    BUNDLE_ID                   "com.phunkshunz.vevhus"
    PLUGIN_MANUFACTURER_CODE    Phnk
    PLUGIN_CODE                 Vvhs
    PRODUCT_NAME                "VEVHUS"
    DESCRIPTION                 "MASKINHALLEN - steam engine crankcase"
    FORMATS                     ${VEVHUS_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
)
target_sources(Vevhus PRIVATE src/PluginProcessor.cpp src/PluginEditor.cpp)
target_compile_definitions(Vevhus PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(Vevhus PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
