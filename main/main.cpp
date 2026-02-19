
#include <cstdio>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"  // Required for Task Watchdog Timer functions
#include "esp_heap_caps.h"

#include "ring_buffer.hpp"
#include "audio_sampling.h"
#include "audio_recognition.hpp"
#include "mfcc.h"
#include "ring_buffer.hpp"
#include "mfcc_constants.hpp"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

static const char* TAG = "Main.cpp";

// Ping/Pong buffers
shared_buffer<int16_t> bufferA;
shared_buffer<int16_t> bufferB;

shared_buffer<int16_t>* write_buffer = &bufferA;
shared_buffer<int16_t>* read_buffer  = &bufferB;

TaskHandle_t inference_handle = nullptr;
SemaphoreHandle_t normalize_sem = nullptr;

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
void mfcc_task(void* arg);
void inference_task(void* arg);

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Initialization ...\n");

    normalize_sem = xSemaphoreCreateBinary();

    // Set up I2S
    i2s_install();
    setup_recognition();

    xTaskCreatePinnedToCore(mfcc_task, "MFCCtask", 6144, NULL, 4, &task_handle, 1);
    xTaskCreate(inference_task, "InferenceTask", 1024 * 12, NULL, 1, &inference_handle);

    ESP_LOGI(TAG, "Initialization End\n");
}

// ------------------ TASK STRUCTURE ------------------


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
                // Wait for notification from the I2S ISR callback
                ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
                std::array<int16_t, FRAME_SIZE> frame;

                if (ring_buffer.read_samples(frame.data())) 
                {

                    /*
                    // Find max absolute sample
                    int16_t max_val = 0;
                    for(int i = 0; i < FRAME_SIZE; i++) {
                        if(abs(frame[i]) > max_val) max_val = abs(frame[i]);
                    }

                    // Avoid division by zero
                    float norm_factor = (max_val == 0) ? 1.0f : 32767.0f / max_val;

                    // Apply normalization safely
                    for(int i = 0; i < FRAME_SIZE; i++) {
                        float tmp = frame[i] * norm_factor;
                        if(tmp > 32767.0f) tmp = 32767.0f;
                        if(tmp < -32768.0f) tmp = -32768.0f;
                        frame[i] = static_cast<int16_t>(tmp);
                    }*/
                    
                    mfccProcessor.set_signal(frame);
                    stage = MfccStage::PRE_EMPHASIS;
                } 
                else 
                {
                    // Wait for more data
                    vTaskDelay(pdMS_TO_TICKS(1));
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
                write_buffer->data()[index_coef] = mfccProcessor.get_coefficient();
                index_coef++;
                
                if (index_coef == NUM_FRAMES)
                {
                    ESP_LOGD(TAG, "Buffer Full! Triggering Model...");
                    auto temp = write_buffer;
                    write_buffer = read_buffer;
                    read_buffer = temp;

                    index_coef = 0;

                    xTaskNotify(inference_handle, 0, eNoAction);
                }

                //if(index_coef % 10 == 0)
                    //xTaskNotify(inference_handle, 0, eNoAction);

                stage = MfccStage::IDLE;
                break;
        }

        // Cooperative multitasking point
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void inference_task(void* arg)
{
    ESP_LOGD(TAG, "Inference task\n");

    for(;;)
    {   
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // Run inference
        run_inference(read_buffer->data());
            
        //ESP_LOGI(TAG, "Model Done. Ready for new audio.");
    }
}
