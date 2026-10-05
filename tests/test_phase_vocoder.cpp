#include <algorithm>
#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

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

// Rough dominant-frequency estimator via zero-crossing rate on a stable segment.
float estimateFrequencyFromZeroCrossings(const float* samples, int numSamples) {
    int crossings = 0;
    for (int i = 1; i < numSamples; ++i) {
        if ((samples[i - 1] <= 0.0f && samples[i] > 0.0f)
         || (samples[i - 1] >= 0.0f && samples[i] < 0.0f))
            ++crossings;
    }
    // Each full cycle has 2 zero crossings.
    return static_cast<float>(crossings) * 0.5f
         * static_cast<float>(kSampleRate) / static_cast<float>(numSamples);
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
    const float detected = estimateFrequencyFromZeroCrossings(output.data() + tailStart, tailLen);

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
    const float detected = estimateFrequencyFromZeroCrossings(output.data() + tailStart, tailLen);

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
