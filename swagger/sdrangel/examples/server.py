from flask import Flask
from flask import request, jsonify
app = Flask(__name__)


@app.route('/sdrangel')
def hello_sdrangel():
    return 'Hello, SDRangel!'


@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/run', methods=['GET', 'POST', 'DELETE'])
def device_run(deviceset_index):
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
    content = request.get_json(silent=True)
    if request.method == 'PATCH':
        return jsonify(content)
    if request.method == 'PUT':
        return jsonify(content)
    if request.method == 'GET':
        return 'GET settings'
