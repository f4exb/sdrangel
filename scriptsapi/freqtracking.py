#!/usr/bin/env python3
'''
Frequency tracker:

Listens on a TCP port for SDRangel reverse API requests.
  - When the request comes from a FreqTracker channel it gets the FreqTracker channel frequency shift
  - When the request comes from another channel it records the difference between the FreqTracker frequency shift and
this channel frequency shift. Then it will periodically send a center frequency change request to this channel with
the FreqTracker channel frequency shift plus the difference thus achieving the locking of this channel to the FreqTracker
channel frequency.
  - If the reply from the channel returns an error it will un-register the channel.

In the SDRangel instance you must activate the reverse API of the FreqTracker channel and the controlled channel(s)
reverse API giving the address and port of this instance of the script. You have to click on the small grey box at the
top left of the plugin GUI to open the channel details dialog where the reverse API can be configured.
'''

import requests
import time
import argparse
from flask import Flask
from flask import request, jsonify

SDRANGEL_API_ADDR = None
SDRANGEL_API_PORT = 8091
TRACKER_OFFSET = 0
TRACKER_DEVICE = 0
TRACKING_DICT = {}

app = Flask(__name__)

# ======================================================================
def getInputOptions():
    """ This is the argument line parser """
# ----------------------------------------------------------------------
    parser = argparse.ArgumentParser(description="Manages PTT from an SDRangel instance automatically")
    parser.add_argument("-A", "--address", dest="addr", help="listening address (default 0.0.0.0)", metavar="IP", type=str)
    parser.add_argument("-P", "--port", dest="port", help="listening port (default 8000)", metavar="PORT", type=int)
    parser.add_argument("-a", "--address-sdr", dest="sdrangel_address", help="SDRangel REST API address (defaults to calling address)", metavar="ADDRESS", type=str)
    parser.add_argument("-p", "--port-sdr", dest="sdrangel_port", help="SDRangel REST API port (default 8091)", metavar="PORT", type=int)

    options = parser.parse_args()

    if options.addr == None:
        options.addr = "0.0.0.0"
    if options.port == None:
        options.port = 8888
    if options.sdrangel_port == None:
        options.sdrangel_port = 8091

    return options.addr, options.port, options.sdrangel_address, options.sdrangel_port

# ======================================================================
def get_sdrangel_ip(request):
    """ Extract originator address from request """
# ----------------------------------------------------------------------
    if SDRANGEL_API_ADDR is not None:
        return SDRANGEL_API_ADDR
    if request.environ.get('HTTP_X_FORWARDED_FOR') is None:
        return request.environ['REMOTE_ADDR']
    else:
        return request.environ['HTTP_X_FORWARDED_FOR']

# ======================================================================
def gen_dict_extract(key, var):
    """ Gets a key value in a dictionnary or sub-dictionnary structure """
# ----------------------------------------------------------------------
    if hasattr(var,'items'):
        for k, v in var.items():
            if k == key:
                yield v
            if isinstance(v, dict):
                for result in gen_dict_extract(key, v):
                    yield result
            elif isinstance(v, list):
                for d in v:
                    for result in gen_dict_extract(key, d):
                        yield result

# ======================================================================
def update_frequency_setting(request_content, frequency):
    """ Finds the channel settings key that contains the inputFrequencyOffset key
        and replace it with a single inputFrequencyOffset key with new frequency
    """
# ----------------------------------------------------------------------
    for k in request_content:
        setting_item = request_content[k]
        if isinstance(setting_item, dict):
            if 'inputFrequencyOffset' in setting_item:
                setting_item.update({
                    'inputFrequencyOffset': frequency
                })


# ======================================================================
def adjust_channels(sdrangel_ip, sdrangel_port):
    """ Adjust registered channels center frequencies
        Remove keys for channels returning error
    """
