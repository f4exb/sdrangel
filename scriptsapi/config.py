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
nb_devicesets = 0

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
    parser.add_option("-d", "--delay", dest="delay_ms", help="force delay after each command (ms)", metavar="TIME", type="int")

    (options, args) = parser.parse_args()

    if (options.address == None):
        options.address = "127.0.0.1:8091"

    return options

# ======================================================================
def get_instance_details():
    global nb_devicesets
    r = requests_methods["GET"](url=base_url)
    if r.status_code // 100 == 2:
        nb_devicesets = r.json()["devicesetlist"]["devicesetcount"]

# ======================================================================
def initialize():
    global nb_devicesets
    for i_ds in reversed(range(nb_devicesets)):
        r_del = requests_methods["DELETE"](url=f"{base_url}/deviceset")
        if r_del.status_code // 100 == 2:
            nb_devicesets -= 1
    r_fs = requests_methods["GET"](url=f"{base_url}/featureset")
    if r_fs.status_code // 100 == 2:
        nb_features = r_fs.json()["featurecount"]
        for i_feat in reversed(range(nb_features)):
            requests_methods["DELETE"](url=f"{base_url}/featureset/feature/{i_feat}")

# ======================================================================
def main():
    try:
        options = getInputOptions()

        global base_url
        base_url = "http://%s/sdrangel" % options.address

        get_instance_details()

        if options.initialize:
            initialize()

        with open(options.json_file) as json_file:
            commands = json.load(json_file)
            for command in commands:
                endpoint = command.get('endpoint', None)
                if endpoint is None:
                    continue
                url = base_url + endpoint
                http_method = command.get('method', None)
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
