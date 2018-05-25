#!/usr/bin/env python

"""
  SDRangel REST API client script
  
  Simple scanner for AM and NFM channels. Builds an array of equally spaced channels. Moves device center frequency
  so that adjacent parts of the spectrum are scanned by the array of channels. Stops when any of the channels
  is active. Resumes when none of the channels is active.
  
  Uses /sdrangel/deviceset/{deviceSetIndex}/channels/report API to get channel information (since v3.13.1)
"""

import requests, json, traceback, sys
from optparse import OptionParser
import time
import datetime
import numpy as np

base_url = "http://127.0.0.1:8091/sdrangel"
deviceset_url = ""
verbosity = 2

requests_methods = {
    "GET": requests.get,
    "PATCH": requests.patch,
    "POST": requests.post,
    "PUT": requests.put,
    "DELETE": requests.delete
}

# ======================================================================
class ScanControl:
    def __init__(self, num_channels, channel_step, start_freq, stop_freq, log2_decim):        
        self.channel_shifts = []
        if num_channels < 2:
            self.channel_shifts = [0]
            limit = 0
        else:
            limit = ((num_channels-1)*channel_step) / 2
            self.channel_shifts = list(np.linspace(-limit, limit, num_channels))
        self.device_start_freq = start_freq + limit 
        self.device_stop_freq = stop_freq - limit
        self.device_step_freq = 2*limit + channel_step
        self.device_sample_rate = (2*limit + channel_step)*(1<<log2_decim)

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string") 
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int", default=0) 
    parser.add_option("-D", "--device-hwid", dest="device_hwid", help="device hardware id", metavar="ID", type="string", default="RTLSDR")
    parser.add_option("-C", "--channel-id", dest="channel_id", help="channel id", metavar="ID", type="string", default="NFMDemod") 
    parser.add_option("-l", "--log2-decim", dest="log2_decim", help="log2 of the desired software decimation factor", metavar="LOG2", type="int", default=4) 
    parser.add_option("-n", "--num-channels", dest="num_channels", help="number of parallel channels", metavar="NUMBER", type="int", default=8) 
    parser.add_option("-s", "--freq-step", dest="freq_step", help="frequency step (Hz)", metavar="FREQUENCY", type="float", default=12500) 
    parser.add_option("-S", "--freq-start", dest="freq_start", help="frequency start (Hz)", metavar="FREQUENCY", type="int", default=446006250) 
    parser.add_option("-T", "--freq-stop", dest="freq_stop", help="frequency stop (Hz)", metavar="FREQUENCY", type="int", default=446193750) 
    parser.add_option("-b", "--af-bw", dest="af_bw", help="audio babdwidth (kHz)", metavar="FREQUENCY_KHZ", type="int" ,default=3) 
    parser.add_option("-r", "--rf-bw", dest="rf_bw", help="RF babdwidth (Hz). Sets to nearest available", metavar="FREQUENCY", type="int", default=10000)
    parser.add_option("--vol", dest="volume", help="audio volume", metavar="VOLUME", type="float", default=1.0)
    parser.add_option("-c", "--create", dest="create", help="create a new device set", metavar="BOOLEAN", action="store_true", default=False)
    parser.add_option("-m", "--mock", dest="mock", help="just print calculated values and exit", metavar="BOOLEAN", action="store_true", default=False)
    parser.add_option("--ppm", dest="lo_ppm", help="LO correction in PPM", metavar="PPM", type="float", default=0.0)
    parser.add_option("--fc-pos", dest="fc_pos", help="Center frequency position 0:inf 1:sup 2:cen", metavar="ENUM", default=2) 
    parser.add_option("-t", "--settling-time", dest="settling_time", help="Scan step settling time in seconds", metavar="SECONDS", type="float", default=1.0)
    parser.add_option("--sq", dest="squelch_db", help="Squelsch threshold in dB", metavar="DECIBEL", type="float", default=-50.0)
    parser.add_option("--sq-gate", dest="squelch_gate", help="Squelsch gate in ms", metavar="MILLISECONDS", type="int", default=50)
    parser.add_option("--baud-rate", dest="baud_rate", help="Baud rate for digial modulation (DV)", metavar="RATE", type="int", default=4800)
    parser.add_option("--fm-dev", dest="fm_dev", help="FM deviation for FM digial modulation (DV)", metavar="FREQUENCY", type="int", default=5400)
    parser.add_option("--re-run", dest="rerun", help="re run with given parameters without setting up device and channels", metavar="BOOLEAN", action="store_true", default=False)
    parser.add_option("-x", "--excl-list", dest="excl_fstr", help="frequencies (in Hz) exclusion comma separated list", metavar="LIST", type="string")
    parser.add_option("--excl-tol", dest="excl_tol", help="match tolerance interval (in Hz) for exclusion frequencies", metavar="FREQUENCY", type="float", default=10.0) 
    parser.add_option("-v", "--verbosity", dest="verbosity", help="verbosity level", metavar="LEVEL_INT", type="int", default = 0) 
    parser.add_option("-L", "--delay", dest="delay", help="delay in number of settling time periods before resuming scan", metavar="NUMBER", type="int", default = 5) 

    (options, args) = parser.parse_args()
    
    if (options.address == None):
        options.address = "127.0.0.1:8091"
    
    if options.excl_fstr is not None:
        excl_flist_str = options.excl_fstr.split(',')
        try:
            options.excl_flist = list(map(lambda x:round(float(x)/options.excl_tol), excl_flist_str))
            print(options.excl_flist)
        except ValueError:
            print("Invalid exclusion frequencies list: %s" % options.excl_fstr)
            options.excl_flist = []
    else:
        options.excl_flist = []
    
    if options.verbosity > 2:
        options.verbosity = 2
    
    return options

