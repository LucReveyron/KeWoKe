#pragma once
#include "mfcc_constants.hpp"

void normalize(const std::array<std::array<int16_t, NUMBER_CEPS>,NUM_FRAMES>& raw_values,
                std::array<std::array<float, NUMBER_CEPS>,NUM_FRAMES> &norm_values);