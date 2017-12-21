#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int") 
    parser.add_option("-t", "--stop", dest="stop", help="stop device", metavar="STOP", action="store_true", default=False)
    parser.add_option("-s", "--start", dest="start", help="start device", metavar="START", action="store_true", default=False)

    (options, args) = parser.parse_args()
    
    if (options.address == None):
        options.address = "127.0.0.1:8888"
        
    if options.device_index < 0:
        otions.device_index = 0
        
    if options.start and options.stop:
        print("Cannot start and stop at the same time")
        exit(1)
        
    if not options.start and not options.stop:
        print("Must start or stop")
        exit(1)

    return options

# ======================================================================
def startDevice(deviceIndex):
    dev_run_url = base_url+("/deviceset/%d/device/run" % deviceIndex)
    r = requests.get(url=dev_run_url)
    if r.status_code / 100  == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "idle":
                r = requests.post(url=dev_run_url)
                if r.status_code == 200:
                    print("Device %d started" % deviceIndex)
                else:
                    print("Error starting device %d" % deviceIndex)
            else:
                print("device %d not in idle state" % deviceIndex)
        else:
            print("Cannot get device %d running state" % deviceIndex)
    else:
        print("Error getting device %d running state" % deviceIndex)

# ======================================================================
def stopDevice(deviceIndex):
    dev_run_url = base_url+("/deviceset/%d/device/run" % deviceIndex)
    r = requests.get(url=dev_run_url)
    if r.status_code / 100  == 2:
        rj = r.json()
        state = rj.get("state", None)
        if state is not None:
            if state == "running":
                r = requests.delete(url=dev_run_url)
                if r.status_code == 200:
                    print("Device %d stopped" % deviceIndex)
                else:
                    print("Error stopping device %d" % deviceIndex)
            else:
                print("device %d not in running state" % deviceIndex)
        else:
            print("Cannot get device %d running state" % deviceIndex)
    else:
        print("Error getting device %d running state" % deviceIndex)
        
# ======================================================================
def main():
    try:
        options = getInputOptions()
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        r = requests.get(url=base_url+"/devicesets")
        if r.status_code / 100  == 2:
            rj = r.json()
            deviceSets = rj.get("deviceSets", None)
            if deviceSets is not None:
                if options.device_index < len(deviceSets):
                    if options.start:
                        startDevice(options.device_index)
                    if options.stop:
                        stopDevice(options.device_index)
                else:
                    print("Invalid device set index. Number of device sets is %d" % len(deviceSets))
            else:
                print("Cannot get device sets configuration")
        else:
            print("Error getting device sets configuration")
                        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

