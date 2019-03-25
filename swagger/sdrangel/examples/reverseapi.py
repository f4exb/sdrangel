#!/usr/bin/env python3
''' Test reverse API
Run with:
export FLASK_APP=reverseapi.py
export FLASK_ENV=development # for debug mode and auto reload
flask run
'''
from flask import Flask
from flask import request, jsonify
app = Flask(__name__)


@app.route('/sdrangel')
def hello_sdrangel():
    return 'Hello, SDRangel!'


@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/run', methods=['GET', 'POST', 'DELETE'])
def device_run(deviceset_index):
    ''' Reply with the expected reply of a working device '''
    originatorIndex = None
    content = request.get_json(silent=True)
    if content:
        originatorIndex = content.get('originatorIndex')
    if request.method == 'POST':
        print(f'Start device {deviceset_index} from device {originatorIndex}')
        reply = { "state": "idle" }
        return jsonify(reply)
    elif request.method == 'DELETE':
        print(f'Stop device {deviceset_index} from device {originatorIndex}')
        reply = { "state": "running" }
        return jsonify(reply)
    elif request.method == 'GET':
        return f'RUN device {deviceset_index}'


@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/settings', methods=['GET', 'PATCH', 'PUT'])
def device_settings(deviceset_index):
    ''' Reply with a copy of the device setting structure received '''
    content = request.get_json(silent=True)
    originatorIndex = content.get('originatorIndex')
    if request.method == 'PATCH':
        print(f'Partial update of device {deviceset_index} from device {originatorIndex}')
        return jsonify(content)
    if request.method == 'PUT':
        print(f'Full update of device {deviceset_index} from device {originatorIndex}')
        return jsonify(content)
    if request.method == 'GET':
        return f'GET settings for device {deviceset_index}'


@app.route('/sdrangel/deviceset/<int:deviceset_index>/channel/<int:channel_index>/settings', methods=['GET', 'PATCH', 'PUT'])
def channel_settings(deviceset_index, channel_index):
    ''' Reply with a copy of the channel setting structure received '''
    content = request.get_json(silent=True)
    originatorDeviceSetIndex = content.get('originatorDeviceSetIndex')
    originatorChannelIndex = content.get('originatorChannelIndex')
    if request.method == 'PATCH':
        print(f'Partial update of (device:channel) {deviceset_index}:{channel_index} from {originatorDeviceSetIndex}:{originatorChannelIndex}')
        return jsonify(content)
    if request.method == 'PUT':
        print(f'Full update of (device:channel) {deviceset_index}:{channel_index} from {originatorDeviceSetIndex}:{originatorChannelIndex}')
        return jsonify(content)
    if request.method == 'GET':
        return f'GET settings for (device:channel) {deviceset_index}:{channel_index}'
