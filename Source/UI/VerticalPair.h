#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Stacks two existing components top/bottom inside a single column so they
// can be added as one item into a SectionPanel's horizontal row (e.g. the
// preamp's TYPE cycle button sitting directly above the CIRCUIT toggle
// instead of floating beside it).
class VerticalPair : public juce::Component {
public:
    void setChildren(juce::Component& topComp, juce::Component& bottomComp)
    {
        top = &topComp;
        bottom = &bottomComp;
        addAndMakeVisible(top);
        addAndMakeVisible(bottom);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        if (top != nullptr)
            top->setBounds(b.removeFromTop(b.getHeight() / 2));
        if (bottom != nullptr)
            bottom->setBounds(b);
    }

private:
    juce::Component* top = nullptr;
    juce::Component* bottom = nullptr;
};