# ======================================================================
def setupDevice(scan_control, options):
    settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
    if settings is None:
        exit(-1)
    
    if options.device_hwid == "AirspyHF":
        if scan_control.device_start_freq > 30000000:
            settings["airspyHFSettings"]["bandIndex"] = 1
        else:
            settings["airspyHFSettings"]["bandIndex"] = 0
        settings["airspyHFSettings"]["centerFrequency"] = scan_control.device_start_freq
        settings["airspyHFSettings"]["devSampleRateIndex"] = 0
        settings['airspyHFSettings']['log2Decim'] = options.log2_decim
        settings['airspyHFSettings']['LOppmTenths'] = int(options.lo_ppm * 10)  # in tenths of PPM
    elif options.device_hwid == "LimeSDR":
        settings["limeSdrInputSettings"]["antennaPath"] = 0
        settings["limeSdrInputSettings"]["devSampleRate"] = scan_control.device_sample_rate
        settings["limeSdrInputSettings"]["log2HardDecim"] = 4
        settings["limeSdrInputSettings"]["log2SoftDecim"] = options.log2_decim
        settings["limeSdrInputSettings"]["centerFrequency"] = scan_control.device_start_freq + 500000
        settings["limeSdrInputSettings"]["ncoEnable"] = 1
        settings["limeSdrInputSettings"]["ncoFrequency"] = -500000
        settings["limeSdrInputSettings"]["lpfBW"] = 1450000
        settings["limeSdrInputSettings"]["lpfFIRBW"] = scan_control.device_step_freq + 100000
        settings["limeSdrInputSettings"]["lpfFIREnable"] = 1
        settings['limeSdrInputSettings']['dcBlock'] = 1
    elif options.device_hwid == "RTLSDR":
        settings['rtlSdrSettings']['devSampleRate'] = scan_control.device_sample_rate
        settings['rtlSdrSettings']['centerFrequency'] = scan_control.device_start_freq
        settings['rtlSdrSettings']['gain'] = 496
        settings['rtlSdrSettings']['log2Decim'] = options.log2_decim
        settings['rtlSdrSettings']['fcPos'] = options.fc_pos
        settings['rtlSdrSettings']['dcBlock'] = options.fc_pos == 2
        settings['rtlSdrSettings']['iqImbalance'] = options.fc_pos == 2
        settings['rtlSdrSettings']['agc'] = 1
        settings['rtlSdrSettings']['loPpmCorrection'] = int(options.lo_ppm)
        settings['rtlSdrSettings']['rfBandwidth'] = scan_control.device_step_freq + 100000
    elif options.device_hwid == "HackRF":
        settings['hackRFInputSettings']['LOppmTenths'] = int(options.lo_ppm * 10) # in tenths of PPM
        settings['hackRFInputSettings']['centerFrequency'] = scan_control.device_start_freq
        settings['hackRFInputSettings']['fcPos'] = options.fc_pos
        settings['hackRFInputSettings']['dcBlock'] = options.fc_pos == 2
        settings['hackRFInputSettings']['iqImbalance'] = options.fc_pos == 2
        settings['hackRFInputSettings']['devSampleRate'] = scan_control.device_sample_rate
        settings['hackRFInputSettings']['lnaExt'] = 1
        settings['hackRFInputSettings']['lnaGain'] = 32
        settings['hackRFInputSettings']['log2Decim'] = options.log2_decim
        settings['hackRFInputSettings']['vgaGain'] = 24

    r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device settings")
    if r is None:
        exit(-1)
        
