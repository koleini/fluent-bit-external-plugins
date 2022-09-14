import numpy as np
import paho.mqtt.client as mqttClient
import json
import time
import os
import cv2

# a simple python script to publish cat/dog images through to an MQTT server

dir = os.path.abspath(os.path.dirname( __file__ ))

Connected = False

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("Connected to broker\n")
        global Connected  # Use global variable
        Connected = True  # Signal connection
    else:
        print("Connection failed\n")


images_dir = os.path.join(dir, 'pics')

# send image to MQTT server
address = "127.0.0.1"
port = 1883
topic = 'flb_tensorflow'

client = mqttClient.Client('flb_client')
client.on_connect = on_connect
client.connect(address, port=port)
client.loop_start()

while not Connected:  # Wait for connection
    time.sleep(0.1)

for img in os.listdir(images_dir):
    try:
        image = cv2.imread(os.path.join(images_dir, img))
        img_str = cv2.imencode('.jpg', image)[1].tobytes()

        result = {
            "frame": np.asarray(image).flatten().tolist()
        }

        result_json = json.dumps(result)

        input('Press any key to send image %s ...\n' % img)

        client.publish("fluentbit/" + topic, result_json)

    except KeyboardInterrupt:
        exit()
