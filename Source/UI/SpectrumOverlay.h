#pragma once
#include "../Toolkit.h"
#include "Theme.h"

// Full-window analyser: the whole editor becomes a large spectrum or spectrogram (plugin UI, built on
// the toolkit's SpectrumRenderer). It owns its own renderer so it can show a different look from the
// small display behind the EQ curve, while sharing the saved colours, block size and smoothing.
class SpectrumOverlay : public juce::Component, private juce::Timer {
public:
    SpectrumOverlay(const spectrum::SampleRing& in, const spectrum::SampleRing& out, std::function<double()> rate)
        : renderer(in, out, std::move(rate))
    {
        for (auto* b : {&modeBtn, &closeBtn}) { addAndMakeVisible(b); b->setColour(juce::TextButton::buttonColourId, Theme::buttonTop); b->setColour(juce::TextButton::textColourOffId, Theme::buttonText); }
        modeBtn.setButtonText("SPECTROGRAM"); closeBtn.setButtonText("CLOSE");
        modeBtn.onClick = [this] { gram = !gram; apply(); };
        closeBtn.onClick = [this] { setVisible(false); };
        setInterceptsMouseClicks(true, true);
    }
    // Takes the project's spectrum settings; the overlay forces its own look (waterfall or analyser).
    void setBase(const SpectrumSettings& s) { base = s; apply(); }
    void visibilityChanged() override { if (isVisible()) startTimerHz(30); else stopTimer(); }
    bool isSpectrogram() const { return gram; }
    SpectrumRenderer& getRenderer() { return renderer; }

    // Draggable EQ nodes shown on top of the analyser, on the same log-frequency axis.
    void setNodeLayer(juce::Component* layer) { nodeLayer = layer; addAndMakeVisible(layer); resized(); }

    void resized() override {
        auto a = getLocalBounds().reduced(14, 10).removeFromTop(26);
        closeBtn.setBounds(a.removeFromRight(80)); a.removeFromRight(8); modeBtn.setBounds(a.removeFromRight(130));
        if (nodeLayer) nodeLayer->setBounds(plotArea().toNearestInt());
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(Theme::bgFill);
        g.setColour(Theme::textDark); g.setFont(juce::FontOptions(20.0f, juce::Font::bold)); g.drawText("SPECTRUM", 18, 8, 240, 30, juce::Justification::centredLeft);
        g.setColour(Theme::textMid); g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
        g.drawText(gram ? "SPECTROGRAM  /  TIME DOWN  /  NEWEST AT TOP" : "FREQUENCY ANALYSER  /  0 TO -90 dBFS", 120, 14, 420, 16, juce::Justification::centredLeft);
        auto plot = plotArea();
        g.setColour(Theme::scopeBg); g.fillRoundedRectangle(plot.expanded(4), 6);
        renderer.paint(g, plot);
        // grid and labels on the same log axis the renderer uses
        g.setFont(juce::FontOptions(9.0f));
        for (float f : {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f}) {
            const float x = plot.getX() + std::log10(f / 20.0f) / std::log10(30000.0f / 20.0f) * plot.getWidth();
            g.setColour(juce::Colour(0x22ffffff)); g.drawVerticalLine(int(x), plot.getY(), plot.getBottom());
            g.setColour(Theme::gridText); g.drawText(f >= 1000 ? juce::String(f / 1000.0f, f >= 10000 ? 0 : (f == 1000 || f == 2000 || f == 5000 ? 0 : 1)) + "k" : juce::String(int(f)), int(x) - 20, int(plot.getBottom()) + 6, 40, 12, juce::Justification::centred);
        }
        if (!gram) for (int db : {0, -12, -24, -36, -48, -60, -72, -84}) {
            const float y = plot.getBottom() - (float(db) - SpectrumRenderer::bottomDb) / (SpectrumRenderer::topDb - SpectrumRenderer::bottomDb) * plot.getHeight();
            g.setColour(juce::Colour(0x18ffffff)); g.drawHorizontalLine(int(y), plot.getX(), plot.getRight());
            g.setColour(Theme::gridText); g.drawText(juce::String(db), int(plot.getX()) + 3, int(y) - 11, 30, 10, juce::Justification::centredLeft);
        }
    }
private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced(18, 0).withTrimmedTop(48).withTrimmedBottom(26); }
    juce::Component* nodeLayer = nullptr;
    void timerCallback() override { renderer.update(1.0f / 30.0f); repaint(); }
    void apply() {
        auto s = base; s.on = true; s.style = gram ? SpectrumSettings::Style::Spectrogram : (base.style == SpectrumSettings::Style::Spectrogram ? SpectrumSettings::Style::Multicolour : base.style);
        if (gram && s.spectrogramMap == 0 && false) s.spectrogramMap = 0;
        renderer.setSettings(s); modeBtn.setButtonText(gram ? "SPECTRUM" : "SPECTROGRAM"); repaint();
    }
    SpectrumRenderer renderer; SpectrumSettings base; bool gram = true; juce::TextButton modeBtn, closeBtn;
};
