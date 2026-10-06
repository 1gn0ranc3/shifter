#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "phase_vocoder.h"

class ShifterAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int kFftSize = 512;  // ~10.7 ms at 48 kHz

    ShifterAudioProcessor();
    ~ShifterAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Shifter"; }
    bool acceptsMidi()   const override { return false; }
    bool producesMidi()  const override { return false; }
    bool isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override                              { return 1; }
    int getCurrentProgram() override                           { return 0; }
    void setCurrentProgram(int) override                       {}
    const juce::String getProgramName(int) override            { return {}; }
    void changeProgramName(int, const juce::String&) override  {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    std::atomic<float>* shiftParameter_ = nullptr;

    int lastAppliedShift_ = 1;  // sentinel outside valid range to force first update

    std::vector<std::unique_ptr<shifter::PhaseVocoder>> vocoders_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShifterAudioProcessor)
};