# ======================================================================
def changeDeviceFrequency(fc, options):
    settings = callAPI(deviceset_url + "/device/settings", "GET", None, None, "Get device settings")
    if settings is None:
        exit(-1)

    if options.device_hwid == "AirspyHF":
        settings["airspyHFSettings"]["centerFrequency"] = fc
    elif options.device_hwid == "LimeSDR":
        settings["limeSdrInputSettings"]["centerFrequency"] = fc + 500000
    elif options.device_hwid == "RTLSDR":
        settings['rtlSdrSettings']['centerFrequency'] = fc
    elif options.device_hwid == "HackRF":
        settings['hackRFInputSettings']['centerFrequency'] = fc

    r = callAPI(deviceset_url + "/device/settings", "PATCH", None, settings, "Patch device center frequncy")
    if r is None:
        exit(-1)

# ======================================================================
def setupChannels(scan_control, options):
    i = 0
    for shift in scan_control.channel_shifts:
        settings = callAPI(deviceset_url + "/channel", "POST", None, {"channelType": options.channel_id, "tx": 0}, "Create demod")
        if settings is None:
            exit(-1)

        settings = callAPI(deviceset_url + "/channel/%d/settings" % i, "GET", None, None, "Get demod settings")
        if settings is None:
            exit(-1)

        if options.channel_id == "NFMDemod":
            settings["NFMDemodSettings"]["inputFrequencyOffset"] = int(shift)
            settings["NFMDemodSettings"]["afBandwidth"] = options.af_bw * 1000
            settings["NFMDemodSettings"]["rfBandwidth"] = options.rf_bw
            settings["NFMDemodSettings"]["volume"] = options.volume
            settings["NFMDemodSettings"]["squelch"] = options.squelch_db * 10 # centi-Bels
            settings["NFMDemodSettings"]["squelchGate"] = options.squelch_gate / 10 # 10's of ms
            settings["NFMDemodSettings"]["title"] = "Channel %d" % i
        elif options.channel_id == "AMDemod":
            settings["AMDemodSettings"]["inputFrequencyOffset"] = int(shift)
            settings["AMDemodSettings"]["rfBandwidth"] = options.rf_bw
            settings["AMDemodSettings"]["volume"] = options.volume
            settings["AMDemodSettings"]["squelch"] = options.squelch_db
            settings["AMDemodSettings"]["title"] = "Channel %d" % i
            settings["AMDemodSettings"]["bandpassEnable"] = 1 # bandpass filter
        elif options.channel_id == "DSDDemod":
            settings["DSDDemodSettings"]["inputFrequencyOffset"] = int(shift)
            settings["DSDDemodSettings"]["rfBandwidth"] = options.rf_bw
            settings["DSDDemodSettings"]["volume"] = options.volume
            settings["DSDDemodSettings"]["squelch"] = options.squelch_db
            settings["DSDDemodSettings"]["baudRate"] = options.baud_rate
            settings["DSDDemodSettings"]["fmDeviation"] = options.fm_dev
            settings["DSDDemodSettings"]["enableCosineFiltering"] = 1
            settings["DSDDemodSettings"]["pllLock"] = 1
            settings["DSDDemodSettings"]["title"] = "Channel %d" % i
        
        r = callAPI(deviceset_url + "/channel/%d/settings" % i, "PATCH", None, settings, "Change demod")
        if r is None:
            exit(-1)
            
        i += 1        
    
