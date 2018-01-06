#!/usr/bin/env python
'''
 PTT (Push To Talk) script example.
 
 Assumes Rx and Tx are contiguous device sets with Rx at index i and Tx at index i+1
 
 When going from Rx to Tx (push) the device set index specified must be the Rx one 
 When going from Tx to Rx (release) the device set index specified must be the Tx one
 Defaults are Rx at index 0 and Tx at index 1 
 
 Default web API base address is http://127.0.0.1:8091/sdrangel (default of sdrangel web API)
 You may change the address and port portion with the -a parameter. Ex: -a 44.168.40.128:8888
'''
import requests, json, traceback, sys, time
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %prog [-a address:port] [-d index][-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-t", "--transmit", dest="transmit", help="transmit", metavar="TRANSMIT", action="store_true", default=False)
    parser.add_option("-d", "--deviceset-index", dest="deviceset_index", help="index of currently active device set (Rx or Tx)", metavar="INDEX", type="int")

    (options, args) = parser.parse_args()
    
    if options.address == None:
        options.address = "127.0.0.1:8091"
    
    if options.deviceset_index == None:
        options.deviceset_index = 0
    
    return options

# ======================================================================
def startDevice(deviceIndex):
    dev_run_url = base_url+("/deviceset/%d/device/run" % deviceIndex)
    r = requests.get(url=dev_run_url)
    if r.status_code / 100 == 2:
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
    if r.status_code / 100 == 2:
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
def setFocus(deviceIndex):
    dev_focus_url = base_url+("/deviceset/%d/focus" % deviceIndex)
    r = requests.patch(url=dev_focus_url)
    if r.status_code / 100 == 2:
        print("Focus set on device set %d" % deviceIndex)
    elif r.status_code == 400:
        print("Set focus on device set is not supported in a server instance")
    else:
        print("Error setting focus on device set %d" % deviceIndex)
        
# ======================================================================
def main():
    try:
        options = getInputOptions()
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        r = requests.get(url=base_url+"/devicesets")
        if r.status_code / 100 == 2:
            rj = r.json()
            deviceSets = rj.get("deviceSets", None)
            if deviceSets is not None:
                if len(deviceSets) > 1:
                    if options.transmit:
                        if deviceSets[options.deviceset_index]["samplingDevice"]["tx"] == 0 and deviceSets[options.deviceset_index+1]["samplingDevice"]["tx"] == 1:
                            stopDevice(options.deviceset_index)
                            time.sleep(1)
                            startDevice(options.deviceset_index+1)
                            setFocus(options.deviceset_index+1)
                        else:
                            print("Incorrect configuration expecting Rx%d and Tx%d" % (options.deviceset_index, options.deviceset_index+1))
                    else:
                        if deviceSets[options.deviceset_index-1]["samplingDevice"]["tx"] == 0 and deviceSets[options.deviceset_index]["samplingDevice"]["tx"] == 1:
                            stopDevice(options.deviceset_index)
                            time.sleep(1)
                            startDevice(options.deviceset_index-1)
                            setFocus(options.deviceset_index-1)
                        else:
                            print("Incorrect configuration expecting Rx%d and Tx%d" % (options.deviceset_index-1, options.deviceset_index))
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

