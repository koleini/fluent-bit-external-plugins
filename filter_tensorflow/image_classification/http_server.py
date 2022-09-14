#!/usr/bin/python3
# encoding: utf-8

import json
import os
import cv2
import time
import threading
import numpy as np
import msgpack
import argparse

from flask import Flask, request, jsonify

window_name = 'image'
image = None
new_image = False
output_size = 5

dir = os.path.abspath(os.path.dirname( __file__ ))

def rest_api(port):

    app = Flask(__name__)

    @app.route('/')
    def index():
        return jsonify({'name': 'FluentBit',
                        'email': 'FluentBit@fluentbit'})

    @app.route('/', methods=['POST'])
    def show_image():
        global image
        global new_image

        image = msgpack.unpackb(request.data)[1]['frame']

        preds = msgpack.unpackb(request.data)[1]['output']
        # preds could be of two formats: plain or ordered.
        #   plain is an array of values representing all the output tensors' values
        #   ordered is a map of the format { '1': {'idx': <INDEX>, 'value': <VALUE>}, ...}
        print(json.dumps(preds, indent=4))

        with open(os.path.join(dir, "imagenet_class_index.json"), 'r') as fp:
            # potential mismatch. Element 0 in the JSON file doesn't match background (element 0 of the predictions)
            imagenet_json = json.load(fp)

        # TODO: check the type of the output to detect the format
        for order in preds:
            if preds[order]['idx'] == 0:
                print('background', end=" ")
            else:
                print(imagenet_json[str(preds[order]['idx'] - 1)], end=" ")

        print()
        new_image = True
        return jsonify({})

    app.run(host="0.0.0.0", port=port)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--port', default=5000, help='The port API server is listening on.')
    parser.add_argument('--wx', type=int, default=0, help='Window position on x axis.')
    parser.add_argument('--wy', type=int, default=0, help='Window position on y axis.')
    args = parser.parse_args()

    t1 = threading.Thread(target=rest_api, args=(args.port,))
    # this is required for the keyboard interrupt to work
    t1.setDaemon(True)
    t1.start()

    # the following process has to be inside the main thread (to work on MacOS)
    cv2.namedWindow(window_name, cv2.WINDOW_NORMAL)
    cv2.moveWindow(window_name, args.wx, args.wy)

    try:
        while True:
            if new_image:
                if isinstance(image, list):
                    img = np.asarray(image).astype(np.uint8)
                else:
                    img = np.frombuffer(image, dtype=np.uint8)

                # reshape the image
                img_np = img.reshape(224, 224, 3)
                img_np = cv2.resize(img_np, (448, 448))

                # show the image
                cv2.imshow(window_name, img_np)

                keyCode = cv2.waitKey(30) & 0xFF
                if keyCode == 27:
                    break

                new_image = False

            time.sleep(0.1)

    except KeyboardInterrupt:
        cv2.destroyAllWindows()
        exit(0)
