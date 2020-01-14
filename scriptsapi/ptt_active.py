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
    - It can pilot a LimeRFE device via USB through SDRangel API (by giving TTY device with --limerfe-dev parameter)
      and with the assumption that the first device set index given in the --link parameter is the Rx and the second
      is the Tx
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
FREQ_SYNC = False
FREQ_HAS_CHANGED = False
LIMERFE_DEVICE = None
LIMERFE_RX = False
LIMERFE_TX = False

app = Flask(__name__)

# ======================================================================
def start_device(device_index, sdrangel_ip, sdrangel_port):
    """ Start the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_run_url = base_url + f'/deviceset/{device_index}/device/run'
    r = requests.get(url=dev_run_url)
    if r.status_code // 100 == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "idle":
                r = requests.post(url=dev_run_url)
                if r.status_code == 200:
                    print(f'start_device: Device {device_index} started')
                else:
                    print(f'start_device: Error starting device {device_index}')
                if LIMERFE_DEVICE:
                    limerfe_switch(device_index, True, sdrangel_ip, sdrangel_port)
            else:
                print(f'start_device: Device {device_index} not in idle state')
        else:
            print(f'start_device: Cannot get device {device_index} running state')
    else:
        print(f'start_device: Error {r.status_code} getting device {device_index} running state')

# ======================================================================
def stop_device(device_index, sdrangel_ip, sdrangel_port):
    """ Stop the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_run_url = base_url + f'/deviceset/{device_index}/device/run'
    r = requests.get(url=dev_run_url)
    if r.status_code // 100 == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "running":
                r = requests.delete(url=dev_run_url)
                if r.status_code == 200:
                    print(f'stop_device: Device {device_index} stopped')
                else:
                    print(f'stop_device: Error stopping device {device_index}')
                if LIMERFE_DEVICE:
                    limerfe_switch(device_index, False, sdrangel_ip, sdrangel_port)
            else:
                print(f'stop_device: Device {device_index} not in running state')
        else:
            print(f'stop_device: Cannot get device {device_index} running state')
    else:
        print(f'stop_device: Error {r.status_code} getting device {device_index} running state')

