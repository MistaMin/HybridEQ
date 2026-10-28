#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ValueFormat.h"
#include "Theme.h"
#include "SectionPanel.h"
#include "CachedArtwork.h"
#include <cmath>
#include <functional>
#include <limits>
#include <GoodLookinUI.h>

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
        design.label = labelText.toStdString();
        design.colour = "#" + accent.toDisplayString(false).toStdString();
        design.style = "console";
        setMouseDragSensitivity(180);
        setScrollWheelEnabled(false);
    }

    void setActive(bool active)
    {
        if (isActive == active)
            return;
        isActive = active;
        artwork.invalidate(); repaint();
    }

    bool getActive() const { return isActive; }

    // Re-tint at runtime (colour triggers); keeps the saved design otherwise untouched.
    void setAccent(juce::Colour c)
    {
        accentColour = c;
        design.colour = "#" + c.toDisplayString(false).toStdString();
        artwork.invalidate(); repaint();
    }
    juce::Colour getAccent() const { return juce::Colour::fromString("ff" + juce::String(design.colour.substr(1))); }
    // Text colours of the section plate this knob sits on (nullptr args = follow the theme).
    void setPlateText(const juce::Colour* text, const juce::Colour* mid)
    {
        hasPlateText = text != nullptr;
        if (hasPlateText) { plateText = *text; plateMid = *mid; }
        refreshTheme();
    }
    void refreshTheme()
    {
        title.setColour(juce::Label::textColourId, hasPlateText ? plateText : Theme::textDark);
        artwork.invalidate(); repaint();
    }

    void resized() override
    {
        auto b = getLocalBounds();
        title.setBounds(b.removeFromTop(14));
        valueArea = b.removeFromBottom(18);
        knobArea = b.reduced(1, 0);
    }

    void paint(juce::Graphics& g) override
    {
        const auto* panel = dynamic_cast<SectionPanel*>(getParentComponent());
        const auto backdrop = panel ? panel->getBackdrop() : juce::Colour();
        const bool flat = goodlookinui::juce_adapter::useFlatKnobs;
        if (cachedValue != getValue() || cachedMin != getMinimum() || cachedMax != getMaximum()
            || cachedBackdrop != backdrop || cachedFlat != flat || cachedSecondaryText != Theme::textMid) {
            artwork.invalidate(); cachedValue = getValue(); cachedMin = getMinimum(); cachedMax = getMaximum();
            cachedBackdrop = backdrop; cachedFlat = flat; cachedSecondaryText = Theme::textMid;
        }
        artwork.draw(g, getLocalBounds(), [this](juce::Graphics& raster) { paintArtwork(raster); });
    }

    void paintArtwork(juce::Graphics& g)
    {
        auto bounds = knobArea.toFloat();
        auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
        auto knobBounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());

        juce::Colour backdrop;   // plate behind this knob: lets the toolkit draw dark marks on light plates and light marks on dark ones
        if (auto* panel = dynamic_cast<SectionPanel*>(getParentComponent())) backdrop = panel->getBackdrop();
        goodlookinui::juce_adapter::drawKnob(g, knobBounds, valueToProportionOfLength(getValue()), design, isActive, backdrop);

        if(getWidth()>=110) {
            g.setColour(hasPlateText?plateMid:Theme::textMid);g.setFont(juce::FontOptions(8.0f));
            const int y=juce::roundToInt(knobBounds.getBottom()-13);
            g.drawText(formatValue(float(getMinimum())),6,y,35,12,juce::Justification::centred);
            g.drawText(formatValue(float(getMaximum())),getWidth()-41,y,35,12,juce::Justification::centred);
        }
        // Exact values sit in a recessed readout beneath the physical control.
        auto readout=valueArea.toFloat().withSizeKeepingCentre(68.0f,15.0f);
        g.setColour(juce::Colour(0xff20292c));g.fillRoundedRectangle(readout,2);
        g.setColour(juce::Colour(0xff465255));g.drawRoundedRectangle(readout,2,0.6f);
        // Value readout below the knob
        g.setColour(isActive ? Theme::knobText : Theme::knobTextInactive);
        g.setFont(juce::FontOptions(design.fontSize, juce::Font::bold));
        g.drawText(formattedValue(), valueArea, juce::Justification::centred);
    }

    std::function<void()> onSelect;   // developer mode: clicking a knob selects it in the inspector
    void mouseDown(const juce::MouseEvent& e) override { if (onSelect) onSelect(); juce::Slider::mouseDown(e); }
    void applyDesign(const goodlookinui::Item& item) {
        design = item;
        title.setText(item.label, juce::dontSendNotification);
        title.setFont(juce::FontOptions(item.fontSize, juce::Font::bold));
        artwork.invalidate(); repaint();
    }
    const goodlookinui::Item& getDesign() const { return design; }
private:
    CachedArtwork artwork;
    double cachedValue = std::numeric_limits<double>::quiet_NaN(), cachedMin = 0, cachedMax = 0;
    juce::Colour cachedBackdrop, cachedSecondaryText;
    bool cachedFlat = false;
    goodlookinui::Item design;
    juce::String formattedValue() const
    {
        return formatValue(static_cast<float>(getValue()));
    }
    juce::String formatValue(float v) const
    {
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
    bool hasPlateText = false;
    juce::Colour plateText, plateMid;
    juce::Label title;
    juce::Rectangle<int> knobArea;
    juce::Rectangle<int> valueArea;
};
