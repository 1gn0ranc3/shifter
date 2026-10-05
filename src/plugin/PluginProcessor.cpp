#include "PluginProcessor.h"

#include <cmath>

#include "PluginEditor.h"

ShifterAudioProcessor::ShifterAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Shifter", createParameterLayout())
{
    mixParameter_        = parameters.getRawParameterValue("mix");
    shiftParameter_      = parameters.getRawParameterValue("shift");
    transientsParameter_ = parameters.getRawParameterValue("transients");
}

ShifterAudioProcessor::~ShifterAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
ShifterAudioProcessor::createParameterLayout() {
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{"shift", 1}, "Shift (semitones)",
        -12, 0, -2));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"transients", 1}, "Transient Preserve",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    return { params.begin(), params.end() };
}

void ShifterAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    const int numChannels = juce::jmax(getTotalNumInputChannels(),
                                       getTotalNumOutputChannels());

    vocoders_.clear();
    dryDelays_.clear();

    for (int ch = 0; ch < numChannels; ++ch) {
        auto v = std::make_unique<shifter::PhaseVocoder>(kFftSize);
        vocoders_.push_back(std::move(v));

        NoInterpDelay dl(kFftSize + 4);
        dl.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1 });
        dl.setDelay(static_cast<float>(kFftSize));
        dryDelays_.push_back(std::move(dl));
    }

    onsetDetector_.prepare(sampleRate);

    envelopeDelay_.prepare({ sampleRate, static_cast<juce::uint32>(samplesPerBlock), 1 });
    envelopeDelay_.setDelay(static_cast<float>(kEnvelopeDelay));
    envelopeDelay_.reset();

    dryScratch_.setSize(numChannels, samplesPerBlock, false, false, true);
    envelopeScratch_.assign(static_cast<std::size_t>(samplesPerBlock), 0.0f);

    lastAppliedShift_ = 1;  // force first-block ratio update

    setLatencySamples(kFftSize);
}

void ShifterAudioProcessor::releaseResources() {
    vocoders_.clear();
    dryDelays_.clear();
    dryScratch_.setSize(0, 0);
    envelopeScratch_.clear();
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
    const float mix          = mixParameter_->load(std::memory_order_relaxed);
    const float transientAmt = transientsParameter_->load(std::memory_order_relaxed);
    const int   shift        = static_cast<int>(shiftParameter_->load(std::memory_order_relaxed));

    if (shift != lastAppliedShift_) {
        const float ratio = std::pow(2.0f, static_cast<float>(shift) / 12.0f);
        for (auto& v : vocoders_) v->setPitchRatio(ratio);
        lastAppliedShift_ = shift;
    }

    if (static_cast<int>(envelopeScratch_.size()) < numSamples) {
        envelopeScratch_.assign(static_cast<std::size_t>(numSamples), 0.0f);
    }

    for (int ch = 0; ch < numChannels; ++ch) {
        dryScratch_.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // Transient envelope: detect on channel 0 of dry input, delay to align with wet output.
    {
        const auto* ref = dryScratch_.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i) {
            const float envNow = onsetDetector_.processSample(ref[i]);
            envelopeDelay_.pushSample(0, envNow);
            envelopeScratch_[static_cast<std::size_t>(i)] = envelopeDelay_.popSample(0);
        }
    }

    for (int ch = 0; ch < numChannels && ch < static_cast<int>(vocoders_.size()); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        vocoders_[ch]->process(data, data, numSamples);
    }

    for (int ch = 0; ch < numChannels && ch < static_cast<int>(dryDelays_.size()); ++ch) {
        auto& delay = dryDelays_[ch];
        const auto* dryIn = dryScratch_.getReadPointer(ch);
        auto* out = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            delay.pushSample(0, dryIn[i]);
            const float dryDelayed = delay.popSample(0);

            const float env = envelopeScratch_[static_cast<std::size_t>(i)] * transientAmt;
            const float effMix = mix * (1.0f - env);

            out[i] = dryDelayed * (1.0f - effMix) + out[i] * effMix;
        }
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
