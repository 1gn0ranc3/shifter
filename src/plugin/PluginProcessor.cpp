#include "PluginProcessor.h"

#include <cmath>

#include "PluginEditor.h"

ShifterAudioProcessor::ShifterAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Shifter", createParameterLayout())
{
    shiftParameter_ = parameters.getRawParameterValue("shift");
}

ShifterAudioProcessor::~ShifterAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
ShifterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"shift", 1}, "Shift (semitones)",
        -12, 0, -2));
    return { params.begin(), params.end() };
}

void ShifterAudioProcessor::prepareToPlay(double /*sampleRate*/, int /*samplesPerBlock*/) {
    const int numChannels = juce::jmax(getTotalNumInputChannels(),
                                       getTotalNumOutputChannels());

    vocoders_.clear();
    for (int ch = 0; ch < numChannels; ++ch) {
        vocoders_.push_back(std::make_unique<shifter::PhaseVocoder>(kFftSize));
    }

    lastAppliedShift_ = 1;
    setLatencySamples(kFftSize);
}

void ShifterAudioProcessor::releaseResources() {
    vocoders_.clear();
}

bool ShifterAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void ShifterAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();
    const int shift = static_cast<int>(shiftParameter_->load(std::memory_order_relaxed));

    if (shift != lastAppliedShift_) {
        const float ratio = std::pow(2.0f, static_cast<float>(shift) / 12.0f);
        for (auto& v : vocoders_) v->setPitchRatio(ratio);
        lastAppliedShift_ = shift;
    }

    for (int ch = 0; ch < numChannels && ch < static_cast<int>(vocoders_.size()); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        vocoders_[ch]->process(data, data, numSamples);
    }
}

juce::AudioProcessorEditor* ShifterAudioProcessor::createEditor() {
    return new ShifterAudioProcessorEditor(*this);
}

void ShifterAudioProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto state = parameters.copyState(); state.isValid()) {
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
    }
}

void ShifterAudioProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
    return new ShifterAudioProcessor();
}
