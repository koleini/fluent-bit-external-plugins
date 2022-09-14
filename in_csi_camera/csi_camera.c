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

#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_input_plugin.h>
#include <fluent-bit/flb_config.h>
#include <fluent-bit/flb_pack.h>

#include "csi_camera.h"
#include "video_capture.h"


pthread_t camera_capture_thread;

/* read camera frames is a separate thread (calls are clocking) */
void *capture_frame_from_camera(void *in_context)
{
    struct flb_csi_camera *ctx = in_context;

    while(true) {
        if (capture_video_frame(ctx) == 0) {
            ctx->frame = video_frame_ptr();
        } /* TODO: error returned */
        else {
            usleep(ctx->frame_capture_sleep_us);
        }

        if (ctx->plugin_exit_called) {
            pthread_exit(NULL);
        }

   /* what to do if capture had a problem? */
   }
}

static int cb_csi_camera_collect(struct flb_input_instance *ins,
                                 struct flb_config *config, void *in_context)
{
    struct flb_csi_camera *ctx = in_context;
    msgpack_packer mp_pck;
    msgpack_sbuffer mp_sbuf;

    ctx = (struct flb_csi_camera *) in_context;

    /*
     * ctx->frame is null when plugin initializes, and points to the frame when the
     *   first frame is captured.
     *
     * ctx->rw_handle.frame_processed is set when the cur"rent frame is processed by the plugin
     */
    if (!ctx->rw_handle.frame_processed) {

        pthread_mutex_lock(&ctx->rw_handle.f_mutex);

        msgpack_sbuffer_init(&mp_sbuf);
        msgpack_packer_init(&mp_pck, &mp_sbuf, msgpack_sbuffer_write);

        /*
         * store the new data into the MessagePack buffer,
         */

        /* packing a messagepack record starts with an array of size 2:
         *  1 - timestamp
         *  2 - record data
         */
        msgpack_pack_array(&mp_pck, 2);
        flb_pack_time_now(&mp_pck);
        /*
         * TODO: do we need to
         *   1. memcpy, or msgpack_pack_bin_body (inside msgpack_pack_bin_with_body) does it automatically?
         *   2. check if the frame is new, or not updated yet?
         *    - Mutex is required so the frame gets locked until the data is packed.
         */

        msgpack_pack_map(&mp_pck, 1);
        msgpack_pack_str_with_body(&mp_pck, "frame", 5);

        /*
         * it is possible to pack the data into an array (of chars), which is still
         * space-efficient. However, the char-packing loop is very time consuming
         * and it doesn't simply copy the memory block, similar to msgpack_pack_bin_body
         *
         * msgpack_pack_array(&mp_pck, ctx->frame_size);
         * for (int i = 0; i < ctx->frame_size; i++) {
         *    msgpack_pack_char(&mp_pck, (char) ctx->frame[i]);
         * }
         */
        msgpack_pack_bin(&mp_pck, ctx->frame_size);
        msgpack_pack_bin_body(&mp_pck, (const void*) ctx->frame, ctx->frame_size);

        /* add msgpack buffer to the input data chunk */
        flb_input_chunk_append_raw(ins, NULL, 0, mp_sbuf.data, mp_sbuf.size);
        msgpack_sbuffer_destroy(&mp_sbuf);

        /* frame is processed (used by the video capture script) */
        ctx->rw_handle.frame_processed = true;

        pthread_mutex_unlock(&ctx->rw_handle.f_mutex);
    }

    return 0;
}

static int cb_csi_camera_init(struct flb_input_instance *in,
                              struct flb_config *config,
                              void *data)
{
    int ret;
    time_t seconds;
    long nanoseconds;
    struct flb_csi_camera *ctx;

    ctx = flb_calloc(1, sizeof(struct flb_csi_camera));

    if (!ctx) {
      flb_errno();
      return -1;
    }

    ctx->ins = in;

    ret = flb_input_config_map_set(in, (void *) ctx);
    if (ret == -1) {
        flb_free(ctx);
        return -1;
    }

    /* validate input parameters */
    if (ctx->capture_width <= 0 || ctx->capture_height <= 0) {
        flb_plg_error(ctx->ins, "Configuration error: check if image capture sizes are valid!");
        return -1;
    }

    if (!ctx->framerate) {
        flb_plg_error(ctx->ins, "Configuration error: framerate has to be an ineteger >= 1!");
        return -1;
    }

    /* pre-calculate frame size */
    ctx->frame_size = ctx->capture_width * ctx->capture_height * 3;

    /*
     * C++ API (video_capture.cpp)
     * TODO:
     * 1. check if capture.read(img) is blocking. If ti is, it will possibly block
     *     the event loop.
     *   - It is blocking and so far (OpenCV 4.1.1) doesn't provide async functionality.
     *       Therefore, we need to implement a multi-threading
     * 2. Is there any event for recieving a frame that we can add to the event loop?
     *   - No
     */
    if (create_video_stream(ctx) != 0) {
      flb_errno();
      return -1;
    }

    /* C API
        Currently, there is no API documentation for OPenCV >= 4. I used the header
        files + v3.4.17 documentation to use the C interfaces.

        https://docs.opencv.org/3.4.17/dd/d01/group__videoio__c.html

        However, when writing the code using C API, I faced C++ dependency error
        for OpenCV 4.x that couldn't fix. Looks like OpenCV 4.x is deprecating C API
        (and that is possibly why there C API documentation pages for OpenCV 4.x are empty).


        error:
        In file included from /usr/include/opencv4/opencv2/core/types_c.h:82:0,
                 from /usr/include/opencv4/opencv2/core/core_c.h:48,
                 from /usr/include/opencv4/opencv2/videoio/videoio_c.h:45,
                 from simple_camera.c:9:
/usr/include/opencv4/opencv2/core/cvdef.h:690:4: error: #error "OpenCV 4.x+ requires enabled C++11 support"
 #  error "OpenCV 4.x+ requires enabled C++11 support"
    ^~~~~
/usr/include/opencv4/opencv2/core/cvdef.h:695:10: fatal error: array: No such file or directory
 #include <array>
          ^~~~~~~
compilation terminated.
    */

    pthread_mutex_init(&ctx->rw_handle.f_mutex, NULL);
    ctx->rw_handle.frame_processed = true;

    /* Set the context
           this is necessary for the plugin to exit properly
           https://github.com/fluent/fluent-bit/blob/v1.8.11/src/flb_input.c#L641
    */
    flb_input_set_context(in, ctx);

    ctx->plugin_exit_called = 0;

    seconds = 0;
    nanoseconds = 1000000000 / ctx->framerate;
    /* framerate is >= 1 */
    if (nanoseconds == 1000000000) {
         nanoseconds = 0;
         seconds = 1;
    }

    ctx->frame_capture_sleep_us = 0.4 * (nanoseconds / 1000);
    /* frame reading thread */
    int t = pthread_create(&camera_capture_thread, NULL, capture_frame_from_camera, ctx);

    if (t != 0) {
        flb_plg_error(ctx->ins, "Error creating the thread!");
        return -1;
    }

    /* set our collector based on time, CPU usage every 1 second */
    ret = flb_input_set_collector_time(in,
                                       cb_csi_camera_collect,
                                       seconds,
                                       nanoseconds,
                                       config);

    if (ret == -1) {
        flb_plg_error(ctx->ins, "could not set collector for CIS camera input plugin");
        return -1;
    }

    ctx->coll_fd = ret;

    return 0;
}

