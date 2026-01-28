#pragma once

#include <array>
#include <cstdio>

template<typename T, size_t BUFFER_SIZE, size_t SAMPLES_SIZE, size_t STRIDE>
class RingBuffer
{
    public:

    RingBuffer(){};
    ~RingBuffer(){};

    std::array<T, SAMPLES_SIZE> get_samples()
    {
        size_t i_overflow = 0;
        std::array<T, SAMPLES_SIZE> samples{};

        for(size_t i = 0; i < SAMPLES_SIZE; i++)
        {
            if(i + next_read < BUFFER_SIZE){
                samples[i] = buffer[i + next_read];
            }
            else{
                samples[i] = buffer[i_overflow];
                i_overflow++;
            }
        }

        // Update current read position in the buffer
        next_read = (next_read + STRIDE >= BUFFER_SIZE) ? (next_read + STRIDE - BUFFER_SIZE) : (next_read + STRIDE);

        return samples;
    };

    void update_buffer(std::array<T, STRIDE> new_data)
    {
        size_t i_overflow = 0;

        for(size_t i = 0; i < STRIDE; i++){
            if(i + next_write < BUFFER_SIZE){
                buffer[i + next_write] = new_data[i];
            }
            else{
                buffer[i_overflow] = new_data[i];
                i_overflow++;
            }
        }

        // Update current write position in the buffer
        next_write = (next_write + STRIDE >= BUFFER_SIZE) ? (next_write + STRIDE - BUFFER_SIZE) : (next_write + STRIDE);
    };

    size_t get_next_read() {return next_read;};
    size_t get_next_write() {return next_write;};

    private:
    size_t next_read = 0;
    size_t next_write = 0;
    std::array<T, BUFFER_SIZE> buffer{};
};