# ----------------------------------------------------------------------
    global TRACKING_DICT
    base_url = f'http://{sdrangel_ip}:{sdrangel_port}/sdrangel'
    remove_keys = []
    for k in TRACKING_DICT:
        device_index = k[0]
        channel_index = k[1]
        tracking_item = TRACKING_DICT[k]
        frequency_correction = TRACKER_OFFSET - tracking_item['trackerFrequency']
        frequency = tracking_item['channelFrequency'] + frequency_correction
        update_frequency_setting(tracking_item['requestContent'], frequency)
        r = requests.patch(url=base_url + f'/deviceset/{device_index}/channel/{channel_index}/settings', json=tracking_item['requestContent'])
        if r.status_code / 100 != 2:
            remove_keys.append(k)
    for k in remove_keys:
        tracking_item = TRACKING_DICT.pop(k, None)
        if tracking_item:
            request_content = tracking_item.get('requestContent')
            if request_content:
                channel_type = request_content.get('channelType')
            else:
                channel_type = 'Undefined'
            device_index = k[0]
            channel_index = k[1]
            print(f'SDRangel: {sdrangel_ip}:{sdrangel_port} Removed {channel_type} [{device_index}:{channel_index}]')


# ======================================================================
def register_channel(device_index, channel_index, channel_frequency, request_content):
    """ Register a channel or change its center frequency reference """
# ----------------------------------------------------------------------
    global TRACKING_DICT
    TRACKING_DICT.update({
        (device_index, channel_index) : {
            'channelFrequency': channel_frequency,
            'trackerFrequency': TRACKER_OFFSET,
            'requestContent': request_content
        }
    })

# ======================================================================
@app.route('/sdrangel')
def hello_sdrangel():
    """ Just to test if it works """
# ----------------------------------------------------------------------
    sdrangel_ip = get_sdrangel_ip(request)
    print(f'SDRangel IP: {sdrangel_ip}')
    return 'Hello, SDRangel!'


# ======================================================================
@app.route('/sdrangel/deviceset/<int:deviceset_index>/channel/<int:channel_index>/settings', methods=['GET', 'PATCH', 'PUT'])
def channel_settings(deviceset_index, channel_index):
    """ Receiving channel settings from reverse API """
# ----------------------------------------------------------------------
    global TRACKER_OFFSET
    global TRACKER_DEVICE
    orig_device_index = None
    orig_channel_index = None
    content = request.get_json(silent=True)
    if content:
        orig_device_index = content.get('originatorDeviceSetIndex')
        orig_channel_index = content.get('originatorChannelIndex')
    if orig_device_index is None or orig_channel_index is None:
        print('device_settings: SDRangel reverse API v4.5.2 or higher required. No or invalid originator information')
        return "SDRangel reverse API v4.5.2 or higher required "
    sdrangel_ip = get_sdrangel_ip(request)
    channel_type = content.get('channelType')
    for freq_offset in gen_dict_extract('inputFrequencyOffset', content):
        if channel_type == "FreqTracker":
            print(f'SDRangel: {sdrangel_ip}:{SDRANGEL_API_PORT} Tracker [{orig_device_index}:{orig_channel_index}] at {freq_offset} Hz')
            TRACKER_OFFSET = freq_offset
            TRACKER_DEVICE = orig_device_index
            adjust_channels(sdrangel_ip, SDRANGEL_API_PORT)
        else:
            register_channel(orig_device_index, orig_channel_index, freq_offset, content)
            print(f'SDRangel: {sdrangel_ip}:{SDRANGEL_API_PORT} {channel_type} [{orig_device_index}:{orig_channel_index}] at {freq_offset} Hz')
    return "OK processed "


# ======================================================================
def main():
    """ This is the main routine """
# ----------------------------------------------------------------------
    global SDRANGEL_API_ADDR
    global SDRANGEL_API_PORT
    addr, port, SDRANGEL_API_ADDR, SDRANGEL_API_PORT = getInputOptions()
    print(f'main: starting at: {addr}:{port}')
    app.run(debug=True, host=addr, port=port)


# ======================================================================
if __name__ == "__main__":
    """ When called from command line... """
# ----------------------------------------------------------------------
    main()
