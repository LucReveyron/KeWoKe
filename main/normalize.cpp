#include <stdint.h>
#include <array>
#include <cmath>
#include "normalize.h"

void compute_mean(const std::array<std::array<int16_t, NUMBER_CEPS>,NUM_FRAMES>& coef,
                                            std::array<float, NUMBER_CEPS>& mean)
{
    for(int i = 0; i < NUMBER_CEPS; i++)
    {
        for(int j = 0; j < NUM_FRAMES; j++)
        {
            mean[i] += coef[j][i];
        }
        mean[i] /= NUM_FRAMES;
    }
}

void normalize(const std::array<std::array<int16_t, NUMBER_CEPS>,NUM_FRAMES>& raw_values,
                std::array<std::array<float, NUMBER_CEPS>,NUM_FRAMES> &norm_values)
{
    float diff = 0.0f;
    float acc = 0.0f;
    static std::array<float, NUMBER_CEPS> array{};
    compute_mean(raw_values, array);

    // WARNING : Combined loop: mean-center column i, normalize column i-1
    for(int i = 0; i <= NUMBER_CEPS; i++)
    {
        for(int j = 0; j < NUM_FRAMES; j++)
        {
            // Mean-center current column
            if(i < NUMBER_CEPS)
            {
                diff = raw_values[j][i] - array[i];
                norm_values[j][i] = diff;
                acc += diff * diff;
            }

            // Normalize previous column
            if(i != 0 && array[i-1] > 0.0f)
            {
                norm_values[j][i-1] /= array[i-1];
            }
        }
       // Compute std dev for current column 
        if(i < NUMBER_CEPS) array[i] = sqrt((1.0f / NUM_FRAMES) * acc);
        // Reset for next column accumulation
        acc = 0.0f;
    }
}