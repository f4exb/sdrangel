#!/usr/bin/env python

import requests, json, traceback, sys, time
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
    parser.add_option("-C", "--channel-id", dest="channel_id", help="channel id", metavar="ID", type="string", default="NFMDemod")     
    parser.add_option("-F", "--device-freq", dest="device_freq", help="device center frequency (kHz)", metavar="FREQ", type="int") 
    parser.add_option("-f", "--channel-freq", dest="channel_freq", help="channel center frequency (Hz)", metavar="FREQ", type="int")
    parser.add_option("-U", "--copy-to-udp", dest="udp_copy", help="UDP audio copy to <address>[:<port>]", metavar="IP:PORT", type="string") 
    parser.add_option("-A", "--antenna-path", dest="antenna_path", help="antenna path index", metavar="INDEX", type="int")    
    parser.add_option("-s", "--sample-rate", dest="sample_rate", help="device to host sample rate (kS/s)", metavar="RATE", type="int")
    parser.add_option("-l", "--log2-decim", dest="log2_decim", help="log2 of the desired software decimation factor", metavar="LOG2", type="int", default=4)     
    parser.add_option("-b", "--af-bw", dest="af_bw", help="audio babdwidth (kHz)", metavar="FREQUENCY_KHZ", type="int" ,default=3) 
    parser.add_option("-r", "--rf-bw", dest="rf_bw", help="RF babdwidth (Hz). Sets to nearest available", metavar="FREQUENCY", type="int", default=10000)
    parser.add_option("--vol", dest="volume", help="audio volume", metavar="VOLUME", type="float", default=1.0)
    parser.add_option("-c", "--create", dest="create", help="create a new device set", metavar="CREATE", action="store_true", default=False)
    parser.add_option("--ppm", dest="lo_ppm", help="LO correction in PPM", metavar="PPM", type="float", default=0.0)    
    parser.add_option("--fc-pos", dest="fc_pos", help="Center frequency position 0:inf 1:sup 2:cen", metavar="ENUM", type="int", default=2) 
    parser.add_option("--sq", dest="squelch_db", help="Squelsch threshold in dB", metavar="DECIBEL", type="float", default=-50.0)
    parser.add_option("--sq-gate", dest="squelch_gate", help="Squelsch gate in ms", metavar="MILLISECONDS", type="int", default=50)
    parser.add_option("--stereo", dest="stereo", help="Broadcast FM stereo", metavar="BOOL", action="store_true", default=False)
    parser.add_option("--lsb-stereo", dest="lsb_stereo", help="Broadcast FM LSB stereo", metavar="BOOL", action="store_true", default=False)
    parser.add_option("--rds", dest="rds", help="Broadcast FM RDS", metavar="BOOL", action="store_true", default=False)
    parser.add_option("--audio-name", dest="audio_name", help="Audio: audio device name", metavar="STRING", type="string")
    parser.add_option("--audio-udp", dest="audio_udp", help="Audio: set copy to UDP", metavar="BOOL_INT", type="int")
    parser.add_option("--audio-rtp", dest="audio_rtp", help="Audio: use RTP over UDP", metavar="BOOL_INT", type="int")
    parser.add_option("--audio-address", dest="audio_address", help="Audio: UDP destination address", metavar="IP_ADDRESS", type="string")
    parser.add_option("--audio-port", dest="audio_port", help="Audio: UDP destination port", metavar="IP_PORT", type="int")
    parser.add_option("--audio-channels", dest="audio_channels", help="Audio: UDP mode (0: L only 1: R only 2: L+R mono 3: LR stereo)", metavar="ENUM_INT", type="int")
    parser.add_option("--baud-rate", dest="baud_rate", help="DSD: baud rate in Baud", metavar="BAUD", type="int", default=4800)
    parser.add_option("--fm-dev", dest="fm_deviation", help="DSD: expected FM deviation", metavar="FREQ", type="int", default=5400)
    
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
        #print(base_url, url, json)
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
def setup_audio(options):
    audio_dict = {}
    if options.audio_name: # must not be None and reference a valid audio device
        audio_dict["name"] = options.audio_name
    if options.audio_udp:
        audio_dict["copyToUDP"] = 0 if options.audio_udp == 0 else 1
    if options.audio_rtp:
        audio_dict["udpUsesRTP"] = 0 if options.audio_rtp == 0 else 1
    if options.audio_address:
        audio_dict["udpAddress"] = options.audio_address
    if options.audio_port:
        audio_dict["udpPort"] = options.audio_port
    if options.audio_channels:
        audio_dict["udpChannelMode"] =  0 if options.audio_channels < 0 else 3 if options.audio_channels > 3 else options.audio_channels
    
    r = callAPI('/audio/output/parameters', "PATCH", None, audio_dict, "setup audio {}".format(options.audio_name))
    if r is None:
        exit(-1)

