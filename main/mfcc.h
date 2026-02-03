#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "freertos/semphr.h"

#include "kissFFT/kiss_fftr.h"
#include "hamming_window.hpp"

constexpr int16_t INT16_MAX_VALUE = 32767;
constexpr int16_t INT16_MIN_VALUE = -32768;
constexpr int16_t ALPHA_Q15 = 31876; // 0.97 in Q15

constexpr int16_t SAMPLE_FREQ = 16000;

template <
    typename T,
    int ROW = NUM_FRAMES,
    int COLUMN = NUMBER_CEPS
>
class shared_buffer
{
    private:
    std::array<std::array<T, COLUMN>, ROW> buffer{};
    SemaphoreHandle_t buffer_mutex;

    public:
        shared_buffer(){buffer_mutex = xSemaphoreCreateMutex();}
        ~shared_buffer()
        {
            if (buffer_mutex != NULL) {
                vSemaphoreDelete(buffer_mutex);  
                buffer_mutex = NULL;  
            }
        }

        void set_buffer(std::array<std::array<T, COLUMN>, ROW>& input_buffer)
        {
            xSemaphoreTake(buffer_mutex, portMAX_DELAY);
            buffer = input_buffer;
            xSemaphoreGive(buffer_mutex);
        }

        void set_row(std::array<T, COLUMN> column, size_t index)
        {
            xSemaphoreTake(buffer_mutex, portMAX_DELAY);
            buffer[index] = column;
            xSemaphoreGive(buffer_mutex);
        }

        std::array<std::array<T, COLUMN>, ROW> get_buffer()
        {
            return buffer;
        }

        std::array<std::array<T, COLUMN>, ROW>& ref_buffer()
        {
            return buffer;
        }

        T get_element(size_t row, size_t column)
        {
            return buffer[row][column];
        }
};

template <
    int FRAME_SIZE = 400,
    int FRAME_STRIDE = 160,
    int NUMBER_FILTERS = 40,
    int NFFT = 512,
    int NUMBER_CEPS = 13
>
class MFCC
{
public:

    constexpr static float min_frequency_mel = 0;
    inline static const float max_frequency_mel = 2595 * log10f(1 + (SAMPLE_FREQ / 2) / 700.0f);

    inline static const auto HAMMING = make_hamming_lut<FRAME_SIZE>();

    MFCC();
    ~MFCC();

    // Methods
    void apply_pre_emphasis();
    void apply_hamming_window();
    void compute_FFT();
    void compute_power_spectrum();
    float from_mel_to_hz(float freq_mel);
    void compute_triangle_filters();
    void apply_mel_banks();
    void compute_DCT(); 

    void set_signal(const std::array<int16_t, FRAME_SIZE>& new_signal);
    void compute_coefficient();
    std::array<int16_t, NUMBER_CEPS> get_coefficient();

private:

    // frame
    std::array<int16_t, FRAME_SIZE> frame{};

    // FFT
    kiss_fftr_cfg cfg;
    std::array<kiss_fft_cpx, NFFT/2 + 1> fft_out{};
    std::array<int32_t, NFFT/2 + 1> power_spectrum{};

    // Mel filters
    std::array<std::array<int16_t, NFFT/2 + 1>, NUMBER_FILTERS> mel_weights{};

    // Mel filter banks
    std::array<int16_t, NUMBER_FILTERS> filter_banks{};

    // MFC coefficients
    std::array<int16_t, NUMBER_CEPS> coef{};
};

// PUBLIC METHODS

