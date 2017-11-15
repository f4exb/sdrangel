'use strict';

module.exports = {
  instanceSummary: instanceSummary,
  instanceDevices: instanceDevices,
  instanceChannels: instanceChannels,
  instanceLoggingGet: instanceLoggingGet,
  instanceLoggingPut: instanceLoggingPut,
  instanceAudioGet: instanceAudioGet,
  instanceAudioPatch: instanceAudioPatch,
  instanceLocationGet: instanceLocationGet,
  instanceLocationPut: instanceLocationPut,
  instanceDVSerialPatch: instanceDVSerialPatch,
  instancePresetGet: instancePresetGet,
  instancePresetPatch: instancePresetPatch
};

function instanceSummary(req, res, next) {

  res.json(
    {
        "version":"4.0.0",
        "logging": {
            "consoleLevel": "info",
            "fileLevel": "debug"
        },
        "devicesetcount":2,
        "devicesets":[
            {
                "samplingDevice": {
                    "index":0,
                    "hwType":"RTLSDR",
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
                    "tx":true,
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
        "user":{"index":1,"name":"Sample text"}
    });
}

function instanceDevices(req, res, next) {
    var directionTx = req.swagger.params.tx.value || false;
    //console.log(tx.value)
    
    if (directionTx) {
        res.json(
        {
            "devicecount": 1,
            "devices": [
                {
                    "hwType":"HackRF",
                    "tx":true,
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"453c64c8257a608f"
                }
            ]
        });
    } else {
        res.json(
        {
            "devicecount": 2,
            "devices": [
                {
                    "hwType":"RTLSDR",
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"R820T2005"
                },
                {
                    "hwType":"HackRF",
                    "streamIndex":0,
                    "sequence":0,
                    "serial":"453c64c8257a608f"
                }
            ]
        });
    }
}

function instanceChannels(req, res, next) {
    var directionTx = req.swagger.params.direction.value || false;
    
    if (directionTx) {
        res.json(
        {
            "channelcount": 6,
            "channels": [
                {
                    "name":"AM Modulator",
                    "id": "AMMod",
                    "tx":true,
                    "version": "3.9.0"
                },
                {
                    "name":"ATV Modulator",
                    "id": "ATVMod",
                    "tx":true,
                    "version": "3.9.0"
                },
                {
                    "name":"NFM Modulator",
                    "id": "NFMMod",
                    "tx":true,
                    "version": "3.9.0"
                },
                {
                    "name":"SSB Modulator",
                    "id": "SSBMod",
                    "tx":true,
                    "version": "3.9.0"
                },
                {
                    "name":"UDP Channel Sink",
                    "id": "UDPSink",
                    "tx":true,
                    "version": "4.0.0"
                },
                {
                    "name":"WFM Modulator",
                    "id": "WFMMod",
                    "tx":true,
                    "version": "3.9.0"
                }
            ]
        });
    } else {
        res.json(
        {
            "channelcount": 9,
            "channels": [
                {
                    "name":"AM Demodulator",
                    "id": "AMemod",
                    "version": "4.0.0"
                },
                {
                    "name":"Broadcast FM Demodulator",
                    "id": "BFMDemod",
                    "version": "4.0.0"
                },
                {
                    "name":"ATV Demodulator",
                    "id": "ATVDemod",
                    "version": "4.0.0"
                },
                {
                    "name":"DSD Demodulator",
                    "id": "DSDDemod",
                    "version": "4.0.0"
                },
                {
                    "name":"NFM Demodulator",
                    "id": "NFDemod",
                    "version": "4.0.0"
                },
                {
                    "name":"SSB Demodulator",
                    "id": "SSBDemod",
                    "version": "4.0.0"
                },
                {
                    "name":"TCP Channel Source",
                    "id": "TCPSource",
                    "version": "4.0.0"
                },
                {
                    "name":"UDP Channel Source",
                    "id": "UDPSource",
                    "version": "4.0.0"
                },
                {
                    "name":"WFM Demodulator",
                    "id": "WFMDemod",
                    "version": "4.0.0"
                }
            ]
        });
    }
}

function instanceLoggingGet(req, res, next) {
  res.json(
    {
        "consoleLevel":"info",
        "fileLevel":"debug",
        "dumpToFile": true,
        "fileName":"sdrangel.log"
    }
  );
}

function instanceLoggingPut(req, res, next) {
    console.log(req.swagger.params.body.value);
    res.json(req.swagger.params.body.value);
}

function instanceAudioGet(req, res, next) {
  res.json(
    {
        "nbInputDevices":7,
        "inputDevices": [
            "Default (use first suitable device)",
            "HDA Intel HDMI",
            "HDA Intel PCH",
            "default",
            "alsa_input.pci-0000_00_1b.0.analog-stereo",
            "alsa_output.pci-0000_00_03.0.hdmi-stereo.monitor",
            "alsa_output.pci-0000_00_1b.0.analog-stereo.monitor"
        ],
        "nbOutputDevices":6,
        "outputDevices": [
            "Default (use first suitable device)",
            "HDA Intel HDMI",
            "HDA Intel PCH",
            "default",
            "alsa_output.pci-0000_00_03.0.hdmi-stereo",
            "alsa_output.pci-0000_00_1b.0.analog-stereo"
        ]
    }
  );
}

function instanceAudioPatch(req, res, next) {
    console.log(req.swagger.params.body.value);
    res.json(req.swagger.params.body.value);
}

function instanceLocationGet(req, res, next) {
  res.json(
    {
        "latitude":43.523774, 
        "longitude":7.044443
    }
  );
}

function instanceLocationPut(req, res, next) {
    res.json(req.swagger.params.body.value);
}

function instanceDVSerialPatch(req, res, next) {
    var setDVSerial = req.swagger.params.dvserial.value || false;
    
    if (setDVSerial) {
        res.json(
        {
            "nbDevices":2,
            "dvSerialDevices": [
                "/dev/ttyUSB0",
                "/dev/ttyUSB1"
            ]
        });        
    } else {
        res.json(
        {
            "nbDevices":0
        });                
    }
}

function instancePresetGet(req, res, next) {
    res.json(
        {
          "nbGroups": 2,
          "groups": [
            {
              "groupName": "ATV",
              "nbPresets": 3,
              "presets": [
                {
                  "centerFrequency": 435.995,
                  "type": "R",
                  "name": "Test Rx 90/2 25k"
                },
                {
                  "centerFrequency": 436.0,
                  "type": "T",
                  "name": "Test Rx 90/2 25k"
                },
                {
                  "centerFrequency": 1243.0,
                  "type": "R",
                  "name": "Mont Agel"
                }                
              ]
            },            
            {
              "groupName": "BFM",
              "nbPresets": 4,
              "presets": [
                {
                  "centerFrequency": 91.5,
                  "type": "R",
                  "name": "Cannes Radio"
                },
                {
                  "centerFrequency": 96.4,
                  "type": "R",
                  "name": "Kiss FM"
                },
                {
                  "centerFrequency": 103.2,
                  "type": "R",
                  "name": "Radio Monaco"
                },
                {
                  "centerFrequency": 103.4,
                  "type": "R",
                  "name": "Frequence K"
                }                
              ]
            }
          ]
        }
    );
}

function instancePresetPatch(req, res, next) {
    res.json(req.swagger.params.body.value);
}
