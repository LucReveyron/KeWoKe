// Legacy library
//#include "driver/i2s.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_err.h"

#include "audio_sampling.h"

static const char* TAG = "audio_sampling";

TaskHandle_t task_handle = nullptr;
i2s_chan_handle_t rx_handle = nullptr;

RingBuffer<int16_t, RING_BUFFER_LEN, FRAME_SIZE, FRAME_STRIDE> ring_buffer;

static size_t accumulated_samples = 0;

static bool IRAM_ATTR i2s_rx_callback(i2s_chan_handle_t handle, i2s_event_data_t *event, void *user_ctx) 
{
    BaseType_t high_task_wakeup = pdFALSE;

    size_t samples_available = event->size / sizeof(int16_t);

    int16_t temp_buffer[samples_available];
    size_t bytes_read = 0;

    i2s_channel_read(handle, temp_buffer, event->size, &bytes_read, 0);

    ring_buffer.write_samples(temp_buffer, samples_available);

    accumulated_samples += samples_available;

    if (accumulated_samples >= FRAME_STRIDE)  // 320
    {
        accumulated_samples -= FRAME_STRIDE;
        vTaskNotifyGiveFromISR(task_handle, &high_task_wakeup);
    }

    if (high_task_wakeup)
        portYIELD_FROM_ISR();

    return high_task_wakeup == pdTRUE;
}

void i2s_install(void)
{
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

    /* Allocate a new RX channel and get the handle of this channel */
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, NULL, &rx_handle));

    /* Setting the configurations, the slot configuration and clock configuration */
    i2s_std_config_t std_cfg = { 
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), 
        .gpio_cfg = { 
            .mclk = I2S_GPIO_UNUSED, 
            .bclk = GPIO_NUM_26, 
            .ws = GPIO_NUM_32, 
            .dout = I2S_GPIO_UNUSED, 
            .din = GPIO_NUM_33, 
            .invert_flags = { 
                .mclk_inv = false, 
                .bclk_inv = false, 
                .ws_inv = false, 
            }, 
        }, 
    };

    /* Initialize the channel */
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle, &std_cfg));

    // Register the callback
    i2s_event_callbacks_t cbs = {};
    cbs.on_recv = i2s_rx_callback;
    
    ESP_ERROR_CHECK(i2s_channel_register_event_callback(rx_handle, &cbs, task_handle));

    ESP_ERROR_CHECK(i2s_channel_enable(rx_handle));

    ESP_LOGI(TAG, "I2S enable\n");
}
