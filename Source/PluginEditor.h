#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/NFSaturatorLookAndFeel.h"
#include "UI/ValueCapsule.h"
#include "UI/ValveButton.h"
#include "PresetManager.h"

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

class NFSaturatorAudioProcessorEditor final:public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NFSaturatorAudioProcessorEditor(NFSaturatorAudioProcessor&);
    ~NFSaturatorAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;void resized() override;
private:
    struct Tick { float deg; juce::String label; bool major; float fontSize; };
    void timerCallback() override;
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
    NFSaturatorLogoButton logoButton;
    std::unique_ptr<juce::FileChooser> presetFileChooser;
    juce::Slider driveKnob,outputKnob;
    NFSaturatorBubble driveBubble,outputBubble;
    ValueCapsule driveCap,outputCap;
    ValveButton tubeValve{"TUBE"}, ironValve{"IRON"}, solidValve{"SOLID"};
    NFSaturatorPowerButton power;
    using SA=juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA=juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<SA> driveA,driveCapA,outputA,outputCapA;
    std::unique_ptr<BA> powerA,tubeA,ironA,solidA;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NFSaturatorAudioProcessorEditor)
};
