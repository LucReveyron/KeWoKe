
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "mfcc_constants.hpp"
#include "ring_buffer.hpp"

extern TaskHandle_t task_handle;  // Handle for the MFCC task
constexpr size_t RING_BUFFER_LEN = 2*1024;

extern RingBuffer<int16_t, RING_BUFFER_LEN, FRAME_SIZE, FRAME_STRIDE> ring_buffer;


void i2s_install(void);