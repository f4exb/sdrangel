#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser
import time
import numpy as np

base_url = "http://127.0.0.1:8091/sdrangel"
deviceset_url = ""

requests_methods = {
    "GET": requests.get,
    "PATCH": requests.patch,
    "POST": requests.post,
    "PUT": requests.put,
    "DELETE": requests.delete
}

class ScanControl:
    def __init__(self, num_channels, channel_step, start_freq, stop_freq, log2_decim):        
        self.channel_shifts = []
        if num_channels < 2:
            self.channel_shifts = [0]
            limit = 0
        else:
            limit = ((num_channels-1)*channel_step) / 2
            self.channel_shifts = list(np.linspace(-limit, limit, num_channels))
        self.device_start_freq = start_freq + limit 
        self.device_stop_freq = stop_freq - limit
        self.device_step_freq = 2*limit + channel_step
        self.device_sample_rate = (2*limit + channel_step)*(1<<log2_decim)

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int", default=0) 
    parser.add_option("-D", "--device-hwid", dest="device_hwid", help="device hardware id", metavar="HWID", type="string", default="RTLSDR") 
    parser.add_option("-l", "--log2-decim", dest="log2_decim", help="log2 of the desired software decimation factor", metavar="LOG2", type="int", default=4) 
    parser.add_option("-n", "--num-channels", dest="num_channels", help="number of parallel channels", metavar="NUMBER", type="int", default=8) 
    parser.add_option("-s", "--freq-step", dest="freq_step", help="frequency step (Hz)", metavar="FREQUENCY", type="int", default=12500) 
    parser.add_option("-S", "--freq-start", dest="freq_start", help="frequency start (Hz)", metavar="FREQUENCY", type="int", default=446006250) 
    parser.add_option("-T", "--freq-stop", dest="freq_stop", help="frequency stop (Hz)", metavar="FREQUENCY", type="int", default=446193750) 
    parser.add_option("-b", "--af-bw", dest="af_bw", help="audio babdwidth (kHz)", metavar="FREQUENCY_KHZ", type="int" ,default=3) 
    parser.add_option("-r", "--rf-bw", dest="rf_bw", help="RF babdwidth (Hz). Sets to nearest available", metavar="FREQUENCY", type="int", default=10000) 
    parser.add_option("-c", "--create", dest="create", help="create a new device set", metavar="BOOLEAN", action="store_true", default=False)
    parser.add_option("-m", "--mock", dest="mock", help="just print calculated values and exit", metavar="BOOLEAN", action="store_true", default=False)

    (options, args) = parser.parse_args()
    
    if (options.address == None):
        options.address = "127.0.0.1:8091"
    
    return options

# ======================================================================
def printResponse(response):
    content_type = response.headers.get("Content-Type", None)
    if content_type is not None:
        if "application/json" in content_type:
            print(json.dumps(response.json(), indent=4, sort_keys=True))
        elif "text/plain" in content_type:
            print(response.text)

# ======================================================================
def callAPI(url, method, params, json, text):
    request_method = requests_methods.get(method, None)
    if request_method is not None:
        r = request_method(url=base_url+url, params=params, json=json)
        if r.status_code / 100 == 2:
            print(text + " succeeded")
            printResponse(r)
            return r.json() # all 200 yield application/json response
        else:
            print(text + " failed")
            printResponse(r)
            return None

# ======================================================================
def main():
    try:
        options = getInputOptions()
        scan_control = ScanControl(options.num_channels, options.freq_step, options.freq_start, options.freq_stop, options.log2_decim)
        
        print("Channel shifts: %s" % scan_control.channel_shifts)
        print("Sample rate: %d" % scan_control.device_sample_rate)
        print("Start: %d" % scan_control.device_start_freq)
        print("Stop: %d" % scan_control.device_stop_freq)
        print("Step: %d" % scan_control.device_step_freq)
        
        if scan_control.device_stop_freq < scan_control.device_start_freq:
            print("Frequency error")
            exit(1)
        
        if options.mock:
            freqs = []
            fc = scan_control.device_start_freq
            while fc <= scan_control.device_stop_freq:
                freqs += [x+fc for x in scan_control.channel_shifts]
                fc += scan_control.device_step_freq            
            print("Scanned frequencies: %s" % freqs)
            exit(0)
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        
        # Set Rx
        
        if options.create:
            r = callAPI("/deviceset", "POST", {"tx": 0}, None, "Add Rx device set")
            if r is None:
                exit(-1)
            
        global deviceset_url
        deviceset_url = "/deviceset/%d" % options.device_index

        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid, "tx": 0}, "setup device on Rx device set")
        if r is None:
            exit(-1)
        
        
        
        settings = callAPI("/deviceset/0/channel", "POST", None, {"channelType": "NFMDemod", "tx": 0}, "Create NFM demod")
        if settings is None:
            exit(-1)

        settings["NFMDemodSettings"]["inputFrequencyOffset"] = 12500
        settings["NFMDemodSettings"]["afBandwidth"] = 5000
        
        r = callAPI("/deviceset/0/channel/0/settings", "PATCH", None, settings, "Change NFM demod")
        if r is None:
            exit(-1)
            
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

