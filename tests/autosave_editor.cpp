// Drives the real developer editor and checks that design changes are written to Designs/*.csv.
// Uses a scratch copy of the Designs folder (HYBRIDEQ_DESIGNS_DIR), never the project's own files.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void check(bool ok, const juce::String& what) { std::printf("  %s  %s\n", ok ? "PASS" : "FAIL", what.toRawUTF8()); if (!ok) ++failures; }
static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto scratch = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("hybrideq_autosave_test");
    scratch.deleteRecursively(); scratch.createDirectory();
    const juce::File source = juce::File::getCurrentWorkingDirectory().getChildFile(argc > 1 ? argv[1] : "Designs");
    for (auto& f : source.findChildFiles(juce::File::findFiles, false, "*.csv")) f.copyFileTo(scratch.getChildFile(f.getFileName()));
    setenv("HYBRIDEQ_DESIGNS_DIR", scratch.getFullPathName().toRawUTF8(), 1);
    std::printf("scratch folder: %s\n", scratch.getFullPathName().toRawUTF8());

    HybridEQProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    pump(500);
    auto& state = processor.apvts.state;
    const auto settings = scratch.getChildFile("Settings.csv");
    const auto looks = scratch.getChildFile("LookTriggers.csv");
    auto text = [](const juce::File& f) { return f.existsAsFile() ? f.loadFileAsString() : juce::String(); };

    std::printf("1) start-up writes nothing\n");
    check(text(settings) == source.getChildFile("Settings.csv").loadFileAsString(), "Settings.csv unchanged after opening the editor");

    std::printf("2) meter face change (LOOK menu path)\n");
    state.setProperty("meterFace", "black", nullptr);
    pump(900);
    check(text(settings).contains("\"meterFace\",\"black\""), "Settings.csv now holds meterFace=black");
    state.setProperty("meterFace", "cream", nullptr);
    pump(900);
    check(text(settings).contains("\"meterFace\",\"cream\""), "going back to cream is saved too (meterFace=cream)");

    std::printf("3) fader style, glow, colour trigger\n");
    state.setProperty("faderStyle", "neon", nullptr);
    state.setProperty("glow", "off", nullptr);
    state.setProperty("lookTrigger", "faceplate", nullptr);
    pump(900);
    const auto s = text(settings);
    check(s.contains("\"faderStyle\",\"neon\"") && s.contains("\"glow\",\"off\"") && s.contains("\"lookTrigger\",\"faceplate\""), "fader style, glow and colour trigger saved together");

    std::printf("4) closing the editor flushes a pending edit\n");
    state.setProperty("meterFace", "lcd", nullptr);
    editor.reset();
    check(text(settings).contains("\"meterFace\",\"lcd\""), "edit made just before closing is written");

    std::printf("\nfiles in scratch folder:\n"); for (auto& f : scratch.findChildFiles(juce::File::findFiles, false)) std::printf("  %s (%d bytes)\n", f.getFileName().toRawUTF8(), int(f.getSize()));
    std::printf("\n%s (%d failed)\n", failures ? "AUTOSAVE NOT WORKING" : "autosave works", failures);
    std::printf("\nSettings.csv ends up as:\n%s\n", settings.loadFileAsString().toRawUTF8());
    scratch.deleteRecursively();
    return failures ? 1 : 0;
}
