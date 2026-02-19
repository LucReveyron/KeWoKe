idf_component_register(SRCS "model_classifier.cc" "audio_recognition.cpp" "main.cpp" "audio_sampling.cpp" "kissFFT/kiss_fft.c" "kissFFT/kiss_fftr.c"
                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES driver esp_psram esp-tflite-micro esp-nn
                       INCLUDE_DIRS ".")

                       
