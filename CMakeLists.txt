cmake_minimum_required(VERSION 3.22)

project(VisionQuest VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: build the C++ runtime into the plugin, so it doesn't depend on the
# Visual C++ Redistributable being installed on the PC that loads it.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Downloads JUCE 8.0.15 automatically on the first configure (about 100 MB).
include(FetchContent)
FetchContent_Declare(JUCE
    URL https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.15.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VisionQuest
    COMPANY_NAME             "DIY Audio"         # same vendor as Echo Space and TimeTwista, so they group in Ableton
    PRODUCT_NAME             "VisionQuest"
    PLUGIN_MANUFACTURER_CODE Diya
    PLUGIN_CODE              Vqst
    BUNDLE_ID                com.diyaudio.visionquest
    FORMATS                  VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    VST3_CATEGORIES          Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD  FALSE)

# Fonts (all SIL Open Font License, see Resources/OFL-*.txt)
juce_add_binary_data(VisionQuestFonts SOURCES
    Resources/Shrikhand-Regular.ttf
    Resources/SpaceGrotesk-Medium.ttf
    Resources/SpaceGrotesk-Bold.ttf
    Resources/JetBrainsMono-SemiBold.ttf
    Resources/Antonio-SemiBold.ttf)

target_sources(VisionQuest PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/Visuals.cpp
    Source/ui/Visuals2.cpp
    Source/ui/Visuals3.cpp)

target_include_directories(VisionQuest PRIVATE Source)

target_compile_definitions(VisionQuest PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VisionQuest
    PRIVATE
        VisionQuestFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Local-only tool that renders every mode to PNG for checking the look.
option(VQ_BUILD_SNAPSHOT "Build the snapshot renderer (local checks only)" OFF)
if(VQ_BUILD_SNAPSHOT)
    juce_add_console_app(VQSnapshot PRODUCT_NAME "VQSnapshot")
    target_sources(VQSnapshot PRIVATE
        tools/snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/Visuals.cpp
        Source/ui/Visuals2.cpp
        Source/ui/Visuals3.cpp)
    target_include_directories(VQSnapshot PRIVATE Source)
    target_compile_definitions(VQSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="VisionQuest" VQ_SNAPSHOT=1)
    target_link_libraries(VQSnapshot PRIVATE
        VisionQuestFonts juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
