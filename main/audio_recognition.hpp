#pragma once

// tfmicro
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/core/c/common.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

// Models
#include "model_classifier.h"

#include "mfcc_constants.hpp"

// Variables for the classifier's output categories.
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

void load_model(const tflite::Model*& model, const void* source_model);
void setup_models();
void setup_interpreters();
void setup_recognition();
void run_inference(std::array<std::array<int16_t, NUMBER_CEPS>, NUM_FRAMES> coefficient);