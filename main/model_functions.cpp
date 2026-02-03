#include <algorithm>
#include <cstdint>
#include <iterator>
#include <vector>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "model_functions.hpp"
#include "model.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

#include "esp_task_wdt.h"

static const char* TAG = "model_functions.cpp";

// Variables for the model's output categories.
constexpr int kCategoryCount = 7;
constexpr const char* kCategoryLabels[kCategoryCount] = {
    "go",
    "no",
    "off",
    "on",
    "stop",
    "unknown",
    "yes"
};

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;

// Create an area of memory to use for input, output, and intermediate arrays.
// The size of this will depend on the model you're using, and may need to be
// determined by experimentation.
constexpr int kFeatureElementCount = NUMBER_CEPS * NUM_FRAMES;
constexpr int kTensorArenaSize = 120 * 1024;
uint8_t tensor_arena[kTensorArenaSize];
int8_t* feature_buffer = nullptr;
int8_t* model_input_buffer = nullptr;

void model_setup()
{
    feature_buffer = (int8_t*) heap_caps_malloc(
        kFeatureElementCount * sizeof(int8_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (!feature_buffer) { ESP_LOGI(TAG,"Feature buffer alloc failed"); return; }

    ESP_LOGI(TAG, "Arena Address: %p", (void*)tensor_arena);
    
    // Map the model into a usable data structure. This doesn't involve any
    // copying or parsing, it's a very lightweight operation.
    model = tflite::GetModel(model_tflite);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGI(TAG,"Model provided is schema version %d not equal to supported "
                    "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    
    // Based on model definition
    static tflite::MicroMutableOpResolver<10> micro_op_resolver; // Adjust size as needed
    if (micro_op_resolver.AddConv2D() != kTfLiteOk) return;
    if (micro_op_resolver.AddBatchMatMul() != kTfLiteOk) return;
    if (micro_op_resolver.AddAdd() != kTfLiteOk) return;
    if (micro_op_resolver.AddRsqrt() != kTfLiteOk) return;
    if (micro_op_resolver.AddRelu() != kTfLiteOk) return;
    if (micro_op_resolver.AddMaxPool2D() != kTfLiteOk) return;
    if (micro_op_resolver.AddMean() != kTfLiteOk) return; // For GlobalAveragePooling2D
    if (micro_op_resolver.AddFullyConnected() != kTfLiteOk) return;
    if (micro_op_resolver.AddSoftmax() != kTfLiteOk) return;

    // Build an interpreter to run the model with.
    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        ESP_LOGI(TAG,"AllocateTensors() failed");
        return;
    }

    // Get information about the memory area to use for the model's input.
    model_input = interpreter->input(0);

    // Check for 4D input [1, 99, 13, 1]
    if ((model_input->dims->size != 4) || 
        (model_input->dims->data[0] != 1) ||
        (model_input->dims->data[1] != 99) ||
        (model_input->dims->data[2] != 13) ||
        (model_input->dims->data[3] != 1) ||
        (model_input->type != kTfLiteInt8)) {
        
        ESP_LOGE(TAG, "Input tensor mismatch! Model wants 4D [1,99,13,1], Type %d", model_input->type);
        return;
    }
    // If it passes, you are ready to copy data!
    ESP_LOGI(TAG, "Input tensor check passed!");

    model_input_buffer = tflite::GetTensorData<int8_t>(model_input);
}

void model_loop(std::array<std::array<int16_t, NUMBER_CEPS>, NUM_FRAMES>& input)
{

    ESP_LOGI(TAG, "Starting Quantization...");
    // Copy feature buffer to input tensor

    ESP_LOGI("DEBUG", "Raw MFCC[0][0]: %d", input[0][0]);
    
    for (int i = 0; i < NUM_FRAMES; i++) {
        for (int j = 0; j < NUMBER_CEPS; j++) {
            // Quantize int16 -> int8
            float raw = static_cast<float>(input[i][j]);
            int8_t q_val = static_cast<int8_t>(raw / 2.0f);

            // Flattened index for the [1, 99, 13, 1] tensor
            model_input_buffer[i * NUMBER_CEPS + j] = q_val;
        }
    }

    int8_t min_v = 127, max_v = -128;
    for (int i = 0; i < kFeatureElementCount; i++) {
        if (model_input_buffer[i] < min_v) min_v = model_input_buffer[i];
        if (model_input_buffer[i] > max_v) max_v = model_input_buffer[i];
    }
    ESP_LOGI("DEBUG", "Quantized Range: Min: %d, Max: %d", min_v, max_v);
    
    ESP_LOGI(TAG, "Starting Invoke (this may take a long time)...");

    // Run the model on the spectrogram input and make sure it succeeds.
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        ESP_LOGI(TAG, "Invoke failed");
        return;
    }

    ESP_LOGI(TAG, "Invoke Finished! ");

    // Obtain a pointer to the output tensor
    TfLiteTensor* output = interpreter->output(0);

    float output_scale = output->params.scale;
    int output_zero_point = output->params.zero_point;
    int max_idx = 0;
    float max_result = 0.0;
    // Dequantize output values and find the max
    for (int i = 0; i < kCategoryCount; i++) {
        float current_result =
            (tflite::GetTensorData<int8_t>(output)[i] - output_zero_point) *
            output_scale;
        if (current_result > max_result) {
        max_result = current_result; // update max result
        max_idx = i; // update category
        }
    }
    if (max_result > 0.1f) {
        ESP_LOGI(TAG,"Detected %7s, score: %.2f", kCategoryLabels[max_idx],
            static_cast<double>(max_result));
    }
}