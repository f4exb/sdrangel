#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

requests_methods = {
    "GET": requests.get,
    "PATCH": requests.patch,
    "POST": requests.post,
    "PUT": requests.put,
    "DELETE": requests.delete
}

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int") 

    (options, args) = parser.parse_args()
    
    if options.address == None:
        options.address = "127.0.0.1:8091"
    
    if options.device_index == None:
        options.device_index = 1
    
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
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        
        r = callAPI("/deviceset", "POST", {"tx": 1}, None, "Add Tx device set")
        if r is None:
            exit(-1)
            
        deviceset_url = "/deviceset/%d" % options.device_index
        
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "LimeSDR", "tx": 1}, "setup LimeSDR on Tx device set")
        if r is None:
            exit(-1)
            
        settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get LimeSDR Tx settings")
        if settings is None:
            exit(-1)

        settings["limeSdrOutputSettings"]["antennaPath"] = 1
        settings["limeSdrOutputSettings"]["devSampleRate"] = 3200000
        settings["limeSdrOutputSettings"]["log2SoftInterp"] = 4
        settings["limeSdrOutputSettings"]["ncoEnable"] = 1
        settings["limeSdrOutputSettings"]["ncoFrequency"] = -500000
        settings["limeSdrOutputSettings"]["lpfFIRBW"] = 100000
        settings["limeSdrOutputSettings"]["lpfFIREnable"] = 1 
        
        r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch LimeSDR Tx settings")
        if r is None:
            exit(-1)
            
        r = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": "NFMMod", "tx": 1}, "Create NFM mod")
        if r is None:
            exit(-1)
        
        settings = callAPI(deviceset_url + "/channel/0/settings", "GET", None, None, "Get NFM mod settings")
        if settings is None:
            exit(-1)
        
        settings["NFMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["NFMModSettings"]["cwKeyer"]["loop"] = 1
        settings["NFMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["NFMModSettings"]["modAFInput"] = 4 # CW text
        settings["NFMModSettings"]["toneFrequency"] = 600
        
        r = callAPI(deviceset_url + "/channel/0/settings", "PATCH", None, settings, "Change NFM mod")
        if r is None:
            exit(-1)
            
        r = callAPI(deviceset_url + "/device/run", "POST", None, None, "Start device on deviceset R1")
        if r is None:
            exit(-1)
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

