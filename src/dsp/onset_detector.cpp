#include "onset_detector.h"

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

OnsetDetector::OnsetDetector() : OnsetDetector(Params{}) {}

OnsetDetector::OnsetDetector(const Params& p)
    : params_(p),
      fft_(log2int(p.fftSize)),
      window_(static_cast<std::size_t>(p.fftSize), 0.0f),
      ring_(static_cast<std::size_t>(p.fftSize), 0.0f),
      complexBuf_(static_cast<std::size_t>(p.fftSize)),
      prevMag_(static_cast<std::size_t>(p.fftSize / 2 + 1), 0.0f),
      fluxHistory_(static_cast<std::size_t>(p.historyLen), 0.0f),
      sortedScratch_(static_cast<std::size_t>(p.historyLen), 0.0f),
      absDevScratch_(static_cast<std::size_t>(p.historyLen), 0.0f)
{
    for (int i = 0; i < p.fftSize; ++i) {
        window_[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i)
                                             / static_cast<float>(p.fftSize - 1)));
    }
}

void OnsetDetector::prepare(double sr) noexcept {
    sampleRate_ = sr;
    holdSamplesTotal_ = static_cast<int>(params_.holdMs * 0.001f * static_cast<float>(sr));
    releaseCoef_ = std::exp(-1.0f / (params_.releaseMs * 0.001f * static_cast<float>(sr)));
    reset();
}

void OnsetDetector::reset() noexcept {
    std::fill(ring_.begin(), ring_.end(), 0.0f);
    std::fill(prevMag_.begin(), prevMag_.end(), 0.0f);
    std::fill(fluxHistory_.begin(), fluxHistory_.end(), 0.0f);
    historyPos_ = 0;
    historyFilled_ = false;
    ringPos_ = 0;
    samplesSinceHop_ = 0;
    holdSamplesRemaining_ = 0;
    envelope_ = 0.0f;
}

float OnsetDetector::processSample(float x) noexcept {
    ring_[ringPos_] = x;
    ringPos_ = (ringPos_ + 1) % params_.fftSize;

    if (holdSamplesRemaining_ > 0) {
        --holdSamplesRemaining_;
        envelope_ = 1.0f;
    } else {
        envelope_ *= releaseCoef_;
    }

    if (++samplesSinceHop_ >= params_.hopSize) {
        samplesSinceHop_ = 0;
        analyzeFrame();
    }

    return envelope_;
}

void OnsetDetector::analyzeFrame() noexcept {
    const int N       = params_.fftSize;
    const int numBins = N / 2 + 1;

    for (int k = 0; k < N; ++k) {
        complexBuf_[k] = { ring_[(ringPos_ + k) % N] * window_[k], 0.0f };
    }
    fft_.perform(complexBuf_.data(), complexBuf_.data(), false);

    // Normalized spectral flux: positive magnitude change divided by total magnitude.
    // Scale-invariant, resistant to steady-state spectral leakage.
    float rawFlux = 0.0f;
    float totalMag = 0.0f;
    for (int k = 0; k < numBins; ++k) {
        const float mag = std::abs(complexBuf_[k]);
        rawFlux  += std::max(0.0f, mag - prevMag_[k]);
        totalMag += mag;
        prevMag_[k] = mag;
    }
    const float flux = rawFlux / (totalMag + 1.0f);

    fluxHistory_[historyPos_] = flux;
    historyPos_ = (historyPos_ + 1) % params_.historyLen;
    if (historyPos_ == 0) historyFilled_ = true;

    if (!historyFilled_) return;

    std::copy(fluxHistory_.begin(), fluxHistory_.end(), sortedScratch_.begin());
    std::sort(sortedScratch_.begin(), sortedScratch_.end());
    const float median = sortedScratch_[params_.historyLen / 2];

    for (int i = 0; i < params_.historyLen; ++i) {
        absDevScratch_[i] = std::abs(fluxHistory_[i] - median);
    }
    std::sort(absDevScratch_.begin(), absDevScratch_.end());
    const float mad = absDevScratch_[params_.historyLen / 2];

    const float threshold = std::max(params_.minThreshold,
                                     median + params_.thresholdK * mad);

    if (flux > threshold) {
        holdSamplesRemaining_ = holdSamplesTotal_;
    }
}

}  // namespace shifter
