
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "mfcc_constants.hpp"

constexpr size_t BUFFER_LEN = 160;

void i2s_install(void);
void i2s_uninstall(void);
size_t i2s_read_samples(int16_t *buffer, size_t len);