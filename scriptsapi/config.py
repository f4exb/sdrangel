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

    (options, args) = parser.parse_args()

    if (options.address == None):
        options.address = "127.0.0.1:8091"

    return options

# ======================================================================
def main():
    try:
        options = getInputOptions()

        global base_url
        base_url = "http://%s/sdrangel" % options.address

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

        print("All done!")

    except Exception as ex:
        tb = traceback.format_exc()
        print(tb, file=sys.stderr)


# ======================================================================
if __name__ == "__main__":
    main()
