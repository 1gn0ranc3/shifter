#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <vector>

#include "ring_buffer.h"

TEST_CASE("RingBuffer: basic FIFO ordering", "[dsp][ring_buffer]") {
    shifter::RingBuffer<16> rb;

    REQUIRE(rb.write(1.0f));
    REQUIRE(rb.write(2.0f));
    REQUIRE(rb.write(3.0f));

    float out = 0.0f;
    REQUIRE(rb.read(out));  REQUIRE(out == 1.0f);
    REQUIRE(rb.read(out));  REQUIRE(out == 2.0f);
    REQUIRE(rb.read(out));  REQUIRE(out == 3.0f);
    REQUIRE_FALSE(rb.read(out));
}

TEST_CASE("RingBuffer: full and empty semantics", "[dsp][ring_buffer]") {
    shifter::RingBuffer<8> rb;

    // Usable capacity is Capacity - 1 = 7.
    for (int i = 0; i < 7; ++i) {
        REQUIRE(rb.write(static_cast<float>(i)));
    }
    REQUIRE_FALSE(rb.write(99.0f));  // full

    for (int i = 0; i < 7; ++i) {
        float out = -1.0f;
        REQUIRE(rb.read(out));
        REQUIRE(out == static_cast<float>(i));
    }

    float out = -1.0f;
    REQUIRE_FALSE(rb.read(out));  // empty
}

TEST_CASE("RingBuffer: wraps around correctly", "[dsp][ring_buffer]") {
    shifter::RingBuffer<4> rb;

    REQUIRE(rb.write(10.0f));
    REQUIRE(rb.write(20.0f));

    float out = 0.0f;
    REQUIRE(rb.read(out));  REQUIRE(out == 10.0f);

    REQUIRE(rb.write(30.0f));
    REQUIRE(rb.write(40.0f));

    REQUIRE(rb.read(out));  REQUIRE(out == 20.0f);
    REQUIRE(rb.read(out));  REQUIRE(out == 30.0f);
    REQUIRE(rb.read(out));  REQUIRE(out == 40.0f);
}

TEST_CASE("RingBuffer: SPSC under contention", "[dsp][ring_buffer]") {
    shifter::RingBuffer<1024> rb;
    constexpr int kItems = 100'000;

    std::thread producer([&] {
        for (int i = 0; i < kItems; ++i) {
            while (!rb.write(static_cast<float>(i))) {
                std::this_thread::yield();
            }
        }
    });

    std::vector<float> received;
    received.reserve(kItems);

    std::thread consumer([&] {
        int got = 0;
        while (got < kItems) {
            float v;
            if (rb.read(v)) {
                received.push_back(v);
                ++got;
            } else {
                std::this_thread::yield();
            }
        }
    });

    producer.join();
    consumer.join();

    REQUIRE(received.size() == kItems);
    for (int i = 0; i < kItems; ++i) {
        REQUIRE(received[i] == static_cast<float>(i));
    }
}