# ======================================================================
def setupDevice(deviceset_url, options):
        r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": "%s" % options.device_hwid, "tx": 0}, "setup device on Rx device set")
        if r is None:
            exit(-1)
            
        settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
        if settings is None:
            exit(-1)

        # calculate RF analog and FIR optimal bandpass filters bandwidths
        lpFIRBW = options.sample_rate*1000 / (1<<options.log2_decim)
        if options.fc_pos == 2: # center of passband
            lpfBW = options.sample_rate*1000 / (1<<options.log2_decim)
        else: # side of passband
            if options.log2_decim == 0:
                lpfBW = options.sample_rate*1000
            elif options.log2_decim == 1:
                lpfBW = options.sample_rate*1000
            elif options.log2_decim == 2:
                lpfBW = options.sample_rate*500
            else:
                lpfBW = options.sample_rate*750

        if options.device_hwid == "Airspy":
            settings['airspySettings']['LOppmTenths'] = int(options.lo_ppm * 10) # in tenths of PPM
            settings["airspySettings"]["centerFrequency"] = options.device_freq*1000
            settings["airspySettings"]["devSampleRateIndex"] = 1
            settings['airspySettings']['log2Decim'] = options.log2_decim
            settings['airspySettings']['fcPos'] = options.fc_pos
            settings['airspySettings']['dcBlock'] = options.fc_pos == 2
            settings['airspySettings']['iqImbalance'] = options.fc_pos == 2
            settings['airspySettings']['lnaGain'] = 14
            settings['airspySettings']['mixerGain'] = 15
            settings['airspySettings']['vgaGain'] = 4
            settings['airspySettings']['lnaAGC'] = 1
            settings['airspySettings']['mixerAGC'] = 1
        elif options.device_hwid == "AirspyHF":
            if options.device_freq > 30000:
                settings["airspyHFSettings"]["bandIndex"] = 1
            else:
                settings["airspyHFSettings"]["bandIndex"] = 0
            settings["airspyHFSettings"]["centerFrequency"] = options.device_freq*1000
            settings["airspyHFSettings"]["devSampleRateIndex"] = 0
            settings['airspyHFSettings']['log2Decim'] = options.log2_decim
            settings['airspyHFSettings']['LOppmTenths'] = int(options.lo_ppm * 10)  # in tenths of PPM
        elif options.device_hwid == "LimeSDR":
            settings["limeSdrInputSettings"]["antennaPath"] = options.antenna_path
            settings["limeSdrInputSettings"]["devSampleRate"] = options.sample_rate*1000
            settings["limeSdrInputSettings"]["log2HardDecim"] = 4
            settings["limeSdrInputSettings"]["log2SoftDecim"] = options.log2_decim
            settings["limeSdrInputSettings"]["centerFrequency"] = options.device_freq*1000 + 500000
            settings["limeSdrInputSettings"]["ncoEnable"] = 1
            settings["limeSdrInputSettings"]["ncoFrequency"] = -500000
            settings["limeSdrInputSettings"]["lpfBW"] = lpfBW
            settings["limeSdrInputSettings"]["lpfFIRBW"] = lpFIRBW
            settings["limeSdrInputSettings"]["lpfFIREnable"] = 1
            settings['limeSdrInputSettings']['dcBlock'] = 1
        elif options.device_hwid == "PlutoSDR":
            settings["plutoSdrInputSettings"]["antennaPath"] = options.antenna_path
            settings["plutoSdrInputSettings"]["devSampleRate"] = options.sample_rate*1000
            settings["plutoSdrInputSettings"]["lpfFIRlog2Decim"] = 1
            settings["plutoSdrInputSettings"]["log2Decim"] = options.log2_decim
            settings["plutoSdrInputSettings"]["centerFrequency"] = options.device_freq*1000
            settings["plutoSdrInputSettings"]["lpfBW"] = lpfBW
            settings["plutoSdrInputSettings"]["lpfFIRBW"] = lpFIRBW
            settings["plutoSdrInputSettings"]["lpfFIREnable"] = 1
            settings['plutoSdrInputSettings']['fcPos'] = options.fc_pos
            settings['plutoSdrInputSettings']['dcBlock'] = options.fc_pos == 2
            settings['plutoSdrInputSettings']['iqImbalance'] = options.fc_pos == 2
            settings['plutoSdrInputSettings']['LOppmTenths'] = int(options.lo_ppm * 10)  # in tenths of PPM
        elif options.device_hwid == "RTLSDR":
            settings['rtlSdrSettings']['devSampleRate'] = options.sample_rate*1000
            settings['rtlSdrSettings']['centerFrequency'] = options.device_freq*1000
            settings['rtlSdrSettings']['gain'] = 496
            settings['rtlSdrSettings']['log2Decim'] = options.log2_decim
            settings['rtlSdrSettings']['fcPos'] = options.fc_pos
            settings['rtlSdrSettings']['dcBlock'] = options.fc_pos == 2
            settings['rtlSdrSettings']['iqImbalance'] = options.fc_pos == 2
            settings['rtlSdrSettings']['agc'] = 1
            settings['rtlSdrSettings']['loPpmCorrection'] = int(options.lo_ppm)
        elif options.device_hwid == "HackRF":
            settings['hackRFInputSettings']['LOppmTenths'] = int(options.lo_ppm * 10) # in tenths of PPM
            settings['hackRFInputSettings']['centerFrequency'] = options.device_freq*1000
            settings['hackRFInputSettings']['fcPos'] = options.fc_pos            
            settings['hackRFInputSettings']['dcBlock'] = options.fc_pos == 2
            settings['hackRFInputSettings']['iqImbalance'] = options.fc_pos == 2
            settings['hackRFInputSettings']['devSampleRate'] = options.sample_rate*1000
            settings['hackRFInputSettings']['lnaExt'] = 1
            settings['hackRFInputSettings']['lnaGain'] = 32
            settings['hackRFInputSettings']['log2Decim'] = options.log2_decim
            settings['hackRFInputSettings']['vgaGain'] = 24
        elif options.device_hwid == "Perseus":
            settings['perseusSettings']['LOppmTenths'] = int(options.lo_ppm * 10) # in tenths of PPM
            settings['perseusSettings']['centerFrequency'] = options.device_freq*1000
            settings["perseusSettings"]["devSampleRateIndex"] = 0
            settings['perseusSettings']['log2Decim'] = options.log2_decim
            settings['perseusSettings']['adcDither'] = 0
            settings['perseusSettings']['adcPreamp'] = 0
            settings['perseusSettings']['wideBand'] = 0
            settings['perseusSettings']['attenuator'] = 0
        
        r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device settings")
        if r is None:
            exit(-1)