template <int F, int ST, int NF, int NFFT, int NCEPS>
MFCC<F,ST,NF,NFFT, NCEPS>::MFCC()
{
    cfg = kiss_fftr_alloc(NFFT, 0, nullptr, nullptr);
    compute_triangle_filters();
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
MFCC<F,ST,NF,NFFT, NCEPS>::~MFCC()
{
    free(cfg);
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::set_signal(const std::array<int16_t, F>& new_signal)
{
    frame = new_signal;
}

// PRIVATE METHODS

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::apply_pre_emphasis()
{
    // Start from the end to keep using the same array without overwrite
    // usefull sample before calculation
    for(size_t i = F - 1; i > 0; i--)
    {
        int32_t filtered = frame[i] - ((ALPHA_Q15 * frame[i-1]) >> 15);

        if (filtered > INT16_MAX_VALUE) filtered = INT16_MAX_VALUE;
        else if (filtered < INT16_MIN_VALUE) filtered = INT16_MIN_VALUE;

        frame[i] = static_cast<int16_t>(filtered);
    }
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::apply_hamming_window()
{
    for (size_t j = 0; j < F; j++)
        frame[j] = (frame[j] * HAMMING[j]) >> 15;
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::compute_FFT()
{
    std::array<kiss_fft_scalar, NFFT> fft_in{};

    for (size_t j = 0; j < F; ++j) 
    {
        fft_in[j] = frame[j];  // direct copy
    }

    for (size_t j = F; j < NFFT; ++j) 
    {
        fft_in[j] = 0;          // zero-padding
    }

    kiss_fftr(cfg, fft_in.data(), fft_out.data());
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::compute_power_spectrum()
{
    for(size_t j = 0; j < NFFT/2 + 1; j++)
    {
        int64_t real_part = int64_t(fft_out[j].r) * fft_out[j].r;
        int64_t im_part = int64_t(fft_out[j].i) * fft_out[j].i;

        power_spectrum[j] = (real_part + im_part) / NFFT; // (magnitude² / NFFT)
    }
    
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
float MFCC<F,ST,NF,NFFT, NCEPS>::from_mel_to_hz(float m)
{
    return 700.f * (powf(10.f, m / 2595.f) - 1.f);
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::compute_triangle_filters()
{
    float delta = (max_frequency_mel - min_frequency_mel) / (NF + 1);

    for (int f = 0; f < NF; f++)
    {
        float f_min = from_mel_to_hz(delta * f);
        float f_center = from_mel_to_hz(delta * (f + 1));
        float f_max = from_mel_to_hz(delta * (f + 2));

        //int32_t sum = 0; // For normalization

        for (int bin = 0; bin < NFFT/2 + 1; bin++)
        {
            float freq_hz = bin * (float(SAMPLE_FREQ) / NFFT);

            float w;
            if (freq_hz < f_min) w = 0;
            else if (freq_hz < f_center) w = (freq_hz - f_min) / (f_center - f_min);
            else if (freq_hz < f_max) w = (f_max - freq_hz) / (f_max - f_center);
            else w = 0;

            mel_weights[f][bin] = int16_t(w * 32767.f);
            //sum += mel_weights[f][bin] * mel_weights[f][bin];
        }

        // Second pass: normalize
        /*if (sum > 0)
        {
            for (int bin = 0; bin < NFFT/2 + 1; bin++)
            {
                mel_weights[f][bin] = (int32_t(mel_weights[f][bin]) * 32767) / sum;
            }
        }*/
    }
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::apply_mel_banks()
{
    constexpr float LOG_SCALE = 1.0f; // adjust for dynamic range

    for(size_t filter = 0; filter < NF; filter++)
    {
        int64_t acc = 0;

        for(int s = 0; s < NFFT/2 + 1; s++)
        {
            acc += power_spectrum[s] * mel_weights[filter][s];
        }
        // Example: log compression
        //float db = 20 * log10f(std::max((float)acc, 1.0f)); 
        // Convert to float energy
        float energy = acc * (1.0f / 32768.0f);

        // Log compression
        float log_energy = logf(std::max(energy, 1e-6f));

        filter_banks[filter] = (int16_t)(log_energy * LOG_SCALE);
    }
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT, NCEPS>::compute_DCT()
{

    // Loop over cepstral coefficients
    for (size_t k = 0; k < NCEPS; k++)
    {
        int64_t acc = 0;

        // Loop over Mel filter banks
        for (size_t n = 0; n < NF; n++)
        {
            // Compute the angle: θ = π/NF * (n + 0.5) * k
            double angle = M_PI * (n + 0.5) * k / NF;

            // Cosine in Q15
            int32_t cos_q15 = int32_t(std::round(std::cos(angle) * 32767.0));

            // Multiply filter bank (Q15) by cos (Q15) → Q30
            acc += int64_t(filter_banks[n]) * cos_q15;
        }

        // Q30 → Q15
        acc >>= 15;

        // Clamp to int16_t
        coef[k] = int16_t(std::clamp(acc, -32768LL, 32767LL));
    }
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
void MFCC<F,ST,NF,NFFT,NCEPS>::compute_coefficient()
{
    apply_pre_emphasis();

    apply_hamming_window();

    compute_FFT();

    compute_power_spectrum();

    apply_mel_banks();

    compute_DCT();
}

template <int F, int ST, int NF, int NFFT, int NCEPS>
std::array<int16_t, NCEPS> MFCC<F,ST,NF,NFFT,NCEPS>::get_coefficient()
{
    return coef;
}