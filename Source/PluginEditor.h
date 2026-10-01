#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFSaturatorLookAndFeel.h"
#include "UI/ValueCapsule.h"
#include "UI/ValveButton.h"
#include "PresetManager.h"
#include "License/LicenseActivationComponent.h"

class NFSaturatorPowerButton final:public juce::ToggleButton
{
public:void paintButton(juce::Graphics&,bool,bool) override;
};

// Invisible hit-target over the NF logo: double-click returns the window to its original size.
class NFSaturatorLogoButton final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onDoubleClick;
    NFSaturatorLogoButton(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void mouseDoubleClick(const juce::MouseEvent&) override { if (onDoubleClick) onDoubleClick(); }
};

// Three-line hamburger icon opening the presets menu.
class NFSaturatorMenuButton final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    NFSaturatorMenuButton(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        const float lineH = juce::jmax(1.6f, b.getHeight()*0.10f);
        g.setColour(juce::Colour(0xffeef2ee));
        for (int i=0;i<3;++i)
        {
            const float y = b.getY() + b.getHeight()*(0.20f + (float)i*0.30f);
            g.fillRoundedRectangle(b.getX(), y, b.getWidth(), lineH, lineH*0.5f);
        }
    }
    void mouseUp(const juce::MouseEvent& e) override { if (contains(e.getPosition()) && onClick) onClick(); }
};

// Temporary floating value readout over a knob while it is being adjusted.
class NFSaturatorBubble final:public juce::Component, private juce::Timer
{
public:
    NFSaturatorBubble(){ setInterceptsMouseClicks(false,false); setAlpha(0.0f); }
    void showRaw(const juce::String& t)
    {
        text = t;
        juce::Desktop::getInstance().getAnimator().cancelAnimation(this, false);
        setAlpha(1.0f); setVisible(true); repaint(); startTimer(800);
    }
    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(juce::Colour(0xff0a140d).withAlpha(0.85f)); g.fillRoundedRectangle(b, 6.0f);
        g.setColour(juce::Colour(0xffd9d4c4).withAlpha(0.6f)); g.drawRoundedRectangle(b, 6.0f, 1.0f);
        g.setColour(juce::Colour(0xfff2efe4));
        g.setFont(juce::Font(juce::FontOptions(juce::jmax(11.0f, b.getHeight()*0.5f), juce::Font::bold)));
        g.drawText(text, b, juce::Justification::centred);
    }
private:
    void timerCallback() override { stopTimer(); juce::Desktop::getInstance().getAnimator().fadeOut(this, 220); }
    juce::String text;
};

// Small horizontal preset tab: [<]  preset name  [>]. Arrows step through the factory presets, a click on the name opens the list.
class NFSaturatorPresetBar final:public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onPrev, onNext, onMenu;
    NFSaturatorPresetBar(){ setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void setName(const juce::String& n){ if (n != name) { name = n; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        const float s = (float)getHeight() / 29.0f;
        auto r = getLocalBounds().toFloat().reduced(1.0f*s);
        g.setColour(juce::Colour(0x50000000));
        g.fillRoundedRectangle(r.translated(0.0f,2.0f*s), 8.0f*s);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff6f4ec), 0.0f, r.getY(), juce::Colour(0xffdcd9cc), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle(r, 8.0f*s);
        g.setColour(juce::Colour(0xff111511));
        g.drawRoundedRectangle(r, 8.0f*s, 1.6f*s);
        // arrows
        g.setColour(juce::Colour(0xff101510));
        const float cy = r.getCentreY(), ax = 13.0f*s, w = 5.0f*s, h = 6.0f*s;
        juce::Path left, right;
        left.addTriangle(r.getX()+ax+w, cy-h, r.getX()+ax+w, cy+h, r.getX()+ax-w, cy);
        right.addTriangle(r.getRight()-ax-w, cy-h, r.getRight()-ax-w, cy+h, r.getRight()-ax+w, cy);
        g.fillPath(left); g.fillPath(right);
        g.setFont(juce::Font(juce::FontOptions(15.0f*s, juce::Font::bold)));
        g.drawFittedText(name, juce::Rectangle<float>(r.getX()+28.0f*s, r.getY(), r.getWidth()-56.0f*s, r.getHeight()).toNearestInt(), juce::Justification::centred, 1);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!contains(e.getPosition())) return;
        const float s = (float)getHeight() / 29.0f;
        if (e.position.x < 28.0f*s) { if (onPrev) onPrev(); }
        else if (e.position.x > (float)getWidth() - 28.0f*s) { if (onNext) onNext(); }
        else if (onMenu) onMenu();
    }
private:
    juce::String name { "Default" };
};

class NFSaturatorAudioProcessorEditor final:public juce::AudioProcessorEditor, private juce::Timer, private juce::ValueTree::Listener, private juce::AsyncUpdater
{
public:
    explicit NFSaturatorAudioProcessorEditor(NFSaturatorAudioProcessor&);
    ~NFSaturatorAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;void resized() override;
private:
    struct Tick { float deg; juce::String label; bool major; float fontSize; };
    void timerCallback() override;
    // the preset name lives in the plug-in state; refresh the tab whenever it changes (may come from a non-message thread)
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier& id) override { if (id.toString() == "presetName") triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { presetBar.setName(nfsat::PresetManager::getCurrentPresetName(processor.apvts)); }
    void showPresetMenu();
    void stepPreset(int direction);
    void drawScale(juce::Graphics&,juce::Point<float> centre,const std::vector<Tick>&);
    juce::Rectangle<int> scaleBounds(juce::Rectangle<float> baseBounds) const;
    void showMainMenu();
    void handleSavePreset();
    void handleLoadPreset();
    void showAbout();

    float layoutScale=1.0f, offsetX=0.0f, offsetY=0.0f, smoothedActivity=0.0f;
    NFSaturatorAudioProcessor& processor;NFSaturatorLookAndFeel look;
    juce::TooltipWindow tooltipWindow{this, 500};
    NFSaturatorMenuButton menuButton;
    NFSaturatorPresetBar presetBar;
    NFSaturatorLogoButton logoButton;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    juce::Slider driveKnob,outputKnob;
    NFSaturatorBubble driveBubble,outputBubble,tubeBubble,ironBubble,solidBubble,mixBubble,inputBubble;
    juce::Slider inputKnob,mixKnob;   // the two small knobs beside the valves
    ValueCapsule driveCap,outputCap;
    ValveButton tubeValve{"TUBE"}, ironValve{"IRON"}, solidValve{"SOLID"};
    NFSaturatorPowerButton power;
    LicenseActivationComponent licenseOverlay;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> driveA,driveCapA,outputA,outputCapA,mixA,inputA;
    std::unique_ptr<BA> powerA,tubeA,ironA,solidA;
    std::unique_ptr<juce::ParameterAttachment> tubeWarmA,ironWarmA,solidWarmA;
    void hookValve(ValveButton& valve, NFSaturatorBubble& bubble, const char* warmId, const char* label, std::unique_ptr<juce::ParameterAttachment>& attachment);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFSaturatorAudioProcessorEditor)
};
