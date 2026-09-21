#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "Theme.h"
#include <vector>

// Rounded card grouping a set of controls under a title, with items laid
// out in an evenly spaced row. Panels are meant to be placed in a wrapping
// FlexBox so the whole editor reflows cleanly when resized.
class SectionPanel : public juce::Component {
public:
    explicit SectionPanel(const juce::String& titleText) : title(titleText) {}

    void addItem(juce::Component& c, int preferredWidth)
    {
        items.push_back({&c, preferredWidth});
        addAndMakeVisible(c);
    }

    // Narrow vertical channel-strip layout: items are stacked top-to-bottom
    // (each item's "width" value is reused as its height in this mode),
    // matching a hardware EQ module rather than a wide horizontal row.
    void setVerticalLayout(bool v, int width = 132)
    {
        vLayout = v;
        vWidth = width;
    }

    // Reserve a tiny square slot (top-right) for a per-band bypass button.
    void setBypassButton(juce::Component& bypass)
    {
        bypassButton = &bypass;
        addAndMakeVisible(bypass);
    }

    int getPreferredHeight() const
    {
        if (!vLayout)
            return 116;
        int h = 40;
        for (auto& item : items)
            h += item.width + 5;
        return h;
    }

    void paint(juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced(2.0f);

        juce::ColourGradient fill(Theme::panelTop, 0.0f, b.getY(),
                                  Theme::panelBottom, 0.0f, b.getBottom(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(b, 7.0f);

        g.setColour(Theme::panelHighlight);
        g.drawHorizontalLine(static_cast<int>(b.getY()) + 1,
                             static_cast<int>(b.getX()) + 8,
                             static_cast<int>(b.getRight()) - 8);
        g.setColour(juce::Colour(0x26000000));
        g.drawHorizontalLine(static_cast<int>(b.getBottom()) - 2,
                             static_cast<int>(b.getX()) + 8,
                             static_cast<int>(b.getRight()) - 8);

        g.setColour(Theme::panelOutline);
        g.drawRoundedRectangle(b, 7.0f, 1.0f);

        g.setColour(Theme::textDark);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        auto titleArea = b.removeFromTop(20.0f);
        if (bypassButton != nullptr)
            titleArea.removeFromRight(28.0f);
        g.drawText(title, titleArea.reduced(10.0f, 0.0f), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(8);
        auto strip = b.removeFromTop(20);
        if (bypassButton != nullptr)
            bypassButton->setBounds(strip.getRight() - 22, strip.getY() + 1, 18, 18);

        if (vLayout) {
            layoutVertical(b);
            return;
        }

        int contentW = 0;
        for (auto& item : items)
            contentW += item.width + 6;
        contentW -= 6;

        int x = b.getX() + (b.getWidth() - contentW) / 2;
        if (contentW > b.getWidth())
            x = b.getX();
        for (auto& item : items) {
            item.component->setBounds(x, b.getY(), item.width, b.getHeight());
            x += item.width + 6;
        }
    }

    int getPreferredWidth() const
    {
        if (vLayout)
            return vWidth + 16;
        int w = 16;
        for (auto& item : items)
            w += item.width + 6;
        return w;
    }

private:
    // Stack every item top-to-bottom, full width, using each item's stored
    // "width" value as its height (narrow hardware channel-strip look).
    void layoutVertical(juce::Rectangle<int> b)
    {
        int y = b.getY();
        for (auto& item : items) {
            item.component->setBounds(b.getX(), y, b.getWidth(), item.width);
            y += item.width + 5;
        }
    }

    struct Item { juce::Component* component; int width; };

    juce::String title;
    juce::Component* bypassButton = nullptr;
    bool vLayout = false;
    int vWidth = 372;
    std::vector<Item> items;
};
