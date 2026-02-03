idf_component_register(SRCS "main.cpp" "audio_sampling.cpp" "normalize.cpp" "kissFFT/kiss_fft.c" "kissFFT/kiss_fftr.c" "model.cc" "model_functions.cpp"
                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES driver tflite-micro esp_psram
                       INCLUDE_DIRS ".")
