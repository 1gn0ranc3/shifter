#include "mini_fft.h"

#include <algorithm>
#include <cmath>

namespace shifter {

MiniFFT::MiniFFT(int order)
    : size_(1 << order), order_(order)
{
    bitReverseTable_.resize(static_cast<std::size_t>(size_));
    for (int i = 0; i < size_; ++i) {
        int rev = 0;
        int v = i;
        for (int j = 0; j < order_; ++j) {
            rev = (rev << 1) | (v & 1);
            v >>= 1;
        }
        bitReverseTable_[static_cast<std::size_t>(i)] = rev;
    }

    twiddles_.resize(static_cast<std::size_t>(size_ / 2));
    const double angleStep = -2.0 * 3.141592653589793 / static_cast<double>(size_);
    for (int i = 0; i < size_ / 2; ++i) {
        twiddles_[static_cast<std::size_t>(i)] = {
            static_cast<float>(std::cos(angleStep * i)),
            static_cast<float>(std::sin(angleStep * i))
        };
    }
}

void MiniFFT::perform(const std::complex<float>* input,
                      std::complex<float>* output,
                      bool inverse) const noexcept
{
    if (input == output) {
        // In-place: pairwise swap according to the bit-reverse table.
        for (int i = 0; i < size_; ++i) {
            const int r = bitReverseTable_[static_cast<std::size_t>(i)];
            if (i < r) std::swap(output[i], output[r]);
        }
    } else {
        for (int i = 0; i < size_; ++i) {
            output[bitReverseTable_[static_cast<std::size_t>(i)]] = input[i];
        }
    }

    for (int len = 2; len <= size_; len <<= 1) {
        const int halfLen    = len / 2;
        const int twidStride = size_ / len;
        for (int i = 0; i < size_; i += len) {
            for (int j = 0; j < halfLen; ++j) {
                std::complex<float> w = twiddles_[static_cast<std::size_t>(j * twidStride)];
                if (inverse) w = std::conj(w);
                const auto u = output[i + j];
                const auto t = w * output[i + j + halfLen];
                output[i + j]           = u + t;
                output[i + j + halfLen] = u - t;
            }
        }
    }

    if (inverse) {
        const float invN = 1.0f / static_cast<float>(size_);
        for (int i = 0; i < size_; ++i) {
            output[i] *= invN;
        }
    }
}

}  // namespace shifter
