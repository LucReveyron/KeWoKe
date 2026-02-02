
#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include "esp_task_wdt.h"  // Required for Task Watchdog Timer functions

#include "audio_sampling.h"
#include "mfcc.h"
#include "ring_buffer.hpp"
#include "mfcc_constants.hpp"
#include "normalize.h"

static const char* TAG = "Main.cpp";

// Global buffers / semaphores
RingBuffer<int16_t, 4*FRAME_STRIDE, FRAME_SIZE, FRAME_STRIDE> ring_buffer;
shared_buffer<int16_t> coef;
shared_buffer<float> norm_coef;

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

    // Create FreeRTOS tasks
    xTaskCreate(audio_task, "AudioTask", 8192, NULL, 5, NULL);
    xTaskCreatePinnedToCore(mfcc_task, "MFCCtask", 6144, NULL, 3, NULL, 1);
    esp_task_wdt_add(NULL);
    xTaskCreate(normalize_task, "NormalizeTask", 8192, NULL, 3, NULL);
    ESP_LOGI(TAG, "Initialization End\n");
}

// ------------------ TASK STRUCTURE ------------------

// Task 1: Audio acquisition
void audio_task(void* arg)
{
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
    MfccStage stage = MfccStage::IDLE;

    for(;;)
    {
        switch (stage)
        {
            case MfccStage::IDLE:
                if (index_coef < NUM_FRAMES)
                {
                    mfccProcessor.set_signal(ring_buffer.get_samples());
                    stage = MfccStage::PRE_EMPHASIS;
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
                // take responsabiltiy of index
                xSemaphoreTake(index_mutex, portMAX_DELAY);
                coef.set_row(mfccProcessor.get_coefficient(), index_coef);
                index_coef++;
                xSemaphoreGive(index_mutex);

                if (index_coef == NUM_FRAMES)
                {
                    xSemaphoreGive(normalize_sem);
                }

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
    for(;;)
    {
        // Wait for semaphore from MFCC task
        // Normalize coef → norm_coef, plot / print results
        if(xSemaphoreTake(normalize_sem, portMAX_DELAY) == pdTRUE)
        {
            normalize(coef.ref_buffer(), norm_coef.ref_buffer());
            for(size_t f = 0; f < 5; f++){
                ESP_LOGI(TAG, "Frame : %d\n",f);
                for(size_t i = 0; i < NUMBER_CEPS; i++)
                {
                     ESP_LOGI(TAG, "%f;",norm_coef.get_element(f, i));
                }
                ESP_LOGI(TAG, "\n");
            }
        }
        //Reset and ready for next batch of coef
        xSemaphoreTake(index_mutex, portMAX_DELAY);
        index_coef = 0;
        xSemaphoreGive(index_mutex);

        vTaskDelay(pdMS_TO_TICKS(1)); // yield
    }
}
