#pragma once
#include "../Toolkit.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/EQFilters.h"
#include "Theme.h"
#include "ValueFormat.h"
#include <vector>
#include <cmath>

struct EQNodeInfo {
    juce::String freqParamId;
    juce::String gainParamId;
    juce::String qParamId;
    juce::Colour colour;
    juce::String label;
};

// Dark analyser scope: log-frequency response curve, draggable per-band
// nodes colour-coded identically to the band knobs below.
class EQDisplay : public juce::Component, public juce::Timer, private juce::AudioProcessorValueTreeState::Listener {
public:
    EQDisplay(juce::AudioProcessorValueTreeState& vts, dsp::AnalogEQ& eqRef)
        : apvts(vts), eq(eqRef)
    {
        nodes = {
            {"lcFreq",   "",         "lcQ",   Theme::lowCutCol,  "LC"},
            {"lowFreq",  "lowGain",  "",      Theme::lowBandCol, "LOW"},
            {"mid1Freq", "mid1Gain", "mid1Q", Theme::mid1Col,    "M1"},
            {"mid2Freq", "mid2Gain", "mid2Q", Theme::mid2Col,    "M2"},
            {"highFreq", "highGain", "",      Theme::highBandCol,"HI"},
            {"hcFreq",   "",         "hcQ",   Theme::highCutCol, "HC"}
        };

        for (auto& node : nodes) {
            apvts.addParameterListener(node.freqParamId, this);
            if (!node.gainParamId.isEmpty())
                apvts.addParameterListener(node.gainParamId, this);
        }

        startTimerHz(20);
    }

    ~EQDisplay() override
    {
        stopTimer();
        for (auto& node : nodes) {
            apvts.removeParameterListener(node.freqParamId, this);
            if (!node.gainParamId.isEmpty())
                apvts.removeParameterListener(node.gainParamId, this);
        }
    }

    // Transparent mode for sitting on top of the full-window analyser: draws only the curve and the
    // draggable nodes (plus a gain scale), uses the whole component as the plot, and lets mouse
    // clicks pass through everywhere except on a node.
    void setOverlayMode(bool on) { overlay = on; repaint(); }
    bool hitTest(int x, int y) override { return !overlay || findNearestNode({float(x), float(y)}) >= 0; }

    // Optional analyser drawn behind the EQ curve (owned elsewhere).
    void setSpectrum(SpectrumRenderer* s) { spectrum = s; startTimerHz(s && s->isOn() ? 30 : 20); }
    void timerCallback() override
    {
        if (overlay && !isShowing()) return;
        if (spectrum && spectrum->isOn()) { if (isShowing()) spectrum->update(1.0f / float(juce::jmax(1, getTimerInterval() > 0 ? 1000 / getTimerInterval() : 30))); }
        repaint();
    }
    void refreshRate() { startTimerHz(spectrum && spectrum->isOn() ? 30 : 20); }

    void parameterChanged(const juce::String&, float) override
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<EQDisplay>(this)] {
            if (safeThis != nullptr)
                safeThis->repaint();
        });
    }

    void paint(juce::Graphics& g) override
    {
        if (overlay) { paintOverlay(g); return; }
        auto bounds = getLocalBounds().toFloat();
        g.setColour(Theme::scopeBg);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(Theme::scopeOutline);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

        auto plot = plotRect();
        drawGrid(g, plot);
        if (spectrum) spectrum->paint(g, plot);
        drawEQCurve(g, plot);
        drawNodes(g, plot);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragNodeIndex = findNearestNode(e.position);
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (dragNodeIndex < 0) return;
        auto plot = plotRect();
        auto& node = nodes[static_cast<size_t>(dragNodeIndex)];

        float freq = xToFreq(e.position.x, plot);
        if (auto* p = apvts.getParameter(node.freqParamId))
            p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(freq));

        if (!node.gainParamId.isEmpty()) {
            float gain = yToGain(e.position.y, plot);
            if (auto* p = apvts.getParameter(node.gainParamId))
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(gain));
        }
    }

    void mouseUp(const juce::MouseEvent&) override { dragNodeIndex = -1; }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        int idx = findNearestNode(e.position);
        if (idx < 0) return;
        auto& node = nodes[static_cast<size_t>(idx)];
        if (node.qParamId.isEmpty()) return;
        if (auto* p = apvts.getParameter(node.qParamId)) {
            // Faster spinning moves Q further per notch: the gap between wheel events sets a boost.
            const auto now = juce::Time::getMillisecondCounter();
            const auto gap = now - lastWheelMs;
            lastWheelMs = now;
            float boost = gap < 30 ? 6.0f : gap < 60 ? 4.0f : gap < 120 ? 2.5f : gap < 250 ? 1.5f : 1.0f;
            if (wheel.isSmooth) boost = juce::jmin(boost, 2.0f);   // trackpads already send many small events
            float current = p->getValue();
            float delta = wheel.deltaY * 0.05f * boost;
            p->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, current + delta));
        }
    }

