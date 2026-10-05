#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "onset_detector.h"
#include "phase_vocoder.h"

class ShifterAudioProcessor : public juce::AudioProcessor {
public:
    static constexpr int kFftSize       = 1024;
    static constexpr int kEnvelopeDelay = 896;  // fftSize - detector hopSize margin

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

    std::atomic<float>* mixParameter_        = nullptr;
    std::atomic<float>* shiftParameter_      = nullptr;
    std::atomic<float>* transientsParameter_ = nullptr;

    int lastAppliedShift_ = 1;  // sentinel: not valid value, forces update on first block

    using NoInterpDelay = juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None>;

    std::vector<std::unique_ptr<shifter::PhaseVocoder>> vocoders_;
    std::vector<NoInterpDelay> dryDelays_;

    shifter::OnsetDetector onsetDetector_;
    NoInterpDelay envelopeDelay_ { kEnvelopeDelay + 4 };

    juce::AudioBuffer<float> dryScratch_;
    std::vector<float>       envelopeScratch_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShifterAudioProcessor)
};
