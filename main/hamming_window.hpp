#pragma once
#include <array>
#include <cmath>
#include <cstdint>

// Compute Hamming value for index n in window of size N
inline int16_t hamming_value(int N, int n)
{
    float angle = 2.0f * M_PI * n / (N - 1);
    float w = 0.54f - 0.46f * std::cos(angle);  // Hamming formula
    return int16_t(w * 32767.f + 0.5f);
}

// Compute Hamming LUT at runtime
template <size_t N>
inline std::array<int16_t, N> make_hamming_lut()
{
    std::array<int16_t, N> lut{};
    for (size_t i = 0; i < N; i++)
        lut[i] = hamming_value(N, i);
    return lut;
}
