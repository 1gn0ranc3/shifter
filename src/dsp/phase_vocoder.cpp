#include "phase_vocoder.h"

#include <algorithm>
#include <cmath>

namespace shifter {

namespace {
int log2int(int n) noexcept {
    int log = 0;
    while ((1 << log) < n) ++log;
    return log;
}
}  // namespace

PhaseVocoder::PhaseVocoder(int fftSize)
    : fftSize_(fftSize),
      hopSize_(fftSize / 4),
      numBins_(fftSize / 2 + 1),
      fft_(log2int(fftSize)),
      window_(static_cast<std::size_t>(fftSize), 0.0f),
      inputRing_(static_cast<std::size_t>(fftSize), 0.0f),
      outputRing_(static_cast<std::size_t>(fftSize), 0.0f),
      complexBuffer_(static_cast<std::size_t>(fftSize)),
      ifftScratch_(static_cast<std::size_t>(fftSize)),
      frameOutput_(static_cast<std::size_t>(fftSize), 0.0f),
      lastInputPhase_(static_cast<std::size_t>(numBins_), 0.0f),
      accumulatedOutputPhase_(static_cast<std::size_t>(numBins_), 0.0f),
      outputMagnitude_(static_cast<std::size_t>(numBins_), 0.0f),
      outputTrueFreq_(static_cast<std::size_t>(numBins_), 0.0f),
      inputPhase_(static_cast<std::size_t>(numBins_), 0.0f),
      outputPhaseFromInput_(static_cast<std::size_t>(numBins_), 0.0f),
      nearestPeak_(static_cast<std::size_t>(numBins_), 0)
{
    // Hann window.
    for (int i = 0; i < fftSize_; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i)
                                             / static_cast<float>(fftSize_ - 1)));
    }

    // OLA normalization for Hann window with 75% overlap. We apply the window
    // twice (analysis + synthesis), so the sum of w^2 over 4 overlapping frames
    // at any output sample is ~1.5 for large N. Dividing by 1.5 restores unity gain.
    windowGainCorrection_ = 2.0f / 3.0f;
}

void PhaseVocoder::reset() noexcept {
    std::fill(inputRing_.begin(),  inputRing_.end(),  0.0f);
    std::fill(outputRing_.begin(), outputRing_.end(), 0.0f);
    std::fill(lastInputPhase_.begin(),         lastInputPhase_.end(),         0.0f);
    std::fill(accumulatedOutputPhase_.begin(), accumulatedOutputPhase_.end(), 0.0f);
    inputPos_ = 0;
    outputReadPos_ = 0;
    outputWritePos_ = 0;
    samplesSinceFrame_ = 0;
}

void PhaseVocoder::setPitchRatio(float ratio) noexcept {
    pitchRatio_ = std::clamp(ratio, 0.25f, 4.0f);
}

void PhaseVocoder::process(const float* input, float* output, int numSamples) noexcept {
    for (int i = 0; i < numSamples; ++i) {
        inputRing_[inputPos_] = input[i];
        inputPos_ = (inputPos_ + 1) % fftSize_;

        output[i] = outputRing_[outputReadPos_] * windowGainCorrection_;
        outputRing_[outputReadPos_] = 0.0f;
        outputReadPos_ = (outputReadPos_ + 1) % fftSize_;

        if (++samplesSinceFrame_ >= hopSize_) {
            processFrame();
            samplesSinceFrame_ = 0;
        }
    }
}