# ======================================================================
def setupChannel(deviceset_url, options):
    i = 0
    
    settings = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": options.channel_id, "tx": 0}, "Create demod")
    if settings is None:
        exit(-1)

    settings = callAPI(deviceset_url + "/channel/%d/settings" % i, "GET", None, None, "Get demod settings")
    if settings is None:
        exit(-1)

    if options.channel_id == "NFMDemod":
        settings["NFMDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["NFMDemodSettings"]["afBandwidth"] = options.af_bw * 1000
        settings["NFMDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["NFMDemodSettings"]["volume"] = options.volume
        settings["NFMDemodSettings"]["squelch"] = options.squelch_db * 10 # centi-Bels
        settings["NFMDemodSettings"]["squelchGate"] = options.squelch_gate / 10 # 10's of ms
        settings["NFMDemodSettings"]["title"] = "Channel %d" % i
    elif options.channel_id == "BFMDemod":
        settings["BFMDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["BFMDemodSettings"]["afBandwidth"] = options.af_bw * 1000
        settings["BFMDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["BFMDemodSettings"]["volume"] = options.volume
        settings["BFMDemodSettings"]["squelch"] = options.squelch_db # dB
        settings["BFMDemodSettings"]["audioStereo"] = 1 if options.stereo else 0
        settings["BFMDemodSettings"]["lsbStereo"] = 1 if options.lsb_stereo else 0
        settings["BFMDemodSettings"]["rdsActive"] = 1 if options.rds else 0
        settings["BFMDemodSettings"]["title"] = "Channel %d" % i
    elif options.channel_id == "WFMDemod":
        settings["WFMDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["WFMDemodSettings"]["afBandwidth"] = options.af_bw * 1000
        settings["WFMDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["WFMDemodSettings"]["volume"] = options.volume
        settings["WFMDemodSettings"]["squelch"] = options.squelch_db # dB
        settings["WFMDemodSettings"]["audioMute"] = 0
        settings["WFMDemodSettings"]["title"] = "Channel %d" % i
    elif options.channel_id == "AMDemod":
        settings["AMDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["AMDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["AMDemodSettings"]["volume"] = options.volume
        settings["AMDemodSettings"]["squelch"] = options.squelch_db
        settings["AMDemodSettings"]["title"] = "Channel %d" % i
        settings["AMDemodSettings"]["bandpassEnable"] = 1 # bandpass filter
    elif options.channel_id == "SSBDemod":
        settings["SSBDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["SSBDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["SSBDemodSettings"]["lowCutoff"] = -300 if options.rf_bw < 0 else 300
        settings["SSBDemodSettings"]["audioBinaural"] = 1
        settings["SSBDemodSettings"]["audioFlipChannels"] = 0
        settings["SSBDemodSettings"]["dsb"] = 0
        settings["SSBDemodSettings"]["audioMute"] = 0
        settings["SSBDemodSettings"]["agc"] = 1
        settings["SSBDemodSettings"]["agcClamping"] = 0
        settings["SSBDemodSettings"]["agcTimeLog2"] = 8
        settings["SSBDemodSettings"]["agcPowerThreshold"] = options.squelch_db
        settings["SSBDemodSettings"]["agcThresholdGate"] = 4
        settings["SSBDemodSettings"]["volume"] = options.volume
        settings["SSBDemodSettings"]["title"] = "Channel %d" % i
    elif options.channel_id == "DSDDemod":
        settings["DSDDemodSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["DSDDemodSettings"]["rfBandwidth"] = options.rf_bw
        settings["DSDDemodSettings"]["volume"] = options.volume
        settings["DSDDemodSettings"]["squelch"] = options.squelch_db
        settings["DSDDemodSettings"]["baudRate"] = options.baud_rate
        settings["DSDDemodSettings"]["fmDeviation"] = options.fm_deviation
        settings["DSDDemodSettings"]["enableCosineFiltering"] = 1
        settings["DSDDemodSettings"]["pllLock"] = 1
        settings["DSDDemodSettings"]["title"] = "Channel %d" % i
    elif options.channel_id == "UDPSrc":
        settings["UDPSrcSettings"]["inputFrequencyOffset"] = options.channel_freq
        settings["UDPSrcSettings"]["rfBandwidth"] = options.rf_bw
        settings["UDPSrcSettings"]["volume"] = options.volume
        settings["UDPSrcSettings"]["squelchDB"] = options.squelch_db
        settings["UDPSrcSettings"]["channelMute"] = 0
        settings["UDPSrcSettings"]["title"] = "Channel %d" % i
    
    r = callAPI(deviceset_url + "/channel/%d/settings" % i, "PATCH", None, settings, "Change demod")
    if r is None:
        exit(-1)

# ======================================================================
def channelsReport(deviceset_url):
    report = callAPI(deviceset_url + "/channels/report", "GET", None, None, "Get channels report")


# ======================================================================
def main():
    try:
        options = getInputOptions()
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        deviceset_url = "/deviceset/%d" % options.device_index
        
        if options.create:
            r = callAPI("/deviceset", "POST", {"tx": 0}, None, "Add Rx device set")
            if r is None:
                exit(-1)
            
        setupDevice(deviceset_url, options)    
        setupChannel(deviceset_url, options)
                        
        r = callAPI(deviceset_url + "/device/run", "POST", None, None, "Start running device")
        if r is None:
            exit(-1)
        
        if options.audio_name:
            time.sleep(1)
            setup_audio(options)
        
#         if options.channel_id == "BFMDemod":
#             channelsReport(deviceset_url)
        
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

