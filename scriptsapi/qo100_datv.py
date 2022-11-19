#!/usr/bin/env python3
# pylint: disable=line-too-long
"""
Quick control of configuration for receiving QO-100 DATV
"""

import traceback
import sys
from optparse import OptionParser # pylint: disable=deprecated-module
import requests
import sdrangel

BASE_URL_PATTERN = "http://{0}/sdrangel"
BASE_URL = None

# ======================================================================
def get_input_options():
    """ Parse options """
# ----------------------------------------------------------------------
    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="Address and port. Default: 127.0.0.1:8091", metavar="ADDRESS", type="string")
    parser.add_option("-d", "--device", dest="device_index", help="Index of device set. Default 0", metavar="INT", type="int")
    parser.add_option("-c", "--channel", dest="channel_index", help="Index of DATV demod channel. Default 0", metavar="INT", type="int")
    parser.add_option("-f", "--frequency", dest="frequency", help="Device center frequency (kHz). Mandatory", metavar="INT", type="int")
    parser.add_option("-s", "--symbol-rate", dest="symbol_rate", help="Symbol rate (kS/s). Mandatory", metavar="INT", type="int")
    parser.add_option("-n", "--no-decim", dest="no_decim", help="Do not change decimation", action="store_true")

    (options, args) = parser.parse_args()

    if options.address is None:
        options.address = "127.0.0.1:8091"
    if options.device_index is None:
        options.device_index = 0
    if options.channel_index is None:
        options.channel_index = 0
    if options.frequency is None:
        raise RuntimeError("Frequency (-f or --frequency) is mandatory")
    if options.symbol_rate is None:
        raise RuntimeError("Symbol rate (-s or --symbol-rate) is mandatory")

    return options

