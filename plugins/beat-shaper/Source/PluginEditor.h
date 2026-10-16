#pragma once

#include "EnvelopeEditor.h"
#include "PluginProcessor.h"

class BeatShaperEditor final : public juce::AudioProcessorEditor,
                               private juce::Timer
{
public:
    explicit BeatShaperEditor (BeatShaperProcessor&);
    ~BeatShaperEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void loadPreset (int index);

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    BeatShaperProcessor& processor;

    juce::LookAndFeel_V4 lookAndFeel;
    EnvelopeEditor envelopeEditor;

    juce::ComboBox presetBox, snapBox;
    juce::Label presetLabel, snapLabel, mixLabel, smoothLabel, hintLabel;
    juce::Slider mixSlider, smoothSlider;
    std::unique_ptr<SliderAttachment> mixAttachment, smoothAttachment;

    int lastEnvelopeVersion = -1;
    bool lastHostPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatShaperEditor)
};
