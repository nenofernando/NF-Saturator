#include "PluginEditor.h"
#include "NFSaturatorBinaryData.h"
#include "FactoryPresets.h"
#include "ManualManager.h"
#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "0.0.0-test"
#endif

namespace
{
juce::Image readyAsset(const char* data, int size) { return juce::ImageCache::getFromMemory(data, size); }

// Base layout (1200x400): Drive left, three valves in the middle, Output right; both knobs identical in size.
constexpr float kKnobY = 180.0f, kKnobBox = 145.0f;
constexpr float kDriveX = 210.0f, kOutputX = 990.0f;
constexpr int kDefaultWidth = 810, kDefaultHeight = 270;   // default window size (owner's choice); double-click on the logo returns to it
constexpr float kValveXs[3] = { 480.0f, 600.0f, 720.0f };
// Two small knobs flanking the valves: INPUT (left) and MIX (right)
constexpr float kInputX = 382.0f, kMixX = 818.0f, kSmallKnobY = 264.0f, kSmallBox = 72.0f;   // small knobs: a bit larger (was 64), still clear of the big knobs and the valves
constexpr float kSmallScale = kSmallBox / 64.0f;   // tick marks and numbers scale with the knob

juce::String formatDrive(double v){ return juce::String(v,1); }
juce::String formatOut(double v){ auto s=juce::String(v,1); if(v>0.05) s="+"+s; else if(v>-0.05) s="0.0"; return s+" dB"; }
}

void NFSaturatorPowerButton::paintButton(juce::Graphics& g,bool,bool)
{
    NFSaturatorLookAndFeel::drawPowerBody(g, getLocalBounds().toFloat());
}

