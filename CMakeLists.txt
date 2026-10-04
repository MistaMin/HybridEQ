cmake_minimum_required(VERSION 3.22)

# Single source of truth for the plugin version: ../VERSION (MAJOR.MINOR.PATCH).
# Bump that file (or answer the installer prompt) - nothing else hardcodes it.
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" HYBRIDEQ_VERSION_STR)
string(STRIP "${HYBRIDEQ_VERSION_STR}" HYBRIDEQ_VERSION_STR)
project(HybridEQ VERSION ${HYBRIDEQ_VERSION_STR} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

set(HARDWAREUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/HardwareUI" CACHE PATH "HardwareUI toolkit location")
add_subdirectory("${HARDWAREUI_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/hardwareui")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HybridEQ.csv")
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/Designs/HybridEQ.csv" HYBRIDEQ_DESIGN_TEXT)
configure_file(Designs/EmbeddedDesign.h.in generated/EmbeddedDesign.h @ONLY)
option(HYBRIDEQ_DESIGN_EDITOR "Include HardwareUI editor in Debug builds" ON)

set(HYBRIDEQ_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND HYBRIDEQ_FORMATS AU)
endif()
set(HYBRIDEQ_AAX_SDK "${CMAKE_SOURCE_DIR}/../aax-sdk-2-8-1" CACHE PATH "Optional Avid AAX SDK")
if(EXISTS "${HYBRIDEQ_AAX_SDK}/Interfaces/AAX.h")
    juce_set_aax_sdk_path("${HYBRIDEQ_AAX_SDK}")
    list(APPEND HYBRIDEQ_FORMATS AAX)
else()
    message(STATUS "AAX SDK not found; building ${HYBRIDEQ_FORMATS}")
endif()

juce_add_plugin(HybridEQ
    COMPANY_NAME            "HybridAudio"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    PLUGIN_MANUFACTURER_CODE Hybr
    PLUGIN_CODE              HyEq
    FORMATS                  ${HYBRIDEQ_FORMATS}
    PRODUCT_NAME             "HybridEQ"
    COPY_PLUGIN_AFTER_BUILD  FALSE
)

target_sources(HybridEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_include_directories(HybridEQ PRIVATE Source "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_compile_definitions(HybridEQ PUBLIC
    HARDWAREUI_ENABLE_EDITOR=$<AND:$<CONFIG:Debug>,$<BOOL:${HYBRIDEQ_DESIGN_EDITOR}>>
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HybridEQ
    PRIVATE
        hardwareui_juce
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
