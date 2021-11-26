#!/usr/bin/env python3
"""
Dumps a SDRangel instance configuration to a sequence of commands in a JSON file
compatible with config.py

Each command is a JSON document with the following keys:
  - endpoint: URL suffix (API function) - mandatory
  - method: HTTP method (GET, PATCH, POST, PUT, DELETE) - mandatory
  - params: pairs of argument and values - optional
  - payload: request body in JSON format - optional
  - msg: descriptive message for console display - optional
  - delay: delay in milliseconds after command - optional
"""

import requests, traceback, sys, json, time
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"


# ======================================================================
class Instance:
    def __init__(self):
        self.devicesets = []
        self.featuresets = []

    def add_deviceset(self, json_data):
        ds = DeviceSet(len(self.devicesets), json_data["direction"], json_data["deviceHwType"], json_data)
        self.devicesets.append(ds)
        return ds

    def add_featureset(self, json_data):
        fs = FeatureSet(len(self.featuresets))
        self.featuresets.append(fs)
        return fs

    def add_deviceset_item(self, direction):
        return {
            "endpoint": "/deviceset",
            "method": "POST",
            "params": {"direction": direction},
            "msg": f"Add device set"
        }

    def add_featureset_item(self):
        return {
            "endpoint": "/featureset",
            "method": "POST",
            "msg": f"Add feature set"
        }

    def get_config_items(self):
        items = []
        for i_ds, deviceset in enumerate(self.devicesets):
            items.append(self.add_deviceset_item(deviceset.direction))
            items.append(deviceset.change_device_item())
            items.append(deviceset.set_device_item())
            for channel in deviceset.channels:
                items.append(channel.add_channel_item())
                items.append(channel.set_channel_item())
            items.append(deviceset.set_spectrum_item())
        for i_fs, featureset in enumerate(self.featuresets):
            items.append(self.add_featureset_item())
            for feature in featureset.features:
                items.append(feature.add_feature_item())
                items.append(feature.set_feature_item())
        return items

# ======================================================================
class DeviceSet:
    def __init__(self, index, direction, hwType, settings):
        self.index = index
        self.direction = direction
        self.hwType = hwType
        self.device_settings = settings
        self.spectrum_settings = {}
        self.channels = []

    def set_spectrum_settings(self, settings):
        self.spectrum_settings = settings

    def add_channel(self, json_data):
        ch = Channel(self.index, len(self.channels), json_data["direction"], json_data["channelType"], json_data)
        self.channels.append(ch)
        return ch

    def change_device_item(self):
        return {
            "endpoint": f"/deviceset/{self.index}/device",
            "method": "PUT",
            "payload": {
                "hwType": self.hwType,
                "direction": self.direction
            },
            "msg": f"Change device to {self.hwType} on device set {self.index}"
        }

    def set_device_item(self):
        return {
            "endpoint": f"/deviceset/{self.index}/device/settings",
            "method": "PUT",
            "payload": self.device_settings,
            "msg": f"Setup device {self.hwType} on device set {self.index}"
        }

    def set_spectrum_item(self):
        return {
            "endpoint": f"/deviceset/{self.index}/spectrum/settings",
            "method": "PUT",
            "payload": self.spectrum_settings,
            "msg": f"Setup spectrum on device set {self.index}"
        }

    def delete_all_channels_items(self):
        nb_channels = len(self.channels)
        items = []
        for i_chan in reversed(range(nb_channels)):
            items.append({
                "endpoint": f"/deviceset/{self.index}/channel/{i_chan}",
                "method": "DELETE",
                "msg": f"Delete channel at index {i_chan} in device set {self.index}"
            })
        return items

# ======================================================================
class Channel:
    def __init__(self, ds_index, index, direction, type, settings):
        self.ds_index = ds_index
        self.index = index
        self.direction = direction
        self.type = type
        self.channel_settings = settings

    def add_channel_item(self):
        return {
            "endpoint": f"/deviceset/{self.ds_index}/channel",
            "method": "POST",
            "payload": {
                "channelType": self.type,
                "direction": self.direction
            },
            "msg": f"Add channel {self.type} in device set index {self.ds_index}"
        }

    def set_channel_item(self):
        return {
            "endpoint": f"/deviceset/{self.ds_index}/channel/{self.index}/settings",
            "method": "PUT",
            "payload": self.channel_settings,
            "msg": f"Setup channel {self.type} at {self.index} in device set {self.ds_index}"
        }

