#pragma once

#include <complex>
#include <vector>

namespace shifter {

// Portable radix-2 Cooley-Tukey FFT. Size must be a power of two.
// Forward transform is unnormalised (same convention as juce::dsp::FFT on
// macOS); inverse applies the 1/N scaling. In-place safe: input and output
// pointers may alias.
class MiniFFT {
public:
    explicit MiniFFT(int order);

    int getSize() const noexcept { return size_; }

    void perform(const std::complex<float>* input,
                 std::complex<float>* output,
                 bool inverse) const noexcept;

private:
    int size_;
    int order_;
    std::vector<int>                 bitReverseTable_;
    std::vector<std::complex<float>> twiddles_;
};

}  // namespace shifter