void PhaseVocoder::processFrame() noexcept {
    // 1. Copy windowed input from the ring (oldest-first) into the FFT scratch.
    for (int k = 0; k < fftSize_; ++k) {
        const int idx = (inputPos_ + k) % fftSize_;
        complexBuffer_[k] = { inputRing_[idx] * window_[k], 0.0f };
    }

    // 2. Forward FFT.
    fft_.perform(complexBuffer_.data(), complexBuffer_.data(), false);

    // 3. Reset output accumulators for this frame.
    std::fill(outputMagnitude_.begin(), outputMagnitude_.end(), 0.0f);
    std::fill(outputTrueFreq_.begin(),  outputTrueFreq_.end(),  0.0f);

    const float expectedPhaseAdvancePerBin = kTwoPi
        * static_cast<float>(hopSize_) / static_cast<float>(fftSize_);
    const float ratio = pitchRatio_;

    // 4. Analysis: for each bin, estimate true frequency, then remap to shifted bin.
    for (int k = 0; k < numBins_; ++k) {
        const auto bin = complexBuffer_[k];
        const float mag   = std::abs(bin);
        const float phase = std::arg(bin);

        inputPhase_[k] = phase;

        float phaseDelta = phase - lastInputPhase_[k];
        lastInputPhase_[k] = phase;

        phaseDelta -= static_cast<float>(k) * expectedPhaseAdvancePerBin;
        phaseDelta = std::remainder(phaseDelta, kTwoPi);

        const float deviation = phaseDelta / expectedPhaseAdvancePerBin;
        const float trueBin   = static_cast<float>(k) + deviation;

        const int newK = static_cast<int>(std::round(static_cast<float>(k) * ratio));
        if (newK >= 0 && newK < numBins_) {
            outputMagnitude_[newK] += mag;
            outputTrueFreq_[newK]   = trueBin * ratio;
            // Which input bin ended up here (used by phase locking below).
            outputPhaseFromInput_[newK] = phase;
        }
    }

    // 5a. Phase-locking (Laroche-Dolson 1999). Find peaks in the output
    // magnitude spectrum, then collapse non-peak bins' phases to track the
    // nearest peak with their original relative offset. This keeps whole
    // "regions of influence" coherent across frames and is the single biggest
    // win against the metallic / underwater phase-vocoder sound on sustain.
    float maxMag = 0.0f;
    for (int k = 0; k < numBins_; ++k) {
        if (outputMagnitude_[k] > maxMag) maxMag = outputMagnitude_[k];
    }
    const float peakFloor = maxMag * 0.01f;

    int lastPeak = -1;
    for (int k = 0; k < numBins_; ++k) {
        const float m  = outputMagnitude_[k];
        const float ml = (k > 0)              ? outputMagnitude_[k - 1] : 0.0f;
        const float mr = (k < numBins_ - 1)   ? outputMagnitude_[k + 1] : 0.0f;
        if (m > peakFloor && m >= ml && m >= mr) lastPeak = k;
        nearestPeak_[k] = lastPeak;
    }
    int nextPeak = -1;
    for (int k = numBins_ - 1; k >= 0; --k) {
        const float m  = outputMagnitude_[k];
        const float ml = (k > 0)              ? outputMagnitude_[k - 1] : 0.0f;
        const float mr = (k < numBins_ - 1)   ? outputMagnitude_[k + 1] : 0.0f;
        if (m > peakFloor && m >= ml && m >= mr) nextPeak = k;
        if (nextPeak >= 0) {
            const int prev = nearestPeak_[k];
            if (prev < 0 || (nextPeak - k) < (k - prev)) {
                nearestPeak_[k] = nextPeak;
            }
        }
    }

    // 5b. Advance peak phases normally. Bins within a small radius of a peak
    // (the Hann main-lobe width in bin units) are locked to the peak so the
    // tonal content stays coherent. Bins outside the radius evolve
    // independently — collapsing them all onto peaks destroys the stochastic
    // phase variation that makes sustain sound natural and yields a robotic
    // vocoder-y tone, which is the trap of overly aggressive phase locking.
    constexpr int kLockRadius = 3;

    for (int k = 0; k < numBins_; ++k) {
        const int peak = nearestPeak_[k];
        const bool inRegion = (peak >= 0) && (std::abs(k - peak) <= kLockRadius);
        if (peak == k || !inRegion) {
            const float phaseAdvance = outputTrueFreq_[k] * expectedPhaseAdvancePerBin;
            accumulatedOutputPhase_[k] = std::remainder(
                accumulatedOutputPhase_[k] + phaseAdvance, kTwoPi);
        }
    }
    for (int k = 0; k < numBins_; ++k) {
        const int peak = nearestPeak_[k];
        const bool inRegion = (peak >= 0) && (std::abs(k - peak) <= kLockRadius);
        if (peak != k && inRegion) {
            const float offset = outputPhaseFromInput_[k] - outputPhaseFromInput_[peak];
            accumulatedOutputPhase_[k] = std::remainder(
                accumulatedOutputPhase_[peak] + offset, kTwoPi);
        }
    }

    for (int k = 0; k < numBins_; ++k) {
        const float mag = outputMagnitude_[k];
        complexBuffer_[k] = {
            mag * std::cos(accumulatedOutputPhase_[k]),
            mag * std::sin(accumulatedOutputPhase_[k])
        };
    }

    // Rebuild conjugate-symmetric upper half for a real-valued output.
    for (int k = numBins_; k < fftSize_; ++k) {
        complexBuffer_[k] = std::conj(complexBuffer_[fftSize_ - k]);
    }

    // 6. Inverse FFT into a separate buffer (avoid relying on in-place behaviour,
    // which has shown platform-specific brittleness in JUCE's FFT on MSVC).
    fft_.perform(complexBuffer_.data(), ifftScratch_.data(), true);

    // 7. Window and OLA into the output ring at outputWritePos_.
    for (int k = 0; k < fftSize_; ++k) {
        frameOutput_[k] = ifftScratch_[k].real() * window_[k];
    }

    for (int k = 0; k < fftSize_; ++k) {
        const int idx = (outputWritePos_ + k) % fftSize_;
        outputRing_[idx] += frameOutput_[k];
    }
    outputWritePos_ = (outputWritePos_ + hopSize_) % fftSize_;
}

}  // namespace shifter
