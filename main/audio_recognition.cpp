#include "esp_log.h"
#include "audio_recognition.hpp"

static const char* TAG = "audio_recognition";

constexpr int CLASSIFIER_ARENA_SIZE = 51 * 1024;

static tflite::MicroMutableOpResolver<8> shared_resolver;
alignas(16) static uint8_t classifier_arena[CLASSIFIER_ARENA_SIZE];

const tflite::Model* model_classifier = nullptr;

tflite::MicroInterpreter* classifier = nullptr;

void load_model(const tflite::Model*& model, const void* source_model)
{
    model = tflite::GetModel(source_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        ESP_LOGI(TAG,"Model provided is schema version %d not equal to supported "
                    "version %d.", model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
}

// Setup function (call once at startup)
void setup_models() {
    // Register ops once
    shared_resolver.AddConv2D();          // CONV_2D
    shared_resolver.AddFullyConnected();  // FULLY_CONNECTED
    shared_resolver.AddMean();            // MEAN
    shared_resolver.AddSoftmax();         // SOFTMAX
    shared_resolver.AddReshape();         // RESHAPE
    shared_resolver.AddPack();            // PACK
    shared_resolver.AddShape();           // SHAPE
    shared_resolver.AddStridedSlice();    // STRIDED_SLICE
}

// Create interpreters as static (once)
void setup_interpreters()
{    
    static tflite::MicroInterpreter classifier_interpreter(
        model_classifier, shared_resolver, classifier_arena, CLASSIFIER_ARENA_SIZE);
    classifier = &classifier_interpreter;

    // Allocate memory from the tensor_arena for the model's tensors.
    TfLiteStatus allocate_status = classifier->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        ESP_LOGI(TAG,"AllocateTensors() failed");
        return;
    }
}

void setup_recognition()
{
    load_model(model_classifier, model_classifier_tflite);
    setup_models();
    setup_interpreters();
}

void run_inference(std::array<std::array<int16_t, NUMBER_CEPS>, NUM_FRAMES> coefficient)
{
    TfLiteTensor* input = classifier->input(0);
    float input_scale = input->params.scale;
    int input_zero_point = input->params.zero_point;
    int8_t* input_ptr = input->data.int8;

    // Flatten and quantize directly into the input tensor
    for (size_t i = 0; i < NUM_FRAMES; i++) {
        for (size_t j = 0; j < NUMBER_CEPS; j++) {
            float mfcc_float = static_cast<float>(coefficient[i][j]);

            // Quantize to int8 and clamp
            int32_t q = static_cast<int32_t>(std::round(mfcc_float / input_scale) + input_zero_point);
            if (q > 127) q = 127;
            if (q < -128) q = -128;
            // Write directly to input tensor
            input_ptr[i * NUMBER_CEPS + j] = static_cast<int8_t>(q);
        }
    }

    // Run classifier
    if (classifier->Invoke() != kTfLiteOk) {
        ESP_LOGI(TAG, "Classifier Invoke() failed");
        return;
    }

    // Get output
    TfLiteTensor* output = classifier->output(0);
    float output_scale = output->params.scale;
    int output_zero_point = output->params.zero_point;

    int max_idx = 0;
    float max_result = 0.0f;

    for (int i = 0; i < kCategoryCount; i++) {
        float score = (tflite::GetTensorData<int8_t>(output)[i] - output_zero_point) * output_scale;
        if (score > max_result) {
            max_result = score;
            max_idx = i;
        }
    }

    if (max_result > 0.65f) {
        ESP_LOGI(TAG, "Detected %7s, score: %.2f", kCategoryLabels[max_idx], static_cast<double>(max_result));
    }
}
