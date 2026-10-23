cmake_minimum_required(VERSION 3.22)
project(LifePedalSim VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(LIFEPEDAL_BUILD_TESTS "Build the offline DSP test" OFF)
option(LIFEPEDAL_MINIMAL_X11 "Linux only: build without Xrandr/Xinerama/Xcursor headers" OFF)

# JUCE: use a local ./JUCE checkout if present, otherwise download it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(LIFEPEDAL_FORMATS VST3)
if(APPLE)
    list(APPEND LIFEPEDAL_FORMATS AU)
endif()

juce_add_plugin(LifePedalSim
    COMPANY_NAME "LifePedalSim"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Lpsm
    PLUGIN_CODE Lpd1
    FORMATS ${LIFEPEDAL_FORMATS}
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Life Pedal Sim")

target_sources(LifePedalSim PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/LifePedalDSP.h)

target_compile_definitions(LifePedalSim PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(LIFEPEDAL_MINIMAL_X11)
    target_compile_definitions(LifePedalSim PUBLIC
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0)
endif()

target_link_libraries(LifePedalSim
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(LIFEPEDAL_BUILD_TESTS)
    add_executable(dsp_test Tests/dsp_test.cpp)
    target_include_directories(dsp_test PRIVATE Source)
    target_compile_definitions(dsp_test PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(dsp_test PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
endif()
