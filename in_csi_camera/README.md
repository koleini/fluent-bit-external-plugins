# Fluent Bit Plugin for Jetson Nano MIPI-CSI camera

[NVIDIA Jetson Nano](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-nano/) developer boards come with one or two CSI camera slots. The CSI camera plugin allows receiving video frames from a CSI cameras attached to one of the slots.

## Requirements

- [Fluent Bit Source code](https://fluentbit.io)
- C compiler: GCC or Clang
- CMake3

## Getting Started

Provide the following variables for the CMake:

- `FLB_SOURCE`: absolute path to source code of Fluent Bit.
- `PLUGIN_NAME`: `in_csi_camera`

Run the following in order to create CSI Camera plugin shared library:

```bash
$ cmake -DCMAKE_CXX_FLAGS="-std=c++11" -DFLB_SOURCE=$FLUENTBIT_DIR -DPLUGIN_NAME=$PLUGIN_NAME -DARES_BUILD=$(find $FLUENTBIT_DIR/build/ -name ares_build.h | xargs dirname) .
$ make
```

## How to use CSI camera plugin

The following is the configuration parameter for the plugin:

```
[INPUT]
    Name           csi_camera
    capture_width  <INTEGERE_VALUE>
    capture_height <INTEGERE_VALUE>
    framerate      <INTEGERE_VALUE>
    flip_method    <INTEGERE_VALUE>   # default: 0
    socket_id      0 | 1
```