static void cb_csi_camera_pause(void *data, struct flb_config *config)
{
    struct flb_csi_camera *ctx = data;

    flb_input_collector_pause(ctx->coll_fd, ctx->ins);
}

static void cb_csi_camera_resume(void *data, struct flb_config *config)
{
    struct flb_csi_camera *ctx = data;
    flb_input_collector_resume(ctx->coll_fd, ctx->ins);
}

static int cb_csi_camera_exit(void *data, struct flb_config *config)
{
    int t;
    struct flb_csi_camera *ctx = data;

    /* informing the frame capture thread to exit using plugin_exit variable */
    ctx->plugin_exit_called = 1;

    /* frame-capture thread maybe waiting on the mutex */
    ctx->rw_handle.frame_processed = true;

    /* release capture device */
    release_video_capture_device();

    void* status;

    /* this is to wait for the capture thread to exit, before exiting the plugin */
    t = pthread_join(camera_capture_thread, &status);
    if (t != 0)
    {
        flb_plg_error(ctx->ins, "Error in image capture thread handling!");
    }

    flb_free(ctx);

    flb_plg_info(ctx->ins, "CSI camera plugin exited");

    return 0;
}

/* list of config_maps */
static struct flb_config_map config_map[] = {
    {
      /*
          https://github.com/fluent/fluent-bit/blob/v1.9.4/include/fluent-bit/flb_config_map.h#L65-L77
          clarification:
              4. flags: it can be used to show specific options set, for instance, nultiple properties
                  allowed: https://github.com/fluent/fluent-bit/blob/master/src/flb_config_map.c#L183
              5. set_property: if set, some extra work (like setting default value) will be done when
                  creating the config_map:
                  https://github.com/fluent/fluent-bit/blob/v1.9.4/src/flb_config_map.c#L323-L349
              6. offset: which filed in plugin context to set the (default) value.

            Q: When does flb_config_map_create is called, and when flb_config_map_set is called?
            Q: What is exactly the FLB_CONFIG_MAP_MULT?
       */
       FLB_CONFIG_MAP_INT, "socket_id", 0,
       0, FLB_TRUE, offsetof(struct flb_csi_camera, sensor_id),
       "Socket Id of the CSI camera",
    },
    {
       FLB_CONFIG_MAP_INT, "capture_width", NULL,
       0, FLB_TRUE, offsetof(struct flb_csi_camera, capture_width),
       "Capture width of the CSI camera (pixels)",
    },
    {
        FLB_CONFIG_MAP_INT, "capture_height", NULL,
        0, FLB_TRUE, offsetof(struct flb_csi_camera, capture_height),
        "Capture height of the CSI camera (pixels)",
    },
    {
        /* some experiments show CSI camera minimum frame rate is 2(?) */
        FLB_CONFIG_MAP_INT, "framerate", NULL,
        0, FLB_TRUE, offsetof(struct flb_csi_camera, framerate),
        "Frame rate",
    },
    {
        FLB_CONFIG_MAP_INT, "flip_method", 0,
        0, FLB_TRUE, offsetof(struct flb_csi_camera, flip_method),
        "Flip method",
    },
    /* EOF
     *  https://github.com/fluent/fluent-bit/blob/v1.9.4/src/flb_config_map.c#L276
     */
    {0}
};

/* in_ prefix and _plugin are required in naming the struct */
struct flb_input_plugin in_csi_camera_plugin = {
    .name         = "csi_camera",
    .description  = "NVIDIA Jetson CSI camera frame reader",
    .cb_init      = cb_csi_camera_init,
    .cb_pre_run   = NULL,
    .cb_collect   = cb_csi_camera_collect,
    .cb_flush_buf = NULL,
    .cb_pause     = cb_csi_camera_pause,
    .cb_resume    = cb_csi_camera_resume,
    .cb_exit      = cb_csi_camera_exit,
    .config_map   = config_map
};
