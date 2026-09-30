#pragma once
#include <JuceHeader.h>
#include "DSP/SaturatorCore.h"

class NFSaturatorAudioProcessor final : public juce::AudioProcessor
{
public:
    NFSaturatorAudioProcessor();
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::AudioProcessorValueTreeState apvts;
    // 0..1: how hard the valves are being driven right now (input level x Drive). Read by the editor to make the valves glow.
    std::atomic<float> driveActivity { 0.0f };

private:
    static constexpr int kOversamplingLog2 = 2; // 4x
    nfsat::SaturatorCore core;
    std::array<nfsat::ChannelState, 2> channelState;
    juce::dsp::Oversampling<float> oversampling { 2, (size_t) kOversamplingLog2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> preGain, compGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> outGain, powerMix;
    std::atomic<float> *driveParam = nullptr, *tubeParam = nullptr, *ironParam = nullptr, *solidParam = nullptr,
                       *outputParam = nullptr, *powerParam = nullptr;
    int preparedBlockSize = 512;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFSaturatorAudioProcessor)
};