# ======================================================================
def set_focus(device_index, sdrangel_ip, sdrangel_port):
    """ Set focus on the specified device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    dev_focus_url = base_url + f'/deviceset/{device_index}/focus'
    r = requests.patch(url=dev_focus_url)
    if r.status_code // 100 == 2:
        print(f'set_focus: Focus set on device set {device_index}')
    elif r.status_code == 400:
        print(f'set_focus: Focus on device set is not supported in a server instance')
    else:
        print(f'set_focus: Error {r.status_code} setting focus on device set {device_index}')

# ======================================================================
def get_sdrangel_ip(request):
    """ Extract originator address from request """
# ----------------------------------------------------------------------
    if request.environ.get('HTTP_X_FORWARDED_FOR') is None:
        return request.environ['REMOTE_ADDR']
    else:
        return request.environ['HTTP_X_FORWARDED_FOR']

# ======================================================================
def get_center_frequency(content):
    """ Look for center frequency recursively """
# ----------------------------------------------------------------------
    for k in content:
        if isinstance(content[k], dict):
            return get_center_frequency(content[k])
        elif k == "centerFrequency":
            return content[k]

# ======================================================================
def change_center_frequency(content, new_frequency):
    """ Change center frequency searching recursively """
# ----------------------------------------------------------------------
    for k in content:
        if isinstance(content[k], dict):
            change_center_frequency(content[k], new_frequency)
        elif k == "centerFrequency":
            content[k] = new_frequency

# ======================================================================
def set_center_frequency(new_frequency, device_index, sdrangel_ip, sdrangel_port):
    """ Set a new center frequency for given device """
# ----------------------------------------------------------------------
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    r = requests.get(url=base_url + f'/deviceset/{device_index}/device/settings')
    if r.status_code // 100 == 2:
        rj = r.json()
        frequency =  get_center_frequency(rj)
        if new_frequency != frequency:
            change_center_frequency(rj, new_frequency)
            r = requests.patch(url=base_url + f'/deviceset/{device_index}/device/settings', json=rj)
            if r.status_code / 100 == 2:
                print(f'set_center_frequency: changed center frequency of device {device_index} to {new_frequency}')
                global FREQ_HAS_CHANGED
                FREQ_HAS_CHANGED = True
                return jsonify(rj)
            else:
                print(f'set_center_frequency: failed to change center frequency of device {device_index} with error {r.status_code}')
        else:
            print(f'set_center_frequency: frequency of device {device_index} is unchanged')
    else:
        print(f'set_center_frequency: error {r.status_code} getting settings for device {device_index}')
    return ""

# ======================================================================
def limerfe_switch(originator_index, start, sdrangel_ip, sdrangel_port):
    """ Start or stop the LimeRFE device connected with first linked originator on Rx and second on Tx.
        the start parameter is True to start and False to stop
    """
# ----------------------------------------------------------------------
    endpoint_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel/limerfe/run'
    try:
        if OTHER_DICT.keys().index(originator_index) == 0: # Rx
            global LIMERFE_RX
            LIMERFE_RX = start
        else:
            global LIMERFE_TX
            LIMERFE_TX = start
    except ValueError:
        print(f'Invalid device index {originator_index}')
        return
    payload = {
        'devicePath': LIMERFE_DEVICE,
        'rxOn': 1 if LIMERFE_RX else 0,
        'txOn': 1 if LIMERFE_TX else 0,
    }
    r = requests.put(url=endpoint_url, json=payload)
    if r.status_code / 100 == 2:
        print(f'LimeRFE at {LIMERFE_DEVICE} switched with Rx: {LIMERFE_RX} Tx: {LIMERFE_TX}')
    else:
        print(f'failed to switch LimeRFE at {LIMERFE_DEVICE} with Rx: {LIMERFE_RX} Tx: {LIMERFE_TX}')

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
    direction = None
    content = request.get_json(silent=True)
    if content:
        originator_index = content.get('originatorIndex')
        direction = content.get('direction')
    if originator_index is None or direction is None:
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
        print(f'device_run: Device {originator_index} (direction={direction}) has started at {sdrangel_ip}:{SDRANGEL_API_PORT}')
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
        print(f'device_run: Device {originator_index} (direction={direction}) has stopped at {sdrangel_ip}:{SDRANGEL_API_PORT}')
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
@app.route('/sdrangel/deviceset/<int:deviceset_index>/device/settings', methods=['GET', 'PATCH', 'PUT'])
def device_settings(deviceset_index):
    ''' Reply with the expected reply of a working device '''
# ----------------------------------------------------------------------
    originator_index = None
    content = request.get_json(silent=True)
    if content:
        originator_index = content.get('originatorIndex')
    if originator_index is None:
        print('device_settings: SDRangel reverse API v4.5.2 or higher required. No or invalid originator information')
        return ""
    sdrangel_ip = get_sdrangel_ip(request)
    other_device_index = OTHER_DICT.get(originator_index)
    if other_device_index is None:
        print('device_settings: Device {originator_index} is not part of the linked pair. Aborting request.')
        return ""
    new_frequency = get_center_frequency(content)
    if new_frequency and FREQ_SYNC:
        global FREQ_HAS_CHANGED
        if FREQ_HAS_CHANGED:
            FREQ_HAS_CHANGED = False
            print('device_settings: frequency was just changed. Ignoring this change.')
        else:
            set_center_frequency(new_frequency, other_device_index, sdrangel_ip, SDRANGEL_API_PORT)
            print(f'device_settings: Device {originator_index} changed frequency to {new_frequency}')
    return ""

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
    parser.add_argument("-f", "--freq-sync", dest="freq_sync", help="synchronize linked devices frequencies", action="store_true")
    parser.add_argument("-L", "--limerfe-dev", dest="limerfe_dev", help="LimeRFE USB serial device (optional)", metavar="DEVICE", type=str)
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
    if options.freq_sync == None:
        options.freq_sync = False

    other_dict = {
        options.linked_devices[0]: options.linked_devices[1],
        options.linked_devices[1]: options.linked_devices[0]
    }

    return options.addr, options.port, options.sdrangel_port, options.delay, options.freq_sync, other_dict, options.limerfe_dev

# ======================================================================
def main():
    """ This is the main routine """
# ----------------------------------------------------------------------
    global SDRANGEL_API_PORT
    global OTHER_DICT
    global DELAY
    global FREQ_SYNC
    global LIMERFE_DEVICE
    addr, port, SDRANGEL_API_PORT, DELAY, FREQ_SYNC, OTHER_DICT, LIMERFE_DEVICE = getInputOptions()
    print(f'main: starting: SDRangel port: {SDRANGEL_API_PORT} links: {OTHER_DICT} freq sync: {FREQ_SYNC} LimeRFE: {LIMERFE_DEVICE}')
    app.run(debug=True, host=addr, port=port)


# ======================================================================
if __name__ == "__main__":
    """ When called from command line... """
# ----------------------------------------------------------------------
    main()

