#include "experimental/OptimizedSpectrumRenderer.h"
#include <chrono>
#include <cstdio>
#include <memory>
using Settings = goodlookinui::juce_adapter::SpectrumSettings;
using Baseline = goodlookinui::juce_adapter::SpectrumRenderer;
namespace sp = goodlookinui::spectrum;
static int failures = 0;
static juce::File outputDirectory;
static void check(bool ok, const char* what) { if (!ok) { ++failures; std::printf("FAIL %s\n", what); } }
struct Fixture {
    sp::SampleRing in, out;
    std::vector<float> samples = std::vector<float>(2048);
    std::uint64_t pos = 0;
    void push() {
        for (auto& s : samples) { const double t = double(pos++) / 48000.; s = float(0.2 * std::sin(t * 1000. * 6.28318530718) + 0.03 * std::sin(t * 67. * 6.28318530718)); }
        const float* ptr[] = {samples.data()}; in.push(ptr,1,int(samples.size())); out.push(ptr,1,int(samples.size()));
    }
};
template<class R> juce::Image paint(R& r, int w, int h) {
    juce::Image image(juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
    juce::Graphics g(image); r.paint(g, {0.f,0.f,float(w),float(h)}); return image;
}
static int difference(const juce::Image& a, const juce::Image& b) {
    int maxDelta = 0;
    for (int y=0;y<a.getHeight();++y) for (int x=0;x<a.getWidth();++x) {
        const auto c=a.getPixelAt(x,y), d=b.getPixelAt(x,y);
        maxDelta=std::max({maxDelta,std::abs(int(c.getRed())-int(d.getRed())),std::abs(int(c.getGreen())-int(d.getGreen())),std::abs(int(c.getBlue())-int(d.getBlue())),std::abs(int(c.getAlpha())-int(d.getAlpha()))});
    }
    return maxDelta;
}
static void save(const juce::Image& im, const juce::String& name) {
    const auto file = outputDirectory.getChildFile(name + ".png");
    file.deleteFile();
    juce::FileOutputStream stream(file);
    check(stream.openedOk() && juce::PNGImageFormat().writeImageToStream(im, stream), "write screenshot");
}
int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI gui;
    outputDirectory = argc > 1 ? juce::File(argv[1]) : juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("hybrideq-spectrum-qa");
    outputDirectory.createDirectory();
    // Production uses the original toolkit renderer again. This fixture is retained
    // for research only; it is not included by Toolkit.h or the plugin targets.
    auto fixture=std::make_unique<Fixture>(); auto& f=*fixture;
    Baseline old(f.in,f.out,[]{return 48000.;}); hybrideq::SpectrumRenderer now(f.in,f.out,[]{return 48000.;});
    Settings s;
    for (auto style : {Settings::Style::Standard, Settings::Style::Multicolour, Settings::Style::Bars, Settings::Style::Spectrogram}) {
        for(int map=0;map<2;++map) {
            s.style=style; s.spectrogramMap=map; s.smoothing=6; old.setSettings(s); now.setSettings(s);
            paint(old,400,180); paint(now,400,180);
            int maxDelta=0;
            for(int i=0;i<220;++i) {
                f.push(); old.update(1.f/30); now.update(1.f/30);
                // Paint every third tick to exercise accumulated rows and wraparound.
                if(i%3==0) maxDelta=std::max(maxDelta,difference(paint(old,400,180),paint(now,400,180)));
            }
            std::printf("pixels style=%s map=%d maximum_channel_delta=%d\n",Settings::styleCode(style),map,maxDelta);
            check(maxDelta==0,"pixel equivalence");
            check(old.columns()==now.columns(),"spectrum columns unchanged");
            check(old.analyser().displayDb()==now.analyser().displayDb(),"FFT magnitudes unchanged");
            check(old.analyser().holdDb()==now.analyser().holdDb(),"peak hold unchanged");
            // Recolour existing history, then resize, with no incoming update.
            s.opacity=0.55f; s.range[1]=0xff00ff00; old.setSettings(s); now.setSettings(s);
            check(difference(paint(old,400,180),paint(now,400,180))==0,"settings invalidate history");
            check(difference(paint(old,320,140),paint(now,320,140))==0,"resize history");
        }
        paint(old,800,360); paint(now,800,360);
        for (int i=0;i<80;++i) { f.push(); old.update(1.f/30); now.update(1.f/30); }
        paint(old,800,360);
        save(paint(now,800,360),juce::String(Settings::styleCode(style)));
    }
    // Matched software-renderer timing: full history; alternate reference/candidate order.
    for(auto style : {Settings::Style::Standard,Settings::Style::Multicolour,Settings::Style::Bars,Settings::Style::Spectrogram}) {
        for(int sizeIndex : {3,5}) {
            s=Settings{}; s.style=style; s.sizeIndex=sizeIndex; old.setSettings(s); now.setSettings(s);
            paint(old,1100,650); paint(now,1100,650);
            for(int i=0;i<660;++i) { f.push(); old.update(1.f/30); now.update(1.f/30); }
            paint(old,1100,650); paint(now,1100,650);
            double a=0,b=0;
            juce::Image ai(juce::Image::ARGB,1100,650,true,juce::SoftwareImageType()), bi(ai.createCopy());
            auto time=[&](auto& r,juce::Image& im) {
                const auto start=std::chrono::steady_clock::now(); r.update(1.f/30);
                juce::Graphics g(im); r.paint(g,{0.f,0.f,1100.f,650.f});
                return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            };
            for(int i=0;i<40;++i) { f.push(); if(i%2==0) {a+=time(old,ai);b+=time(now,bi);}else{b+=time(now,bi);a+=time(old,ai);} }
            std::printf("bench style=%s fft=%d reference_ms=%.3f optimized_ms=%.3f speedup=%.2f\n",Settings::styleCode(style),sp::fftSizes[sizeIndex],a/40,b/40,a/b);
        }
    }
    std::printf("failures=%d\n",failures); return failures?1:0;
}
