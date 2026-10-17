#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include <cstdio>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    HybridEQProcessor processor;
    processor.prepareToPlay(48000.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    if (!editor) {
        std::fprintf(stderr, "Failed to create editor\n");
        return 1;
    }

    editor->setSize(1180, 880);
    editor->setVisible(true);

    // Let the message queue process so layout and theme settling complete
    juce::MessageManager::getInstance()->runDispatchLoopUntil(1200);

    const auto bounds = editor->getLocalBounds();
    const juce::Image snapshot = editor->createComponentSnapshot(bounds, false, 2.0f); // 2x scale for crisp display

    juce::File out = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("HybridEQ_UI.png");

    out.deleteFile();
    juce::FileOutputStream fos(out);
    if (!fos.openedOk()) {
        std::fprintf(stderr, "Failed to open output file: %s\n", out.getFullPathName().toRawUTF8());
        return 1;
    }

    juce::PNGImageFormat png;
    if (png.writeImageToStream(snapshot, fos)) {
        std::printf("Snapshot successfully written to: %s\n", out.getFullPathName().toRawUTF8());
        return 0;
    }

    std::fprintf(stderr, "Failed to encode PNG\n");
    return 1;
}
