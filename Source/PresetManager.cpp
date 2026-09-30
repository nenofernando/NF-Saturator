#include "PresetManager.h"

namespace nfsat
{
namespace
{
constexpr const char* kSignature = "NFSaturatorPreset";
constexpr int kFormatVersion = 1;
constexpr juce::int64 kMaxFileBytes = 1 * 1024 * 1024;
}

juce::File PresetManager::getPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("NF Audio Tools").getChildFile("NF Saturator").getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

juce::Result PresetManager::savePreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr) return juce::Result::fail("Could not serialise the current state.");
    xml->setAttribute("nfsatPresetSignature", kSignature);
    xml->setAttribute("nfsatPresetFormatVersion", kFormatVersion);
    juce::TemporaryFile temp(file);
    if (!xml->writeTo(temp.getFile())) return juce::Result::fail("Could not write the preset file.");
    if (!temp.overwriteTargetFileWithTemporary()) return juce::Result::fail("Could not finalise the preset file.");
    return juce::Result::ok();
}

juce::Result PresetManager::loadPreset(juce::AudioProcessorValueTreeState& apvts, const juce::File& file)
{
    if (!file.existsAsFile()) return juce::Result::fail("Invalid NF Saturator preset");
    const auto size = file.getSize();
    if (size <= 0 || size > kMaxFileBytes) return juce::Result::fail("Invalid NF Saturator preset");
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || xml->getStringAttribute("nfsatPresetSignature") != kSignature
        || xml->getIntAttribute("nfsatPresetFormatVersion", -1) <= 0 || !xml->hasTagName(apvts.state.getType()))
        return juce::Result::fail("Invalid NF Saturator preset");
    apvts.replaceState(juce::ValueTree::fromXml(*xml));
    return juce::Result::ok();
}
}
