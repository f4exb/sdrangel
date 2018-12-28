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
    if request.method == 'POST':
        print("Start device %d" % deviceset_index)
        reply = { "state": "idle" }
        return jsonify(reply)
    elif request.method == 'DELETE':
        print("Stop device %d" % deviceset_index)
        reply = { "state": "running" }
        return jsonify(reply)
    elif request.method == 'GET':
        return "RUN device %d" % deviceset_index


@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/settings', methods=['GET', 'PATCH', 'PUT'])
def device_settings(deviceset_index):
    ''' Reply with a copy of the device setting structure received '''
    content = request.get_json(silent=True)
    if request.method == 'PATCH':
        print("Partial update of device %d" % deviceset_index)
        return jsonify(content)
    if request.method == 'PUT':
        print("Full update of device %d" % deviceset_index)
        return jsonify(content)
    if request.method == 'GET':
        return 'GET settings for device %d' % deviceset_index


@app.route('/sdrangel/deviceset/<int:deviceset_index>/channel/<int:channel_index>/settings', methods=['GET', 'PATCH', 'PUT'])
def channel_settings(deviceset_index, channel_index):
    ''' Reply with a copy of the channel setting structure received '''
    content = request.get_json(silent=True)
    if request.method == 'PATCH':
        print("Partial update of device %d channel %d" % (deviceset_index, channel_index))
        return jsonify(content)
    if request.method == 'PUT':
        print("Full update of device %d channel %d" % (deviceset_index, channel_index))
        return jsonify(content)
    if request.method == 'GET':
        return 'GET settings for device %d and channel %d' % (deviceset_index, channel_index)
