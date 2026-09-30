#pragma once
#include <JuceHeader.h>

namespace nfsat
{
// Presets are the APVTS state (same XML as DAW session recall) plus two validation attributes.
struct PresetManager
{
    static juce::File getPresetsDirectory();
    static juce::Result savePreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
    static juce::Result loadPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file);
};
}
