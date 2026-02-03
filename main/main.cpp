
#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"  // Required for Task Watchdog Timer functions
#include "esp_heap_caps.h"

#include "audio_sampling.h"
#include "mfcc.h"
#include "ring_buffer.hpp"
#include "mfcc_constants.hpp"
#include "normalize.h"
#include "model_functions.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

static const char* TAG = "Main.cpp";

// Global buffers / semaphores
RingBuffer<int16_t, 4*FRAME_STRIDE, FRAME_SIZE, FRAME_STRIDE> ring_buffer;
shared_buffer<int16_t> ping_coef;
shared_buffer<int16_t> pong_coef;

SemaphoreHandle_t normalize_sem;
SemaphoreHandle_t index_mutex = xSemaphoreCreateMutex();

static MFCC<> mfccProcessor;
size_t index_coef;

enum class MfccStage {
    IDLE,
    PRE_EMPHASIS,
    WINDOW,
    FFT,
    POWER,
    MEL,
    DCT,
    STORE
};

// Task prototypes
void audio_task(void* arg);
void mfcc_task(void* arg);
void normalize_task(void* arg);

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initialization ...\n");
    // Set up I2S
    i2s_install();

    // Create semaphore for signaling normalize task
    normalize_sem = xSemaphoreCreateBinary();
    index_coef = 0;

    model_setup();

    // Create FreeRTOS tasks
    // Pin Audio and MFCC to CPU 1 (Keep them together for fast data transfer)
    xTaskCreatePinnedToCore(audio_task, "AudioTask", 8192, NULL, 5, NULL, 1);
    xTaskCreatePinnedToCore(mfcc_task, "MFCCtask", 6144, NULL, 4, NULL, 1);

    // We give it a priority of 1 so it yields to system background tasks.
    xTaskCreate(normalize_task, "NormalizeTask", 1024 * 12, NULL, 1, NULL);

    // IMPORTANT: De-register app_main from WDT before finishing
    esp_task_wdt_delete(NULL);

    ESP_LOGI(TAG, "Initialization End\n");
}

// ------------------ TASK STRUCTURE ------------------

// Task 1: Audio acquisition
void audio_task(void* arg)
{
    ESP_LOGI(TAG, "Audio task\n");
    std::array<int16_t, FRAME_STRIDE> signal{};
    static int16_t buffer[BUFFER_LEN];

    for(;;)
    {
        // Read audio samples from I2S and push to ring buffer
        size_t len_samples = i2s_read_samples(buffer, BUFFER_LEN);

        for (size_t i = 0; i < len_samples; i++) {

            signal[i] = buffer[i];
        }

        ring_buffer.update_buffer(signal);

        vTaskDelay(pdMS_TO_TICKS(1)); // yield
    }
}

// Task 2: MFCC computation
void mfcc_task(void* arg)
{
    ESP_LOGI(TAG, "MFCC task\n");
    MfccStage stage = MfccStage::IDLE;

    for(;;)
    {
        switch (stage)
        {
            case MfccStage::IDLE:
                xSemaphoreTake(index_mutex, portMAX_DELAY);
                if (index_coef < NUM_FRAMES)
                {
                    xSemaphoreGive(index_mutex);
                    mfccProcessor.set_signal(ring_buffer.get_samples());
                    stage = MfccStage::PRE_EMPHASIS;
                }
                else 
                {
                    xSemaphoreGive(index_mutex);
                    // We are waiting for normalize_task to reset index_coef
                    vTaskDelay(pdMS_TO_TICKS(10)); 
                }
                break;

            case MfccStage::PRE_EMPHASIS:
                mfccProcessor.apply_pre_emphasis();
                stage = MfccStage::WINDOW;
                break;

            case MfccStage::WINDOW:
                mfccProcessor.apply_hamming_window();
                stage = MfccStage::FFT;
                break;

            case MfccStage::FFT:
                mfccProcessor.compute_FFT();
                stage = MfccStage::POWER;
                break;

            case MfccStage::POWER:
                mfccProcessor.compute_power_spectrum();
                stage = MfccStage::MEL;
                break;

            case MfccStage::MEL:
                mfccProcessor.apply_mel_banks();
                stage = MfccStage::DCT;
                break;

            case MfccStage::DCT:
                mfccProcessor.compute_DCT();
                stage = MfccStage::STORE;
                break;

            case MfccStage::STORE:
                xSemaphoreTake(index_mutex, portMAX_DELAY);
                ping_coef.set_row(mfccProcessor.get_coefficient(), index_coef);
                index_coef++;
                
                if (index_coef == NUM_FRAMES)
                {
                    ESP_LOGD(TAG, "Buffer Full! Triggering Model...");
                    xSemaphoreGive(normalize_sem);
                }
                xSemaphoreGive(index_mutex);
                stage = MfccStage::IDLE;
                break;
        }

        // Cooperative multitasking point
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// Task 3: Normalize and plot
void normalize_task(void* arg)
{
    ESP_LOGI(TAG, "Norm task\n");

    for(;;)
    {
        if(xSemaphoreTake(normalize_sem, portMAX_DELAY) == pdTRUE)
        {
            //QUICKLY copy/swap data and reset the index
            xSemaphoreTake(index_mutex, portMAX_DELAY);
            pong_coef.set_buffer(ping_coef.ref_buffer()); // Transfer data to "Pong"
            index_coef = 0;                               // Reset "Ping" index immediately
            xSemaphoreGive(index_mutex);

            // Run inference
            model_loop(pong_coef.ref_buffer());
            
            ESP_LOGI(TAG, "Model Done. Ready for new audio.");
        }
    }
}
