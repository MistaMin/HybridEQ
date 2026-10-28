#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Cache only a component's own artwork, never its interactive children. The raster
// follows the graphics context's physical scale, including editor zoom and HiDPI.
class CachedArtwork {
public:
    void invalidate() { dirty = true; }
    template<class Paint>
    void draw(juce::Graphics& g, juce::Rectangle<int> bounds, Paint&& paint) {
        if (bounds.isEmpty()) return;
        const float scale = juce::jmax(0.01f, g.getInternalContext().getPhysicalPixelScaleFactor());
        const int width = juce::jmax(1, int(std::ceil(float(bounds.getWidth()) * scale)));
        const int height = juce::jmax(1, int(std::ceil(float(bounds.getHeight()) * scale)));
        if (dirty || logicalBounds != bounds || pixelScale != scale) {
            if (image.getWidth() != width || image.getHeight() != height)
                image = juce::Image(juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
            else
                image.clear(image.getBounds());
            juce::Graphics raster(image);
            raster.addTransform(juce::AffineTransform::scale(scale).translated(-float(bounds.getX()) * scale, -float(bounds.getY()) * scale));
            paint(raster);
            logicalBounds = bounds; pixelScale = scale; dirty = false;
        }
        g.drawImageTransformed(image, juce::AffineTransform::scale(1.0f / scale).translated(float(bounds.getX()), float(bounds.getY())));
    }
private:
    juce::Image image;
    juce::Rectangle<int> logicalBounds;
    float pixelScale = 0;
    bool dirty = true;
};