class Controller:
    """ Controller main class """
    def __init__(self, options):
        self.options = options
        self.device_hwtype = None

    # ======================================================================
    def change_device_center_frequency(self, content, new_frequency):
        """ Change device center frequency searching recursively """
    # ----------------------------------------------------------------------
        for k in content:
            if isinstance(content[k], dict):
                self.change_device_center_frequency(content[k], new_frequency)
            elif k == sdrangel.DEVICE_TYPES[self.device_hwtype]["cf_key"]:
                content[k] = new_frequency

    # ======================================================================
    def change_decim(self, content, new_decim):
        """ Change device log2 decimation searching recursively """
    # ----------------------------------------------------------------------
        for k in content:
            if isinstance(content[k], dict):
                self.change_decim(content[k], new_decim)
            elif k == sdrangel.DEVICE_TYPES[self.device_hwtype]["decim_key"]:
                content[k] = new_decim

    # ======================================================================
    def change_channel_center_frequency(self, content, new_frequency):
        """ Change DATV channel center frequency searching recursively """
    # ----------------------------------------------------------------------
        for k in content:
            if isinstance(content[k], dict):
                self.change_channel_center_frequency(content[k], new_frequency)
            elif k == "centerFrequency":
                content[k] = new_frequency

    # ======================================================================
    def change_rfbw(self, content, new_rfbw):
        """ Change DATV channel RF bandwidth searching recursively """
    # ----------------------------------------------------------------------
        for k in content:
            if isinstance(content[k], dict):
                self.change_rfbw(content[k], new_rfbw)
            elif k == "rfBandwidth":
                content[k] = new_rfbw

    # ======================================================================
    def change_symbol_rate(self, content, new_symbol_rate):
        """ Change DATV channel symbol rate searching recursively """
    # ----------------------------------------------------------------------
        for k in content:
            if isinstance(content[k], dict):
                self.change_symbol_rate(content[k], new_symbol_rate)
            elif k == "symbolRate":
                content[k] = new_symbol_rate

    # ======================================================================
    def get_device_sr_and_decim(self, hwtype, settings):
        """ Return device sample rate and decimation """
    # ----------------------------------------------------------------------
        decim_key = sdrangel.DEVICE_TYPES[self.device_hwtype]["decim_key"]
        if hwtype in ("Airspy", "AirspyHF"):
            sr_index = settings.get("devSampleRateIndex", 0)
            if sr_index == 1:
                sr = 3000000
            else:
                sr = 6000000
            log2_decim = settings.get(decim_key, 0)
        else:
            sr = settings.get("devSampleRate", 0)
            log2_decim = settings.get(decim_key, 0)
        return sr, 1<<log2_decim

    # ======================================================================
    @staticmethod
    def calc_decim(device_sr, symbol_rate):
        """ Calculate ideal decimation and return as log2 """
    # ----------------------------------------------------------------------
        quot = device_sr // (2*symbol_rate*1000)
        for i in range(6):
            if quot < 1<<i:
                break
        return 0 if i < 1 else i - 1

    # ======================================================================
    def set_device(self):
        """ Set the device """
    # ----------------------------------------------------------------------
        device_settings_url = f"{BASE_URL}/deviceset/{self.options.device_index}/device/settings"
        r = requests.get(url=device_settings_url, timeout=10)
        if r.status_code // 100 == 2: # OK
            rj = r.json()
            self.device_hwtype = rj.get("deviceHwType", None)
            device_settings = rj.get(sdrangel.DEVICE_TYPES[self.device_hwtype]["settings"], None)
            device_sr, device_decim = self.get_device_sr_and_decim(self.device_hwtype, device_settings)
            new_decim = self.calc_decim(device_sr, self.options.symbol_rate)
            print(f"sr: {device_sr} S/s decim: {device_decim} new decim: {1<<new_decim}")
            if not self.options.no_decim:
                self.change_decim(rj, new_decim)
            self.change_device_center_frequency(rj, self.options.frequency*1000)
            r = requests.patch(url=device_settings_url, json=rj, timeout=10)
            if r.status_code / 100 == 2:
                print(f'set_device: changed #{self.options.device_index}: frequency: {self.options.frequency} kHz log2Decim: {new_decim} No decim: {self.options.no_decim}')
            else:
                raise RuntimeError(f'set_device: failed to change device {self.options.device_index} with error {r.status_code}')
        else:
            raise RuntimeError(f"Cannot get device info for device set index {self.options.device_index} HTTP return code {r.status_code}")

    # ======================================================================
    def set_channel(self):
        """ Set the channel - assume DATV demod """
    # ----------------------------------------------------------------------
        channel_settings_url = f"{BASE_URL}/deviceset/{self.options.device_index}/channel/{self.options.channel_index}/settings"
        r = requests.get(url=channel_settings_url, timeout=10)
        if r.status_code // 100 == 2: # OK
            rj = r.json()
            rfbw = round(1.5*self.options.symbol_rate)*1000
            self.change_channel_center_frequency(rj, 0)
            self.change_rfbw(rj, rfbw)
            self.change_symbol_rate(rj, self.options.symbol_rate*1000)
            r = requests.patch(url=channel_settings_url, json=rj, timeout=10)
            if r.status_code / 100 == 2:
                print(f'set_channel: changed {self.options.device_index}:{self.options.channel_index}: rfbw: {rfbw} Hz symbol rate: {self.options.symbol_rate} kS/s')
            else:
                raise RuntimeError(f'set_device: failed to change channel {self.options.device_index}:{self.options.channel_index} with error {r.status_code}')
        else:
            raise RuntimeError(f"Cannot get channel info for channel {self.options.device_index}:{self.options.channel_index} HTTP return code {r.status_code}")


# ======================================================================
def main():
    """ Main program """
# ----------------------------------------------------------------------
    try:
        options = get_input_options()

        global BASE_URL # pylint: disable=global-statement
        BASE_URL =  BASE_URL_PATTERN.format(options.address)
        print(f"Frequency: 10{options.frequency/1000} MHz Symbol rate: {options.symbol_rate} kS/s")
        controller = Controller(options)
        controller.set_device()
        controller.set_channel()

    except Exception as ex: # pylint: disable=broad-except
        tb = traceback.format_exc()
        print(f"Exception caught {ex}")
        print(tb, file=sys.stderr)

# ======================================================================
if __name__ == "__main__":
    main()
