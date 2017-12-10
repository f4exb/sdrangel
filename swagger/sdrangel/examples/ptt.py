#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8888/sdrangel"

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-t", "--transmit", dest="transmit", help="transmit", metavar="TRANSMIT", action="store_true", default=False)

    (options, args) = parser.parse_args()
    
    if (options.address == None):
        options.address = "127.0.0.1:8888"
    
    return options

# ======================================================================
def startDevice(deviceIndex):
    dev_run_url = base_url+("/deviceset/%d/device/run" % deviceIndex)
    r = requests.get(url=dev_run_url)
    if r.status_code == 200:
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
    if r.status_code == 200:
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
        if r.status_code == 200:
            rj = r.json()
            deviceSets = rj.get("deviceSets", None)
            if deviceSets is not None:
                if len(deviceSets) > 1:
                    if deviceSets[0]["samplingDevice"]["tx"] == 0 and deviceSets[1]["samplingDevice"]["tx"] == 1:
                        if options.transmit:
                            stopDevice(0)
                            startDevice(1)
                        else:
                            stopDevice(1)
                            startDevice(0)
                    else:
                        print("Incorrect configuration expecting Rx0 and Tx1")
                else:
                    print("Need at least a Rx and a Tx device set")
            else:
                print("Cannot get device sets configuration")
        else:
            print("Error getting device sets configuration")
                        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

