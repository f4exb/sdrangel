## Python scripts interfacing with the API ##

These scripts are designed to work in Python 3 preferably with version 3.6 or higher. Dependencies are installed with pip in a virtual environment. The sequence of operations is the following:

```
virtualenv -p /usr/bin/python3 venv # Create virtual environment
. ./venv/bin/activate               # Activate virtual environment
pip install -r requirements.txt     # Install requirements
```

<h2>freqtracking.py</h2>

This script is used to achieve frequency tracking with the FreqTracker plugin. Ideally you would start it before connecting the Frequency Tracker plugin in SDRangel. It works continuously (daemon style) until stop via Ctl-C.

Options are:

  - `-h` or `--help` show help message and exit
  - `-A` or `--address` listening address (default `0.0.0.0`)
  - `-P` or `--port`  listening port (default `8888`)
  - `-a` or `--address-sdr` SDRangel REST API address (defaults to calling address)
  - `-p` or `--port-sdr` SDRangel REST API port (default `8091`)
  - `-f` or `--tracker-frequency` Absolute frequency the tracker should aim at (in Hz optional)
  - `-r` or `--refcorr-limit` Limit of the tracker frequency reference correction. Correction is not apply if error is in this +/- frequency range (Hz, optional, default 1000 Hz)
  - `-d` or `--transverter-device` Transverter device index to use for tracker frequency correction (optional)

With default options (no parameters) it will listen on all available interfaces including loopback at `127.0.0.1` and at port `8888`. It will identify the SDRangel API address with the first request from SDRangel and connect back at port `8091`.

Normal sequence of operations:

  - Start `freqtracking.py` in a terminal
  - In SDRangel connect the Frequency Tracker plugin by clicking on the grey square at the left of the top bar of the Frequency Tracker GUI. It opens the channel settings dialog. Check the 'Reverse API' box. Next to this box is the address and port at which the channel will be connected. If you use the defaults for `freqtracking.py` you may leave it as it is else you have to adjust it to the address and port of `freqtracking.py` (options `-A` and `-P`).
  - In the same manner connect the channel you want to be controlled by `freqtracking.py`. You may connect any number of channels like this. When a channel is removed `freqtracking.py` will automatically remove it from its list at the first attempt to synchronize that will fail.

<h2>ptt_active.py</h2>

PTT (Push To Talk) actively listening system. For a pair of given device set indexes it actively listens to start and stop commands on the corresponding devices to swich over to the other

Options are:

  - `-h` or `--help` show help message and exit
  - `-A` or `--address` listening IP address. Default `0.0.0.0` (all interfaces)
  - `-P` or `--port` listening port. Default `8000`
  - `-p` or `--port-sdr` SDRangel instance REST API listening port. Default `8091`
  - `-l` or `--link` Pair of indexes of the device sets to link. Default `0 1`
  - `-d` or `--delay` Switch over delay in seconds. Default `1`
  - `-f` or `--freq-sync` Synchronize devices center frequencies

Normal sequence of operations:

In this example we have a Rx device on index 0 and a Tx device on index 1. All settings are assumed to be the default settings.

  - Start `ptt_active.py` in a terminal
  - On the Rx device right click on the start/stop button and activate reverse API at address `127.0.0.1` port `8000` (default)
  - On the Tx device right click on the start/stop button and activate reverse API at address `127.0.0.1` port `8000` (default)
  - Start the Rx or Tx device
  - Stop the running device (Rx or Tx) this will switch over automatically to the other

Important: you should initiate switch over by stopping the active device and not by starting the other.

<h2>config.py</h2>

Sends a sequence of commands recorded in a JSON file which is in the form of a list of commands.

Options are:

  - `-h` or `--help` show help message and exit
  - `-a` or `--address` address and port of SDRangel instance. Default is `127.0.0.1:8091`
  - `-j` or `--json-file` JSON file containing description of API commands

Each command in the JSON file is a JSON document with the following keys:

  - `endpoint`: URL suffix (API function) - mandatory
  - `method`: HTTP method (GET, PATCH, POST, PUT, DELETE) - mandatory
  - `params`: pairs of argument and values - optional
  - `payload`: request body in JSON format - optional
  - `msg`: descriptive message for console display - optional
  - `delay`: delay in milliseconds after command - optional

Example of a JSON file (delay is an example you normally do not need it):

```json
[
    {
        "endpoint": "/deviceset/0/device",
        "method": "PUT",
        "payload": {
            "hwType": "HackRF"
        },
        "msg": "Set HackRF on Rx 0"
    },
    {
        "endpoint": "/deviceset/0/device/settings",
        "method": "PUT",
        "payload": {
            "deviceHwType": "HackRF",
            "hackRFInputSettings": {
              "LOppmTenths": 0,
              "bandwidth": 1750000,
              "biasT": 0,
              "centerFrequency": 433800000,
              "dcBlock": 1,
              "devSampleRate": 1536000,
              "fcPos": 2,
              "iqCorrection": 0,
              "lnaExt": 1,
              "lnaGain": 32,
              "log2Decim": 2,
              "reverseAPIAddress": "127.0.0.1",
              "reverseAPIDeviceIndex": 0,
              "reverseAPIPort": 8888,
              "useReverseAPI": 0,
              "vgaGain": 24
            },
            "direction": 0
        },
        "msg": "Setup HackRF on Rx 0"
    },
    {
        "endpoint": "/deviceset/0/channel",
        "method": "POST",
        "payload": {
            "channelType": "RemoteSink",
            "direction": 0
        },
        "msg": "Add a remote sink channel"
    },
    {
        "endpoint": "/deviceset/0/channel/0/settings",
        "method": "PUT",
        "payload": {
            "RemoteSinkSettings": {
              "dataAddress": "192.168.1.5",
              "dataPort": 9094,
              "filterChainHash": 0,
              "log2Decim": 3,
              "nbFECBlocks": 8,
              "reverseAPIAddress": "127.0.0.1",
              "reverseAPIChannelIndex": 0,
              "reverseAPIDeviceIndex": 0,
              "reverseAPIPort": 8888,
              "rgbColor": -7601148,
              "title": "Channel 0",
              "txDelay": 0,
              "useReverseAPI": 0
            },
            "channelType": "RemoteSink",
            "direction": 0
        },
        "msg": "Setup remote sink on channel 0",
        "delay": 1000
    },
    {
        "endpoint": "/deviceset/0/device/run",
        "method": "POST",
        "msg": "Start device on deviceset R0"
    }
]
```
If you have presets defined you may also use presets instead of having to set up the device and channels. In this case you only "PUT" the right device and apply the preset like this:

```json
[
    {
        "endpoint": "/deviceset/0/device",
        "method": "PUT",
        "payload": {
            "hwType": "RTLSDR"
        },
        "msg": "setup RTLSDR on Rx 0"
    },
    {
        "endpoint": "/preset",
        "method": "PATCH",
        "payload": {
            "deviceSetIndex": 0,
            "preset": {
                "groupName": "QO100",
                "centerFrequency": 489675000,
                "type": "R",
                "name": "Narrowband master"
            }
        },
        "msg": "load preset on Rx 0"
    },
    {
        "endpoint": "/deviceset/0/device/run",
        "method": "POST",
        "msg": "Start device on deviceset R0"
    }
]
```