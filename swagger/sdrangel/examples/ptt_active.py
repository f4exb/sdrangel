#!/usr/bin/env python3
''' Active PTT
  Handles the switchover between two arbitrary device sets
    - Both device sets should have the reverse API feature set with the address and port of this server
    - Once in place and you have started one of the devices you should only stop one or the other never start
      There are two reasons for this:
        - This module reacts on an action already taken so if you start Tx then the Rx is not stopped immediately
          and damage to the Rx could occur. If you start with a stop action you cannot get in this situation.
        - For half duplex devices (only the HackRF) it will lock Tx or Rx. You can always recover the situation
          by stopping the running side.
    - There is no assumption on the Rx or Tx nature you may as well switchover 2 Rx or 2 Tx
    - Both devices have not to belong to the same physical device necessarily. You could mix a RTL-SDR Rx and a
      HackRF Tx for example
'''
import requests
import time
import argparse
from flask import Flask
from flask import request, jsonify

SDRANGEL_API_PORT = 8091
START_COUNT = 0
STOP_COUNT = 0
OTHER_DICT = {}
RUNNING = set()
DELAY = 1

app = Flask(__name__)

# ======================================================================
def start_device(device_index, sdrangel_ip, sdrangel_port):
    """ Start the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_run_url = base_url + f'/deviceset/{device_index}/device/run'
    r = requests.get(url=dev_run_url)
    if r.status_code / 100 == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "idle":
                r = requests.post(url=dev_run_url)
                if r.status_code == 200:
                    print(f'start_device: Device {device_index} started')
                else:
                    print(f'start_device: Error starting device {device_index}')
            else:
                print(f'start_device: Device {device_index} not in idle state')
        else:
            print(f'start_device: Cannot get device {device_index} running state')
    else:
        print(f'start_device: Error getting device {device_index} running state')

# ======================================================================
def stop_device(device_index, sdrangel_ip, sdrangel_port):
    """ Stop the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_run_url = base_url + f'/deviceset/{device_index}/device/run'
    r = requests.get(url=dev_run_url)
    if r.status_code / 100 == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "running":
                r = requests.delete(url=dev_run_url)
                if r.status_code == 200:
                    print(f'stop_device: Device {device_index} stopped')
                else:
                    print(f'stop_device: Error stopping device {device_index}')
            else:
                print(f'stop_device: Device {device_index} not in running state')
        else:
            print(f'stop_device: Cannot get device {device_index} running state')
    else:
        print(f'stop_device: Error getting device {device_index} running state')

# ======================================================================
def set_focus(device_index, sdrangel_ip, sdrangel_port):
    """ Set focus on the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_focus_url = base_url + f'/deviceset/{device_index}/focus'
    r = requests.patch(url=dev_focus_url)
    if r.status_code / 100 == 2:
        print(f'set_focus: Focus set on device set {device_index}')
    elif r.status_code == 400:
        print(f'set_focus: Focus on device set is not supported in a server instance')
    else:
        print(f'set_focus: Error setting focus on device set {device_index}')

# ======================================================================
def get_sdrangel_ip(request):
    """ Extract originator address from request """
# ----------------------------------------------------------------------
    if request.environ.get('HTTP_X_FORWARDED_FOR') is None:
        return request.environ['REMOTE_ADDR']
    else:
        return request.environ['HTTP_X_FORWARDED_FOR']

# ======================================================================
@app.route('/sdrangel')
def hello_sdrangel():
    """ Just to test if it works """
# ----------------------------------------------------------------------
    return 'Hello, SDRangel!'

# ======================================================================
@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/run', methods=['GET', 'POST', 'DELETE'])
def device_run(deviceset_index):
    ''' Reply with the expected reply of a working device '''
# ----------------------------------------------------------------------
    originator_index = None
    tx = None
    content = request.get_json(silent=True)
    if content:
        originator_index = content.get('originatorIndex')
        tx = content.get('tx')
    if originator_index is None or tx is None:
        print('device_run: SDRangel reverse API v4.5.2 or higher required. No or invalid originator information')
        return ""
    sdrangel_ip = get_sdrangel_ip(request)
    other_device_index = OTHER_DICT.get(originator_index)
    if other_device_index is None:
        print('device_run: Device {originator_index} is not part of the linked pair. Aborting request.')
        return ""
    global RUNNING
    print(f'device_run: Device: {originator_index} Other device: {other_device_index} Running: {RUNNING}')
    if request.method == 'POST':
        print(f'device_run: Device {originator_index} (tx={tx}) has started at {sdrangel_ip}:{SDRANGEL_API_PORT}')
        if originator_index not in RUNNING:
            RUNNING.add(originator_index)
        if other_device_index in RUNNING:
            time.sleep(DELAY)
            stop_device(other_device_index, sdrangel_ip, SDRANGEL_API_PORT)
            RUNNING.remove(other_device_index)
        else:
            print(f'device_run: Device {other_device_index} is stopped already')
        reply = { "state": "idle" }
        return jsonify(reply)
    elif request.method == 'DELETE':
        print(f'device_run: Device {originator_index} (tx={tx}) has stopped at {sdrangel_ip}:{SDRANGEL_API_PORT}')
        if originator_index in RUNNING:
            RUNNING.remove(originator_index)
        if other_device_index not in RUNNING:
            time.sleep(DELAY)
            start_device(other_device_index, sdrangel_ip, SDRANGEL_API_PORT)
            set_focus(other_device_index, sdrangel_ip, SDRANGEL_API_PORT)
            RUNNING.add(other_device_index)
        else:
            print(f'device_run: Device {other_device_index} is running already')
        reply = { "state": "running" }
        return jsonify(reply)
    elif request.method == 'GET':
        return f'RUN device {deviceset_index}'

# ======================================================================
def getInputOptions():
    """ This is the argument line parser """
# ----------------------------------------------------------------------
    parser = argparse.ArgumentParser(description="Manages PTT from an SDRangel instance automatically")
    parser.add_argument("-A", "--address", dest="addr", help="listening address", metavar="IP", type=str)
    parser.add_argument("-P", "--port", dest="port", help="listening port", metavar="PORT", type=int)
    parser.add_argument("-p", "--port-sdr", dest="sdrangel_port", help="SDRangel REST API port", metavar="PORT", type=int)
    parser.add_argument("-l", "--link", dest="linked_devices", help="pair of indexes of devices to link", metavar="LIST", type=int, nargs=2)
    parser.add_argument("-d", "--delay", dest="delay", help="switchover delay in seconds", metavar="SECONDS", type=int)
    options = parser.parse_args()

    if options.addr == None:
        options.addr = "0.0.0.0"
    if options.port == None:
        options.port = 8000
    if options.sdrangel_port == None:
        options.sdrangel_port = 8091
    if options.linked_devices == None:
        options.linked_devices = [0, 1]
    if options.delay == None:
        options.delay = 1

    other_dict = {
        options.linked_devices[0]: options.linked_devices[1],
        options.linked_devices[1]: options.linked_devices[0]
    }

    return options.addr, options.port, options.sdrangel_port, options.delay, other_dict

# ======================================================================
def main():
    """ This is the main routine """
# ----------------------------------------------------------------------
    global SDRANGEL_API_PORT
    global OTHER_DICT
    global DELAY
    addr, port, SDRANGEL_API_PORT, DELAY, OTHER_DICT = getInputOptions()
    print(f'main: starting: SDRangel port: {SDRANGEL_API_PORT} links: {OTHER_DICT}')
    app.run(debug=True, host=addr, port=port)


# ======================================================================
if __name__ == "__main__":
    """ When called from command line... """
# ----------------------------------------------------------------------
    main()

