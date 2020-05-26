"""
Constants that refer to SDRangel software
"""

# Device keys depending on hardware type (deviceHwType)
DEVICE_TYPES = {
    "AirspyHF": {
        "settings": "airspyHFSettings",
        "cf_key": "centerFrequency",
    }
}

# Channel keys depending on channel type (id)
CHANNEL_TYPES = {
    "DSDDemod": {
        "settings": "DSDDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "NFMDemod": {
        "settings": "NFMDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    }
}