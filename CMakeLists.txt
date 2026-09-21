cmake_minimum_required(VERSION 3.22)

# Single source of truth for the plugin version: ../VERSION (MAJOR.MINOR.PATCH).
# Bump that file (or answer the installer prompt) - nothing else hardcodes it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HYBRIDEQ_VERSION_STR)
string(STRIP "${HYBRIDEQ_VERSION_STR}" HYBRIDEQ_VERSION_STR)
project(HybridEQ VERSION ${HYBRIDEQ_VERSION_STR} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_set_aax_sdk_path("${CMAKE_SOURCE_DIR}/../aax-sdk-2-8-1")

juce_add_plugin(HybridEQ
    COMPANY_NAME            "HybridAudio"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE Hybr
    PLUGIN_CODE              HyEq
    FORMATS                  VST3 AU AAX Standalone
    PRODUCT_NAME             "HybridEQ"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(HybridEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(HybridEQ PRIVATE Source)

target_compile_definitions(HybridEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HybridEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
