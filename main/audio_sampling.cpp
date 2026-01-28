// Legacy library
//#include "driver/i2s.h"
#include "freertos/FreeRTOS.h" 
#include "freertos/task.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "audio_sampling.h"

i2s_chan_handle_t rx_handle = nullptr;

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
    i2s_channel_init_std_mode(rx_handle, &std_cfg);

    i2s_channel_enable(rx_handle);
}

size_t i2s_read_samples(int16_t *buffer, size_t len)
{
    size_t bytes_read = 0;

    if(rx_handle != nullptr)
        i2s_channel_read(rx_handle, buffer, len * sizeof(int16_t), &bytes_read, pdMS_TO_TICKS(1000));

    return bytes_read / sizeof(int16_t);
}

// Legacy library implementation
/*void i2s_install(void)
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE_HZ,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 1024,
        .use_apll = false,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 0};

    i2s_pin_config_t i2s_mic_pins = {
        .bck_io_num = 26,
        .ws_io_num = 32,
        .data_out_num = I2S_PIN_NO_CHANGE,
        .data_in_num = 33};

        // Install and start I2S driver
        esp_err_t err = i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
        if (err != ESP_OK) {
            printf("Failed to install I2S driver: %d\n", err);
            esp_restart();
        }

        // Configure I2S pins
        err = i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
        if (err != ESP_OK) {
            printf("Failed to set I2S pins: %d\n", err);
            esp_restart();
        }

        i2s_zero_dma_buffer(I2S_NUM_0);
}

size_t i2s_read_samples(int16_t *buffer, size_t len)
{
    size_t bytes_read = 0;

    ESP_ERROR_CHECK(
        i2s_read(
            I2S_NUM_0,
            buffer,
            len * sizeof(int16_t),
            &bytes_read,
            portMAX_DELAY
        )
    );

    return bytes_read / sizeof(int16_t);
}*/

