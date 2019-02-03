#!/usr/bin/env python

import requests, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# commands list. Each command is a list:
#   - URL suffix (API function)
#   - HTTP method (GET, PATCH, POST, PUT, DELETE)
#   - Params as key:value pairs or None if unused
#   - JSON body or None if unused
#   - Descriptive message fragment
commands = [
    ["/deviceset/0/device", "PUT", None, {"hwType": "AirspyHF"}, "setup AirspyHF on Rx 0"],
    ["/preset", "PATCH", None, {"deviceSetIndex": 0, "preset": {"groupName": "OM144", "centerFrequency": 145480000, "type": "R", "name": "Rept + Simplex + DV"}}, "load preset on Rx 0"],
    ["/deviceset", "POST", None, None, "add Rx 1 device set"],
    ["/deviceset/1/device", "PUT", None, {"hwType": "RemoteInput"}, "setup RemoteInput on Rx 1"],
    ["/preset", "PATCH", None, {"deviceSetIndex": 1, "preset": {"groupName": "PRO400", "centerFrequency": 463750000, "type": "R", "name": "PM nice low remote"}}, "load preset on Rx 1"],
    ["/deviceset", "POST", None, None, "add Rx 2 device set"],
    ["/deviceset/2/device", "PUT", None, {"hwType": "Perseus"}, "setup Perseus on Rx 2"],
    ["/preset", "PATCH", None, {"deviceSetIndex": 2, "preset": {"groupName": "40m", "centerFrequency": 7074000, "type": "R", "name": "FT8"}}, "load preset on Rx 2"],
    ["/dvserial", "PATCH", {"dvserial": 1}, None, "set DV serial processing for AMBE frames decoding"],
    ["/deviceset/0/device/run", "POST", None, None, "Start device on deviceset R0"],
    ["/deviceset/1/device/run", "POST", None, None, "Start device on deviceset R1"],
    ["/deviceset/2/device/run", "POST", None, None, "Start device on deviceset R2"],
]

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
def main():
    try:
        options = getInputOptions()
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address

        for command in commands:
            url = base_url + command[0]
            method = requests_methods.get(command[1], None)
            if method is not None:
                r = method(url=url, params=command[2], json=command[3])
                if r.status_code / 100 == 2:
                    print("Done: %s" % command[4])
                else:
                    print("Error %d:%s" % (r.status_code, command[4]))
                    print(r.text)
                    exit(1)
            else:
                print("requests method error")
                exit(1)
                
        print("All done!")
    
    except Exception as ex:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


# ======================================================================
if __name__ == "__main__":
    main()

