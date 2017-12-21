#!/usr/bin/env python
import requests, json

base_url = "http://127.0.0.1:8091/sdrangel"

requests_methods = {
    "GET": requests.get,
    "PATCH": requests.patch,
    "POST": requests.post,
    "PUT": requests.put,
    "DELETE": requests.delete
}

def getHwType():
    r = requests.get(url=base_url+"/deviceset/0")
    if r.status_code / 100  == 2:
        rj = r.json()
        devj = rj.get('samplingDevice', None)
        if devj is not None:
            return devj.get('hwType' ,None)
        else:
            return None
    else:
        return None
        
def selectRtlSdr():
    r = requests.put(url=base_url+"/deviceset/0/device", json={"hwType": "RTLSDR"})
    if r.status_code / 100  == 2:
        print json.dumps(r.json(), indent=4, sort_keys=True)
        return True
    else:
        return False
        
def getRtlSdrSettings():
    r = requests.get(url=base_url+"/deviceset/0/device/settings")
    if r.status_code / 100  == 2:
        rj = r.json()
        hwType = rj.get('deviceHwType', None)
        if hwType is not None and hwType == "RTLSDR":
            settings = rj.get('rtlSdrSettings', None)
            return settings
        else:
            return None
    else:
        return None
        
def patchRtlSdrSettings(settings):
    new_settings = {"deviceHwType": "RTLSDR", "tx": 0, "rtlSdrSettings": settings}
    r = requests.patch(url=base_url+"/deviceset/0/device/settings", json=new_settings)
    if r.status_code / 100  == 2:
        print json.dumps(r.json(), indent=4, sort_keys=True)
    else:
        print "Error HTTP:", r.status_code
    
def deviceRun(run):
    if run:
        r = requests.post(url=base_url+"/deviceset/0/device/run")
    else:
        r = requests.delete(url=base_url+"/deviceset/0/device/run")
    if r.status_code / 100  == 2:
        print json.dumps(r.json(), indent=4, sort_keys=True)
    else:
        print "Error HTTP:", r.status_code
    
        
def main():
    hwType = getHwType()
    if hwType is not None:
        if hwType != "RTLSDR":
            if not selectRtlSdr():
                return
    else:
        return
    settings = getRtlSdrSettings()
    if settings is not None:
        deviceRun(True)
        settings["agc"] = 1
        settings["dcBlock"] = 1
        settings["gain"] = 445
        settings["centerFrequency"] = 433900000
        patchRtlSdrSettings(settings)
        

if __name__ == "__main__":
    main()
        