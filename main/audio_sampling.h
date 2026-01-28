
#pragma once
#include <stdint.h>
#include <stddef.h>

constexpr int16_t  SAMPLE_RATE = 16000; // in Hz
constexpr size_t BUFFER_LEN = 160;

void i2s_install(void);
void i2s_uninstall(void);
size_t i2s_read_samples(int16_t *buffer, size_t len);