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

#ifndef FLB_INPUT_CSI_H
#define FLB_INPUT_CSI_H

/* handling frame capture/storage and processing */
struct rw_handling {
    pthread_mutex_t f_mutex;
    /* plugin sets this flag after processing the current thread */
    int frame_processed;
};

struct flb_csi_camera {
    int sensor_id;
    int capture_width;
    int capture_height;
    int framerate;
    int flip_method;

    int frame_size;

    const char *frame; /* C++ object */

    /*
     * if previous frame is not processed or a new frame couldn't captured,
     *  sleep the thread for this time (0.4 of the plugin collection time)
     */
    int frame_capture_sleep_us;
    /* tells frame capture thread to exit (set when plugin is exiting) */
    int plugin_exit_called;

    int coll_fd;

    struct rw_handling rw_handle;

    struct flb_input_instance *ins;
};

#endif
