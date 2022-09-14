set(src
  tensorflow.c
  )

include_directories("${TENSORFLOW_SOURCE}"
                    "${ARES_BUILD}"
                    "${FLB_SOURCE}/build/lib/monkey/include/monkey"
                    "${TENSORFLOW_SOURCE}/bazel-tensorflow/external/flatbuffers/include")

# assuming that TensorFlow dynamic libraries are moved to /usr/lib
link_directories(/usr/lib
                 /usr/lib/aarch64-linux-gnu)

FLB_PLUGIN("${PLUGIN_NAME}" "${src}" "-ltensorflowlite_c -ltensorflowlite_gpu_delegate -lGL -lEGL")

add_library(gpu STATIC gpu.cpp)
set_property(TARGET gpu PROPERTY POSITION_INDEPENDENT_CODE ON)
target_link_libraries(flb-${PLUGIN_NAME} gpu)
