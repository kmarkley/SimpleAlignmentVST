cmake_minimum_required(VERSION 3.22)

project(SimpleAlignment VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── JUCE ──────────────────────────────────────────────────────────────────────
# Add JUCE as a submodule before configuring:
#   git submodule add https://github.com/juce-framework/JUCE.git JUCE
#   git submodule update --init --recursive
# Then build:
#   cmake -B build -DCMAKE_BUILD_TYPE=Release
#   cmake --build build --config Release

add_subdirectory(JUCE)

# ── Plugin target ─────────────────────────────────────────────────────────────
juce_add_plugin(SimpleAlignment
    COMPANY_NAME            "kmarkley"
    PLUGIN_MANUFACTURER_CODE Kmar
    PLUGIN_CODE             Salg
    FORMATS                 VST3
    PRODUCT_NAME            "SimpleAlignment"
    PLUGIN_NAME             "SimpleAlignment"

    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE

    VST3_CATEGORIES         "Fx" "Surround"
)

# ── Source files ──────────────────────────────────────────────────────────────
target_sources(SimpleAlignment PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

# ── Compile definitions ───────────────────────────────────────────────────────
target_compile_definitions(SimpleAlignment PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ── Link JUCE modules ─────────────────────────────────────────────────────────
target_link_libraries(SimpleAlignment PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
