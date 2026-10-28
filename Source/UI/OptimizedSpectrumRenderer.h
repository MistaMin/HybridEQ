// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Marcos Deida
#pragma once
#include <SpectrumRenderer.h>

// Project-owned adapter; shared toolkit stays pinned at its existing revision.
namespace hybrideq {
namespace spectrum = goodlookinui::spectrum;
using SpectrumSettings = goodlookinui::juce_adapter::SpectrumSettings;

// Draws the analyser behind the EQ curve, on the same log-frequency axis. The FFT runs on the
// UI thread only (the audio thread just copies samples into a ring), only while the spectrum is
// on and visible, and only when new audio has arrived.
class SpectrumRenderer {
public:
    SpectrumRenderer(const spectrum::SampleRing& inputRing, const spectrum::SampleRing& outputRing, std::function<double()> sampleRate)
        : in(inputRing), out(outputRing), getRate(std::move(sampleRate)) { apply(settings); }

    const SpectrumSettings& getSettings() const { return settings; }
    void setSettings(const SpectrumSettings& s) { settings = s; apply(s); columnsValid = false; coloursValid = false; gramDirty = true; }
    bool isOn() const { return settings.on; }
    const spectrum::Analyser& analyser() const { return analyser_; }
    float lastUpdateMs() const { return updateMs; }
    const std::vector<float>& columns() const { return cols; }
    std::uint64_t updateTick() const { return tick; }

    // One UI tick (call about 30 times a second while visible).
    void update(float seconds) {
        if (!settings.on) return;
        const double rate = getRate();
        if (rate > 1000.0 && rate != analyser_.sampleRate()) analyser_.setSampleRate(rate);
        const auto t0 = juce::Time::getHighResolutionTicks();
        analyser_.analyse(settings.useOutput ? out : in);
        analyser_.decay(seconds);
        ++tick; columnsValid = false;
        if (settings.style == SpectrumSettings::Style::Spectrogram && lastN > 0 && hist.columns() == lastN) { computeColumns(lastN, lastFMin, lastFMax); hist.push(cols.data(), gramFloorDb, gramCeilDb); ++historyRevision; }
        updateMs = float(juce::Time::highResolutionTicksToSeconds(juce::Time::getHighResolutionTicks() - t0) * 1000.0);
    }

    // Reduces the analyser to `n` display columns (with optional smoothing) into cols / holdCols.
    void computeColumns(int n, float fMin, float fMax) {
        if (columnsValid && columnN == n && columnMin == fMin && columnMax == fMax) return;
        columnsValid = true; columnN = n; columnMin = fMin; columnMax = fMax;
        if (int(cols.size()) != n) { cols.assign(std::size_t(n), spectrum::floorDb); holdCols.assign(std::size_t(n), spectrum::floorDb); }
        spectrum::toColumns(analyser_.displayDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, cols.data());
        spectrum::toColumns(analyser_.holdDb().data(), analyser_.numBins(), analyser_.sampleRate(), n, fMin, fMax, holdCols.data());
        if (settings.smoothing > 0) {
            if (int(scratch.size()) != n) { scratch.assign(std::size_t(n), 0.0f); prefix.assign(std::size_t(n) + 1, 0.0); smoothed.assign(std::size_t(n), 0.0f); }
            const float octaves = float(std::log2(double(fMax) / double(fMin)));
            spectrum::smoothColumns(cols.data(), smoothed.data(), n, octaves, settings.smoothing, scratch.data(), prefix.data()); cols = smoothed;
            spectrum::smoothColumns(holdCols.data(), smoothed.data(), n, octaves, settings.smoothing, scratch.data(), prefix.data()); holdCols = smoothed;
        }
    }

    // dB scale of the display: top .. bottom of the plot.
    static constexpr float topDb = 0.0f, bottomDb = -90.0f;

