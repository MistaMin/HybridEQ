#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ValueFormat.h"
#include "Theme.h"
#include <cmath>

enum class KnobValueType { Frequency, Gain, Q };

// A large rotary control styled like the reference console: dark graphite
// knurled body with the band's accent colour as a thin indicator ring, and
// the current value rendered directly inside the knob body. When the band is
// bypassed the knob turns neutral grey via setActive(false).
class RotaryKnob : public juce::Slider {
public:
    RotaryKnob(const juce::String& labelText, KnobValueType type, juce::Colour accent)
        : valueType(type), accentColour(accent)
    {
        setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
        setRotaryParameters(juce::MathConstants<float>::pi * 1.2f,
                             juce::MathConstants<float>::pi * 2.8f, true);
        setPopupDisplayEnabled(false, false, nullptr);

        title.setText(labelText, juce::dontSendNotification);
        title.setJustificationType(juce::Justification::centred);
        title.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        title.setColour(juce::Label::textColourId, Theme::textDark);
        title.setInterceptsMouseClicks(false, false);
        addAndMakeVisible(title);
    }

    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        repaint();
    }

    bool getActive() const { return isActive; }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(13));
        valueArea = b.removeFromBottom(14);
        knobArea = b.reduced(1, 0);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = knobArea.toFloat();
        auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto knobBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
        auto radius = diameter * 0.5f;
        auto centre = knobBounds.getCentre();

        // Soft drop shadow under the knob
        g.setColour(juce::Colour(0x55000000));
        g.fillEllipse(knobBounds.translated(0.0f, radius * 0.06f));

        // Dark metal bezel ring
        juce::ColourGradient bezel(Theme::knobTop, centre.x, knobBounds.getY(),
                                   Theme::knobBottom, centre.x, knobBounds.getBottom(), false);
        g.setGradientFill(bezel);
        g.fillEllipse(knobBounds);
        g.setColour(Theme::knobRim);
        g.drawEllipse(knobBounds.reduced(0.5f), 1.0f);

        // Small tick dots around the bezel rim
        g.setColour(isActive ? Theme::knobTick : juce::Colour(0xFF4A4A4C));
        const int numTicks = 11;
        for (int i = 0; i < numTicks; ++i) {
            float t = static_cast<float>(i) / (numTicks - 1);
            float a = juce::jmap(t, -0.8f * juce::MathConstants<float>::pi,
                                    0.8f * juce::MathConstants<float>::pi);
            float tx = centre.x + std::sin(a) * radius * 0.92f;
            float ty = centre.y - std::cos(a) * radius * 0.92f;
            g.fillEllipse(tx - 1.3f, ty - 1.3f, 2.6f, 2.6f);
        }

        // Coloured cap (band accent colour, grey when inactive)
        auto cap = knobBounds.reduced(radius * 0.28f);
        juce::Colour capCol = isActive ? accentColour : Theme::knobInactiveBody;
        juce::ColourGradient capFill(capCol.brighter(0.25f), cap.getX(), cap.getY(),
                                     capCol.darker(0.35f), cap.getRight(), cap.getBottom(), false);
        g.setGradientFill(capFill);
        g.fillEllipse(cap);
        g.setColour(juce::Colour(0xFF101011));
        g.drawEllipse(cap, 1.2f);

        // Gloss highlight (upper half of cap)
        juce::ColourGradient gloss(juce::Colour(0x45FFFFFF), centre.x, cap.getY(),
                                   juce::Colour(0x00000000), centre.x, centre.y, false);
        g.setGradientFill(gloss);
        g.fillEllipse(cap.reduced(cap.getWidth() * 0.12f).withBottom(centre.y));

        // Pointer line showing current rotation
        auto capRadius = cap.getWidth() * 0.5f;
        float angle = juce::jmap(static_cast<float>(valueToProportionOfLength(getValue())),
                                  -0.8f * juce::MathConstants<float>::pi,
                                  0.8f * juce::MathConstants<float>::pi);
        juce::Colour pointerCol = isActive ? Theme::knobText : Theme::knobTextInactive;
        g.setColour(pointerCol);
        juce::Path pointer;
        pointer.addRoundedRectangle(-1.4f, -capRadius * 0.88f, 2.8f, capRadius * 0.62f, 1.4f);
        g.saveState();
        g.addTransform(juce::AffineTransform::rotation(angle).translated(centre));
        g.fillPath(pointer);
        g.restoreState();

        // Value readout below the knob
        g.setColour(isActive ? Theme::knobText : Theme::knobTextInactive);
        g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
        g.drawText(formattedValue(), valueArea, juce::Justification::centred);
    }

private:
    juce::String formattedValue() const
    {
        float v = static_cast<float>(getValue());
        switch (valueType) {
            case KnobValueType::Frequency: return ValueFormat::frequency(v);
            case KnobValueType::Gain:      return ValueFormat::gainDB(v);
            case KnobValueType::Q:         return ValueFormat::qFactor(v);
        }
        return {};
    }

    KnobValueType valueType;
    juce::Colour accentColour;
    bool isActive = true;
    juce::Label title;
    juce::Rectangle<int> knobArea;
    juce::Rectangle<int> valueArea;
};
