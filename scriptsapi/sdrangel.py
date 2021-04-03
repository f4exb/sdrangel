"""
Constants that refer to SDRangel software
"""

# Device keys depending on hardware type (deviceHwType)
DEVICE_TYPES = {
    "Airspy": {
        "settings": "airspySettings",
        "cf_key": "centerFrequency",
    },
    "AirspyHF": {
        "settings": "airspyHFSettings",
        "cf_key": "centerFrequency",
    },
    "RTLSDR": {
        "settings": "rtlSdrSettings",
        "cf_key": "centerFrequency",
    },
}

# Channel keys depending on channel type (id)
CHANNEL_TYPES = {
    "AMDemod": {
        "settings": "AMDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "BFMDemod": {
        "settings": "BFMDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "volume"
    },
    "ChirpChatDemod": {
        "settings": "ChirpChatDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "decodeActive"
    },
    "DATVDemod": {
        "settings": "DATVDemodSettings",
        "df_key": "centerFrequency",
        "mute_key": "audioMute"
    },
    "DSDDemod": {
        "settings": "DSDDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "FreeDVDemod": {
        "settings": "FreeDVDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "NFMDemod": {
        "settings": "NFMDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "SSBDemod": {
        "settings": "SSBDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    },
    "UDPSink": {
        "settings": "UDPSinkSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "channelMute"
    },
    "WFMDemod": {
        "settings": "WFMDemodSettings",
        "df_key": "inputFrequencyOffset",
        "mute_key": "audioMute"
    }
}
