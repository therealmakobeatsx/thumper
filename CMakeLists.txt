cmake_minimum_required(VERSION 3.22)

option(THUMPER_MACOS_UNIVERSAL "Build macOS targets for Apple Silicon and Intel" ON)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin"
   AND THUMPER_MACOS_UNIVERSAL
   AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
endif()

project(THUMPER VERSION 1.0.0 LANGUAGES C CXX)

include(CTest)

option(THUMPER_FETCH_JUCE "Download the pinned JUCE release if it is not already available" ON)

include(FetchContent)

if(THUMPER_FETCH_JUCE)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    find_package(JUCE CONFIG REQUIRED)
endif()

juce_add_binary_data(THUMPERAssets
    SOURCES
        Assets/background.png
)

set(THUMPER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND THUMPER_FORMATS AU)
endif()

juce_add_plugin(THUMPER
    COMPANY_NAME "THUMPER Audio"
    BUNDLE_ID "audio.thumper.plugin"
    PLUGIN_MANUFACTURER_CODE Thmp
    PLUGIN_CODE Thmp
    FORMATS ${THUMPER_FORMATS}
    PRODUCT_NAME "THUMPER"
    DESCRIPTION "Aggressive one-knob ducking and pumping effect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(THUMPER
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

target_compile_features(THUMPER PRIVATE cxx_std_17)

target_compile_definitions(THUMPER
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(THUMPER
    PRIVATE
        THUMPERAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(THUMPER PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(THUMPERProcessorTests Tests/PluginProcessorTests.cpp)
    target_compile_features(THUMPERProcessorTests PRIVATE cxx_std_17)
    target_include_directories(THUMPERProcessorTests PRIVATE Source)
    target_link_libraries(THUMPERProcessorTests PRIVATE THUMPER THUMPERAssets
        juce::juce_audio_processors juce::juce_graphics)
    add_test(NAME THUMPERProcessorTests COMMAND THUMPERProcessorTests)
endif()

if(APPLE)
    find_program(THUMPER_DITTO_EXECUTABLE ditto REQUIRED)
    find_program(THUMPER_LIPO_EXECUTABLE lipo REQUIRED)

    set(THUMPER_MACOS_PACKAGE_DIR "${CMAKE_BINARY_DIR}/THUMPER-macOS-package")
    set(THUMPER_MACOS_ARTEFACT_DIR "${CMAKE_BINARY_DIR}/THUMPER_artefacts/$<CONFIG>")

    add_custom_target(THUMPER_macOS_package
        COMMAND "${THUMPER_LIPO_EXECUTABLE}"
                "${THUMPER_MACOS_ARTEFACT_DIR}/AU/THUMPER.component/Contents/MacOS/THUMPER"
                -verify_arch arm64 x86_64
        COMMAND "${THUMPER_LIPO_EXECUTABLE}"
                "${THUMPER_MACOS_ARTEFACT_DIR}/VST3/THUMPER.vst3/Contents/MacOS/THUMPER"
                -verify_arch arm64 x86_64
        COMMAND "${THUMPER_LIPO_EXECUTABLE}"
                "${THUMPER_MACOS_ARTEFACT_DIR}/Standalone/THUMPER.app/Contents/MacOS/THUMPER"
                -verify_arch arm64 x86_64
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${THUMPER_MACOS_PACKAGE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${THUMPER_MACOS_PACKAGE_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${THUMPER_MACOS_ARTEFACT_DIR}/AU/THUMPER.component"
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.component"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${THUMPER_MACOS_ARTEFACT_DIR}/VST3/THUMPER.vst3"
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.vst3"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${THUMPER_MACOS_ARTEFACT_DIR}/Standalone/THUMPER.app"
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.app"
        COMMAND "${CMAKE_COMMAND}" -E rm -f "${CMAKE_BINARY_DIR}/THUMPER-macOS.zip"
        COMMAND "${THUMPER_DITTO_EXECUTABLE}" -c -k --sequesterRsrc --keepParent
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.component"
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.vst3"
                "${THUMPER_MACOS_PACKAGE_DIR}/THUMPER.app"
                "${CMAKE_BINARY_DIR}/THUMPER-macOS.zip"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        DEPENDS THUMPER_AU THUMPER_VST3 THUMPER_Standalone
        VERBATIM
    )
endif()