# ======================================================================
def checkScanning(fc, options, display_message):
    reports = callAPI(deviceset_url + "/channels/report", "GET", None, None, "Get channels report")
    if reports is None:
        exit(-1)
    reportKey = "%sReport" % options.channel_id
    for i in range(reports["channelcount"]):
        channel = reports["channels"][i]
        if "report" in channel:
            if reportKey in channel["report"]:
                if options.channel_id == "DSDDemod": # DSD is special because it only stops on voice
                    stopCondition = channel["report"][reportKey]["slot1On"] == 1 or channel["report"][reportKey]["slot2On"] == 1
                else:
                    stopCondition = channel["report"][reportKey]["squelch"] == 1
                if stopCondition:
                    f_channel = channel["deltaFrequency"]+fc
                    f_frac = round(f_channel/options.excl_tol)
                    if f_frac not in options.excl_flist:
                        if display_message: # display message only when stopping for the first time
                            print("%s Stopped at %d Hz" % (datetime.datetime.now().strftime("%H:%M:%S"),f_frac*options.excl_tol))
                        return False # stop scanning
    return True # continue scanning
    
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
            if verbosity >= 1:
                print(text + " succeeded")
            if verbosity >= 2:
                printResponse(r)
            return r.json() # all 200 yield application/json response
        else:
            if verbosity >= 1:
                print(text + " failed")
            if verbosity >= 2:
                printResponse(r)
            return None

# ======================================================================
def main():
    try:
        options = getInputOptions()
        scan_control = ScanControl(options.num_channels, options.freq_step, options.freq_start, options.freq_stop, options.log2_decim)
        
        # Print calculated scan parameters

        print("Channel shifts: %s" % scan_control.channel_shifts)
        print("Sample rate: %d" % scan_control.device_sample_rate)
        print("Start: %d" % scan_control.device_start_freq)
        print("Stop: %d" % scan_control.device_stop_freq)
        print("Step: %d" % scan_control.device_step_freq)
        
        if scan_control.device_stop_freq < scan_control.device_start_freq:
            print("Frequency error")
            exit(1)
        
        freqs = []
        nb_steps = 0
        fc = scan_control.device_start_freq
        while fc <= scan_control.device_stop_freq:
            freqs += [x+fc for x in scan_control.channel_shifts]
            fc += scan_control.device_step_freq
            nb_steps += 1            
        print("Scanned frequencies: %s" % freqs)
        print("Skipped frequencies: %s" % options.excl_flist)
        print("In %d steps" % nb_steps)

        if options.mock: # Stop there if we are just mocking (no API access)
            exit(0)
        
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        
        # Set Rx       
            
        global deviceset_url
        deviceset_url = "/deviceset/%d" % options.device_index
        
        if not options.rerun: # Skip device and channels settings in re-run mode
            if options.create:
                r = callAPI("/deviceset", "POST", {"tx": 0}, None, "Add Rx device set")
                if r is None:
                    exit(-1)
            
            r = callAPI(deviceset_url + "/device", "PUT", None, {"hwType": options.device_hwid, "tx": 0}, "setup device on Rx device set")
            if r is None:
                exit(-1)
            
            # Set device and channels

            setupDevice(scan_control, options)
            setupChannels(scan_control, options)

        # Start running and scanning

        r = callAPI(deviceset_url + "/device/run", "POST", None, None, "Start running device")
        if r is None:
            exit(-1)
        fc = scan_control.device_start_freq

        global verbosity
        verbosity = options.verbosity      

        print("Move center to %d Hz" % fc)
        changeDeviceFrequency(fc, options)

        try:
            scanning = False
            resume_delay = 0
            while True:
                time.sleep(options.settling_time)
                scanning = checkScanning(fc, options, scanning and resume_delay == 0) # shall we move on ?
                if scanning:
                    if resume_delay > 0:
                        resume_delay -= 1
                    else:
                        fc += scan_control.device_step_freq
                        if fc > scan_control.device_stop_freq:
                            fc = scan_control.device_start_freq
                            if verbosity > 0:
                                print("New pass")
                        if verbosity > 0:
                            print("Move center to %d Hz" % fc)
                        changeDeviceFrequency(fc, options)
                else:
                    resume_delay = options.delay
        except KeyboardInterrupt:
            print("Terminated by user")
            pass
        finally:
            verbosity = 2      
            r = callAPI(deviceset_url + "/device/run", "DELETE", None, None, "Stop running device")
            if r is None:
                exit(-1)
            
    except KeyboardInterrupt:
        pass
    except Exception, msg:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()

