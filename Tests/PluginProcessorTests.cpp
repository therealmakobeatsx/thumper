#include "PluginProcessor.h"

#include <BinaryData.h>

#include <cmath>
#include <iostream>

namespace
{
bool expect(bool condition, const char* message)
{
    if (! condition)
        std::cerr << "FAIL: " << message << '\n';

    return condition;
}
}

int main()
{
    const auto background = juce::ImageCache::getFromMemory(BinaryData::background_png,
                                                             BinaryData::background_pngSize);
    auto success = expect(background.isValid() && background.getWidth() == 1536
                              && background.getHeight() == 1024,
                          "the original background is embedded and decodes at its native dimensions");

    ThumperAudioProcessor processor;

    success &= expect(processor.getBusCount(true) == 2, "stereo main and sidechain input buses are declared");
    success &= expect(processor.getBus(true, 1)->getName() == "Sidechain",
                      "the auxiliary input is named Sidechain");

    auto* parameter = processor.parameters.getParameter("thump");
    success &= expect(parameter != nullptr, "THUMP is exposed as an automatable parameter");
    if (parameter == nullptr)
        return 1;

    parameter->setValueNotifyingHost(1.0f);
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> sidechainBlock(4, 512);
    sidechainBlock.clear();
    for (int sample = 0; sample < sidechainBlock.getNumSamples(); ++sample)
    {
        sidechainBlock.setSample(0, sample, 0.25f);
        sidechainBlock.setSample(1, sample, 0.25f);
        sidechainBlock.setSample(2, sample, 0.1f);
        sidechainBlock.setSample(3, sample, 0.1f);
    }

    juce::MidiBuffer midi;
    processor.processBlock(sidechainBlock, midi);

    success &= expect(processor.isSidechainConnected(), "active external sidechain is detected");
    success &= expect(processor.getGainReductionDb() > 15.0f, "external sidechain triggers gain reduction");
    success &= expect(std::abs(sidechainBlock.getSample(0, 511)) < 0.125f,
                      "the main signal is audibly ducked by the external sidechain");

    parameter->setValueNotifyingHost(0.37f);
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    parameter->setValueNotifyingHost(0.82f);
    processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    success &= expect(std::abs(processor.parameters.getRawParameterValue("thump")->load() - 0.37f) < 0.002f,
                      "THUMP automation state round-trips");

    success &= expect(processor.getBus(true, 1)->enable(false), "the sidechain bus can be disabled");
    parameter->setValueNotifyingHost(1.0f);
    processor.prepareToPlay(48000.0, 512);

    juce::AudioBuffer<float> autoDetectBlock(2, 512);
    for (int sample = 0; sample < autoDetectBlock.getNumSamples(); ++sample)
    {
        const auto value = 0.1f * std::sin(2.0f * juce::MathConstants<float>::pi * 60.0f
                                           * static_cast<float>(sample) / 48000.0f);
        autoDetectBlock.setSample(0, sample, value);
        autoDetectBlock.setSample(1, sample, value);
    }

    processor.processBlock(autoDetectBlock, midi);
    success &= expect(! processor.isSidechainConnected(), "disabled external sidechain selects auto detect");
    success &= expect(processor.getGainReductionDb() > 2.0f, "auto detect triggers on a low-frequency hit");

    return success ? 0 : 1;
}
