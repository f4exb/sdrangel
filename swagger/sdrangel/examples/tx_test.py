#!/usr/bin/env python

import requests, json, traceback, sys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"
deviceset_url = ""

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
    parser.add_option("-C", "--channel-id", dest="channel_id", help="channel id", metavar="ID", type="string", default="NFMDemod") 
    parser.add_option("-F", "--device-freq", dest="device_freq", help="device center frequency (kHz)", metavar="FREQ", type="int") 
    parser.add_option("-f", "--channel-freq", dest="channel_freq", help="channel center frequency (Hz)", metavar="FREQ", type="int")
    parser.add_option("-s", "--sample-rate", dest="sample_rate", help="host to device sample rate (S/s)", metavar="RATE", type="int")
    parser.add_option("-l", "--log2-interp", dest="log2_interp", help="log2 of interpolation factor", metavar="RATE", type="int")
    parser.add_option("-L", "--log2-interp-hard", dest="log2_interp_hard", help="log2 of hardware interpolation factor", metavar="RATE", type="int")
    parser.add_option("-A", "--antenna-path", dest="antenna_path", help="antenna path number", metavar="NUMBER", type="int")
    parser.add_option("-c", "--create", dest="create", help="create a new device set", metavar="CREATE", action="store_true", default=False)
    parser.add_option("--ppm", dest="lo_ppm", help="LO correction in ppm", metavar="FILENAME", type="float", default=0)
    parser.add_option("--image", dest="image_file", help="image file for ATV modulator (sends image)", metavar="FILENAME", type="string")
    parser.add_option("--video", dest="video_file", help="video file for ATV modulator (sends video)", metavar="FILENAME", type="string")

    (options, args) = parser.parse_args()
    
    if options.address == None:
        options.address = "127.0.0.1:8091"
    
    if options.device_index == None:
        options.device_index = 1
        
    if options.device_hwid == None:
        options.device_hwid = "FileSource"
    
    if options.device_freq == None:
        options.device_freq = 435000
    
    if options.channel_freq == None:
        options.channel_freq = 0
        
    if options.sample_rate == None:
        options.sample_rate = 2600000
    
    if options.log2_interp == None:
        options.log2_interp = 4

    if options.log2_interp_hard == None:
        options.log2_interp = 4

    if options.antenna_path == None:
        options.antenna_path = 1

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
def setupBladeRFXB200(fc):
    if fc < 50000:
        return 5 # BLADERF_XB200_AUTO_3DB
    elif fc < 54000:
        return 0 # BLADERF_XB200_50M
    elif fc < 144000:
        return 5 # BLADERF_XB200_AUTO_3DB
    elif fc < 148000:
        return 1 # BLADERF_XB200_144M
    elif fc < 222000:
        return 5 # BLADERF_XB200_AUTO_3DB
    elif fc < 225000:
        return 2 # BLADERF_XB200_222M
    else:
        return 5 # BLADERF_XB200_AUTO_3DB

