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
    tubeWarmParam = apvts.getRawParameterValue("tubeWarm");
    ironWarmParam = apvts.getRawParameterValue("ironWarm");
    solidWarmParam = apvts.getRawParameterValue("solidWarm");
    mixParam = apvts.getRawParameterValue("mix");
    inputParam = apvts.getRawParameterValue("inputGain");
}

void NFSaturatorAudioProcessor::prepareToPlay(double sr, int samplesPerBlock)
{
    preparedBlockSize = juce::jmax(1, samplesPerBlock);
    oversampling.initProcessing((size_t) preparedBlockSize);
    oversampling.reset();
    internalRate = sr * (double) (1 << kOversamplingLog2);
    warm = { (double) tubeWarmParam->load(), (double) ironWarmParam->load(), (double) solidWarmParam->load() };
    channelState = {};

    const int latency = (int) std::ceil(oversampling.getLatencyInSamples());
    setLatencySamples(latency);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) preparedBlockSize, 2 };
    dryDelay.prepare(spec);
    dryDelay.setDelay((float) latency);
    dryDelay.reset();
    dryBuffer.setSize(2, preparedBlockSize);

    const double osRate = sr * (double) (1 << kOversamplingLog2);
    nfsat::Stages st;
    st.tube = tubeParam->load() > 0.5f; st.iron = ironParam->load() > 0.5f; st.solid = solidParam->load() > 0.5f;
    st.tubeAmt = warm[0]; st.ironAmt = warm[1]; st.solidAmt = warm[2];
    const double drive = driveParam->load();
    preGain.reset(osRate, 0.03);  preGain.setCurrentAndTargetValue((float) nfsat::preGainLinear(drive));
    compGain.reset(osRate, 0.03); compGain.setCurrentAndTargetValue((float) nfsat::compensationGain(drive, st));
    outGain.reset(sr, 0.02);      outGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputParam->load()));
    inGain.reset(sr, 0.02);       inGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputParam->load()));
    inGainBlock.assign((size_t) preparedBlockSize, 1.0f);
    wetMix.reset(sr, 0.02);       wetMix.setCurrentAndTargetValue(mixParam->load());
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
    // Warmth of each valve follows its control smoothly (about 50 ms) so dragging never zippers.
    const double aSm = 1.0 - std::exp(-(double) total / (0.05 * juce::jmax(1.0, getSampleRate())));
    const double warmTarget[3] = { (double) tubeWarmParam->load(), (double) ironWarmParam->load(), (double) solidWarmParam->load() };
    for (int i = 0; i < 3; ++i) warm[(size_t) i] += aSm * (warmTarget[(size_t) i] - warm[(size_t) i]);
    nfsat::Stages st;
    st.tube = tubeParam->load() > 0.5f; st.iron = ironParam->load() > 0.5f; st.solid = solidParam->load() > 0.5f;
    st.tubeAmt = warm[0]; st.ironAmt = warm[1]; st.solidAmt = warm[2];
    const nfsat::StageParams stageParams = nfsat::makeParams(st, internalRate);
    preGain.setTargetValue((float) nfsat::preGainLinear(drive));
    compGain.setTargetValue((float) nfsat::compensationGain(drive, st));
    outGain.setTargetValue(juce::Decibels::decibelsToGain(outputParam->load()));
    powerMix.setTargetValue(powerParam->load() > 0.5f ? 1.0f : 0.0f);
    wetMix.setTargetValue(juce::jlimit(0.0f, 1.0f, mixParam->load()));
    inGain.setTargetValue(juce::Decibels::decibelsToGain(inputParam->load()));
    if ((int) inGainBlock.size() < preparedBlockSize) inGainBlock.assign((size_t) preparedBlockSize, 1.0f);
    if (dryBuffer.getNumSamples() < preparedBlockSize) dryBuffer.setSize(2, preparedBlockSize, false, false, true);

    double energy = 0.0; int energyCount = 0;
    for (int start = 0; start < total; start += preparedBlockSize)
    {
        const int len = juce::jmin(preparedBlockSize, total - start);

        // INPUT trim (gain in front of the valves), smoothed; the same gain sequence is reused for the MIX dry path.
        for (int i = 0; i < len; ++i) inGainBlock[(size_t) i] = inGain.getNextValue();

        // Dry path, delayed by the oversampling latency so Power on/off crossfades cleanly.
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* in = buffer.getReadPointer(ch, start);
            auto* dry = dryBuffer.getWritePointer(ch);
            for (int i = 0; i < len; ++i) { dryDelay.pushSample(ch, in[i]); dry[i] = dryDelay.popSample(ch); }
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* d = buffer.getWritePointer(ch, start);
            for (int i = 0; i < len; ++i) d[i] *= inGainBlock[(size_t) i];
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
                d[i] = (float) (nfsat::processStages(x, stageParams, channelState[(size_t) ch]) * cg);
            }
        }
        oversampling.processSamplesDown(block);

        for (int i = 0; i < len; ++i)
        {
            const float og = outGain.getNextValue(), m = powerMix.getNextValue(), w = wetMix.getNextValue();
            for (int ch = 0; ch < numCh; ++ch)
            {
                float* out = buffer.getWritePointer(ch, start);
                const float dryOriginal = dryBuffer.getReadPointer(ch)[i];     // untouched input, time-aligned with the saturated signal
                const float dryTrimmed = dryOriginal * inGainBlock[(size_t) i];// the dry that goes into the MIX blend follows INPUT
                const float blended = w * out[i] + (1.0f - w) * dryTrimmed;    // MIX: parallel saturation (1 = all saturated)
                out[i] = m * blended * og + (1.0f - m) * dryOriginal;          // Power crossfades to the untouched dry
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
    // Warmth of each valve (drag up on the valve): 0 = base character, 1 = hottest.
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"tubeWarm",1},"Tube Warmth",juce::NormalisableRange<float>(0.0f,1.0f,0.01f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"ironWarm",1},"Iron Warmth",juce::NormalisableRange<float>(0.0f,1.0f,0.01f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"solidWarm",1},"Solid Warmth",juce::NormalisableRange<float>(0.0f,1.0f,0.01f),0.0f));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"outputGain",1},"Output",juce::NormalisableRange<float>(-12.0f,12.0f,0.1f),0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"inputGain",1},"Input",juce::NormalisableRange<float>(-12.0f,12.0f,0.1f),0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<juce::AudioParameterFloat>(ID{"mix",1},"Mix",juce::NormalisableRange<float>(0.0f,1.0f,0.01f),1.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    p.push_back(std::make_unique<juce::AudioParameterBool>(ID{"power",1},"Power",true));
    return {p.begin(),p.end()};
}

void NFSaturatorAudioProcessor::getStateInformation(juce::MemoryBlock& dest){if(auto xml=apvts.copyState().createXml())copyXmlToBinary(*xml,dest);}
void NFSaturatorAudioProcessor::setStateInformation(const void* data,int size){if(auto xml=getXmlFromBinary(data,size);xml&&xml->hasTagName(apvts.state.getType()))apvts.replaceState(juce::ValueTree::fromXml(*xml));}
juce::AudioProcessorEditor* NFSaturatorAudioProcessor::createEditor(){return new NFSaturatorAudioProcessorEditor(*this);}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new NFSaturatorAudioProcessor();}
