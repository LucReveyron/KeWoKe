idf_component_register(SRCS "main.cpp" "audio_sampling.cpp" "kissFFT/kiss_fft.c" "kissFFT/kiss_fftr.c"
                       PRIV_REQUIRES spi_flash
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS ".")
