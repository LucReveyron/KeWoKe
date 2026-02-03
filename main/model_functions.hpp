#pragma once

#include "mfcc_constants.hpp"

void model_setup();

void model_loop(std::array<std::array<int16_t, NUMBER_CEPS>, NUM_FRAMES>& input);