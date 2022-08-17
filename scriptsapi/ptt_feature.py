#!/usr/bin/env python3
"""
PTT feature control
Switches a PTT Feature on/off with optional link to a LimeRFE feature
"""
import sys
import traceback
import requests
import time
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="Address and port. Default: 127.0.0.1:8091", metavar="ADDRESS", type="string")
    parser.add_option("-p", "--ptt-index", dest="ptt_index", help="Index of PTT feture", metavar="INTEGER", type="int")
    parser.add_option("-l", "--limerfe-index", dest="limerfe_index", help="Index of LimeRFE feture", metavar="INTEGER", type="int")
    parser.add_option("-t", "--transmit", dest="transmit", help="PTT on else off", action="store_true")

    (options, args) = parser.parse_args()

    if (options.address == None):
        options.address = "127.0.0.1:8091"

    return options


# ======================================================================
def turn_ptt(options):
    if options.limerfe_index is not None:
        limerfe_url = f"{base_url}/featureset/feature/{options.limerfe_index}/actions"
        if options.transmit:
            limerfe_payload_rx = {
                "featureType": "LimeRFE",
                "LimeRFEActions": {
                    "setRx": 0
                }
            }
            r = requests.post(url=limerfe_url, json=limerfe_payload_rx) # switch off Rx
            print(f"LimeRFE switch off Rx {r.status_code}")
        else:
            limerfe_payload_tx = {
                "featureType": "LimeRFE",
                "LimeRFEActions": {
                    "setTx": 0
                }
            }
            r = requests.post(url=limerfe_url, json=limerfe_payload_tx) # switch off Tx
            print(f"LimeRFE switch off Tx {r.status_code}")
        time.sleep(0.2)

    ptt_payload = {
        "featureType": "SimplePTT",
        "SimplePTTActions": {
            "ptt": 1 if options.transmit else 0
        }
    }
    ptt_url = f"{base_url}/featureset/feature/{options.ptt_index}/actions"
    r = requests.post(url=ptt_url, json=ptt_payload)
    print(f"SimplePTT post action {r.status_code}")

    if options.limerfe_index is not None:
        time.sleep(0.2)
        limerfe_url = f"{base_url}/featureset/feature/{options.limerfe_index}/actions"
        if options.transmit:
            limerfe_payload_tx = {
                "featureType": "LimeRFE",
                "LimeRFEActions": {
                    "setTx": 1
                }
            }
            r = requests.post(url=limerfe_url, json=limerfe_payload_tx) # switch on Tx
            print(f"LimeRFE switch on Tx {r.status_code}")
        else:
            limerfe_payload_rx = {
                "featureType": "LimeRFE",
                "LimeRFEActions": {
                    "setRx": 1
                }
            }
            for i in range(5):
                r = requests.post(url=limerfe_url, json=limerfe_payload_rx) # switch on Rx
                print(f"LimeRFE switch on Rx {r.status_code}")
                if r.status_code // 100 == 2:
                    break
                time.sleep(0.5)


# ======================================================================
def main():
    try:
        options = getInputOptions()

        global base_url
        base_url = "http://%s/sdrangel" % options.address

        if options.ptt_index is None:
            print("You must at least give the PTT feature index (-p option)")
            sys.exit(1)

        turn_ptt(options)

    except Exception as ex:
        tb = traceback.format_exc()
        print(tb, file=sys.stderr)


# ======================================================================
if __name__ == "__main__":
    main()
