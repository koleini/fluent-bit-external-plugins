/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2019-2022 Masoud Koleini
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef FLB_FILTER_TF_H
#define FLB_FILTER_TF_H

struct out_ordering_buffer {
    void *ordered_output;
    int *ordered_output_idx;
};

struct flb_tensorflow {
    TfLiteModel* model;
    TfLiteInterpreterOptions* interpreter_options;
    TfLiteInterpreter* interpreter;
    TfLiteDelegate* delegate;
    flb_sds_t input_field;
    TfLiteType input_tensor_type;
    TfLiteType output_tensor_type;

    /* IO buffer */
    void* input;
    void* output;
    int input_tensor_size;
    int input_byte_size;
    int output_tensor_size;
    int output_byte_size;
    int device;

    /* feature scaling/normalization */
    bool include_input_fields;
    float* normalization_value;

    /* output format */
    int output_size;
    struct out_ordering_buffer out_ordering_buffer;

    struct flb_filter_instance *ins;
};

void max_output_ordering_float(struct flb_tensorflow *ctx)
{
    int f_idx;
    int* ordered_output_idx;
    float* ordered_output;

    f_idx = 0;
    ordered_output = (float *) ctx->out_ordering_buffer.ordered_output;
    ordered_output_idx = ctx->out_ordering_buffer.ordered_output_idx;

    ordered_output[0] = ((float*) ctx->output)[0];
    ordered_output_idx[0] = 0;

    for (int i = 1; i < ctx->output_tensor_size; i++) {
        float value = ((float*) ctx->output)[i];

        if (f_idx == ctx->output_size - 1 && value <= ordered_output[f_idx]) {
            continue;
        }

        for (int j = 0; j <= f_idx; j++) {
            if (value > ordered_output[j]) {
                if (j < ctx->output_size - 1) {
                    for (int shift_idx = f_idx + 1; shift_idx > j; shift_idx--) {
                        ordered_output[shift_idx] = ordered_output[shift_idx - 1];
                        ordered_output_idx[shift_idx] = ordered_output_idx[shift_idx - 1];
                    }

                    if (f_idx < ctx->output_size - 1) {
                        f_idx++;
                    }
                }
                ordered_output[j] = value;
                ordered_output_idx[j] = i;
                break;
            }
        }
    }
}

#endif