NFSaturatorAudioProcessorEditor::NFSaturatorAudioProcessorEditor(NFSaturatorAudioProcessor& p)
    :AudioProcessorEditor(&p),processor(p),
     driveCap("0 - 10",4.0,false,formatDrive),outputCap("-12 to +12 dB",0.0,false,formatOut)
{
    setLookAndFeel(&look);
    setResizable(true,true);
    getConstrainer()->setFixedAspectRatio(3.0);
    getConstrainer()->setSizeLimits(750,250,1800,600);
    {
        const int w = juce::jlimit(750, 1800, (int) p.apvts.state.getProperty("uiWidth", 810));   // size chosen with the resize handle survives close / reopen
        setSize(w, w/3);
    }

    addAndMakeVisible(logoButton);
    logoButton.setTooltip("Double-click: reset UI size");
    logoButton.onDoubleClick = [this]{ setSize(kDefaultWidth,kDefaultHeight); };
    addAndMakeVisible(menuButton);
    menuButton.setTooltip("Manuals and About");
    menuButton.onClick = [this]{ showMainMenu(); };
    addAndMakeVisible(presetBar);
    presetBar.setTooltip("Preset: click the name for the list, arrows = previous / next");
    presetBar.onPrev = [this]{ stepPreset(-1); };
    presetBar.onNext = [this]{ stepPreset(+1); };
    presetBar.onMenu = [this]{ showPresetMenu(); };
    presetBar.setName(nfsat::PresetManager::getCurrentPresetName(processor.apvts));
    processor.apvts.state.addListener(this);

    struct K{ juce::Slider* s; double def; };
    for(auto k:{K{&driveKnob,4.0},K{&outputKnob,0.0}}){
        addAndMakeVisible(*k.s);
        k.s->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.s->setRotaryParameters(juce::MathConstants<float>::pi*1.25f, juce::MathConstants<float>::pi*2.75f, true);
        k.s->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        k.s->setDoubleClickReturnValue(true,k.def);
    }
    addAndMakeVisible(driveCap);addAndMakeVisible(outputCap);
    addAndMakeVisible(driveBubble);addAndMakeVisible(outputBubble);addAndMakeVisible(mixBubble);
    for(auto* k:{&inputKnob,&mixKnob}){
        addAndMakeVisible(*k);
        k->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k->setRotaryParameters(juce::MathConstants<float>::pi*1.25f, juce::MathConstants<float>::pi*2.75f, true);
        k->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
    }
    addAndMakeVisible(inputBubble);
    inputKnob.setDoubleClickReturnValue(true,0.0);
    mixKnob.setDoubleClickReturnValue(true,1.0);
    inputKnob.setTooltip("INPUT: gain in front of the valves (-12..+12 dB). More input = more saturation and punch. Double-click: 0 dB");
    mixKnob.setTooltip("MIX: blend of the original (dry) and the saturated signal. 100% = all saturated; lower = parallel saturation. Double-click: 100%");
    inputKnob.onValueChange = [this]{ if (inputKnob.isMouseOverOrDragging()) inputBubble.showRaw(formatOut(inputKnob.getValue())); };
    mixKnob.onValueChange   = [this]{ if (mixKnob.isMouseOverOrDragging())   mixBubble.showRaw(juce::String(juce::roundToInt((float) mixKnob.getValue() * 100.0f)) + "%"); };
    for(auto* b:{&tubeBubble,&ironBubble,&solidBubble}) addAndMakeVisible(*b);
    addAndMakeVisible(power);power.setClickingTogglesState(true);
    for(auto* v:{&tubeValve,&ironValve,&solidValve}) addAndMakeVisible(*v);
    tubeValve.setTooltip("TUBE: triode-style warmth (even harmonics). Click = on/off, drag up = warmer, drag down = cooler, Alt-click = reset");
    ironValve.setTooltip("IRON: transformer-style weight (low-end saturation). Click = on/off, drag up = warmer, drag down = cooler, Alt-click = reset");
    solidValve.setTooltip("SOLID: transistor / op-amp bite. Click = on/off, drag up = warmer (rounder), drag down = cooler, Alt-click = reset");

    driveKnob.onValueChange  = [this]{ if (driveKnob.isMouseOverOrDragging())  driveBubble.showRaw(formatDrive(driveKnob.getValue())); };
    outputKnob.onValueChange = [this]{ if (outputKnob.isMouseOverOrDragging()) outputBubble.showRaw(formatOut(outputKnob.getValue())); };

    auto& a=processor.apvts;
    mixA=std::make_unique<SA>(a,"mix",mixKnob);inputA=std::make_unique<SA>(a,"inputGain",inputKnob);
    driveA=std::make_unique<SA>(a,"drive",driveKnob);driveCapA=std::make_unique<SA>(a,"drive",driveCap.slider);
    outputA=std::make_unique<SA>(a,"outputGain",outputKnob);outputCapA=std::make_unique<SA>(a,"outputGain",outputCap.slider);
    powerA=std::make_unique<BA>(a,"power",power);power.onStateChange=[this]{repaint();};
    tubeA=std::make_unique<BA>(a,"tube",tubeValve);ironA=std::make_unique<BA>(a,"iron",ironValve);solidA=std::make_unique<BA>(a,"solid",solidValve);
    hookValve(tubeValve,tubeBubble,"tubeWarm","TUBE",tubeWarmA);
    hookValve(ironValve,ironBubble,"ironWarm","IRON",ironWarmA);
    hookValve(solidValve,solidBubble,"solidWarm","SOLID",solidWarmA);
    startTimerHz(30);
}
NFSaturatorAudioProcessorEditor::~NFSaturatorAudioProcessorEditor(){processor.apvts.state.removeListener(this);cancelPendingUpdate();stopTimer();setLookAndFeel(nullptr);}

// Wires a valve's vertical drag to its "warmth" parameter (with proper host gestures) and shows a floating value.
void NFSaturatorAudioProcessorEditor::hookValve(ValveButton& valve, NFSaturatorBubble& bubble, const char* warmId, const char* label,
                                                std::unique_ptr<juce::ParameterAttachment>& attachment)
{
    auto* param = processor.apvts.getParameter(warmId);
    attachment = std::make_unique<juce::ParameterAttachment>(*param, [&valve](float v){ valve.setAmount(v); }, nullptr);
    attachment->sendInitialUpdate();
    auto* att = attachment.get();
    valve.onDragStart = [att]{ att->beginGesture(); };
    valve.onDragEnd   = [att]{ att->endGesture(); };
    valve.onAmountDrag = [att,&valve,&bubble,label](float v)
    {
        att->setValueAsPartOfGesture(v);
        bubble.showRaw(juce::String(label) + " " + juce::String(juce::roundToInt(v * 100.0f)) + "%");
        if (v > 0.001f && !valve.getToggleState()) valve.setToggleState(true, juce::sendNotificationSync);   // warming an off valve switches it on
    };
}

