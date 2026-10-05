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

float peakAbs(const float* data, int n) {
    float peak = 0.0f;
    for (int i = 0; i < n; ++i) peak = std::max(peak, std::abs(data[i]));
    return peak;
}

float rms(const float* data, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += static_cast<double>(data[i]) * data[i];
    return static_cast<float>(std::sqrt(sum / std::max(1, n)));
}

}  // namespace

TEST_CASE("PhaseVocoder: silence in -> silence out", "[dsp][vocoder]") {
    shifter::PhaseVocoder pv(kFftSize);
    pv.setPitchRatio(1.0f);

    const int N = 4 * kFftSize;
    std::vector<float> in(static_cast<std::size_t>(N), 0.0f);
    std::vector<float> out(static_cast<std::size_t>(N), 999.0f);  // sentinel

    pv.process(in.data(), out.data(), N);

    for (float v : out) {
        REQUIRE(std::abs(v) < 1e-6f);
    }
}

TEST_CASE("PhaseVocoder: non-silent input produces bounded, non-silent output",
          "[dsp][vocoder]") {
    shifter::PhaseVocoder pv(kFftSize);
    pv.setPitchRatio(1.0f);

    const int N = 8 * kFftSize;
    auto in = makeSinusoid(440.0f, N);
    std::vector<float> out(static_cast<std::size_t>(N), 0.0f);

    pv.process(in.data(), out.data(), N);

    // After warmup, output must be non-trivial but not blown up.
    const int tailStart = 3 * kFftSize;
    const int tailLen   = N - tailStart;
    const float peak    = peakAbs(out.data() + tailStart, tailLen);
    const float energy  = rms(out.data() + tailStart, tailLen);

    INFO("tail peak = " << peak << ", rms = " << energy);
    REQUIRE(peak    > 0.05f);
    REQUIRE(peak    < 20.0f);
    REQUIRE(energy  > 0.01f);
    REQUIRE(std::isfinite(peak));
    REQUIRE(std::isfinite(energy));
}

TEST_CASE("PhaseVocoder: different pitch ratios produce different outputs",
          "[dsp][vocoder]") {
    const int N = 8 * kFftSize;
    auto in = makeSinusoid(440.0f, N);

    std::vector<float> outIdentity(static_cast<std::size_t>(N), 0.0f);
    std::vector<float> outShifted (static_cast<std::size_t>(N), 0.0f);

    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(1.0f);
        pv.process(in.data(), outIdentity.data(), N);
    }
    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(std::pow(2.0f, -2.0f / 12.0f));
        pv.process(in.data(), outShifted.data(), N);
    }

    const int tailStart = 3 * kFftSize;
    double sumAbsDiff = 0.0;
    for (int i = tailStart; i < N; ++i) {
        sumAbsDiff += std::abs(outIdentity[i] - outShifted[i]);
    }

    INFO("sum of absolute diff over tail = " << sumAbsDiff);
    REQUIRE(sumAbsDiff > 10.0);  // ratio change must perturb output non-trivially
}

TEST_CASE("PhaseVocoder: chunked processing matches single call", "[dsp][vocoder]") {
    const float ratio = std::pow(2.0f, -2.0f / 12.0f);
    const int N = 4 * kFftSize;
    auto in = makeSinusoid(440.0f, N);

    std::vector<float> outWhole(static_cast<std::size_t>(N), 0.0f);
    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(ratio);
        pv.process(in.data(), outWhole.data(), N);
    }

    std::vector<float> outChunks(static_cast<std::size_t>(N), 0.0f);
    {
        shifter::PhaseVocoder pv(kFftSize);
        pv.setPitchRatio(ratio);
        const int chunk = 128;
        for (int i = 0; i < N; i += chunk) {
            const int n = std::min(chunk, N - i);
            pv.process(in.data() + i, outChunks.data() + i, n);
        }
    }

    for (int i = 0; i < N; ++i) {
        REQUIRE_THAT(outChunks[static_cast<std::size_t>(i)],
                     Catch::Matchers::WithinAbs(outWhole[static_cast<std::size_t>(i)], 1e-5f));
    }
}
