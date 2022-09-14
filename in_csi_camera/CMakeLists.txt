set(src
  csi_camera.c
  )

include_directories("${ARES_BUILD}"
                    "/usr/lib/opencv"
                    "/usr/include/opencv4")

link_directories("/usr/lib"
                 "/usr/lib/aarch64-linux-gnu")

FLB_PLUGIN("${PLUGIN_NAME}" "${src}" "-lopencv_core -lopencv_highgui -lopencv_videoio")

add_library(video_capture STATIC video_capture.cpp)
set_property(TARGET video_capture PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(flb-${PLUGIN_NAME} video_capture)

target_link_libraries(flb-${PLUGIN_NAME} "-lopencv_core -lopencv_highgui -lopencv_videoio")
