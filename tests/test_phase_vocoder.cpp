#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <juce_dsp/juce_dsp.h>

#include "phase_vocoder.h"

namespace {

constexpr int    kFftSize    = 1024;
constexpr int    kSampleRate = 48'000;
constexpr double kPi         = 3.141592653589793;

std::vector<float> makeSinusoid(float freqHz, int numSamples) {
    std::vector<float> out(static_cast<std::size_t>(numSamples));
    const double w = 2.0 * kPi * freqHz / static_cast<double>(kSampleRate);
    for (int i = 0; i < numSamples; ++i) {
        out[static_cast<std::size_t>(i)] = static_cast<float>(std::sin(w * i));
    }
    return out;
}

// Dominant-frequency estimator via FFT peak detection. Robust to HF noise and
// phase-vocoder ripple, unlike zero-crossing counting.
float estimateDominantFrequency(const float* samples, int numSamples) {
    int fftSize = 1;
    int fftOrder = 0;
    while (fftSize < numSamples) {
        fftSize *= 2;
        ++fftOrder;
    }

    std::vector<std::complex<float>> buf(static_cast<std::size_t>(fftSize),
                                         std::complex<float>{ 0.0f, 0.0f });
    for (int i = 0; i < numSamples; ++i) {
        buf[static_cast<std::size_t>(i)] = { samples[i], 0.0f };
    }

    juce::dsp::FFT fft(fftOrder);
    fft.perform(buf.data(), buf.data(), false);

    int peakBin = 1;
    float peakMag = std::abs(buf[1]);
    const int halfSize = fftSize / 2;
    for (int k = 2; k < halfSize; ++k) {
        const float mag = std::abs(buf[static_cast<std::size_t>(k)]);
        if (mag > peakMag) {
            peakMag = mag;
            peakBin = k;
        }
    }

    return static_cast<float>(peakBin)
         * static_cast<float>(kSampleRate)
         / static_cast<float>(fftSize);
}

}  // namespace

TEST_CASE("PhaseVocoder: silence in -> silence out", "[dsp][vocoder]") {
    shifter::PhaseVocoder pv(kFftSize);
    pv.setPitchRatio(1.0f);

    const int N = 4 * kFftSize;
    std::vector<float> in(N, 0.0f);
    std::vector<float> out(N, 999.0f);  // sentinel

    pv.process(in.data(), out.data(), N);

    for (float v : out) {
        REQUIRE(std::abs(v) < 1e-6f);
    }
}

TEST_CASE("PhaseVocoder: identity ratio passes signal through", "[dsp][vocoder]") {
    shifter::PhaseVocoder pv(kFftSize);
    pv.setPitchRatio(1.0f);

    const int N = 8 * kFftSize;  // ~170 ms at 48k; plenty past warmup
    auto input = makeSinusoid(440.0f, N);
    std::vector<float> output(static_cast<std::size_t>(N), 0.0f);

    pv.process(input.data(), output.data(), N);

    // Skip warmup region, measure dominant frequency on the tail.
    const int tailStart = 3 * kFftSize;
    const int tailLen   = N - tailStart;
    const float detected = estimateDominantFrequency(output.data() + tailStart, tailLen);

    REQUIRE_THAT(detected, Catch::Matchers::WithinRel(440.0f, 0.05f));
}

TEST_CASE("PhaseVocoder: shift down produces lower frequency", "[dsp][vocoder]") {
    shifter::PhaseVocoder pv(kFftSize);
    // -2 semitones ≈ 0.8909
    const float ratio = std::pow(2.0f, -2.0f / 12.0f);
    pv.setPitchRatio(ratio);

    const int N = 8 * kFftSize;
    auto input = makeSinusoid(440.0f, N);
    std::vector<float> output(static_cast<std::size_t>(N), 0.0f);

    pv.process(input.data(), output.data(), N);

    const int tailStart = 3 * kFftSize;
    const int tailLen   = N - tailStart;
    const float detected = estimateDominantFrequency(output.data() + tailStart, tailLen);

    const float expected = 440.0f * ratio;  // ~392 Hz
    REQUIRE_THAT(detected, Catch::Matchers::WithinRel(expected, 0.05f));
}

TEST_CASE("PhaseVocoder: chunked processing matches single call", "[dsp][vocoder]") {
    const float ratio = std::pow(2.0f, -2.0f / 12.0f);
    const int N = 4 * kFftSize;
    auto input = makeSinusoid(440.0f, N);

    std::vector<float> outWhole(static_cast<std::size_t>(N), 0.0f);
    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(ratio);
        pv.process(input.data(), outWhole.data(), N);
    }

    std::vector<float> outChunks(static_cast<std::size_t>(N), 0.0f);
    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(ratio);
        const int chunk = 128;
        for (int i = 0; i < N; i += chunk) {
            const int n = std::min(chunk, N - i);
            pv.process(input.data() + i, outChunks.data() + i, n);
        }
    }

    for (int i = 0; i < N; ++i) {
        REQUIRE_THAT(outChunks[static_cast<std::size_t>(i)],
                     Catch::Matchers::WithinAbs(outWhole[static_cast<std::size_t>(i)], 1e-5f));
    }
}