private:
    SpectrumRenderer* spectrum = nullptr;
    bool overlay = false;
    juce::uint32 lastWheelMs = 0;

    juce::Rectangle<float> plotRect() const
    {
        auto b = getLocalBounds().toFloat();
        return overlay ? b : b.reduced(6.0f).withTrimmedBottom(12.0f);
    }

    void paintOverlay(juce::Graphics& g) const
    {
        auto plot = plotRect();
        // EQ gain scale on the right edge (the nodes sit on this axis, not on the dB-level axis behind)
        g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        for (float db : {24.0f, 12.0f, 0.0f, -12.0f, -24.0f}) {
            const float y = gainToY(db, plot);
            g.setColour(juce::Colour(db == 0.0f ? 0x55ffffff : 0x22ffffff));
            for (float x = plot.getX(); x < plot.getRight(); x += 8.0f) g.fillRect(x, y, 4.0f, 1.0f);
            g.setColour(Theme::gridText);
            g.drawText((db > 0 ? "+" : "") + juce::String(int(db)) + " dB EQ", int(plot.getRight()) - 64, int(y) - 12, 62, 11, juce::Justification::centredRight);
        }
        juce::Path path;
        bool started = false;
        for (float x = plot.getX(); x <= plot.getRight(); x += 1.0f) {
            const float y = gainToY(static_cast<float>(eq.getMagnitudeDB(static_cast<double>(xToFreq(x, plot)))), plot);
            if (!started) { path.startNewSubPath(x, y); started = true; } else path.lineTo(x, y);
        }
        g.setColour(juce::Colour(0x66000000)); g.strokePath(path, juce::PathStrokeType(3.5f));
        g.setColour(Theme::curveStroke); g.strokePath(path, juce::PathStrokeType(1.8f));
        drawNodes(g, plot);
    }
    float freqToX(float freq, juce::Rectangle<float> b) const
    {
        float logMin = std::log10(20.0f);
        float logMax = std::log10(30000.0f);
        float norm = (std::log10(freq) - logMin) / (logMax - logMin);
        return b.getX() + norm * b.getWidth();
    }

    float xToFreq(float x, juce::Rectangle<float> b) const
    {
        float norm = (x - b.getX()) / b.getWidth();
        float logMin = std::log10(20.0f);
        float logMax = std::log10(30000.0f);
        return std::pow(10.0f, logMin + norm * (logMax - logMin));
    }

    float gainToY(float gainDB, juce::Rectangle<float> b) const
    {
        float norm = (gainDB + 24.0f) / 48.0f;
        return b.getBottom() - norm * b.getHeight();
    }

    float yToGain(float y, juce::Rectangle<float> b) const
    {
        float norm = (b.getBottom() - y) / b.getHeight();
        return norm * 48.0f - 24.0f;
    }

    void drawGrid(juce::Graphics& g, juce::Rectangle<float> b) const
    {
        float freqs[] = {20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 30000};

        g.setColour(Theme::gridMinor);
        for (float f : freqs) {
            float x = freqToX(f, b);
            g.drawVerticalLine(static_cast<int>(x), b.getY(), b.getBottom());
        }

        for (float db = -18.0f; db <= 18.0f; db += 6.0f) {
            float y = gainToY(db, b);
            g.drawHorizontalLine(static_cast<int>(y), b.getX(), b.getRight());
        }

        g.setColour(Theme::gridMajor);
        float y0 = gainToY(0.0f, b);
        g.drawHorizontalLine(static_cast<int>(y0), b.getX(), b.getRight());

        g.setColour(Theme::gridText);
        g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
        for (float f : freqs) {
            float x = freqToX(f, b);
            g.drawText(ValueFormat::frequency(f),
                       static_cast<int>(x) - 18, static_cast<int>(b.getBottom()) + 3, 36, 13,
                       juce::Justification::centred);
        }
    }

    void drawEQCurve(juce::Graphics& g, juce::Rectangle<float> b) const
    {
        juce::Path path;
        bool started = false;
        for (float x = b.getX(); x <= b.getRight(); x += 1.0f) {
            float freq = xToFreq(x, b);
            float y = gainToY(static_cast<float>(eq.getMagnitudeDB(static_cast<double>(freq))), b);
            if (!started) { path.startNewSubPath(x, y); started = true; }
            else path.lineTo(x, y);
        }

        float y0 = gainToY(0.0f, b);
        juce::Path filled(path);
        filled.lineTo(b.getRight(), y0);
        filled.lineTo(b.getX(), y0);
        filled.closeSubPath();
        g.setColour(Theme::curveFill);
        g.fillPath(filled);

        g.setColour(Theme::curveStroke);
        g.strokePath(path, juce::PathStrokeType(1.5f));
    }

    void drawNodes(juce::Graphics& g, juce::Rectangle<float> b) const
    {
        for (auto& node : nodes) {
            float freq = 1000.0f;
            if (auto* p = apvts.getRawParameterValue(node.freqParamId))
                freq = p->load();
            float gain = 0.0f;
            if (!node.gainParamId.isEmpty())
                if (auto* p = apvts.getRawParameterValue(node.gainParamId))
                    gain = p->load();

            float x = freqToX(freq, b);
            float y = node.gainParamId.isEmpty()
                ? gainToY(static_cast<float>(eq.getMagnitudeDB(static_cast<double>(freq))), b)
                : gainToY(gain, b);

            g.setColour(node.colour);
            g.fillEllipse(x - 6.0f, y - 6.0f, 12.0f, 12.0f);
            g.setColour(juce::Colour(0xCCFFFFFF));
            g.drawEllipse(x - 6.0f, y - 6.0f, 12.0f, 12.0f, 1.0f);

            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            float pillW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), node.label) + 10.0f;
            auto pill = juce::Rectangle<float>(pillW, 14.0f)
                            .withCentre({x, juce::jmax(b.getY() + 9.0f, y - 22.0f)});
            g.setColour(node.colour.withAlpha(0.92f));
            g.fillRoundedRectangle(pill, 4.0f);
            g.setColour(juce::Colour(0xFF101216));
            g.drawText(node.label, pill, juce::Justification::centred);
        }
    }

    int findNearestNode(juce::Point<float> pos) const
    {
        auto b = plotRect();
        float bestDist = overlay ? 18.0f : 30.0f;
        int bestIdx = -1;
        for (size_t i = 0; i < nodes.size(); ++i) {
            auto& node = nodes[i];
            float freq = 1000.0f;
            if (auto* p = apvts.getRawParameterValue(node.freqParamId))
                freq = p->load();
            float gain = 0.0f;
            if (!node.gainParamId.isEmpty())
                if (auto* p = apvts.getRawParameterValue(node.gainParamId))
                    gain = p->load();
            float x = freqToX(freq, b);
            float y = node.gainParamId.isEmpty()
                ? gainToY(static_cast<float>(eq.getMagnitudeDB(static_cast<double>(freq))), b)
                : gainToY(gain, b);
            float d = pos.getDistanceFrom({x, y});
            if (d < bestDist) { bestDist = d; bestIdx = static_cast<int>(i); }
        }
        return bestIdx;
    }

    juce::AudioProcessorValueTreeState& apvts;
    dsp::AnalogEQ& eq;
    std::vector<EQNodeInfo> nodes;
    int dragNodeIndex = -1;
};
