cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

project(ARPOptoComp VERSION 1.1.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(ARPAssets SOURCES
    assets/bg.png assets/glass.png assets/knob.png
    assets/bay0.png assets/bay1.png assets/bay2.png assets/bay3.png assets/bay4.png)

juce_add_plugin(ARPOptoComp
    COMPANY_NAME "ARPEGIA"
    PLUGIN_MANUFACTURER_CODE Arpg
    PLUGIN_CODE Aoc1
    FORMATS VST3 AU
    PRODUCT_NAME "ARP-Opto-comp"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(ARPOptoComp PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)

target_compile_definitions(ARPOptoComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(ARPOptoComp
    PRIVATE
        ARPAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