    void paint(juce::Graphics& g, juce::Rectangle<float> plot, float fMin = 20.0f, float fMax = 30000.0f) {
        if (!settings.on) return;
        const int n = juce::jlimit(8, 2048, int(plot.getWidth()));
        computeColumns(n, fMin, fMax);
        lastN = n; lastFMin = fMin; lastFMax = fMax;
        const float step = plot.getWidth() / float(n), alpha = settings.opacity;
        // the analyser has nothing above Nyquist: stop drawing there instead of dropping to the floor
        const int nv = juce::jlimit(2, n, int(std::ceil(std::log(std::min(double(fMax), analyser_.sampleRate() * 0.5) / double(fMin)) / std::log(double(fMax) / double(fMin)) * double(n))));
        auto yFor = [&](float db) { return plot.getBottom() - juce::jlimit(0.0f, 1.0f, (db - bottomDb) / (topDb - bottomDb)) * plot.getHeight(); };
        prepareColours(n, fMin, fMax);
        auto colourFor = [&](int c) { return juce::Colour(base[std::size_t(c)]); };
        g.saveState(); g.reduceClipRegion(plot.toNearestInt());

        if (settings.style == SpectrumSettings::Style::Spectrogram) {
            paintSpectrogram(g, plot, n, fMin, fMax, alpha);
        } else if (settings.style == SpectrumSettings::Style::Bars) {
            const int group = 3; const float w = step * float(group);
            for (int c = 0; c + group <= nv; c += group) {
                float m = spectrum::floorDb, pk = spectrum::floorDb; for (int k = 0; k < group; ++k) { m = std::max(m, cols[std::size_t(c + k)]); pk = std::max(pk, holdCols[std::size_t(c + k)]); }
                if (m <= spectrum::floorDb + 1.0f) continue;
                const auto col = colourFor(c + group / 2); const float x = plot.getX() + float(c) * step, y = yFor(m);
                g.setGradientFill(juce::ColourGradient(col.withAlpha(alpha), x, y, col.darker(0.7f).withAlpha(alpha * 0.45f), x, plot.getBottom(), false));
                g.fillRect(x, y, w - 1.0f, plot.getBottom() - y);
                if (settings.peakHold) { g.setColour(col.brighter(0.6f).withAlpha(alpha)); g.fillRect(x, yFor(pk) - 1.0f, w - 1.0f, 1.5f); }
            }
        } else {
            line.clear(); fill.clear();
            for (int c = 0; c < nv; ++c) {
                const float x = plot.getX() + (float(c) + 0.5f) * step, y = yFor(cols[std::size_t(c)]);
                if (c == 0) { line.startNewSubPath(x, y); fill.startNewSubPath(x, plot.getBottom()); fill.lineTo(x, y); } else { line.lineTo(x, y); fill.lineTo(x, y); }
            }
            fill.lineTo(plot.getX() + (float(nv) - 0.5f) * step, plot.getBottom()); fill.closeSubPath();
            if (settings.style == SpectrumSettings::Style::Standard) {
                const auto c = juce::Colour(settings.standard);
                g.setGradientFill(juce::ColourGradient(c.withAlpha(alpha * 0.45f), 0, plot.getY(), c.withAlpha(alpha * 0.04f), 0, plot.getBottom(), false)); g.fillPath(fill);
                g.setColour(c.withAlpha(alpha)); g.strokePath(line, juce::PathStrokeType(1.3f));
            } else {
                // one translucent column per pixel in that frequency's colour, then a glowing multicolour top line
                for (int c = 0; c < nv; ++c) {
                    const float x = plot.getX() + float(c) * step, y = yFor(cols[std::size_t(c)]); if (y >= plot.getBottom() - 0.5f) continue;
                    const auto col = colourFor(c);
                    g.setGradientFill(juce::ColourGradient(col.withAlpha(alpha * 0.55f), x, y, col.withAlpha(alpha * 0.04f), x, plot.getBottom(), false));
                    g.fillRect(x, y, step + 0.5f, plot.getBottom() - y);
                }
                juce::ColourGradient grad(colourFor(0).withAlpha(alpha), plot.getX(), 0, colourFor(n - 1).withAlpha(alpha), plot.getRight(), 0, false);
                for (int i = 1; i < 24; ++i) grad.addColour(double(i) / 24.0, colourFor(juce::jmin(n - 1, i * n / 24)).withAlpha(alpha));
                g.setGradientFill(grad); g.strokePath(line, juce::PathStrokeType(5.0f)); // soft halo
                g.setGradientFill(grad); g.strokePath(line, juce::PathStrokeType(1.6f));
            }
            if (settings.peakHold) {
                hold.clear(); for (int c = 0; c < nv; ++c) { const float x = plot.getX() + (float(c) + 0.5f) * step, y = yFor(holdCols[std::size_t(c)]); c == 0 ? hold.startNewSubPath(x, y) : hold.lineTo(x, y); }
                g.setColour(juce::Colours::white.withAlpha(0.28f * alpha)); g.strokePath(hold, juce::PathStrokeType(0.8f));
            }
        }
        g.restoreState();
    }

private:
    // Frequency colours and the quantised colour maps depend on settings/geometry, not audio.
    void prepareColours(int n, float fMin, float fMax) {
        if (coloursValid && colourN == n && colourMin == fMin && colourMax == fMax) return;
        coloursValid = true; colourN = n; colourMin = fMin; colourMax = fMax;
        for (int c = 0; c < n; ++c)
            base[std::size_t(c)] = spectrum::colourAt(fMin * std::pow(fMax / fMin, (float(c) + 0.5f) / float(n)), settings.range);
        if (settings.style != SpectrumSettings::Style::Spectrogram) return;
        const auto alpha = std::uint32_t(std::lround(settings.opacity * 255.0f));
        heat.resize(256);
        if (settings.spectrogramMap == 1) shades.resize(std::size_t(n) * 256);
        for (int v = 0; v < 256; ++v) {
            heat[std::size_t(v)] = (alpha << 24) | (spectrum::heatColour(std::uint8_t(v)) & 0x00ffffffu);
            if (settings.spectrogramMap == 1) for (int c = 0; c < n; ++c)
                shades[std::size_t(c) * 256 + std::size_t(v)] = (alpha << 24) | (spectrum::shadeColour(base[std::size_t(c)], std::uint8_t(v)) & 0x00ffffffu);
        }
        gramDirty = true;
    }

