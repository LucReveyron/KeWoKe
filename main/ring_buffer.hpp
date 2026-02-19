#pragma once

#include <array>
#include <cstdio>
#include <atomic>
#include "freertos/semphr.h"

template<typename T, size_t BUFFER_SIZE, size_t SAMPLES_SIZE, size_t STRIDE>
class RingBuffer
{
    // Check buffer size
    static_assert(BUFFER_SIZE > 0, "RingBuffer size must be > 0");
    static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0, "RingBuffer size must be a power of 2");
    static_assert(SAMPLES_SIZE <= BUFFER_SIZE, "SAMPLES_SIZE must be <= BUFFER_SIZE");
    static_assert(STRIDE <= BUFFER_SIZE, "STRIDE must be <= BUFFER_SIZE");
    
public:
    RingBuffer() : next_read(0), next_write(0) {};
    ~RingBuffer() {};
    
    // Read SAMPLES_SIZE samples starting at next_read (with wrap-around)
    // Returns true if successful, false if not enough data available
    bool read_samples(T* dest) {
        size_t current_read = next_read.load(std::memory_order_relaxed);
        size_t current_write = next_write.load(std::memory_order_acquire);
        
        // Check if we have enough samples available
        size_t available = (current_write - current_read) & (BUFFER_SIZE - 1);
        if (available < SAMPLES_SIZE) {
            return false; // Not enough data
        }
        
        // Copy data (no lock needed - this data won't be overwritten)
        for (size_t i = 0; i < SAMPLES_SIZE; i++) {
            dest[i] = buffer[(current_read + i) & (BUFFER_SIZE - 1)];
        }
        
        // Update read pointer atomically
        next_read.store((current_read + STRIDE) & (BUFFER_SIZE - 1), std::memory_order_release);
        
        return true;
    }
    
    // Write samples starting at next_write (with wrap-around)
    // Returns true if successful, false if buffer is full
    bool write_samples(const T* src, size_t size) {
        size_t current_write = next_write.load(std::memory_order_relaxed);
        size_t current_read = next_read.load(std::memory_order_acquire);
        
        // Check if we have enough space
        size_t available_space = (current_read - current_write - 1) & (BUFFER_SIZE - 1);
        if (available_space < size) {
            return false; // Buffer full - would overwrite unread data
        }
        
        // Copy data (no lock needed - reader won't access this yet)
        for (size_t i = 0; i < size; i++) {
            buffer[(current_write + i) & (BUFFER_SIZE - 1)] = src[i];
        }
        
        // Update write pointer atomically - makes data visible to reader
        next_write.store((current_write + size) & (BUFFER_SIZE - 1), std::memory_order_release);
        
        return true;
    }
    
    // Get SAMPLES_SIZE array from the buffer starting at next_read (with wrap-around)
    std::array<T, SAMPLES_SIZE> get_samples_as_array() {
        std::array<T, SAMPLES_SIZE> samples;
        read_samples(samples.data());
        return samples; 
    }
    
    size_t get_next_read() { return next_read.load(std::memory_order_relaxed); }
    size_t get_next_write() { return next_write.load(std::memory_order_relaxed); }
    
    // Get number of samples available to read
    size_t available() {
        size_t current_write = next_write.load(std::memory_order_acquire);
        size_t current_read = next_read.load(std::memory_order_relaxed);
        return (current_write - current_read) & (BUFFER_SIZE - 1);
    }
    
    T buffer[BUFFER_SIZE]; // Public for DMA access
    
private:
    std::atomic<size_t> next_read;
    std::atomic<size_t> next_write;
};