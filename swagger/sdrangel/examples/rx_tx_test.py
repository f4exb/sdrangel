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
    parser.add_option("-R", "--device-hwid-rx", dest="device_hwid_rx", help="device hardware id for Rx", metavar="HWID", type="string") 
    parser.add_option("-T", "--device-hwid-tx", dest="device_hwid_tx", help="device hardware id for Tx", metavar="HWID", type="string") 
    parser.add_option("-F", "--device-freq", dest="device_freq", help="device center frequency (kHz)", metavar="FREQ", type="int") 
    parser.add_option("-f", "--channel-freq", dest="channel_freq", help="channel center frequency (Hz)", metavar="FREQ", type="int")
    parser.add_option("-U", "--copy-to-udp", dest="udp_copy", help="UDP audio copy to <address>[:<port>]", metavar="IP:PORT", type="string")
    parser.add_option("-s", "--sample-rate-rx", dest="sample_rate_rx", help="device to host (Rx) sample rate (kS/s)", metavar="RATE", type="int")
    parser.add_option("-S", "--sample-rate-tx", dest="sample_rate_tx", help="host to device (Tx) sample rate (kS/s)", metavar="RATE", type="int")    
    parser.add_option("-n", "--antenna-path-rx", dest="antenna_path_rx", help="antenna path index (Rx)", metavar="INDEX", type="int")    
    parser.add_option("-N", "--antenna-path-tx", dest="antenna_path_tx", help="antenna path index (Tx)", metavar="INDEX", type="int")    

    (options, args) = parser.parse_args()
    
    if options.address == None:
        options.address = "127.0.0.1:8091"
    
    if options.device_hwid_rx == None:
        options.device_hwid_rx = "FileSource"
    
    if options.device_hwid_tx == None:
        options.device_hwid_tx = "FileSink"
    
    if options.device_freq == None:
        options.device_freq = 435000
    
    if options.channel_freq == None:
        options.channel_freq = 0
    
    if options.sample_rate_rx == None:
        options.sample_rate_rx = 2600
    
    if options.sample_rate_tx == None:
        options.sample_rate_tx = 2600
    
    if options.antenna_path_rx == None:
        options.antenna_path_rx = 0
    
    if options.antenna_path_tx == None:
        options.antenna_path_tx = 0
    
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
        
        r = callAPI("/devicesets", "GET", None, None, "Get device set configuration")
        if r is None:
            exit(-1)
            
        nb_devicesets = r['devicesetcount']
        
        if nb_devicesets == 0: # server starts without device set so add Rx device set
            r1 = callAPI("/deviceset", "POST", {"tx": 0}, None, "Add Rx device set")
            if r1 is None:
                exit(-1)
            
        ### Rx setup
            
        deviceset_index_rx = 0
        deviceset_url = "/deviceset/%d" % deviceset_index_rx
            
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid_rx, "tx": 0}, "setup device on Rx device set")
        if r is None:
            exit(-1)

        settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
        if settings is None:
            exit(-1)
            
        if options.device_hwid_rx == "LimeSDR":
            settings["limeSdrInputSettings"]["antennaPath"] = options.antenna_path_rx
            settings["limeSdrInputSettings"]["devSampleRate"] = options.sample_rate_rx*1000
            settings["limeSdrInputSettings"]["log2HardDecim"] = 4
            settings["limeSdrInputSettings"]["log2SoftDecim"] = 3
            settings["limeSdrInputSettings"]["centerFrequency"] = options.device_freq*1000 + 500000
            settings["limeSdrInputSettings"]["ncoEnable"] = 1
            settings["limeSdrInputSettings"]["ncoFrequency"] = -500000
            settings["limeSdrInputSettings"]["lpfBW"] = 1450000
            settings["limeSdrInputSettings"]["lpfFIRBW"] = 100000
            settings["limeSdrInputSettings"]["lpfFIREnable"] = 1
            settings['limeSdrInputSettings']['dcBlock'] = 1
        elif options.device_hwid_rx == "RTLSDR":
            settings['rtlSdrSettings']['devSampleRate'] = options.sample_rate_rx*1000
            settings['rtlSdrSettings']['centerFrequency'] = options.device_freq*1000
            settings['rtlSdrSettings']['gain'] = 496
            settings['rtlSdrSettings']['log2Decim'] = 4
            settings['rtlSdrSettings']['dcBlock'] = 1
            settings['rtlSdrSettings']['agc'] = 1
        elif options.device_hwid_rx == "HackRF":
            settings['hackRFInputSettings']['LOppmTenths'] = -51
            settings['hackRFInputSettings']['centerFrequency'] = options.device_freq*1000
            settings['hackRFInputSettings']['dcBlock'] = 1
            settings['hackRFInputSettings']['devSampleRate'] = options.sample_rate_rx*1000
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
        
        ### Tx setup        
        
        r = callAPI("/deviceset", "POST", {"tx": 1}, None, "Add Tx device set")
        if r is None:
            exit(-1)
        
        deviceset_url = "/deviceset/%d" % (deviceset_index_rx + 1)
        
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid_tx, "tx": 1}, "setup device on Tx device set")
        if r is None:
            exit(-1)
            
        settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
        if settings is None:
            exit(-1)

        if options.device_hwid_tx == "LimeSDR":
            settings["limeSdrOutputSettings"]["antennaPath"] = options.antenna_path_tx
            settings["limeSdrOutputSettings"]["devSampleRate"] = options.sample_rate_tx*1000
            settings["limeSdrOutputSettings"]["log2HardInterp"] = 4
            settings["limeSdrOutputSettings"]["log2SoftInterp"] = 4
            settings["limeSdrOutputSettings"]["centerFrequency"] = options.device_freq*1000 + 500000
            settings["limeSdrOutputSettings"]["ncoEnable"] = 1
            settings["limeSdrOutputSettings"]["ncoFrequency"] = -500000
            settings["limeSdrOutputSettings"]["lpfBW"] = 4050000
            settings["limeSdrOutputSettings"]["lpfFIRBW"] = 100000
            settings["limeSdrOutputSettings"]["lpfFIREnable"] = 1
        elif options.device_hwid_tx == "HackRF":
            settings['hackRFOutputSettings']['LOppmTenths'] = -51
            settings['hackRFOutputSettings']['centerFrequency'] = options.device_freq*1000
            settings['hackRFOutputSettings']['devSampleRate'] = options.sample_rate_tx*1000
            settings['hackRFOutputSettings']['lnaExt'] = 0
            settings['hackRFOutputSettings']['log2Interp'] = 4
            settings['hackRFOutputSettings']['vgaGain'] = 24
        
        r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device settings")
        if r is None:
            exit(-1)
            
        r = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": "NFMMod", "tx": 1}, "Create NFM mod")
        if r is None:
            exit(-1)
        
        settings = callAPI(deviceset_url + "/channel/0/settings", "GET", None, None, "Get NFM mod settings")
        if settings is None:
            exit(-1)
        
        settings["NFMModSettings"]["title"] = "Test NFM"
        settings["NFMModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["NFMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["NFMModSettings"]["cwKeyer"]["loop"] = 1
        settings["NFMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["NFMModSettings"]["modAFInput"] = 4 # CW text
        settings["NFMModSettings"]["toneFrequency"] = 600
        
        r = callAPI(deviceset_url + "/channel/0/settings", "PATCH", None, settings, "Change NFM mod")
        if r is None:
            exit(-1)
            
        deviceset_url = "/deviceset/%d" % deviceset_index_rx
            
        r = callAPI(deviceset_url + "/focus", "PATCH", None, None, "set focus on Rx device set")
        if r is None:
            exit(-1)
            
            
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

