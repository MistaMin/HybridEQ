#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <GoodLookinUI.h>
#include "Theme.h"

// One key for the three preamp modes. Each click steps Off -> Circuit -> HQ -> Off.
//   Off      Preamp Circuit off: the light static models.
//   Circuit  Preamp Circuit on, HQ off: the reduced circuit model.
//   HQ       Preamp Circuit on, HQ on: the full component-level solver.
// It drives the two stored parameters (preampCircuit, preampHQ), so saved sessions and host automation keep
// their meaning; the key just follows them.
class PreampModeKey : public juce::Button,
                      private juce::AudioProcessorValueTreeState::Listener,
                      private juce::AsyncUpdater {
public:
    PreampModeKey(juce::AudioProcessorValueTreeState& state, juce::Colour accent)
        : juce::Button("CIRCUIT"), apvts(state), accentColour(accent)
    {
        circuit = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("preampCircuit"));
        hq = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter("preampHQ"));
        jassert(circuit != nullptr && hq != nullptr);
        apvts.addParameterListener("preampCircuit", this);
        apvts.addParameterListener("preampHQ", this);
        setTooltip("Preamp model - click to step: CIRCUIT unlit = light static model; CIRCUIT = reduced circuit model "
                   "(light, fine at 1x); CIRCUIT HQ = full component-level solver (2x or more oversampling).");
    }

    ~PreampModeKey() override
    {
        cancelPendingUpdate();
        apvts.removeParameterListener("preampCircuit", this);
        apvts.removeParameterListener("preampHQ", this);
    }

    // 0 = off, 1 = circuit (reduced), 2 = HQ
    int mode() const
    {
        if (circuit == nullptr || !circuit->get()) return 0;
        return hq != nullptr && hq->get() ? 2 : 1;
    }

    void setActive(bool active)
    {
        if (isActive == active) return;
        isActive = active;
        repaint();
    }

    void clicked() override
    {
        if (circuit == nullptr || hq == nullptr) return;
        const int next = (mode() + 1) % 3;
        const auto set = [](juce::AudioParameterBool& p, bool on) {
            if (p.get() == on) return;
            p.beginChangeGesture();
            p.setValueNotifyingHost(on ? 1.0f : 0.0f);
            p.endChangeGesture();
        };
        if (next != 0) set(*hq, next == 2);       // choose the engine first, then switch the circuit on
        set(*circuit, next != 0);
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool down) override
    {
        const int m = mode();
        const bool lit = m != 0;
        auto b = getLocalBounds().toFloat();
        const float h = juce::jmin(b.getHeight(), 30.0f);
        auto r = juce::Rectangle<float>(b.getWidth() - 4.0f, h).withCentre(b.getCentre());

        juce::Colour top, bottom;
        if (lit) {
            const juce::Colour base = isActive ? accentColour : Theme::buttonInactiveTop;
            top = base.brighter(0.18f);
            bottom = base.darker(0.22f);
        } else {
            top = isActive ? Theme::buttonTop : Theme::buttonInactiveTop;
            bottom = isActive ? Theme::buttonBottom : Theme::buttonInactiveBottom;
        }
        if (down) { top = top.darker(0.06f); bottom = bottom.darker(0.06f); }

        const juce::Colour mid = top.interpolatedWith(bottom, 0.5f);
        const juce::Colour textCol = mid.getPerceivedBrightness() > 0.55f ? juce::Colours::black : juce::Colours::white;

        goodlookinui::juce_adapter::drawKey(g, r, top, down);
        g.setColour(Theme::buttonOutline.withAlpha(0.6f));
        g.drawRoundedRectangle(r, 4.0f, 1.0f);
        if (highlighted && !lit) {
            g.setColour(juce::Colour(0x12FFFFFF));
            g.fillRoundedRectangle(r, 4.0f);
        }
        g.setColour(textCol);
        g.setFont(juce::FontOptions(9.5f, juce::Font::bold));
        g.drawText(m == 2 ? "CIRCUIT HQ" : "CIRCUIT", r, juce::Justification::centred);
    }

private:
    void parameterChanged(const juce::String&, float) override { triggerAsyncUpdate(); }   // may arrive off the message thread
    void handleAsyncUpdate() override { repaint(); }

    juce::AudioProcessorValueTreeState& apvts;
    juce::AudioParameterBool* circuit = nullptr;
    juce::AudioParameterBool* hq = nullptr;
    juce::Colour accentColour;
    bool isActive = true;
};
