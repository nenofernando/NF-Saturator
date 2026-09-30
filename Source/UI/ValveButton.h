#pragma once
#include <JuceHeader.h>

// One "valve": a glass tube with two plates and a glowing filament, a name and an on/off lamp.
//  - click                : turns the valve on / off
//  - drag up / down       : "warmth" 0..1 - the light gets hotter (orange -> red-hot) and the valve's sound character grows;
//                           dragging back down cools it to the base state (0). Alt/Option-click resets it to 0.
// Drawn entirely in code in a 120 x 240 base box and scaled with the component width.
class ValveButton final : public juce::ToggleButton
{
public:
    explicit ValveButton(const juce::String& label) : name(label)
    {
        setClickingTogglesState(true);
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
    }

    std::function<void()> onDragStart, onDragEnd;
    std::function<void(float)> onAmountDrag;   // called with the new warmth while the user drags

    // 0..1: filament brightness (set by the editor from the signal level; on/off is handled here).
    void setGlow(float g) { if (std::abs(g - glow) > 0.004f) { glow = g; repaint(); } }
    // 0..1: warmth (from the parameter)
    void setAmount(float a) { if (std::abs(a - amount) > 0.0005f) { amount = a; repaint(); } }
    float getAmount() const { return amount; }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragged = false; startAmount = amount;
        if (e.mods.isAltDown())          // Alt/Option-click: reset warmth, do not toggle
        {
            setClickingTogglesState(false);
            if (onDragStart) onDragStart();
            amount = 0.0f; repaint();
            if (onAmountDrag) onAmountDrag(0.0f);
            if (onDragEnd) onDragEnd();
        }
        juce::ToggleButton::mouseDown(e);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragged && std::abs(e.getDistanceFromDragStartY()) > 4 && !e.mods.isAltDown())
        {
            dragged = true;
            setClickingTogglesState(false);   // a drag must not toggle the valve
            if (onDragStart) onDragStart();
        }
        if (dragged)
        {
            const float range = 150.0f * (float) getWidth() / 120.0f;          // full warmth over ~150 base pixels
            amount = juce::jlimit(0.0f, 1.0f, startAmount - (float) e.getDistanceFromDragStartY() / range);
            repaint();
            if (onAmountDrag) onAmountDrag(amount);
        }
        juce::ToggleButton::mouseDrag(e);
    }
    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::ToggleButton::mouseUp(e);
        if (dragged && onDragEnd) onDragEnd();
        dragged = false;
        setClickingTogglesState(true);
    }

    void paintButton(juce::Graphics& g, bool over, bool) override
    {
        const bool on = getToggleState();
        const float s = (float) getWidth() / 120.0f;
        g.addTransform(juce::AffineTransform::scale(s));
        constexpr float w = 74.0f, h = 150.0f, cx = 60.0f, top = 8.0f;
        const float level = on ? glow : 0.06f;
        const float heat = on ? amount : 0.0f;                                   // how hot the valve is set
        const auto orange = juce::Colour(0xffff9a2e), hot = juce::Colour(0xffff3d0f);
        const auto haloCol = orange.interpolatedWith(hot, heat);

        // halo: bigger, more intense and redder as the valve is warmed up
        {
            const float r = 62.0f + 22.0f * heat;
            juce::ColourGradient halo(haloCol.withAlpha(0.16f + 0.34f * level + 0.22f * heat), cx, top + h * 0.58f,
                                      haloCol.withAlpha(0.0f), cx + r, top + h * 0.58f, true);
            g.setGradientFill(halo);
            g.fillEllipse(cx - r, top + h * 0.58f - r * 1.06f, r * 2.0f, r * 2.12f);
        }

        g.saveState();
        g.addTransform(juce::AffineTransform::translation(cx, top));

        // glass envelope (takes a warm tint as it heats up)
        juce::Path glass;
        glass.startNewSubPath(-w / 2, h);
        glass.lineTo(-w / 2, 34.0f);
        glass.quadraticTo(-w / 2, 0.0f, 0.0f, 0.0f);
        glass.quadraticTo(w / 2, 0.0f, w / 2, 34.0f);
        glass.lineTo(w / 2, h);
        glass.closeSubPath();
        g.setColour(juce::Colour(0xff0b120d).withAlpha(0.55f));
        g.fillPath(glass);
        if (heat > 0.0f) { g.setColour(hot.withAlpha(0.16f * heat * (0.5f + level))); g.fillPath(glass); }
        g.setColour(juce::Colour(0xffdfe6e0).withAlpha(over ? 0.75f : 0.55f));
        g.strokePath(glass, juce::PathStrokeType(2.0f));

        // getter flash
        juce::Path getter;
        getter.startNewSubPath(-22.0f, 30.0f); getter.quadraticTo(-22.0f, 10.0f, 0.0f, 8.0f);
        getter.quadraticTo(22.0f, 10.0f, 22.0f, 30.0f); getter.quadraticTo(0.0f, 22.0f, -22.0f, 30.0f); getter.closeSubPath();
        g.setColour(juce::Colour(0xffb7bdb9).withAlpha(0.5f));
        g.fillPath(getter);

        // plates and grid (plates glow a little when hot)
        for (float x : { -20.0f, 8.0f })
        {
            juce::ColourGradient pg(juce::Colour(0xff2c302d), x, 0.0f, juce::Colour(0xff2c302d), x + 12.0f, 0.0f, false);
            pg.addColour(0.5, juce::Colour(0xff8a8f8b).interpolatedWith(hot, 0.35f * heat));
            g.setGradientFill(pg);
            g.fillRoundedRectangle(x, 42.0f, 12.0f, 78.0f, 3.0f);
        }
        g.setColour(juce::Colour(0xff9aa09c).interpolatedWith(orange, 0.5f * heat));
        for (int i = 0; i < 7; ++i) g.drawLine(-6.0f, 50.0f + (float) i * 10.0f, 6.0f, 50.0f + (float) i * 10.0f, 1.4f);

        // filament: from warm orange to red-hot
        {
            const auto mid = juce::Colour(0xffffb347).interpolatedWith(juce::Colour(0xffff5a1a), heat);
            const auto edge = juce::Colour(0xffff7a1a).interpolatedWith(juce::Colour(0xffe01a05), heat);
            juce::ColourGradient fg(juce::Colour(0xfffff2c4).withAlpha(0.35f + 0.65f * level), 0.0f, 96.0f, edge.withAlpha(0.0f), 0.0f, 62.0f - 6.0f * heat, true);
            fg.addColour(0.4, mid.withAlpha(0.3f + 0.6f * level));
            g.setGradientFill(fg);
            g.fillEllipse(-14.0f - 3.0f * heat, 62.0f - 4.0f * heat, 28.0f + 6.0f * heat, 68.0f + 8.0f * heat);
            g.setColour(juce::Colour(0xfffff6d8).interpolatedWith(juce::Colour(0xffffd9a0), heat).withAlpha(0.5f + 0.5f * level));
            g.drawLine(0.0f, 66.0f, 0.0f, 124.0f, 2.4f + 0.8f * heat);
        }

        // glass highlight
        g.setGradientFill(juce::ColourGradient(juce::Colours::white.withAlpha(0.38f), -w / 2 + 5.0f, 0.0f,
                                               juce::Colours::white.withAlpha(0.04f), -w / 2 + 20.0f, 0.0f, false));
        g.fillRoundedRectangle(-w / 2 + 5.0f, 14.0f, 15.0f, h - 24.0f, 7.0f);

        // base and pins
        juce::ColourGradient bg(juce::Colour(0xff3a3d3b), -w / 2, 0.0f, juce::Colour(0xff2a2c2b), w / 2, 0.0f, false);
        bg.addColour(0.45, juce::Colour(0xff9da19e));
        g.setGradientFill(bg);
        g.fillRoundedRectangle(-w / 2 - 2.0f, h, w + 4.0f, 22.0f, 4.0f);
        g.setColour(juce::Colour(0xff141614));
        g.fillRoundedRectangle(-w / 2 - 2.0f, h + 16.0f, w + 4.0f, 6.0f, 3.0f);
        g.setColour(juce::Colour(0xffc9a95a));
        for (float px : { -22.0f, -8.0f, 8.0f, 22.0f }) g.fillRect(px - 2.0f, h + 22.0f, 4.0f, 9.0f);
        g.restoreState();

        // name and lamp
        g.setColour(juce::Colours::white.withAlpha(on ? 1.0f : 0.55f));
        g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
        g.drawText(name, juce::Rectangle<float>(0.0f, 204.0f, 120.0f, 20.0f), juce::Justification::centred);
        g.setColour(on ? juce::Colour(0xffb8863a) : juce::Colour(0xff555a56));
        g.fillEllipse(cx - 5.5f, 224.0f, 11.0f, 11.0f);
        g.setColour(on ? juce::Colour(0xffffdd7a).interpolatedWith(juce::Colour(0xffff6a2a), heat) : juce::Colour(0xff3a3f3b));
        g.fillEllipse(cx - 4.5f, 225.0f, 9.0f, 9.0f);
    }

private:
    juce::String name;
    float glow = 0.3f, amount = 0.0f, startAmount = 0.0f;
    bool dragged = false;
};
