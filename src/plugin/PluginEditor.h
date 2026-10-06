#pragma once

#include <memory>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

class ShifterAudioProcessorEditor : public juce::AudioProcessorEditor {
public:
    explicit ShifterAudioProcessorEditor(ShifterAudioProcessor&);
    ~ShifterAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    ShifterAudioProcessor& processor_;

    juce::ComboBox shiftCombo_;
    juce::Label    shiftLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shiftAttachment_;

    juce::Slider transientsSlider_;
    juce::Label  transientsLabel_;
    juce::AudioProcessorValueTreeState::SliderAttachment transientsAttachment_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShifterAudioProcessorEditor)
};
