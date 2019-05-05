## Python scripts interfacing with the API ##

These scripts are designed to work in Python 3 preferably with version 3.6 or higher. Dependencies are installed with pip in a virtual environment. The sequence of operations is the following:

```
virtualenv -p /usr/bin/python3 venv # Create virtual environment
. ./venv/bin/activate               # Activate virtual environment
pip install -r requirements.txt     # Install requirements
```

<h2>freqtracking.py</h2>

This script is used to achieve frequency tracking with the FreqTracker plugin.