    // Keep colour pixels in a circular image too. Only newly arrived rows need colouring;
    // two source rectangles display the ring in newest-first order without scrolling/copying it.
    void paintSpectrogram(juce::Graphics& g, juce::Rectangle<float> plot, int n, float, float, float) {
        const int rows = juce::jlimit(8, 1400, int(plot.getHeight()));
        if (hist.columns() != n || hist.rows() != rows) {
            hist.configure(n, rows); gram = juce::Image(juce::Image::ARGB, n, rows, true);
            gramHead = 0; paintedRevision = historyRevision; gramDirty = true;
        }
        const int added = int(std::min<std::uint64_t>(historyRevision - paintedRevision, std::uint64_t(rows)));
        gramHead = (gramHead + rows - added) % rows;
        const int repaintRows = gramDirty ? rows : added;
        if (repaintRows > 0) {
            juce::Image::BitmapData px(gram, juce::Image::BitmapData::writeOnly);
            for (int age = 0; age < repaintRows; ++age) {
                auto* dst = reinterpret_cast<std::uint32_t*>(px.getLinePointer((gramHead + age) % rows));
                if (age >= hist.filled()) { std::fill(dst, dst + n, 0); continue; }
                const auto* src = hist.row(age);
                for (int c = 0; c < n; ++c)
                    dst[c] = settings.spectrogramMap == 1 ? shades[std::size_t(c) * 256 + src[c]] : heat[src[c]];
            }
        }
        gramDirty = false; paintedRevision = historyRevision;
        const int first = rows - gramHead;
        g.drawImage(gram, int(plot.getX()), int(plot.getY()), n, first, 0, gramHead, n, first);
        if (gramHead > 0)
            g.drawImage(gram, int(plot.getX()), int(plot.getY()) + first, n, gramHead, 0, 0, n, gramHead);
    }

    void apply(const SpectrumSettings& s) {
        const int size = spectrum::fftSizes[juce::jlimit(0, int(std::size(spectrum::fftSizes)) - 1, s.sizeIndex)];
        const double rate = getRate ? getRate() : 48000.0;
        if (size != analyser_.fftSize() || (rate > 1000.0 && rate != analyser_.sampleRate())) analyser_.configure(size, rate > 1000.0 ? rate : 48000.0);
        analyser_.setTilt(s.tilt); analyser_.setFall(s.fall);
        if (base.size() < 2048) base.assign(2048, 0xff000000u);
    }
    const spectrum::SampleRing& in; const spectrum::SampleRing& out;
    std::function<double()> getRate;
    SpectrumSettings settings; spectrum::Analyser analyser_;
    std::vector<float> cols, holdCols, scratch, smoothed; std::vector<double> prefix; juce::Path line, fill, hold; float updateMs = 0.0f;
    spectrum::SpectrogramHistory hist; juce::Image gram; std::vector<std::uint32_t> base; std::uint64_t tick = 0; int lastN = 0; float lastFMin = 20.0f, lastFMax = 30000.0f;
    bool columnsValid = false, coloursValid = false, gramDirty = true;
    int columnN = 0, colourN = 0, gramHead = 0;
    float columnMin = 0, columnMax = 0, colourMin = 0, colourMax = 0;
    std::vector<std::uint32_t> heat, shades;
    std::uint64_t historyRevision = 0, paintedRevision = 0;
    static constexpr float gramFloorDb = -90.0f, gramCeilDb = -10.0f;
};

} // namespace hybrideq
