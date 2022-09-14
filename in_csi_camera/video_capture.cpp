/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2022 Masoud Koleini
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

#ifdef __cplusplus
#define EXTERNC extern "C"
#else
#define EXTERNC
#endif

#include <iostream>

#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/core/types.hpp>

#include "csi_camera.h"

EXTERNC int create_video_stream(struct flb_csi_camera *);

EXTERNC int capture_video_frame(struct flb_csi_camera *);
EXTERNC const char *video_frame_ptr();

EXTERNC void release_video_capture_device();

#undef EXTERNC

cv::VideoCapture capture;
cv::Mat img;

std::string gstreamer_pipeline (int sensor_id, int capture_width, int capture_height, int display_width, int display_height, int framerate, int flip_method) {
    return "nvarguscamerasrc sensor_id=" + std::to_string(sensor_id) + " ! video/x-raw(memory:NVMM), width=(int)" + std::to_string(capture_width) + ", height=(int)" +
           std::to_string(capture_height) + ", format=(string)NV12, framerate=(fraction)" + std::to_string(framerate) +
           "/1 ! nvvidconv flip-method=" + std::to_string(flip_method) + " ! video/x-raw, width=(int)" + std::to_string(display_width) + ", height=(int)" +
           std::to_string(display_height) + ", format=(string)BGRx ! videoconvert ! video/x-raw, format=(string)BGR ! appsink drop=true";
}

int create_video_stream(struct flb_csi_camera *ctx)
{
    std::string pipeline = gstreamer_pipeline(ctx->sensor_id,
                                              ctx->capture_width,
                                              ctx->capture_height,
                                              ctx->capture_width,
                                              ctx->capture_height,
                                              ctx->framerate,
                                              ctx->flip_method);

    /* https://docs.opencv.org/3.4/d8/dfe/classcv_1_1VideoCapture.html#a57c0e81e83e60f36c83027dc2a188e80 */
    capture = cv::VideoCapture(pipeline, cv::CAP_GSTREAMER);

    /*
     *  Note: 3+ second delay in frames is happening. It was even more until the followings are set
     *   1. cv::CAP_PROP_BUFFERSIZE -> 1
     *   2. drop=true in gstreamer_pipeline
     * can we make it better (always read the last frame?)
     *    https://stackoverflow.com/questions/30032063/opencv-videocapture-lag-due-to-the-capture-buffer
     */

     capture.set(cv::CAP_PROP_BUFFERSIZE, 1);

    if(!capture.isOpened()) {
        std::cout << "Failed to open CSI camera!" << std::endl;
        return -1;
    }

    return 0;
}

int capture_video_frame(struct flb_csi_camera *ctx)
{
    /* capture the next frame only when the previous one is processed */
    if (!ctx->rw_handle.frame_processed) {
      return -1;
    }

    /* capture the latest frame. Don't overlap with plugin's frame processing */
    pthread_mutex_lock(&ctx->rw_handle.f_mutex);

    /*
     * unlock the mutex in the case of error when reading a frame
     * cv::VideoCapture::read is blocking (not an async function) and has to run
     * in a separate thread.
     */
    if (!capture.read(img)) {
        pthread_mutex_unlock(&ctx->rw_handle.f_mutex);
		    return -1;
	  }

    /* frame_processed is unset when the frame is captured. Will be set by the
     *  plugin after is is packed.
     */
    ctx->rw_handle.frame_processed = false;

    pthread_mutex_unlock(&ctx->rw_handle.f_mutex);

    return 0;
}

const char *video_frame_ptr()
{
    return (const char *) img.ptr<char>(0, 0, 0);
}

void release_video_capture_device()
{
    capture.release();
}
