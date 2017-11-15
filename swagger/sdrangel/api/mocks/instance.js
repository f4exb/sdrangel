'use strict';

module.exports = {
  instanceSummary: instanceSummary,
  instanceDevices: instanceDevices,
  instanceChannels: instanceChannels
};

function instanceSummary(req, res, next) {

  res.json([
    {
        "version":"4.0.0",
        "logging": {
            "consoleLevel": "info",
            "fileLevel": "debug",
            "dumpToFile": false
        },
        "devicesetcount":2,
        "devicesets":[
            {
                "samplingDevice": {
                    "index":0,
                    "hwType":"RTLSDR",
                    "rx":true,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"R820T2005",
                    "centerFrequency": 435000000,
                    "bandwidth": 64000
                },
                "channelcount":2,
                "channels":[
                    {"index":0,"id":"AMDemod","deltaFrequency":-12500},
                    {"index":1,"id":"AMDemod","deltaFrequency":12500}
                ]
            },
            {
                "samplingDevice": {
                    "index":1,
                    "hwType":"HackRF",
                    "rx":false,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"453c64c8257a608f",
                    "centerFrequency": 435000000,
                    "bandwidth": 128000
                },
                "channelcount":1,
                "channels":[
                    {"index":0,"id":"AMMod","deltaFrequency":12500}
                ]
            }

        ],
        "user":{"index":1,"name":"Sample text"}}
    ]);
}

function instanceDevices(req, res, next) {
    var direction = req.swagger.params.direction.value || "rx";
    //console.log(direction.value)
    
    if (direction === "tx") {
        res.json([
        {
            "devicecount": 1,
            "devices": [
                {
                    "hwType":"HackRF",
                    "rx":false,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"453c64c8257a608f"
                }
            ]
        }]);
    } else {
        res.json([
        {
            "devicecount": 2,
            "devices": [
                {
                    "hwType":"RTLSDR",
                    "rx":true,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"R820T2005"
                },
                {
                    "hwType":"HackRF",
                    "rx":true,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"453c64c8257a608f"
                }
            ]
        }]);
    }
}

function instanceChannels(req, res, next) {
    var direction = req.swagger.params.direction.value || "rx";
    
    if (direction === "tx") {
        res.json([
        {
            "channelcount": 6,
            "channels": [
                {
                    "name":"AM Modulator",
                    "id": "AMMod",
                    "rx":false,
                    "version": "3.9.0"
                },
                {
                    "name":"ATV Modulator",
                    "id": "ATVMod",
                    "rx":false,
                    "version": "3.9.0"
                },
                {
                    "name":"NFM Modulator",
                    "id": "NFMMod",
                    "rx":false,
                    "version": "3.9.0"
                },
                {
                    "name":"SSB Modulator",
                    "id": "SSBMod",
                    "rx":false,
                    "version": "3.9.0"
                },
                {
                    "name":"UDP Channel Sink",
                    "id": "UDPSink",
                    "rx":false,
                    "version": "4.0.0"
                },
                {
                    "name":"WFM Modulator",
                    "id": "WFMMod",
                    "rx":false,
                    "version": "3.9.0"
                }
            ]
        }]);
    } else {
        res.json([
        {
            "channelcount": 9,
            "channels": [
                {
                    "name":"AM Demodulator",
                    "id": "AMemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"Broadcast FM Demodulator",
                    "id": "BFMDemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"ATV Demodulator",
                    "id": "ATVDemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"DSD Demodulator",
                    "id": "DSDDemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"NFM Demodulator",
                    "id": "NFDemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"SSB Demodulator",
                    "id": "SSBDemod",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"TCP Channel Source",
                    "id": "TCPSource",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"UDP Channel Source",
                    "id": "UDPSource",
                    "rx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"WFM Demodulator",
                    "id": "WFMDemod",
                    "rx":true,
                    "version": "4.0.0"
                }
            ]
        }]);
    }
        
}
