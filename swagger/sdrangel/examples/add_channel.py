#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int") 
    parser.add_option("-c", "--channel-id", dest="channel_id", help="channel ID of channel to add", metavar="ID", type="string")

    (options, args) = parser.parse_args()
    
    if options.address is None:
        options.address = "127.0.0.1:8888"
        
    if options.device_index is None or options.device_index < 0:
        options.device_index = 0
        
    if options.channel_id is None:
        print("Please specify channel Id")
        exit(1)

    return options


# ======================================================================
def main():
    try:
        options = getInputOptions()
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        device_url = base_url + ("/deviceset/%d/channel" % options.device_index)
        r = requests.post(url=device_url, json={"tx": 0, "channelType": options.channel_id})
        if r.status_code / 100 == 2:
            print("Success")
            print json.dumps(r.json(), indent=4, sort_keys=True)
        else:
            print("Error adding channel. HTTP: %d" % r.status_code)
            print json.dumps(r.json(), indent=4, sort_keys=True)
                        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()
