#pragma once

#include "PluginProcessor.h"

class ThumperAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit ThumperAudioProcessorEditor(ThumperAudioProcessor&);
    ~ThumperAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ThumpSlider final : public juce::Slider
    {
    public:
        explicit ThumpSlider(ThumperAudioProcessor& owner) : processor(owner) {}
        void paint(juce::Graphics&) override;

    private:
        ThumperAudioProcessor& processor;
    };

    void timerCallback() override;

    ThumperAudioProcessor& processor;
    juce::Image background;
    ThumpSlider thumpSlider;
    juce::Label percentageLabel;
    juce::Label stateLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thumpAttachment;
    bool displayedSidechainState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThumperAudioProcessorEditor)
};
