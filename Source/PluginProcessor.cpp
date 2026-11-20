#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>
#include <iterator>

namespace
{
constexpr auto thumpParameterId = "thump";
constexpr float sidechainActiveThreshold = 0.0001f;
constexpr float detectorRearmRatio = 0.55f;
constexpr int detectorRefractoryMs = 70;

float getReleaseSeconds(float amount)
{
    constexpr float positions[] { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    constexpr float times[] { 0.05f, 0.12f, 0.25f, 0.38f, 0.5f };

    for (size_t i = 1; i < std::size(positions); ++i)
    {
        if (amount <= positions[i])
        {
            const auto proportion = (amount - positions[i - 1]) / (positions[i] - positions[i - 1]);
            return juce::jmap(proportion, times[i - 1], times[i]);
        }
    }

    return times[std::size(times) - 1];
}
}

ThumperAudioProcessor::ThumperAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "THUMPER_STATE", createParameterLayout())
{
    thumpParameter = parameters.getRawParameterValue(thumpParameterId);
}

juce::AudioProcessorValueTreeState::ParameterLayout ThumperAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { thumpParameterId, 1 },
        "THUMP",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.65f,
        juce::AudioParameterFloatAttributes().withLabel("%")));
    return layout;
}

const juce::String ThumperAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool ThumperAudioProcessor::acceptsMidi() const
{
    return false;
}

bool ThumperAudioProcessor::producesMidi() const
{
    return false;
}

bool ThumperAudioProcessor::isMidiEffect() const
{
    return false;
}

double ThumperAudioProcessor::getTailLengthSeconds() const
{
    return 0.5;
}

int ThumperAudioProcessor::getNumPrograms()
{
    return 1;
}

int ThumperAudioProcessor::getCurrentProgram()
{
    return 0;
}

void ThumperAudioProcessor::setCurrentProgram(int)
{
}

const juce::String ThumperAudioProcessor::getProgramName(int)
{
    return {};
}

void ThumperAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void ThumperAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    lowPassState = 0.0f;
    detectorEnvelope = 0.0f;
    detectorBaseline = 0.0f;
    pumpEnvelope = 0.0f;
    samplesUntilDetectorRearm = 0;
    sidechainHoldSamples = 0;
    detectorArmed = true;
    wasSidechainActive = false;
    gainReductionDb.store(0.0f, std::memory_order_relaxed);
    sidechainConnected.store(false, std::memory_order_relaxed);
}

void ThumperAudioProcessor::releaseResources()
{
}

bool ThumperAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto sidechain = layouts.getChannelSet(true, 1);
    return sidechain.isDisabled() || sidechain == juce::AudioChannelSet::stereo();
}

void ThumperAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainInput = getBusBuffer(buffer, true, 0);
    auto sidechainInput = getBusBuffer(buffer, true, 1);
    auto mainOutput = getBusBuffer(buffer, false, 0);

    const auto sampleCount = mainOutput.getNumSamples();
    const auto sidechainChannels = sidechainInput.getNumChannels();
    const auto* sidechainBus = getBus(true, 1);
    const auto hasSidechainBuffer = sidechainBus != nullptr && sidechainBus->isEnabled()
                                    && sidechainChannels > 0;

    float sidechainRms = 0.0f;
    if (hasSidechainBuffer && sampleCount > 0)
    {
        double squaredSum = 0.0;
        for (int channel = 0; channel < sidechainChannels; ++channel)
            for (int sample = 0; sample < sampleCount; ++sample)
            {
                const auto value = sidechainInput.getSample(channel, sample);
                squaredSum += static_cast<double>(value) * value;
            }

        const auto count = static_cast<double>(sidechainChannels) * sampleCount;
        sidechainRms = static_cast<float>(std::sqrt(squaredSum / count));
    }

    if (! hasSidechainBuffer)
        sidechainHoldSamples = 0;
    else if (sidechainRms > sidechainActiveThreshold)
        sidechainHoldSamples = static_cast<int>(0.25 * currentSampleRate);
    else
        sidechainHoldSamples = juce::jmax(0, sidechainHoldSamples - sampleCount);

    const auto externalSidechainActive = sidechainHoldSamples > 0;
    sidechainConnected.store(externalSidechainActive, std::memory_order_relaxed);

    if (externalSidechainActive != wasSidechainActive)
    {
        lowPassState = 0.0f;
        detectorEnvelope = 0.0f;
        detectorBaseline = 0.0f;
        samplesUntilDetectorRearm = 0;
        detectorArmed = true;
        wasSidechainActive = externalSidechainActive;
    }

    const auto amount = thumpParameter->load(std::memory_order_relaxed);
    const auto depthDb = 20.0f * amount;
    const auto releaseSeconds = getReleaseSeconds(amount);
    const auto releaseCoefficient = std::exp(-1.0f / static_cast<float>(releaseSeconds * currentSampleRate));
    const auto lowPassCoefficient = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 140.0f
                                                   / static_cast<float>(currentSampleRate));
    const auto detectorAttackCoefficient = 1.0f - std::exp(-1.0f / (0.001f * static_cast<float>(currentSampleRate)));
    const auto detectorReleaseCoefficient = std::exp(-1.0f / (0.04f * static_cast<float>(currentSampleRate)));
    const auto baselineCoefficient = std::exp(-1.0f / (0.5f * static_cast<float>(currentSampleRate)));
    const auto detectorThreshold = juce::jmap(amount, 0.035f, 0.008f);
    const auto refractorySamples = static_cast<int>(detectorRefractoryMs * currentSampleRate / 1000.0);
    for (int sample = 0; sample < sampleCount; ++sample)
    {
        float detectorInput = 0.0f;
        const auto detectorChannels = externalSidechainActive ? sidechainChannels : mainInput.getNumChannels();
        const auto& detectorBuffer = externalSidechainActive ? sidechainInput : mainInput;

        for (int channel = 0; channel < detectorChannels; ++channel)
            detectorInput += detectorBuffer.getSample(channel, sample);

        if (detectorChannels > 0)
            detectorInput /= static_cast<float>(detectorChannels);

        lowPassState += lowPassCoefficient * (detectorInput - lowPassState);
        const auto detectorLevel = std::abs(lowPassState);
        const auto detectorCoefficient = detectorLevel > detectorEnvelope
                                             ? detectorAttackCoefficient
                                             : detectorReleaseCoefficient;
        detectorEnvelope = detectorCoefficient * detectorEnvelope
                           + (1.0f - detectorCoefficient) * detectorLevel;
        detectorBaseline = baselineCoefficient * detectorBaseline
                           + (1.0f - baselineCoefficient) * detectorEnvelope;

        if (samplesUntilDetectorRearm > 0)
            --samplesUntilDetectorRearm;

        const auto triggerThreshold = juce::jmax(detectorThreshold, detectorBaseline * 2.5f);
        if (! detectorArmed && samplesUntilDetectorRearm == 0
            && detectorEnvelope < triggerThreshold * detectorRearmRatio)
            detectorArmed = true;

        if (detectorArmed && samplesUntilDetectorRearm == 0
            && detectorEnvelope > triggerThreshold)
        {
            pumpEnvelope = 1.0f;
            detectorArmed = false;
            samplesUntilDetectorRearm = refractorySamples;
        }

        const auto gain = juce::Decibels::decibelsToGain(-depthDb * pumpEnvelope);
        for (int channel = 0; channel < mainOutput.getNumChannels(); ++channel)
        {
            const auto input = channel < mainInput.getNumChannels()
                                   ? mainInput.getSample(channel, sample)
                                   : 0.0f;
            mainOutput.setSample(channel, sample, input * gain);
        }

        pumpEnvelope *= releaseCoefficient;
        if (pumpEnvelope < 1.0e-6f)
            pumpEnvelope = 0.0f;
    }

    gainReductionDb.store(depthDb * pumpEnvelope, std::memory_order_relaxed);
}

bool ThumperAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* ThumperAudioProcessor::createEditor()
{
    return new ThumperAudioProcessorEditor(*this);
}

void ThumperAudioProcessor::getStateInformation(juce::MemoryBlock& destinationData)
{
    const auto state = parameters.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destinationData);
}

void ThumperAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        const auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid() && state.getType() == parameters.state.getType())
            parameters.replaceState(state);
    }
}

float ThumperAudioProcessor::getGainReductionDb() const noexcept
{
    return gainReductionDb.load(std::memory_order_relaxed);
}

bool ThumperAudioProcessor::isSidechainConnected() const noexcept
{
    return sidechainConnected.load(std::memory_order_relaxed);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ThumperAudioProcessor();
}
