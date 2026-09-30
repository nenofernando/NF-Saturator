cmake_minimum_required(VERSION 3.22)
project(NFSaturator VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS Universal Binary")
option(NFSaturator_FETCH_JUCE "Fetch JUCE when not installed" ON)
option(NFSaturator_ENABLE_AAX "Build AAX (licensed SDK required)" OFF)
# The AAX SDK is expected in ~/Documents/AAX_SDK (owner's Mac). Override with -DNFSaturator_AAX_SDK_PATH=...
# (the Windows script does this from the AAX_SDK_PATH environment variable).
if(WIN32)
 set(_nfsaturator_home "$ENV{USERPROFILE}")
else()
 set(_nfsaturator_home "$ENV{HOME}")
endif()
set(NFSaturator_AAX_SDK_PATH "${_nfsaturator_home}/Documents/AAX_SDK" CACHE PATH "Licensed AAX SDK path")

find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
 if(NOT NFSaturator_FETCH_JUCE)
  message(FATAL_ERROR "JUCE not found. Set JUCE_DIR or enable NFSaturator_FETCH_JUCE.")
 endif()
 include(FetchContent)
 FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.4 GIT_SHALLOW TRUE)
 FetchContent_MakeAvailable(JUCE)
endif()

set(NFSaturator_FORMATS VST3)
if(APPLE)
 list(APPEND NFSaturator_FORMATS AU)
endif()
if(NFSaturator_ENABLE_AAX)
 if(NOT EXISTS "${NFSaturator_AAX_SDK_PATH}")
  message(FATAL_ERROR "NFSaturator_ENABLE_AAX requires the AAX SDK at NFSaturator_AAX_SDK_PATH (${NFSaturator_AAX_SDK_PATH}).")
 endif()
 juce_set_aax_sdk_path("${NFSaturator_AAX_SDK_PATH}")
 list(APPEND NFSaturator_FORMATS AAX)
endif()

juce_add_plugin(NFSaturator
 COMPANY_NAME "NF Audio Tools"
 COMPANY_COPYRIGHT "NF Audio Tools by Nenno Fernando. (C) 2026 NF Audio Tools. All rights reserved."
 BUNDLE_ID "com.nfaudiotools.nfsaturator"
 AAX_IDENTIFIER "com.nfaudiotools.nfsaturator"
 PLUGIN_MANUFACTURER_CODE Nfat
 PLUGIN_CODE Nfsa
 FORMATS ${NFSaturator_FORMATS}
 PRODUCT_NAME "NF Saturator"
 PLUGIN_NAME "NF Saturator"
 DESCRIPTION "Simple musical valve saturator"
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
 EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
 VST3_CATEGORIES Fx Distortion Stereo
 AU_MAIN_TYPE kAudioUnitType_Effect
 AAX_CATEGORY AAX_ePlugInCategory_Harmonic)
juce_generate_juce_header(NFSaturator)

# Artwork embedded as binary data (header NFSaturatorBinaryData.h).
file(GLOB NFSaturator_READY_ASSETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/PNG_READY_1200x400/*.png")
# Manuals are optional: drop the PDFs in Assets/Manuals (see the README there) and rebuild.
set(NFSaturator_MANUAL_PT "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Manuals/NF_Saturator_Manual_Portugues_V1.0.pdf")
set(NFSaturator_MANUAL_EN "${CMAKE_CURRENT_SOURCE_DIR}/Assets/Manuals/NF_Saturator_Manual_English_V1.0.pdf")
set(NFSaturator_MANUAL_ASSETS "")
if(EXISTS "${NFSaturator_MANUAL_PT}")
 list(APPEND NFSaturator_MANUAL_ASSETS "${NFSaturator_MANUAL_PT}")
endif()
if(EXISTS "${NFSaturator_MANUAL_EN}")
 list(APPEND NFSaturator_MANUAL_ASSETS "${NFSaturator_MANUAL_EN}")
endif()
juce_add_binary_data(NFSaturatorAssets HEADER_NAME NFSaturatorBinaryData.h NAMESPACE NFSaturatorBinaryData SOURCES ${NFSaturator_READY_ASSETS} ${NFSaturator_MANUAL_ASSETS})
target_sources(NFSaturator PRIVATE
 Source/PluginProcessor.cpp Source/PluginProcessor.h Source/PluginEditor.cpp Source/PluginEditor.h
 Source/DSP/SaturatorCore.h Source/DSP/CompLookup.h Source/DSP/CompTableData.h
 Source/UI/ValueCapsule.cpp Source/UI/ValueCapsule.h Source/UI/ValveButton.h
 Source/PresetManager.cpp Source/PresetManager.h Source/FactoryPresets.h Source/ManualManager.cpp Source/ManualManager.h
 Source/UI/NFSaturatorLookAndFeel.cpp Source/UI/NFSaturatorLookAndFeel.h)
if(EXISTS "${NFSaturator_MANUAL_PT}")
 target_compile_definitions(NFSaturator PRIVATE NFSAT_HAS_MANUAL_PT=1)
endif()
if(EXISTS "${NFSaturator_MANUAL_EN}")
 target_compile_definitions(NFSaturator PRIVATE NFSAT_HAS_MANUAL_EN=1)
endif()
target_compile_definitions(NFSaturator PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
target_link_libraries(NFSaturator PRIVATE NFSaturatorAssets juce::juce_audio_utils juce::juce_dsp PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

add_executable(NFSaturatorDspTests Tests/SaturatorTests.cpp)
if(MSVC)
 target_compile_options(NFSaturatorDspTests PRIVATE /W4)
else()
 target_compile_options(NFSaturatorDspTests PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_test(NAME NFSaturatorDspTests COMMAND NFSaturatorDspTests)
