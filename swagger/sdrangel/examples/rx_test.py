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
    parser.add_option("-D", "--device-hwid", dest="device_hwid", help="device hardware id", metavar="HWID", type="string") 
    parser.add_option("-F", "--device-freq", dest="device_freq", help="device center frequency (kHz)", metavar="FREQ", type="int") 
    parser.add_option("-f", "--channel-freq", dest="channel_freq", help="channel center frequency (Hz)", metavar="FREQ", type="int")
    parser.add_option("-U", "--copy-to-udp", dest="udp_copy", help="UDP audio copy to <address>[:<port>]", metavar="IP:PORT", type="string") 
    parser.add_option("-A", "--antenna-path", dest="antenna_path", help="antenna path index", metavar="INDEX", type="int")    
    parser.add_option("-s", "--sample-rate", dest="sample_rate", help="device to host sample rate (kS/s)", metavar="RATE", type="int")
    parser.add_option("-c", "--create", dest="create", help="create a new device set", metavar="CREATE", action="store_true", default=False)

    (options, args) = parser.parse_args()
    
    if options.address == None:
        options.address = "127.0.0.1:8091"
    
    if options.device_index == None:
        options.device_index = 0
        
    if options.device_hwid == None:
        options.device_hwid = "FileSource"
    
    if options.device_freq == None:
        options.device_freq = 435000
    
    if options.channel_freq == None:
        options.channel_freq = 0
        
    if options.antenna_path == None:
        options.antenna_path = 0
    
    if options.sample_rate == None:
        options.sample_rate = 2600
    
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
        
        if options.create:
            r = callAPI("/deviceset", "POST", {"tx": 0}, None, "Add Rx device set")
            if r is None:
                exit(-1)
            
        deviceset_url = "/deviceset/%d" % options.device_index
        
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid, "tx": 0}, "setup device on Rx device set")
        if r is None:
            exit(-1)
            
        settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
        if settings is None:
            exit(-1)

        if options.device_hwid == "LimeSDR":
            settings["limeSdrInputSettings"]["antennaPath"] = options.antenna_path
            settings["limeSdrInputSettings"]["devSampleRate"] = options.sample_rate*1000
            settings["limeSdrInputSettings"]["log2HardDecim"] = 4
            settings["limeSdrInputSettings"]["log2SoftDecim"] = 3
            settings["limeSdrInputSettings"]["centerFrequency"] = options.device_freq*1000 + 500000
            settings["limeSdrInputSettings"]["ncoEnable"] = 1
            settings["limeSdrInputSettings"]["ncoFrequency"] = -500000
            settings["limeSdrInputSettings"]["lpfBW"] = 1450000
            settings["limeSdrInputSettings"]["lpfFIRBW"] = 100000
            settings["limeSdrInputSettings"]["lpfFIREnable"] = 1
            settings['limeSdrInputSettings']['dcBlock'] = 1
        elif options.device_hwid == "RTLSDR":
            settings['rtlSdrSettings']['devSampleRate'] = options.sample_rate*1000
            settings['rtlSdrSettings']['centerFrequency'] = options.device_freq*1000
            settings['rtlSdrSettings']['gain'] = 496
            settings['rtlSdrSettings']['log2Decim'] = 4
            settings['rtlSdrSettings']['dcBlock'] = 1
            settings['rtlSdrSettings']['agc'] = 1
        elif options.device_hwid == "HackRF":
            settings['hackRFInputSettings']['LOppmTenths'] = -51
            settings['hackRFInputSettings']['centerFrequency'] = options.device_freq*1000
            settings['hackRFInputSettings']['dcBlock'] = 1
            settings['hackRFInputSettings']['devSampleRate'] = options.sample_rate*1000
            settings['hackRFInputSettings']['lnaExt'] = 1
            settings['hackRFInputSettings']['lnaGain'] = 32
            settings['hackRFInputSettings']['log2Decim'] = 4
            settings['hackRFInputSettings']['vgaGain'] = 24
        
        r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device settings")
        if r is None:
            exit(-1)
            
        r = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": "NFMDemod", "tx": 0}, "Create NFM demod")
        if r is None:
            exit(-1)
        
        settings = callAPI(deviceset_url + "/channel/0/settings", "GET", None, None, "Get NFM demod settings")
        if settings is None:
            exit(-1)
        
        settings["NFMDemodSettings"]["title"] = "Test NFM"
        settings["NFMDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["NFMDemodSettings"]["rfBandwidth"] = 12500
        settings["NFMDemodSettings"]["fmDeviation"] = 3000
        settings["NFMDemodSettings"]["afBandwidth"] = 4000
        settings["NFMDemodSettings"]["squelch"] = -700
        settings["NFMDemodSettings"]["volume"] = 2.0
        
        if options.udp_copy is not None:
            address_port = options.udp_copy.split(':')
            if len(address_port) > 1:
                settings["NFMDemodSettings"]["udpPort"] = address_port[1]
            if len(address_port) > 0:
                settings["NFMDemodSettings"]["udpAddress"] = address_port[0]
            settings["NFMDemodSettings"]["copyAudioToUDP"] = 1
        
        r = callAPI(deviceset_url + "/channel/0/settings", "PATCH", None, settings, "Change NFM demod")
        if r is None:
            exit(-1)
            
        r = callAPI(deviceset_url + "/device/run", "POST", None, None, "Start running device")
        if r is None:
            exit(-1)
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

