#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include "tensorflow/lite/c/c_api_experimental.h"

#include "tensorflow/lite/c/c_api.h"
#include "tensorflow/lite/c/c_api_internal.h"
#include "tensorflow/lite/interpreter.h"

EXTERNC TfLiteStatus TfLiteInterpreterModifyGraphWithDelegate(const TfLiteInterpreter* interpreter, TfLiteDelegate* delegate);

#undef EXTERNC

/* C++ functions */
#include <iostream>

TfLiteStatus TfLiteInterpreterModifyGraphWithDelegate(
    const TfLiteInterpreter* interpreter, TfLiteDelegate* delegate) {
    return interpreter->impl->ModifyGraphWithDelegate(delegate);
}