void NFSaturatorAudioProcessorEditor::timerCallback()
{
    // The filaments follow the signal: quick rise, slower fall.
    const bool powered = power.getToggleState();
    const float target = powered ? processor.driveActivity.load() : 0.0f;
    smoothedActivity = target > smoothedActivity ? target : smoothedActivity + (target - smoothedActivity) * 0.15f;
    const float glow = 0.22f + 0.78f * smoothedActivity;
    for(auto* v:{&tubeValve,&ironValve,&solidValve}) v->setGlow(powered ? glow : 0.05f);
}

juce::Rectangle<int> NFSaturatorAudioProcessorEditor::scaleBounds(juce::Rectangle<float> b) const
{
    return { juce::roundToInt(offsetX + b.getX()*layoutScale), juce::roundToInt(offsetY + b.getY()*layoutScale),
             juce::roundToInt(b.getWidth()*layoutScale), juce::roundToInt(b.getHeight()*layoutScale) };
}

// 3-line button: manuals and about only (presets live in the preset tab)
void NFSaturatorAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader("MANUALS");
#ifdef NFSAT_HAS_MANUAL_PT
    menu.addItem(10, "Manual - Portugues");
#else
    menu.addItem(10, "Manual - Portugues", false);
#endif
#ifdef NFSAT_HAS_MANUAL_EN
    menu.addItem(11, "Manual - English");
#else
    menu.addItem(11, "Manual - English", false);
#endif
    menu.addSeparator();
    menu.addItem(3, "About");
    juce::Component::SafePointer<NFSaturatorAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 3) safeThis->showAbout();
#ifdef NFSAT_HAS_MANUAL_PT
            else if (result == 10) nfsat::ManualManager::openManual(NFSaturatorBinaryData::NF_Saturator_Manual_Portugues_V1_0_pdf, NFSaturatorBinaryData::NF_Saturator_Manual_Portugues_V1_0_pdfSize, "NF_Saturator_Manual_Portugues_V1.0.pdf");
#endif
#ifdef NFSAT_HAS_MANUAL_EN
            else if (result == 11) nfsat::ManualManager::openManual(NFSaturatorBinaryData::NF_Saturator_Manual_English_V1_0_pdf, NFSaturatorBinaryData::NF_Saturator_Manual_English_V1_0_pdfSize, "NF_Saturator_Manual_English_V1.0.pdf");
#endif
        });
}

// Preset tab: factory presets by category, then save / load
void NFSaturatorAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto current = nfsat::PresetManager::getCurrentPresetName(processor.apvts);
    juce::String lastCategory;
    for (int i = 0; i < nfsat::kNumFactoryPresets; ++i)
    {
        const auto& f = nfsat::kFactoryPresets[i];
        if (lastCategory != f.category) { menu.addSectionHeader(f.category); lastCategory = f.category; }
        menu.addItem(100 + i, f.name, true, current == f.name);
    }
    menu.addSeparator();
    menu.addItem(1, "Save Preset...");
    menu.addItem(2, "Load Preset...");
    juce::Component::SafePointer<NFSaturatorAudioProcessorEditor> safeThis(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&presetBar).withMinimumWidth(presetBar.getWidth()),
        [safeThis](int result)
        {
            if (safeThis == nullptr || result == 0) return;
            if (result == 1) safeThis->handleSavePreset();
            else if (result == 2) safeThis->handleLoadPreset();
            else if (result >= 100) nfsat::PresetManager::applyFactoryPreset(safeThis->processor.apvts, result - 100);
        });
}

