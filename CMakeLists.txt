cmake_minimum_required(VERSION 3.22)
project(EqualLoudnessProcessor VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# Use a local JUCE checkout if given (-DJUCE_DIR=...), otherwise download JUCE 8.0.4.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)
endif()

set(ELP_FORMATS Standalone VST3)
if(APPLE)
    list(APPEND ELP_FORMATS AU)
endif()

juce_add_plugin(EqualLoudnessProcessor
    COMPANY_NAME "Concierge of Sound"
    BUNDLE_ID "com.conciergeofsound.equalloudnessprocessor"
    PRODUCT_NAME "Equal-Loudness Processor"
    PLUGIN_MANUFACTURER_CODE Cosd
    PLUGIN_CODE Elpr
    FORMATS ${ELP_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "The processor reads your audio interface inputs: the program feed and the measurement mics."
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(EqualLoudnessProcessor PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(EqualLoudnessProcessor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EqualLoudnessProcessor
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Offline test harness (not part of the app)
option(ELP_TESTS "Build the offline test" OFF)
if(ELP_TESTS)
    juce_add_console_app(ELPTest PRODUCT_NAME "ELPTest")
    target_sources(ELPTest PRIVATE tests/ProcessorTest.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(ELPTest PRIVATE JucePlugin_Name="ELP" JucePlugin_Build_Standalone=0 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(ELPTest PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