# ======================================================================
class FeatureSet:
    def __init__(self, index):
        self.index = index
        self.features = []

    def add_feature(self, json_data):
        feat = Feature(self.index, len(self.features), json_data["featureType"], json_data)
        self.features.append(feat)
        return feat

    def delete_all_features_items(self):
        nb_features = len(self.features)
        items = []
        for i_feat in reversed(range(nb_features)):
            items.append({
                "endpoint": f"/featureset/{self.index}/feature/{i_feat}",
                "method": "DELETE",
                "msg": f"Delete feature at index {i_feat} in feature set {self.index}"
            })
        return items

# ======================================================================
class Feature:
    def __init__(self, fs_index, index, type, settings):
        self.fs_index = fs_index
        self.index = index
        self.type = type
        self.feature_settings = settings

    def add_feature_item(self):
        return {
            "endpoint": f"/featureset/{self.fs_index}/feature",
            "method": "POST",
            "payload": {
                "featureType": self.type
            },
            "msg": f"Add feature {self.type} in feature set index {self.fs_index}"
        }

    def set_feature_item(self):
        return {
            "endpoint": f"/featureset/{self.fs_index}/feature/{self.index}/settings",
            "method": "PUT",
            "payload": self.feature_settings,
            "msg": f"Setup feature {self.type} at {self.index} in feature set {self.fs_index}"
        }


# ======================================================================
def getInputOptions():

    parser = OptionParser(usage="usage: %%prog [-t]\n")
    parser.add_option("-a", "--address", dest="address", help="Address and port. Default: 127.0.0.1:8091", metavar="ADDRESS", type="string")
    parser.add_option("-j", "--json-file", dest="json_file", help="JSON output file where commands are stored. Mandatory", metavar="FILE", type="string")

    (options, args) = parser.parse_args()

    if (options.address == None):
        options.address = "127.0.0.1:8091"

    return options


# ======================================================================
def dump():
    instance = None
    r = requests.get(url=f"{base_url}")
    if r.status_code // 100 == 2:
        instance = Instance()
        nb_devicesets = r.json()["devicesetlist"]["devicesetcount"]
        for i_deviceset in range(nb_devicesets):
            r_ds = requests.get(url=f"{base_url}/deviceset/{i_deviceset}")
            if r_ds.status_code // 100 == 2:
                nb_channels = r_ds.json()["channelcount"]
                r_dev = requests.get(url=f"{base_url}/deviceset/{i_deviceset}/device/settings")
                if r_dev.status_code // 100 == 2:
                    ds = instance.add_deviceset(r_dev.json())
                    for i_channel in range(nb_channels):
                        r_chan = requests.get(url=f"{base_url}/deviceset/{i_deviceset}/channel/{i_channel}/settings")
                        if r_chan.status_code // 100 == 2:
                            ds.add_channel(r_chan.json())
                    r_spec = requests.get(url=f"{base_url}/deviceset/{i_deviceset}/spectrum/settings")
                    if r_spec.status_code // 100 == 2:
                        ds.set_spectrum_settings(r_spec.json())
        nb_featuresets = r.json()["featuresetlist"]["featuresetcount"]
        for i_featureset in range(nb_featuresets):
            r_fs = requests.get(url=f"{base_url}/featureset/{i_featureset}")
            if r_fs.status_code // 100 == 2:
                nb_features = r_fs.json()["featurecount"]
                fs = instance.add_featureset(r_fs.json())
                for i_feature in range(nb_features):
                    r_feat = requests.get(url=f"{base_url}/featureset/{i_featureset}/feature/{i_feature}/settings")
                    if r_feat.status_code // 100 == 2:
                        fs.add_feature(r_feat.json())
    return instance


# ======================================================================
def main():
    try:
        options = getInputOptions()

        global base_url
        base_url = "http://%s/sdrangel" % options.address

        instance = dump()
        json_items = instance.get_config_items()

        with open(options.json_file,'w') as json_file:
            json.dump(json_items, json_file, indent=2)

        print("All done!")

    except Exception as ex:
        tb = traceback.format_exc()
        print(tb, file=sys.stderr)


# ======================================================================
if __name__ == "__main__":
    main()