void NFSaturatorAudioProcessorEditor::stepPreset(int direction)
{
    const auto current = nfsat::PresetManager::getCurrentPresetName(processor.apvts);
    int index = -1;
    for (int i = 0; i < nfsat::kNumFactoryPresets; ++i) if (current == nfsat::kFactoryPresets[i].name) { index = i; break; }
    const int n = nfsat::kNumFactoryPresets;
    index = index < 0 ? (direction > 0 ? 0 : n - 1) : (index + direction + n) % n;
    nfsat::PresetManager::applyFactoryPreset(processor.apvts, index);
}

void NFSaturatorAudioProcessorEditor::showAbout()
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "About NF Saturator",
        juce::String("NF Saturator V") + JucePlugin_VersionString + "\nNF Audio Tools - Nenno Fernando");
}

void NFSaturatorAudioProcessorEditor::handleSavePreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Save NF Saturator Preset", nfsat::PresetManager::getPresetsDirectory(), "*.nfsatpreset");
    juce::Component::SafePointer<NFSaturatorAudioProcessorEditor> safeThis(this);
    const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    presetFileChooser->launchAsync(chooserFlags, [safeThis](const juce::FileChooser& fc)
    {
        if (safeThis == nullptr) return;
        auto file = fc.getResult();
        if (file != juce::File{})
        {
            if (!file.hasFileExtension("nfsatpreset")) file = file.withFileExtension("nfsatpreset");
            auto result = nfsat::PresetManager::savePreset(safeThis->processor.apvts, file);
            if (result.failed()) juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF Saturator", result.getErrorMessage());
        }
        safeThis->presetFileChooser.reset();
    });
}

void NFSaturatorAudioProcessorEditor::handleLoadPreset()
{
    presetFileChooser = std::make_unique<juce::FileChooser>("Load NF Saturator Preset", nfsat::PresetManager::getPresetsDirectory(), "*.nfsatpreset");
    juce::Component::SafePointer<NFSaturatorAudioProcessorEditor> safeThis(this);
    presetFileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis](const juce::FileChooser& fc)
        {
            if (safeThis == nullptr) return;
            auto file = fc.getResult();
            if (file != juce::File{})
            {
                auto result = nfsat::PresetManager::loadPreset(safeThis->processor.apvts, file);
                if (result.failed()) juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "NF Saturator", result.getErrorMessage());
            }
            safeThis->presetFileChooser.reset();
        });
}

void NFSaturatorAudioProcessorEditor::drawScale(juce::Graphics& g,juce::Point<float> c,const std::vector<Tick>& ticks)
{
    const float radius = 85.0f;
    g.setColour(juce::Colours::white);
    for (const auto& t : ticks)
    {
        const float a = t.deg * juce::MathConstants<float>::pi / 180.0f;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        const float len = t.major ? 13.0f : 6.0f;
        g.drawLine(juce::Line<float>(c + dir*radius, c + dir*(radius+len)), t.major ? 2.4f : 1.4f);
        if (t.label.isNotEmpty())
        {
            const auto p = c + dir*(radius+29.0f);
            g.setFont(juce::Font(juce::FontOptions(t.fontSize)));
            g.drawText(t.label, juce::Rectangle<float>(p.x-26.0f, p.y-12.0f, 52.0f, 24.0f), juce::Justification::centred);
        }
    }
}

void NFSaturatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0a1b11));
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(layoutScale).translated(offsetX, offsetY));

    static const juce::Image chassis = readyAsset(NFSaturatorBinaryData::_01_chassis_1200x400_png, NFSaturatorBinaryData::_01_chassis_1200x400_pngSize);
    { juce::Graphics::ScopedSaveState s(g); g.setOpacity(1.0f);
      if (chassis.isValid()) g.drawImage(chassis, {0.0f,0.0f,1200.0f,400.0f}, juce::RectanglePlacement::stretchToFit); }

    g.setColour(juce::Colour(0xffeef2ee));
    { static const juce::Image nfLogo=readyAsset(NFSaturatorBinaryData::_10_logo_nf_audio_tools_png,NFSaturatorBinaryData::_10_logo_nf_audio_tools_pngSize);
      if(nfLogo.isValid()) g.drawImage(nfLogo,juce::Rectangle<float>(42.0f,3.0f,108.0f,62.0f),juce::RectanglePlacement::centred); }
    g.drawLine(148.0f,14.0f,148.0f,45.0f,2.0f);
    g.setFont(juce::Font(juce::FontOptions(30.0f,juce::Font::bold)).withExtraKerningFactor(.08f));
    g.drawText("NF SATURATOR",168,9,400,44,juce::Justification::centredLeft);

    // Scales: Drive 0..10 (eleven numbered marks), Output -12 / 0 / +12 with 0 dB at 12 o'clock.
    { std::vector<Tick> t; for(int i=0;i<=10;++i) t.push_back({-135.0f+(float)i*27.0f, juce::String(i), true, 16.0f});
      drawScale(g,{kDriveX,kKnobY},t); }
    { // Output: same look as Drive - a numbered mark every 2 dB, -12 ... 0 ... +12 (0 dB at 12 o'clock)
      std::vector<Tick> t;
      for(int i=0;i<=12;++i){ const int v=-12+2*i; t.push_back({-135.0f+(float)i*22.5f, v>0 ? "+"+juce::String(v) : juce::String(v), true, 16.0f}); }
      drawScale(g,{kOutputX,kKnobY},t); }

    // Tiny scales around the small knobs (7 marks over the 270-degree sweep; min / centre / max a little longer and numbered).
    // Kept short and close to the knob so they never reach the scales of the big knobs.
    {
        static const char* const inputLabels[3] = { "-12", "0", "+12" };   // dB
        static const char* const mixLabels[3]   = { "0", "50", "100" };    // % wet
        const float centres[2] = { kInputX, kMixX };
        for (int k = 0; k < 2; ++k)
            for (int i = 0; i < 7; ++i)
            {
                const float a = (-135.0f + (float) i * 45.0f) * juce::MathConstants<float>::pi / 180.0f;
                const juce::Point<float> c(centres[k], kSmallKnobY), dir(std::sin(a), -std::cos(a));
                const bool major = (i % 3) == 0;
                g.setColour(juce::Colours::white);
                g.drawLine(juce::Line<float>(c + dir * (30.0f * kSmallScale), c + dir * ((major ? 37.0f : 34.0f) * kSmallScale)), major ? 1.9f : 1.3f);
                if (major)
                {
                    const auto p = c + dir * (46.0f * kSmallScale);
                    g.setFont(juce::Font(juce::FontOptions(11.0f)));
                    g.drawText(k == 0 ? inputLabels[i / 3] : mixLabels[i / 3], juce::Rectangle<float>(p.x - 16.0f, p.y - 7.0f, 32.0f, 14.0f), juce::Justification::centred);
                }
            }
    }

    // Small knobs beside the valves: INPUT (left) and MIX (right)
    // (names sit on the same row and in the same type as TUBE / IRON / SOLID: valve names are drawn at y = 78 + 204)
    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(15.0f,juce::Font::bold)));
    g.drawText("INPUT", juce::Rectangle<int>((int)kInputX-60,300,120,20), juce::Justification::centred);
    g.drawText("MIX",   juce::Rectangle<int>((int)kMixX-60,300,120,20), juce::Justification::centred);

    g.setColour(juce::Colours::white);g.setFont(juce::Font(juce::FontOptions(20.0f,juce::Font::bold)));
    g.drawText("DRIVE", juce::Rectangle<int>((int)kDriveX-80,266,160,24), juce::Justification::centred);
    g.drawText("OUTPUT", juce::Rectangle<int>((int)kOutputX-80,266,160,24), juce::Justification::centred);

    NFSaturatorLookAndFeel::drawScrew(g, {7.0f,    14.0f, 46.0f, 46.0f});
    NFSaturatorLookAndFeel::drawScrew(g, {1143.0f, 14.0f, 46.0f, 46.0f});
    NFSaturatorLookAndFeel::drawScrew(g, {7.0f,    329.0f, 46.0f, 46.0f});
    NFSaturatorLookAndFeel::drawScrew(g, {1143.0f, 329.0f, 46.0f, 46.0f});
    NFSaturatorLookAndFeel::drawLed(g, {1095.0f, 24.0f, 20.0f, 20.0f}, power.getToggleState());

    g.setColour(juce::Colours::white);g.setFont(15.0f);
    g.drawText("NF AUDIO TOOLS", juce::Rectangle<int>(0,358,1200,18), juce::Justification::centred);
    juce::GlyphArrangement footerGlyphs;
    footerGlyphs.addLineOfText(g.getCurrentFont(), "NF AUDIO TOOLS", 0.0f, 0.0f);
    const float footerTextWidth = footerGlyphs.getBoundingBox(0,-1,true).getWidth();
    const float midX = 600.0f, lineY = 367.0f, gap = footerTextWidth*0.5f + 14.0f;
    g.drawLine(midX-190.0f, lineY, midX-gap, lineY, 1.4f);
    g.drawLine(midX+gap, lineY, midX+190.0f, lineY, 1.4f);

    g.setFont(juce::Font(juce::FontOptions(12.5f)));
    g.drawText("V" JucePlugin_VersionString, juce::Rectangle<int>(70,357,80,20), juce::Justification::centredLeft);
}

