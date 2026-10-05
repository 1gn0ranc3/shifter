#include <cmath>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "onset_detector.h"

namespace {
constexpr double kSampleRate = 48'000.0;
constexpr double kPi         = 3.141592653589793;
}

TEST_CASE("OnsetDetector: silence produces zero envelope", "[dsp][onset]") {
    shifter::OnsetDetector od;
    od.prepare(kSampleRate);

    float maxEnv = 0.0f;
    for (int i = 0; i < 20'000; ++i) {
        maxEnv = std::max(maxEnv, od.processSample(0.0f));
    }
    REQUIRE(maxEnv < 1e-4f);
}

TEST_CASE("OnsetDetector: sudden burst triggers envelope", "[dsp][onset]") {
    shifter::OnsetDetector od;
    od.prepare(kSampleRate);

    // Warmup with silence so the adaptive threshold stabilises.
    for (int i = 0; i < 10'000; ++i) {
        (void)od.processSample(0.0f);
    }

    // Introduce an abrupt sinusoidal burst.
    float envDuringBurst = 0.0f;
    const double w = 2.0 * kPi * 1'000.0 / kSampleRate;
    for (int i = 0; i < 2'000; ++i) {
        const float x = static_cast<float>(std::sin(w * i));
        const float env = od.processSample(x);
        envDuringBurst = std::max(envDuringBurst, env);
    }

    REQUIRE(envDuringBurst > 0.9f);
}

TEST_CASE("OnsetDetector: continuous tone does not retrigger after startup", "[dsp][onset]") {
    shifter::OnsetDetector od;
    od.prepare(kSampleRate);

    const double w = 2.0 * kPi * 440.0 / kSampleRate;

    // Startup includes the attack of the tone itself. Push through warmup.
    for (int i = 0; i < 15'000; ++i) {
        (void)od.processSample(static_cast<float>(std::sin(w * i)));
    }

    // After warmup, continuous tone should not keep triggering: envelope should settle low.
    float settledEnvMax = 0.0f;
    for (int i = 15'000; i < 30'000; ++i) {
        const float env = od.processSample(static_cast<float>(std::sin(w * i)));
        settledEnvMax = std::max(settledEnvMax, env);
    }
    REQUIRE(settledEnvMax < 0.3f);
}

TEST_CASE("OnsetDetector: envelope decays after onset", "[dsp][onset]") {
    shifter::OnsetDetector od;
    od.prepare(kSampleRate);

    // Warmup.
    for (int i = 0; i < 10'000; ++i) {
        (void)od.processSample(0.0f);
    }

    // Brief burst.
    const double w = 2.0 * kPi * 1'000.0 / kSampleRate;
    for (int i = 0; i < 300; ++i) {
        (void)od.processSample(static_cast<float>(std::sin(w * i)));
    }

    // Then silence; envelope should decay well below 0.5 within 100 ms.
    const int decayWindow = static_cast<int>(0.1 * kSampleRate);
    float envAfter = 0.0f;
    for (int i = 0; i < decayWindow; ++i) {
        envAfter = od.processSample(0.0f);
    }
    REQUIRE(envAfter < 0.1f);
}