# ======================================================================
def setupDevice(options):
    settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
    if settings is None:
        exit(-1)

    # calculate RF analog and FIR optimal bandpass filters bandwidths
    lpFIRBW = options.sample_rate / (1<<options.log2_interp)
    lpfBW = lpFIRBW * 1.2

    if options.device_hwid == "BladeRF":
        settings['bladeRFOutputSettings']['centerFrequency'] = options.device_freq*1000
        settings['bladeRFOutputSettings']['devSampleRate'] = options.sample_rate
        settings['bladeRFOutputSettings']['vga1'] = -20
        settings['bladeRFOutputSettings']['vga2'] = 6
        settings['bladeRFOutputSettings']['bandwidth'] = 1500*1000
        settings['bladeRFOutputSettings']['log2Interp'] = options.log2_interp
        settings['bladeRFOutputSettings']['xb200'] = 1 # assume XB200 is mounted
        settings['bladeRFOutputSettings']['xb200Path'] = 1 if options.device_freq < 300000 else 0
        settings['bladeRFOutputSettings']['xb200Filter'] = setupBladeRFXB200(options.device_freq)
    elif options.device_hwid == "LimeSDR":
        settings["limeSdrOutputSettings"]["antennaPath"] = options.antenna_path
        settings["limeSdrOutputSettings"]["devSampleRate"] = options.sample_rate
        settings["limeSdrOutputSettings"]["log2HardInterp"] = options.log2_interp_hard
        settings["limeSdrOutputSettings"]["log2SoftInterp"] = options.log2_interp
        settings["limeSdrOutputSettings"]["centerFrequency"] = options.device_freq*1000  + 500000
        settings["limeSdrOutputSettings"]["ncoEnable"] = 1
        settings["limeSdrOutputSettings"]["ncoFrequency"] = -500000        
        settings["limeSdrOutputSettings"]["lpfBW"] = 4050000
        settings["limeSdrOutputSettings"]["lpfFIRBW"] = 100000
        settings["limeSdrOutputSettings"]["lpfFIREnable"] = 1
        settings["limeSdrOutputSettings"]["gain"] = 30
    elif options.device_hwid == "PlutoSDR":
        settings["plutoSdrOutputSettings"]["antennaPath"] = options.antenna_path
        settings["plutoSdrOutputSettings"]["devSampleRate"] = options.sample_rate
        settings["plutoSdrOutputSettings"]["lpfFIRlog2Interp"] = options.log2_interp_hard
        settings["plutoSdrOutputSettings"]["log2Interp"] = options.log2_interp
        settings["plutoSdrOutputSettings"]["centerFrequency"] = options.device_freq*1000
        settings["plutoSdrOutputSettings"]["lpfBW"] = lpfBW
        settings["plutoSdrOutputSettings"]["lpfFIRBW"] = lpFIRBW
        settings["plutoSdrOutputSettings"]["lpfFIREnable"] = 1
        settings["plutoSdrOutputSettings"]["att"] = -24 # -6 dB
    elif options.device_hwid == "HackRF":
        settings['hackRFOutputSettings']['LOppmTenths'] = round(options.lo_ppm*10)
        settings['hackRFOutputSettings']['centerFrequency'] = options.device_freq*1000
        settings['hackRFOutputSettings']['devSampleRate'] = options.sample_rate
        settings['hackRFOutputSettings']['lnaExt'] = 0
        settings['hackRFOutputSettings']['log2Interp'] = options.log2_interp
        settings['hackRFOutputSettings']['vgaGain'] = 24
    
    r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device settings")
    if r is None:
        exit(-1)

