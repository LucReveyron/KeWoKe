## Embedded Keword Recognition on ESP_EYES
### Todo :
- [x] Homemade MFCC
- [x] Recover audio using I2S 
- [x] Compute MFCC
- [x] Normalize coefficient to adapt to tensorflow Lite model
- [x] Add model
- [x] Compute inference
- [ ] Optimize internal DCT to avoid float operations on MFCC
- [ ] Add samples from ESP-EYE microphone to the dataset and retrain

## ESP32 Real-Time Audio Recognition (MFCC + Inference)

This project implements a real-time audio recognition pipeline on an ESP32 using FreeRTOS. It captures audio through I2S, computes MFCC (Mel-Frequency Cepstral Coefficients) features, and performs inference using a preloaded recognition model.

The system is designed for embedded, resource-constrained environments and emphasizes deterministic task scheduling and efficient memory usage.

## 🚀 Features

- Real-time audio acquisition via I2S (with DMA and ISR)

- MFCC feature extraction pipeline (homemade, need improvement to be really only fixed-point):

  -  Pre-emphasis

  - Hamming window

  - FFT

  - Power spectrum

  - Mel filter banks

  - DCT (Implementation in float need to be remplace by int32 looking table)

- Double buffering (Ping-Pong) for continuous processing

- FreeRTOS task-based architecture

- Asynchronous inference triggering
