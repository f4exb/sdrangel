#!/usr/bin/env python3

import requests, traceback, sys, json
from optparse import OptionParser

base_url = "http://127.0.0.1:8091/sdrangel"

# commands list in JSON file. Each command is a list:
#   - URL suffix (API function)
#   - HTTP method (GET, PATCH, POST, PUT, DELETE)
#   - Params as key:value pairs or None if unused
#   - JSON body or None if unused
#   - Descriptive message fragment

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
    parser.add_option("-j", "--json-file", dest="json_file", help="JSON file containing commands", metavar="FILE", type="string")

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
                url = base_url + command[0]
                method = requests_methods.get(command[1], None)
                if method is not None:
                    r = method(url=url, params=command[2], json=command[3])
                    if r.status_code // 100 == 2:
                        print("Done: %s" % command[4])
                    else:
                        print("Error %d:%s" % (r.status_code, command[4]))
                        print(r.text)
                        exit(1)
                else:
                    print("requests method error")
                    exit(1)

        print("All done!")

    except Exception as ex:
        tb = traceback.format_exc()
        print(tb, file=sys.stderr)


# ======================================================================
if __name__ == "__main__":
    main()

