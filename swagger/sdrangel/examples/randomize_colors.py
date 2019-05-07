#!/usr/bin/env python

''' Randomize channel colors for a specified device set
      -a: address and port of SDRangel REST API (default: 127.0.0.1:8091)
      -d: index of device set to apply changes to (default 0)
    By default colors are changed randomly (hue, saturation, value). Optionally you can change one or several of the
    hue, saturation and value parameters with these options:
      -H: change hue randomly
      -s: change saturation randomly
      -v: change value (luminance) randomly
'''

import requests, json, traceback, sys, random
import colorsys
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"


# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-adsvH]\n")
    parser.add_option("-a", "--address", dest="address", help="address and port", metavar="ADDRESS", type="string")
    parser.add_option("-d", "--device-index", dest="device_index", help="device set index", metavar="INDEX", type="int")
    parser.add_option("-s", "--saturation", dest="saturation", help="change channels color saturation", metavar="SATURATION", action="store_true", default=False)
    parser.add_option("-v", "--value", dest="value", help="change channels color value", metavar="VALUE", action="store_true", default=False)
    parser.add_option("-H", "--hue", dest="hue", help="change channels color hue", metavar="HUE", action="store_true", default=False)

    (options, args) = parser.parse_args()

    if options.address is None:
        options.address = "127.0.0.1:8091"

    if options.device_index is None or options.device_index < 0:
        options.device_index = 0

    return options

# ======================================================================
def get_channel_static_data(channel_info):
        if channel_info["id"] == "AMDemod":
            settings_key = "AMDemodSettings"
            direction = 0
        elif channel_info["id"] == "AMMod":
            settings_key = "AMModSettings"
            direction = 1
        elif channel_info["id"] == "ATVMod":
            settings_key = "ATVModSettings"
            direction = 1
        elif channel_info["id"] == "BFMDemod":
            settings_key = "BFMDemodSettings"
            direction = 0
        elif channel_info["id"] == "DSDDemod":
            settings_key = "DSDDemodSettings"
            direction = 0
        elif channel_info["id"] == "NFMDemod":
            settings_key = "NFMDemodSettings"
            direction = 0
        elif channel_info["id"] == "NFMMod":
            settings_key = "NFMModSettings"
            direction = 1
        elif channel_info["id"] == "RemoteSink":
            settings_key = "RemoteSinkSettings"
            direction = 0
        elif channel_info["id"] == "RemoteSource":
            settings_key = "RemoteSourceSettings"
            direction = 1
        elif channel_info["id"] == "SSBMod":
            settings_key = "SSBModSettings"
            direction = 1
        elif channel_info["id"] == "SSBDemod":
            settings_key = "SSBDemodSettings"
            direction = 0
        elif channel_info["id"] == "UDPSource":
            settings_key = "UDPSourceSettings"
            direction = 1
        elif channel_info["id"] == "UDPSink":
            settings_key = "UDPSinkSettings"
            direction = 0
        elif channel_info["id"] == "WFMDemod":
            settings_key = "WFMDemodSettings"
            direction = 0
        elif channel_info["id"] == "WFMMod":
            settings_key = "WFMModSettings"
            direction = 1
        else:
            settings_key = None
            direction = None
        return settings_key, direction

# ======================================================================
def randomize_channels_colors(options, channels):
    for channel_info in channels:
        settings_key, direction = get_channel_static_data(channel_info)
        if settings_key is None:
            continue
        color = random.randint(0, (1<<24))
        rn, gn, bn = color_to_rgb(color)
        payload_json = {
            "channelType": channel_info["id"],
            "direction": direction,
            settings_key: {
                "rgbColor": color
            }
        }
        settings_url = base_url + ("/deviceset/{0}/channel/{1}/settings".format(options.device_index, channel_info["index"]))
        rep = requests.patch(url=settings_url, json=payload_json)
        if rep.status_code / 100 == 2:
            print("Changed channel {0} color to rgb({1},{2},{3})".format(channel_info["index"], rn, gn, bn))
        else:
            print("Error changing channel {0} color to rgb({1},{2},{3})".format(channel_info["index"], rn, gn, bn))

# ======================================================================
def randomize_channels_hsv(options, channels):
    for channel_info in channels:
        settings_key, direction = get_channel_static_data(channel_info)
        settings_url = base_url + ("/deviceset/{0}/channel/{1}/settings".format(options.device_index, channel_info["index"]))
        rep = requests.get(url=settings_url)
        if rep.status_code / 100 == 2:
            rj = rep.json()
            channel_settings = rj.get(settings_key)
            if channel_settings is None:
                print("Cannot get channel {0} settings with key {1}".format(channel_info["index"], settings_key))
                continue
            color = channel_settings["rgbColor"]
            r, g, b = color_to_rgb(color)
            h, s, v = colorsys.rgb_to_hsv(r/255., g/255., b/255.)
            if options.hue:
                h = random.random()
            if options.saturation:
                s = random.random()
            if options.value:
                v = random.random()
            r, g, b = colorsys.hsv_to_rgb(h, s, v)
            rn = int(r * 255.9999)
            gn = int(g * 255.9999)
            bn = int(b * 255.9999)
            new_color = rgb_to_color(rn, gn, bn)
            payload_json = {
                "channelType": channel_info["id"],
                "direction": direction,
                settings_key: {
                    "rgbColor": new_color
                }
            }
            settings_url = base_url + ("/deviceset/{0}/channel/{1}/settings".format(options.device_index, channel_info["index"]))
            rep = requests.patch(url=settings_url, json=payload_json)
            if rep.status_code / 100 == 2:
                print("Changed channel {0} color to rgb({1},{2},{3})".format(channel_info["index"], rn, gn, bn))
            else:
                print("Error changing channel {0} color to rgb({1},{2},{3})".format(channel_info["index"], rn, gn, bn))
        else:
            print("Cannot get channel {0} info".format(channel_info["index"]))
            continue

# ======================================================================
def color_to_rgb(color):
    r = (color & 0xFF0000)>>16
    g = (color & 0x00FF00)>>8
    b = (color & 0x0000FF)
    return r, g, b

# ======================================================================
def rgb_to_color(r, g, b):
    return r*(1<<16) + g*(1<<8) + b

# ======================================================================
def main():
    try:
        options = getInputOptions()
        global base_url
        base_url = "http://%s/sdrangel" % options.address
        deviceset_url = base_url + ("/deviceset/%d" % options.device_index)
        r = requests.get(url=deviceset_url)
        if r.status_code / 100 == 2:
            rj = r.json()
            channels = rj.get("channels")
            if options.saturation or options.value or options.hue:
                randomize_channels_hsv(options, channels)
            else:
                randomize_channels_colors(options, channels) # full randomization (identical to -Hsv)
        else:
            print("Error getting deviceset %d info. HTTP: %d" % (options.device_index, r.status_code))
            print json.dumps(r.json(), indent=4, sort_keys=True)

    except Exception as ex:
        tb = traceback.format_exc()
        print >> sys.stderr, tb


if __name__ == "__main__":
    main()
