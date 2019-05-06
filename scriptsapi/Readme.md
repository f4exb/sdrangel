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

With default options (no parameters) it will listen on all available interfaces including loopback at `127.0.0.1` and at port `8888`. It will identify the SDRangel API address with the first request from SDRangel and connect back at port `8091`.

Normal sequence of operations:

  - Start `freqtracking.py` in a terminal
  - In SDRangel connect the Frequency Tracker plugin by clicking on the grey square at the left of the top bar of the Frequency Tracker GUI. It opens the channel settings dialog. Check the 'Reverse API' box. Next to this box is the address and port at which the channel will be connected. If you use the defaults for `freqtracking.py` you may leave it as it is else you have to adjust it to the address and port of `freqtracking.py` (options `-A` and `-P`).
  - In the same manner connect the channel you want to be controlled by `freqtracking.py`. You may connect any number of channels like this. When a channel is removed `freqtracking.py` will automatically remove it from its list at the first attempt to synchronize that will fail.