# ======================================================================
def setupChannel(options):
    r = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": options.channel_id, "tx": 1}, "Create modulator")
    if r is None:
        exit(-1)
    
    settings = callAPI(deviceset_url + "/channel/0/settings", "GET", None, None, "Get modulator settings")
    if settings is None:
        exit(-1)
    
    if options.channel_id == "NFMMod":
        settings["NFMModSettings"]["title"] = "Test NFM"
        settings["NFMModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["NFMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["NFMModSettings"]["cwKeyer"]["loop"] = 1
        settings["NFMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["NFMModSettings"]["modAFInput"] = 4 # CW text
        settings["NFMModSettings"]["toneFrequency"] = 600
    elif options.channel_id == "AMMod":
        settings["AMModSettings"]["title"] = "Test AM"
        settings["AMModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["AMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["AMModSettings"]["cwKeyer"]["loop"] = 1
        settings["AMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["AMModSettings"]["modAFInput"] = 4 # CW text
        settings["AMModSettings"]["toneFrequency"] = 600
        settings["AMModSettings"]["modFactor"] = 0.9
        settings["AMModSettings"]["rfBandwidth"] = 7500
    elif options.channel_id == "ATVMod":
        settings["ATVModSettings"]["title"] = "Test ATV"
        settings["ATVModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["ATVModSettings"]["rfBandwidth"] = 30000
        settings["ATVModSettings"]["forceDecimator"] = 1 # This is to engage filter
        
        if options.image_file is not None:
            settings["ATVModSettings"]["imageFileName"] = options.image_file
            settings["ATVModSettings"]["atvModInput"] = 6   # m_atvModulation
        elif options.video_file is not None:
            settings["ATVModSettings"]["videoFileName"] = options.video_file
            settings["ATVModSettings"]["atvModInput"] = 7   # ATVModInputVideo            
            settings["ATVModSettings"]["videoPlayLoop"] = 1
            settings["ATVModSettings"]["videoPlay"] = 1
        else:
            settings["ATVModSettings"]["atvModInput"] = 1   # ATVModInputHBars
            
        settings["ATVModSettings"]["atvStd"] = 5        # ATVStdHSkip
        settings["ATVModSettings"]["atvModulation"] = 1 # ATVModulationFM
        settings["ATVModSettings"]["fps"] = 2
        settings["ATVModSettings"]["nbLines"] = 90
        settings["ATVModSettings"]["uniformLevel"] = 1.0 # 100% white
        settings["ATVModSettings"]["fmExcursion"] = 0.2 # FM excursion is 20% of channel bandwidth
        settings["ATVModSettings"]["overlayText"] = "F4EXB"
        settings["ATVModSettings"]["showOverlayText"] = 1 
    elif options.channel_id == "SSBMod":
        settings["SSBModSettings"]["title"] = "Test SSB"
        settings["SSBModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["SSBModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["SSBModSettings"]["cwKeyer"]["loop"] = 1
        settings["SSBModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["SSBModSettings"]["modAFInput"] = 4 # CW text
        settings["SSBModSettings"]["toneFrequency"] = 600
        settings["SSBModSettings"]["bandwidth"] = 1000
        settings["SSBModSettings"]["lowCut"] = 300
    elif options.channel_id == "UDPSink":
        settings["UDPSinkSettings"]["title"] = "Test UDP Sink"
        settings["UDPSinkSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["UDPSinkSettings"]["rfBandwidth"] = 12500
        settings["UDPSinkSettings"]["fmDeviation"] = 6000
        settings["UDPSinkSettings"]["autoRWBalance"] = 1
        settings["UDPSinkSettings"]["stereoInput"] = 0
        settings["UDPSinkSettings"]["udpAddress"] = "127.0.0.1"
        settings["UDPSinkSettings"]["udpPort"] = 9998
        settings["UDPSinkSettings"]["inputSampleRate"] = 48000
        settings["UDPSinkSettings"]["sampleFormat"] = 1 # FormatNFM
        settings["UDPSinkSettings"]["gainIn"] = 2.5
        settings["UDPSinkSettings"]["gainOut"] = 2.8
    elif options.channel_id == "WFMMod":
        settings["WFMModSettings"]["title"] = "Test WFM"
        settings["WFMModSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["WFMModSettings"]["cwKeyer"]["text"] = "VVV DE F4EXB  "
        settings["WFMModSettings"]["cwKeyer"]["loop"] = 1
        settings["WFMModSettings"]["cwKeyer"]["mode"] = 1 # text
        settings["WFMModSettings"]["modAFInput"] = 4 # CW text
        settings["WFMModSettings"]["toneFrequency"] = 600
        settings["WFMModSettings"]["fmDeviation"] = 25000
        settings["WFMModSettings"]["rfBandwidth"] = 75000
        settings["WFMModSettings"]["afBandwidth"] = 3000
    
    r = callAPI(deviceset_url + "/channel/0/settings", "PATCH", None, settings, "Change modulator")
    if r is None:
        exit(-1)
    

# ======================================================================
def main():
    try:
        options = getInputOptions()
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        
        if options.create:
            r = callAPI("/deviceset", "POST", {"tx": 1}, None, "Add Tx device set")
            if r is None:
                exit(-1)
        
        global deviceset_url   
        deviceset_url = "/deviceset/%d" % options.device_index
        
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid, "tx": 1}, "setup device on Tx device set")
        if r is None:
            exit(-1)
            
        setupDevice(options)
        setupChannel(options)
            
        r = callAPI(deviceset_url + "/device/run", "POST", None, None, "Start running device")
        if r is None:
            exit(-1)
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

