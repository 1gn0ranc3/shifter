#pragma once

#include <complex>
#include <vector>

#include <juce_dsp/juce_dsp.h>

namespace shifter {

// Spectral-flux onset detector with adaptive median+MAD threshold.
// Produces a smooth transient envelope in [0, 1] per input sample:
//   - jumps to 1.0 on detected onset,
//   - held for holdMs,
//   - exponentially decays with releaseMs time constant.
// Realtime-safe: all allocations happen in the constructor.
class OnsetDetector {
public:
    struct Params {
        int   fftSize    = 256;      // ~5.3 ms window at 48 kHz
        int   hopSize    = 128;      // detection every ~2.7 ms
        int   historyLen = 80;       // flux history (~215 ms) for threshold stats
        float thresholdK = 3.0f;     // threshold = median + K * MAD
        float minThreshold = 0.15f;  // absolute floor on normalized flux [0, 1]
        float holdMs     = 3.0f;     // envelope held at 1.0 after onset
        float releaseMs  = 10.0f;    // exponential decay time constant
    };

    OnsetDetector();
    explicit OnsetDetector(const Params& p);

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    float processSample(float x) noexcept;

private:
    void analyzeFrame() noexcept;

    Params params_;
    double sampleRate_ = 48000.0;
    static constexpr float kTwoPi = 6.28318530717958647692f;

    juce::dsp::FFT fft_;
    std::vector<float> window_;
    std::vector<float> ring_;
    int ringPos_ = 0;
    int samplesSinceHop_ = 0;

    std::vector<std::complex<float>> complexBuf_;
    std::vector<float> prevMag_;

    std::vector<float> fluxHistory_;
    std::vector<float> sortedScratch_;
    std::vector<float> absDevScratch_;
    int historyPos_ = 0;
    bool historyFilled_ = false;

    int holdSamplesRemaining_ = 0;
    int holdSamplesTotal_     = 240;
    float releaseCoef_        = 0.999f;
    float envelope_           = 0.0f;
};

}  // namespace shifter
