#pragma once

#include <complex>
#include <vector>

#include "mini_fft.h"

namespace shifter {

// Streaming phase vocoder for real-time pitch shifting on a single channel.
// Uses analysis/synthesis STFT with a Hann window and 75% overlap (hop = fftSize/4).
// Shift is implemented by spectral bin remapping with phase propagation (SMB-style),
// so analysis and synthesis hops are identical — no resampling needed.
//
// Algorithmic latency = fftSize samples. Memory is allocated once in the constructor;
// process() is realtime-safe (no allocations, no locks, no syscalls).
class PhaseVocoder {
public:
    explicit PhaseVocoder(int fftSize);

    void reset() noexcept;

    // Pitch ratio: 1.0 = unchanged, 0.5 = octave down, 2.0 = octave up.
    // Clamped to [0.25, 4.0]. Safe to call from the audio thread.
    void setPitchRatio(float ratio) noexcept;

    // In-place safe: input and output may be the same pointer.
    void process(const float* input, float* output, int numSamples) noexcept;

    int getLatencySamples() const noexcept { return fftSize_; }

private:
    void processFrame() noexcept;

    const int fftSize_;
    const int hopSize_;
    const int numBins_;
    static constexpr float kTwoPi = 6.28318530717958647692f;

    MiniFFT fft_;

    std::vector<float> window_;
    float windowGainCorrection_ = 1.0f;

    // Input ring: holds the last fftSize samples.
    std::vector<float> inputRing_;
    int inputPos_ = 0;

    // Output ring: OLA accumulator, size fftSize.
    std::vector<float> outputRing_;
    int outputReadPos_ = 0;
    int outputWritePos_ = 0;

    // Samples pushed since the last frame was processed.
    int samplesSinceFrame_ = 0;

    // Scratch (allocated once).
    std::vector<std::complex<float>> complexBuffer_;
    std::vector<std::complex<float>> ifftScratch_;
    std::vector<float>               frameOutput_;

    // Spectral state across frames.
    std::vector<float> lastInputPhase_;
    std::vector<float> accumulatedOutputPhase_;
    std::vector<float> outputMagnitude_;
    std::vector<float> outputTrueFreq_;

    // Phase-locking (Laroche-Dolson 1999) scratch.
    std::vector<float> inputPhase_;              // phase of each input bin this frame
    std::vector<float> outputPhaseFromInput_;    // input phase of the bin that landed here
    std::vector<int>   nearestPeak_;             // index of nearest output peak for each bin

    float pitchRatio_ = 1.0f;
};

}  // namespace shifter
