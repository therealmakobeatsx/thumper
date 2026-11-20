#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class ThumperAudioProcessor final : public juce::AudioProcessor
{
public:
    ThumperAudioProcessor();
    ~ThumperAudioProcessor() override = default;

    using juce::AudioProcessor::processBlock;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destinationData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

    float getGainReductionDb() const noexcept;
    bool isSidechainConnected() const noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float>* thumpParameter = nullptr;
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<bool> sidechainConnected { false };

    double currentSampleRate = 44100.0;
    float lowPassState = 0.0f;
    float detectorEnvelope = 0.0f;
    float detectorBaseline = 0.0f;
    float pumpEnvelope = 0.0f;
    int samplesUntilDetectorRearm = 0;
    int sidechainHoldSamples = 0;
    bool detectorArmed = true;
    bool wasSidechainActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThumperAudioProcessor)
};