void NFSaturatorAudioProcessorEditor::resized()
{
    if (getWidth() > 0) processor.apvts.state.setProperty("uiWidth", getWidth(), nullptr);   // remembered for the next time the window opens
    const float scaleX = getWidth() / 1200.0f, scaleY = getHeight() / 400.0f;
    layoutScale = juce::jmin(scaleX, scaleY);
    offsetX = (getWidth()  - 1200.0f * layoutScale) * 0.5f;
    offsetY = (getHeight() - 400.0f  * layoutScale) * 0.5f;

    auto layoutKnob = [&](juce::Slider& knob, ValueCapsule& cap, NFSaturatorBubble& bubble, float cx)
    {
        knob.setBounds(scaleBounds({cx-kKnobBox*0.5f, kKnobY-kKnobBox*0.5f, kKnobBox, kKnobBox}));
        cap.setBounds(scaleBounds({cx-65.0f, 294.0f, 130.0f, 54.0f}));
        bubble.setBounds(scaleBounds({cx-38.0f, 202.0f, 76.0f, 24.0f}));
    };
    layoutKnob(driveKnob,driveCap,driveBubble,kDriveX);
    layoutKnob(outputKnob,outputCap,outputBubble,kOutputX);

    ValveButton* valves[3] = { &tubeValve, &ironValve, &solidValve };
    for (int i = 0; i < 3; ++i) valves[i]->setBounds(scaleBounds({kValveXs[i]-60.0f, 78.0f, 120.0f, 240.0f}));
    NFSaturatorBubble* valveBubbles[3] = { &tubeBubble, &ironBubble, &solidBubble };
    for (int i = 0; i < 3; ++i) valveBubbles[i]->setBounds(scaleBounds({kValveXs[i]-45.0f, 56.0f, 90.0f, 24.0f}));

    inputKnob.setBounds(scaleBounds({kInputX-kSmallBox*0.5f, kSmallKnobY-kSmallBox*0.5f, kSmallBox, kSmallBox}));
    mixKnob.setBounds(scaleBounds({kMixX-kSmallBox*0.5f, kSmallKnobY-kSmallBox*0.5f, kSmallBox, kSmallBox}));
    // Same bubble (76 x 24, value only) and same spot relative to the knob as on DRIVE / OUTPUT (a little below its centre).
    inputBubble.setBounds(scaleBounds({kInputX-38.0f, kSmallKnobY+3.0f, 76.0f, 24.0f}));
    mixBubble.setBounds(scaleBounds({kMixX-38.0f, kSmallKnobY+3.0f, 76.0f, 24.0f}));
    power.setBounds(scaleBounds({1075.0f, 43.0f, 66.0f, 66.0f}));
    logoButton.setBounds(scaleBounds({42.0f, 3.0f, 108.0f, 62.0f}));
    presetBar.setBounds(scaleBounds({848.0f, 28.0f, 157.0f, 21.0f}));
    menuButton.setBounds(scaleBounds({1020.0f, 25.0f, 34.0f, 28.0f}));
}
