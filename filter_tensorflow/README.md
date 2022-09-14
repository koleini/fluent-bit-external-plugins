# Fluent Bit TensorFlow Plugin

[TensorFlow Lite](https://www.tensorflow.org/lite/) is a lightweight open-source deep learning framework, mainly used for mobile and IoT applications. It is designed for inference (not training), hence, requires a pre-trained models converted into TensorFlow Lite model format. (`FlatBuffer`). You can read more on converting TensorFlow models [here](https://www.tensorflow.org/lite/convert)

## Requirements

- [Fluent Bit Source code](https://fluentbit.io) version >= 1.2
- [TensorFlow Source Code](https://github.com/tensorflow/tensorflow) version >= v2.0.0
- C compiler: GCC or Clang
- CMake3

## Getting Started

Build the TensorFlow Lite static library using the following command. (assuming that `$TENSORFLOW_SOURCE` holds the absolute location of the `tensorflow` directory, and you have already downloaded bazel binary into ):

```bash
$ cd $TENSORFLOW_SOURCE
$ bazel build -c opt //tensorflow/lite/c:tensorflowlite_c  # see https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/c
$ sudo cp bazel-bin/tensorflow/lite/c/libtensorflowlite_c.so /usr/lib/

$ sudo apt install -y libgles2-mesa-dev
$ bazel build -c opt tensorflow/lite/delegates/gpu:libtensorflowlite_gpu_delegate.so
$ sudo cp bazel-bin/tensorflow/lite/delegates/gpu/libtensorflowlite_gpu_delegate.so /usr/lib/
```

Provide the following variables for the CMake:

- `FLB_SOURCE`: absolute path to source code of Fluent Bit.
- `TENSORFLOW_SOURCE`: absolute path to source code of TensorFlow.
- `PLUGIN_NAME`: `filter_tensorflow`

Run the following in order to create TensorFlow plugin shared library:

```bash
$ cmake -DFLB_SOURCE=$FLB_SOURCE -DTENSORFLOW_SOURCE=$TENSORFLOW_SOURCE -DPLUGIN_NAME=$PLUGIN_NAME -DARES_BUILD=$(find $FLB_SOURCE/build/ -name ares_build.h | xargs dirname) .
$ make
```

Add the path to created __.so__ file to plugins configuration file (e.g. `conf/plugins.conf`) in order to be used by Fluent Bit (see [plugins configuration](https://github.com/fluent/fluent-bit/blob/master/conf/plugins.conf)).


## How to use TensorFlow plugin

It is possible to pass records coming from any input through TensorFlow filter plugin and apply inference on a specific field. The following is the configuration parameters:

```
[FILTER]
    Name                  tensorflow
    Match                 <INPUT_TAG>               # input tag to match (e.g. mqtt.data)
    input_field           <INPUT_FIELD_NAME>        # record key that contains data for inference
    model_file            <ADDRESS_OF_MODEL_FILE>   # full address of the .tflite file (model)
    include_input_fields  false | true              # if to contain input data in output record
    normalization_value   <INTEGERE_VALUE>          # normalization value
    device                cpu | gpu                 # inference device
    output_size           <INTEGERE_VALUE>          # number of tensor outputs to be includes in plugin's output
```

## Image classification demo

### Limitations

- This demo uses MQTT plugin to receive the images of size 224 X 224 X 3 pixels.
However, the Fluent Bit's MQTT plugin (version `1.5.2` or older) can handle data of the size [1024 bytes](https://github.com/fluent/fluent-bit/blob/v1.5.2/plugins/in_mqtt/mqtt_conn.h#L40). You need to build Fluent Bit with modified MQTT data size to handle larger data (e.g 1024 * 1024).

## Demo 1: cats vs dogs

The directory `image_classification` contains the following files in order to show how to use TensorFlow plugin:
- model file `cat_vs_dog.tflite`. This is a pre-trained model for classifying cats vs dogs
- cat/dog photos inside `pics` folder
- Python code `send_image_to_mqtt.py` that sends cat/dog pictures to the MQTT server running on Fluent Bit
- REST API server `http_server.py` which receives images and inferences in MessagePack format and displays the image.

Create a Fluent Bit configuration file like this:
```
[SERVICE]
    Flush        1
    Daemon       Off
    Log_Level    info
    Plugins_File plugins.conf

[INPUT]
    Name  mqtt
    Tag   mqtt.data

[FILTER]
    Name                  tensorflow
    input_field           frame
    model_file            /path/to/cat_vs_dog.tflite
    include_input_fields  false
    normalization_value   255
    device                cpu
    Match                 mqtt.data

[OUTPUT]
    Name stdout
    Match *
```

And add the address of TensorFlow plugin library to `plugins.conf`:
```
[PLUGINS]
    Path /path/to/flb-filter_tensorflow.so
```
Run Fluent Bit with the configuration file:
```bash
build/bin/fluent-bit -c /path/to/config.conf
```
If TensorFlow plugin initializes correctly, it reports successful creation of the interpreter and prints a summary of the model's input/output types and dimensions:
```
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] TensorFlow Lite interpreter created!
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0]  ===== input #1 =====
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] type: FLOAT32 dimensions: {1, 224, 224, 3}
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0]  ===== output #1 ====
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] type: FLOAT32 dimensions: {1, 2}
```
For instance, the cat-vs-dog model accepts input of float type and size 224 X 224 X 3 (image size).

Now, run `python image_classification/send_image_to_mqtt.py` to send images to Fluent Bit using . This script
converts images into a JSON object as the value of key `frame` (see `input_field` in plugin configuration file),
and publishes it into the MQTT running on the local machine. If you are sending an image of a cat, you should see
similar output in Fluent Bit logs:
```
[0] mqtt.data: [1663120410.110665816, {"inference_time"=>0.098639, "output"=>[0.875000, 0.125000]}]
```
where `inference_time` is the time spent on inference, and `output` shows the probabilities of the image
being cat (`0.875000` for this example), or being dog (` 0.125000`).

## Demo2: detecting dog images using MobileNetV3

This demo runs MobileNetV3 inference on the images coming from MQTT plugin, filters out dog images, and sends them to an HTTP server to display.

### output_size parameter

In the absence of `output_size` parameter, the inference output is a list of the values of the output tensor(s). For the models with a large output size like MobileNet V3, only the ones with the highest values may be relevant. Setting the `output_size` parameter makes the plugin to only send the `output_size` number of the highest values and their corresponding indexes to the output. for instance, if the model is MobileNetV3 and `output_size` is set to 3, the output will be similar to the following:

```
[0] mqtt.data: [1660580701.706285573, {"inference_time"=>0.059373, "output"=>
                                                       {"1"=>{"idx"=>282, "value"=>8.413498},
                                                        "2"=>{"idx"=>286, "value"=>7.756614},
                                                        "3"=>{"idx"=>283, "value"=>7.698653} }}]
```

The output format helps users to apply further processing through Stream Processor.
For instance, it is possible to filter out only dog images by passing the output to the following stream
processing rule. The rules checks if one of the 3 top classification values reside in one of the categories of the dog breeds.
(you need to set `include_input_fields` to `true` in order to keep the `frame` filed in the output of the filter):

```
[STREAM_TASK]
    Name dogs
    Exec CREATE STREAM dogs AS SELECT frame FROM TAG:'mqtt.data' WHERE
                                                    (output['1']['idx'] >= 152 AND output['1']['idx'] <= 269) OR
                                                    (output['2']['idx'] >= 152 AND output['2']['idx'] <= 269) OR
                                                    (output['3']['idx'] >= 152 AND output['3']['idx'] <= 269) ;
```

### MobileNet V3 model for TensorFlow Lite

Download MobileNet V3 trained model from [TensorFlow Hub](https://tfhub.dev/s?deployment-format=lite&tf-version=tf2).

Create a Fluent Bit configuration file like this:
```
[SERVICE]
    Flush         1
    Daemon        Off
    Log_Level     info
    Plugins_File  plugins.conf
    Streams_file  stream.conf

[INPUT]
    Name  mqtt
    Tag   mqtt.data

[FILTER]
    Name                  tensorflow
    input_field           frame
    model_file            /path/to/mobilenet.tflite
    include_input_fields  true
    normalization_value   255
    output_size           3
    device                cpu
    Match                 mqtt.data

[OUTPUT]
    Name    http
    host    127.0.0.1
    port    5000
    format  msgpack
    Match   dog
```

Update the `model_file` value in the configuration file with the path to the downloaded MobileNet V3 tflite model.
In addition, add the stream processing task in the previous section the `conf/stream.conf`.

The HTTP plugin sends the result in MessagePack format to save the bandwidth.
Run `python image_classification/http_server.py` for a REST API server that receives the data in MessagePack format and displays the image.

Run Fluent Bit with the configuration file:
```bash
build/bin/fluent-bit -c /path/to/config.conf
```

You should see the TensorFlow plugin starts properly with the followings appear on the stdout:

```
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] TensorFlow Lite interpreter created!
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0]  ===== input #1 =====
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] type: FLOAT32 dimensions: {1, 224, 224, 3}
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0]  ===== output #1 ====
[2022/09/11 20:50:00] [ info] [filter:tensorflow:tensorflow.0] type: FLOAT32 dimensions: {1, 1001}
```

Now, send the images using `image_classification/send_image_to_mqtt.py` and makes sure only dog images are displayed on your screen.
