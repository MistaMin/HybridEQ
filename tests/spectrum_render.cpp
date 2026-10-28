#include "UI/OptimizedSpectrumRenderer.h"
#include "UI/RotaryKnob.h"
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
    {
        RotaryKnob knob("FREQUENCY", KnobValueType::Frequency, juce::Colour(0xff35e0d0));
        knob.setBounds(0,0,120,98); knob.setRange(20,20000); knob.setValue(20,juce::dontSendNotification);
        auto imageFor = [&](bool cached, float scale = 1.f) {
            juce::Image im(juce::Image::ARGB,int(120*scale),int(98*scale),true,juce::SoftwareImageType());
            juce::Graphics g(im); g.addTransform(juce::AffineTransform::scale(scale));
            if(cached) knob.paint(g); else knob.paintArtwork(g);
            return im;
        };
        const auto initial = imageFor(true);
        knob.setValue(10000,juce::dontSendNotification);
        const auto changed = imageFor(true);
        check(difference(initial,changed)>0,"knob responds without timer dispatch");
        check(difference(changed,imageFor(false))==0,"cached knob matches direct artwork");
        check(difference(imageFor(true,2.f),imageFor(false,2.f))==0,"HiDPI knob artwork");
        save(imageFor(true,2.f),"knob");
        knob.setActive(false);
        check(difference(imageFor(true),imageFor(false))==0,"bypass invalidates knob cache");
        goodlookinui::juce_adapter::useFlatKnobs=true;
        check(difference(imageFor(true),imageFor(false))==0,"flat style invalidates knob cache");
        goodlookinui::juce_adapter::useFlatKnobs=false;
        for(float scale : {1.f,2.f}) {
            double direct=0,cached=0;
            juce::Image im(juce::Image::ARGB,int(120*scale),int(98*scale),true,juce::SoftwareImageType());
            auto time=[&](bool cache) {
                const auto start=std::chrono::steady_clock::now(); juce::Graphics g(im); g.addTransform(juce::AffineTransform::scale(scale));
                for(int k=0;k<18;++k) {if(cache)knob.paint(g);else knob.paintArtwork(g);}
                return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            };
            for(int i=0;i<60;++i) {if(i%2==0){direct+=time(false);cached+=time(true);}else{cached+=time(true);direct+=time(false);}}
            std::printf("bench 18_unchanged_knobs scale=%.0f direct_ms=%.3f cached_ms=%.3f speedup=%.2f\n",scale,direct/60,cached/60,direct/cached);
        }
        CachedArtwork cache; int renders=0;
        juce::Image im(juce::Image::ARGB,100,100,true,juce::SoftwareImageType()); juce::Graphics g(im);
        auto render=[&](juce::Graphics& r){++renders;r.fillAll(juce::Colours::blue);};
        cache.draw(g,{0,0,100,100},render); cache.draw(g,{0,0,100,100},render);
        check(renders==1,"unchanged artwork is rendered once");
        cache.invalidate(); cache.draw(g,{0,0,100,100},render); check(renders==2,"explicit artwork invalidation");
        cache.draw(g,{0,0,80,80},render); check(renders==3,"resize artwork invalidation");
        g.addTransform(juce::AffineTransform::scale(1.5f)); cache.draw(g,{0,0,80,80},render);
        check(renders==4,"fractional scale artwork invalidation");
    }
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
