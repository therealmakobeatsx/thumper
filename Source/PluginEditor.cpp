#include "PluginEditor.h"

#include <BinaryData.h>

#include <cmath>

namespace
{
constexpr int editorWidth = 1152;
constexpr int editorHeight = 768;
constexpr float meterMaximumDb = 20.0f;

juce::Colour orange()
{
    return juce::Colour::fromRGB(255, 133, 45);
}
}

ThumperAudioProcessorEditor::ThumperAudioProcessorEditor(ThumperAudioProcessor& owner)
    : AudioProcessorEditor(&owner),
      processor(owner),
      background(juce::ImageCache::getFromMemory(BinaryData::background_png,
                                                  BinaryData::background_pngSize)),
      thumpSlider(owner)
{
    setSize(editorWidth, editorHeight);
    setResizable(false, false);

    thumpSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    thumpSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    thumpSlider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                                    juce::MathConstants<float>::pi * 2.75f,
                                    true);
    thumpSlider.setRange(0.0, 1.0, 0.001);
    thumpSlider.setDoubleClickReturnValue(true, 0.65);
    thumpSlider.setName("THUMP");
    addAndMakeVisible(thumpSlider);

    percentageLabel.setJustificationType(juce::Justification::centred);
    percentageLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    percentageLabel.setFont(juce::Font(juce::FontOptions(28.0f, juce::Font::bold)));
    percentageLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(percentageLabel);

    stateLabel.setJustificationType(juce::Justification::centred);
    stateLabel.setFont(juce::Font(juce::FontOptions(18.0f, juce::Font::bold)));
    stateLabel.setInterceptsMouseClicks(false, false);
    stateLabel.setText(juce::String::fromUTF8("\xe2\x97\x8f AUTO DETECT"), juce::dontSendNotification);
    stateLabel.setColour(juce::Label::textColourId, orange());
    addAndMakeVisible(stateLabel);

    thumpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, "thump", thumpSlider);

    startTimerHz(30);
    timerCallback();
}

void ThumperAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colours::black);
    if (background.isValid())
        graphics.drawImageWithin(background, 0, 0, getWidth(), getHeight(),
                                 juce::RectanglePlacement::centred);
}

void ThumperAudioProcessorEditor::resized()
{
    thumpSlider.setBounds(250, 300, 280, 280);
    percentageLabel.setBounds(250, 575, 280, 45);
    stateLabel.setBounds(300, 688, 552, 42);
}

void ThumperAudioProcessorEditor::timerCallback()
{
    const auto amount = static_cast<int>(std::lround(thumpSlider.getValue() * 100.0));
    percentageLabel.setText(juce::String(amount) + "%", juce::dontSendNotification);

    const auto isConnected = processor.isSidechainConnected();
    if (isConnected != displayedSidechainState)
    {
        displayedSidechainState = isConnected;
        stateLabel.setText(isConnected ? juce::String::fromUTF8("\xe2\x97\x8f SIDECHAIN CONNECTED")
                                       : juce::String::fromUTF8("\xe2\x97\x8f AUTO DETECT"),
                           juce::dontSendNotification);
        stateLabel.setColour(juce::Label::textColourId, isConnected ? juce::Colour::fromRGB(95, 220, 130)
                                                                    : orange());
    }

    thumpSlider.repaint();
}

void ThumperAudioProcessorEditor::ThumpSlider::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto centre = bounds.getCentre();
    const auto outerRadius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.47f;
    const auto startAngle = juce::MathConstants<float>::pi * 1.25f;
    const auto endAngle = juce::MathConstants<float>::pi * 2.75f;

    juce::Path meterTrack;
    meterTrack.addCentredArc(centre.x, centre.y, outerRadius, outerRadius, 0.0f,
                             startAngle, endAngle, true);
    graphics.setColour(juce::Colours::black.withAlpha(0.68f));
    graphics.strokePath(meterTrack, juce::PathStrokeType(8.0f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));

    const auto reduction = juce::jlimit(0.0f, meterMaximumDb, processor.getGainReductionDb());
    if (reduction > 0.01f)
    {
        juce::Path meterArc;
        meterArc.addCentredArc(centre.x, centre.y, outerRadius, outerRadius, 0.0f,
                               startAngle,
                               startAngle + (endAngle - startAngle) * reduction / meterMaximumDb,
                               true);
        graphics.setColour(orange());
        graphics.strokePath(meterArc, juce::PathStrokeType(8.0f,
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
    }

    const auto faceRadius = outerRadius * 0.76f;
    graphics.setColour(juce::Colour::fromRGB(23, 24, 25).withAlpha(0.96f));
    graphics.fillEllipse(centre.x - faceRadius, centre.y - faceRadius,
                         faceRadius * 2.0f, faceRadius * 2.0f);
    graphics.setColour(juce::Colours::white.withAlpha(0.24f));
    graphics.drawEllipse(centre.x - faceRadius, centre.y - faceRadius,
                         faceRadius * 2.0f, faceRadius * 2.0f, 2.0f);

    const auto proportion = static_cast<float>((getValue() - getMinimum()) / (getMaximum() - getMinimum()));
    const auto pointerAngle = startAngle + proportion * (endAngle - startAngle);
    const auto pointerStart = faceRadius * 0.18f;
    const auto pointerEnd = faceRadius * 0.78f;
    juce::Path pointer;
    pointer.startNewSubPath(centre.x + std::cos(pointerAngle) * pointerStart,
                            centre.y + std::sin(pointerAngle) * pointerStart);
    pointer.lineTo(centre.x + std::cos(pointerAngle) * pointerEnd,
                   centre.y + std::sin(pointerAngle) * pointerEnd);
    graphics.setColour(orange());
    graphics.strokePath(pointer, juce::PathStrokeType(5.0f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));

    graphics.setColour(juce::Colours::white.withAlpha(0.82f));
    graphics.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
    graphics.drawFittedText("THUMP", getLocalBounds().withSizeKeepingCentre(130, 25)
                                                    .withY(getHeight() / 2 + 38),
                            juce::Justification::centred, 1);
}
