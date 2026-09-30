#include "PluginProcessor.h"
#include "PluginEditor.h"

NFSaturatorAudioProcessor::NFSaturatorAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
                                     .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
      apvts(*this,nullptr,"NF_SATURATOR_STATE",createParameters())
{
    driveParam = apvts.getRawParameterValue("drive");
    tubeParam = apvts.getRawParameterValue("tube");
    ironParam = apvts.getRawParameterValue("iron");
    solidParam = apvts.getRawParameterValue("solid");
    outputParam = apvts.getRawParameterValue("outputGain");
    powerParam = apvts.getRawParameterValue("power");
}

void NFSaturatorAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    oversampling.initProcessing((size_t) preparedBlockSize);
    oversampling.reset();
    core.prepare(sr * (double) (1 << kOversamplingLog2));
    channelState = {};

    const int latency = (int) std::ceil(oversampling.getLatencyInSamples());
    setLatencySamples(latency);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) preparedBlockSize, 2 };
    dryDelay.prepare(spec);
    dryDelay.setDelay((float) latency);
    dryDelay.reset();
    dryBuffer.setSize(2, preparedBlockSize);

    const double osRate = sr * (double) (1 << kOversamplingLog2);
    const nfsat::Stages st { tubeParam->load() > 0.5f, ironParam->load() > 0.5f, solidParam->load() > 0.5f };
    const double drive = driveParam->load();
    preGain.reset(osRate, 0.03);  preGain.setCurrentAndTargetValue((float) nfsat::preGainLinear(drive));
    compGain.reset(osRate, 0.03); compGain.setCurrentAndTargetValue((float) nfsat::compensationGain(drive, st));
    outGain.reset(sr, 0.02);      outGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputParam->load()));
    powerMix.reset(sr, 0.01);     powerMix.setCurrentAndTargetValue(powerParam->load() > 0.5f ? 1.0f : 0.0f);
}

bool NFSaturatorAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in=l.getMainInputChannelSet(),out=l.getMainOutputChannelSet();
    return (in==juce::AudioChannelSet::mono()||in==juce::AudioChannelSet::stereo())&&in==out;
}

void NFSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals guard;
    const int numCh = juce::jmin(2, buffer.getNumChannels());
    const int total = buffer.getNumSamples();
    if (numCh <= 0 || total <= 0) return;

    const double drive = driveParam->load();
    const nfsat::Stages st { tubeParam->load() > 0.5f, ironParam->load() > 0.5f, solidParam->load() > 0.5f };
    preGain.setTargetValue((float) nfsat::preGainLinear(drive));
    compGain.setTargetValue((float) nfsat::compensationGain(drive, st));
    outGain.setTargetValue(juce::Decibels::decibelsToGain(outputParam->load()));
    powerMix.setTargetValue(powerParam->load() > 0.5f ? 1.0f : 0.0f);
    if (dryBuffer.getNumSamples() < preparedBlockSize) dryBuffer.setSize(2, preparedBlockSize, false, false, true);

    double energy = 0.0; int energyCount = 0;
    for (int start = 0; start < total; start += preparedBlockSize)
    {
        const int len = juce::jmin(preparedBlockSize, total - start);

        // Dry path, delayed by the oversampling latency so Power on/off crossfades cleanly.
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* in = buffer.getReadPointer(ch, start);
            auto* dry = dryBuffer.getWritePointer(ch);
            for (int i = 0; i < len; ++i) { dryDelay.pushSample(ch, in[i]); dry[i] = dryDelay.popSample(ch); }
        }

        juce::dsp::AudioBlock<float> block(buffer.getArrayOfWritePointers(), (size_t) numCh, (size_t) start, (size_t) len);
        auto up = oversampling.processSamplesUp(block);
        const int upLen = (int) up.getNumSamples();
        for (int i = 0; i < upLen; ++i)
        {
            const double pg = preGain.getNextValue(), cg = compGain.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* d = up.getChannelPointer((size_t) ch);
                const double x = (double) d[i] * pg;
                if (ch == 0) { energy += x * x; ++energyCount; }
                d[i] = (float) (core.processStages(x, st, channelState[(size_t) ch]) * cg);
            }
        }
        oversampling.processSamplesDown(block);

        for (int i = 0; i < len; ++i)
        {
            const float og = outGain.getNextValue(), m = powerMix.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* out = buffer.getWritePointer(ch, start);
                out[i] = m * out[i] * og + (1.0f - m) * dryBuffer.getReadPointer(ch)[i];
            }
        }
    }

    if (energyCount > 0)
    {
        const double rmsDb = 10.0 * std::log10(juce::jmax(1.0e-12, energy / (double) energyCount));
        driveActivity.store((float) juce::jlimit(0.0, 1.0, (rmsDb + 30.0) / 30.0));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NFSaturatorAudioProcessor::createParameters()
{
    using ID=juce::ParameterID;std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"drive",1},"Drive",juce::NormalisableRange<float>(0.0f,10.0f,0.1f),4.0f));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"tube",1},"Tube",true));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"iron",1},"Iron",true));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"solid",1},"Solid",false));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"outputGain",1},"Output",juce::NormalisableRange<float>(-12.0f,12.0f,0.1f),0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"power",1},"Power",true));
    return {p.begin(),p.end()};
}

void NFSaturatorAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void NFSaturatorAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* NFSaturatorAudioProcessor::createEditor(){return new NFSaturatorAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFSaturatorAudioProcessor();}
