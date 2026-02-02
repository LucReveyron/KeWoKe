#pragma once
#include <stdint.h>

constexpr int16_t SAMPLE_RATE = 16000; // in Hz
constexpr int FRAME_SIZE = 400;
constexpr int FRAME_STRIDE = 160;
constexpr int NUMBER_CEPS = 13;
constexpr int NUM_FRAMES = 1 + (SAMPLE_RATE - FRAME_SIZE + FRAME_STRIDE - 1) / FRAME_STRIDE;