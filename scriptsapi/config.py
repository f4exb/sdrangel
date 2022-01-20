#!/usr/bin/env python3
"""
Sends a sequence of commands recorded in a JSON file which is in the
form of a list of commands.

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
app_gui = False
nb_devicesets = 0
nb_featuresets = 0

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
    parser.add_option("-a", "--address", dest="address", help="Address and port. Default: 127.0.0.1:8091", metavar="ADDRESS", type="string")
    parser.add_option("-j", "--json-file", dest="json_file", help="JSON file containing commands. Mandatory", metavar="FILE", type="string")
    parser.add_option("-i", "--init", dest="initialize", help="Initialize instance before running script", action="store_true")
    parser.add_option("-1", "--ignore-first-posts", dest="ignore_first_posts", help="Ignore first deviceset or featureset post in sequence", action="store_true")
    parser.add_option("-d", "--delay", dest="delay_ms", help="force delay after each command (ms)", metavar="TIME", type="int")

    (options, args) = parser.parse_args()

    if (options.address == None):
        options.address = "127.0.0.1:8091"

    return options

# ======================================================================
def get_instance_details():
    global app_gui
    global nb_devicesets
    global nb_featuresets
    r = requests_methods["GET"](url=base_url)
    if r.status_code // 100 == 2:
        app_gui = r.json()["appname"] == "SDRangel"
        nb_devicesets = r.json()["devicesetlist"]["devicesetcount"]
        nb_featuresets = r.json()["featuresetlist"]["featuresetcount"]

# ======================================================================
def initialize():
    global nb_devicesets
    global nb_featuresets
    for i_ds in reversed(range(nb_devicesets)):
        if app_gui and i_ds == 0:
            r_ds = requests_methods["GET"](url=f"{base_url}/deviceset/0")
            if r_ds.status_code // 100 == 2:
                nb_channels = r_ds.json()["channelcount"]
                for i_chan in reversed(range(nb_channels)):
                    requests_methods["DELETE"](url=f"{base_url}/deviceset/0/channel/{i_chan}")
        else:
            r_del = requests_methods["DELETE"](url=f"{base_url}/deviceset")
            if r_del.status_code // 100 == 2:
                nb_devicesets -= 1
    for i_fs in reversed(range(nb_featuresets)):
        if app_gui and i_fs == 0:
            r_fs = requests_methods["GET"](url=f"{base_url}/featureset/0")
            if r_fs.status_code // 100 == 2:
                nb_features = r_fs.json()["featurecount"]
                for i_feat in reversed(range(nb_features)):
                    requests_methods["DELETE"](url=f"{base_url}/featureset/0/feature/{i_feat}")
        else:
            r_del = requests_methods["DELETE"](url=f"{base_url}/featureset")
            if r_del.status_code // 100 == 2:
                nb_featuresets -= 1


# ======================================================================
def main():
    try:
        options = getInputOptions()

        global base_url
        base_url = "http://%s/sdrangel" % options.address

        get_instance_details()

        if options.initialize:
            initialize()

        nb_featureset_posts = 0
        nb_deviceset_posts = 0

        with open(options.json_file) as json_file:
            commands = json.load(json_file)
            for command in commands:
                endpoint = command.get('endpoint', None)
                if endpoint is None:
                    continue
                url = base_url + endpoint
                http_method = command.get('method', None)
                if endpoint == "/deviceset" and http_method == "POST":
                    nb_deviceset_posts += 1
                    if nb_deviceset_posts == 1 and options.ignore_first_posts:
                        print("First deviceset creation ignored")
                        continue
                if endpoint == "/featureset" and http_method == "POST":
                    nb_featureset_posts += 1
                    if nb_featureset_posts == 1 and options.ignore_first_posts:
                        print("First featureset creation ignored")
                        continue
                method = requests_methods.get(http_method, None) if http_method is not None else None
                if method is not None:
                    request_params = command.get('params', None)
                    request_json = command.get('payload', None)
                    r = method(url=url, params=request_params, json=request_json)
                    if r.status_code // 100 == 2:
                        print("Done: %s" % command.get('msg', ''))
                    else:
                        print("Error %d:%s" % (r.status_code, command.get('msg', '')))
                        print(r.text)
                        exit(1)
                else:
                    print("requests method error")
                    exit(1)
                delay = command.get('delay', None)
                if delay is not None and isinstance(delay, int):
                    time.sleep(delay/1000)
                elif options.delay_ms is not None:
                    time.sleep(options.delay_ms/1000)

        print("All done!")

    except Exception as ex:
        tb = traceback.format_exc()
        print(tb, file=sys.stderr)


# ======================================================================
if __name__ == "__main__":
    main()
