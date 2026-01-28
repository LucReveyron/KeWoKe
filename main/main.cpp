#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"

#include "audio_sampling.h"
#include "mfcc.h"
#include "ring_buffer.hpp"

const int FRAME_SIZE = 400;
const int FRAME_STRIDE = 160;
const int NUMBER_CEPS = 13;
const int NUM_FRAMES = 1 + (SAMPLE_RATE - FRAME_SIZE + FRAME_STRIDE - 1) / FRAME_STRIDE;

int16_t buffer[BUFFER_LEN];
RingBuffer<int16_t, 4*FRAME_STRIDE, FRAME_SIZE, FRAME_STRIDE> ring_buffer;

std::array<int16_t, FRAME_STRIDE> signal{};
std::array<std::array<int16_t, NUMBER_CEPS>,NUM_FRAMES> coef{};
size_t index_coef;

MFCC<> mfccProcessor;

bool full_flag;

void main_task(void *arg)
{
    while(true)
    {
        size_t len_samples = i2s_read_samples(buffer, BUFFER_LEN);

        for (size_t i = 0; i < len_samples; i++) {

            signal[i] = buffer[i];
        }

        ring_buffer.update_buffer(signal);

        mfccProcessor.set_signal(ring_buffer.get_samples());

        mfccProcessor.compute_coefficient();

        coef[index_coef] = mfccProcessor.get_coefficient();

        //printf("Next_read = %d ; Next_write = %d\n",ring_buffer.get_next_read(), ring_buffer.get_next_write());
        index_coef = (index_coef + 1) % NUM_FRAMES;

        if(index_coef == NUM_FRAMES - 1){
            for(size_t f = 0; f < NUM_FRAMES; f++){
                printf("Frame : %d\n",f);
                for(size_t i = 0; i < NUMBER_CEPS; i++)
                {
                    printf("%d;",coef[f][i]);
                }
                printf("\n");
            }
        }

        //vTaskDelay(pdMS_TO_TICKS(10));
    }
}

extern "C" void app_main(void)
{
    // Set up I2S
    i2s_install();

    // Disable logs
    esp_log_level_set("*", ESP_LOG_NONE);

    index_coef = 0;

    xTaskCreate(main_task, "main", 4096, NULL, 5, NULL);
}

