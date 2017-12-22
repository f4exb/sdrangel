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
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        
        settings = callAPI("/deviceset/0/channel", "POST", None, {"channelType": "NFMDemod", "tx": 0}, "Create NFM demod")
        if settings is None:
            exit(-1)

        settings["NFMDemodSettings"]["inputFrequencyOffset"] = 12500
        settings["NFMDemodSettings"]["afBandwidth"] = 5000
        
        r = callAPI("/deviceset/0/channel/0/settings", "PATCH", None, settings, "Change NFM demod")
        if r is None:
            exit(-1)
            
        r = callAPI("/deviceset", "POST", {"tx": 1}, None, "Add Tx device set")
        if r is None:
            exit(-1)
            
        settings = callAPI("/deviceset/1/channel", "POST", None, {"channelType": "NFMMod", "tx": 1}, "Create NFM mod")
        if settings is None:
            exit(-1)
        
        settings["NFMModSettings"]["inputFrequencyOffset"] = 12500
        settings["NFMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["NFMModSettings"]["cwKeyer"]["loop"] = 1
        settings["NFMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["NFMModSettings"]["modAFInput"] = 4 # CW text
        
        r = callAPI("/deviceset/1/channel/0/settings", "PATCH", None, settings, "Change NFM mod")
        if r is None:
            exit(-1)
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

