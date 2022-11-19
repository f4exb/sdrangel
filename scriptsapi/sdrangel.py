"""
Constants that refer to SDRangel software
"""

# Device keys depending on hardware type (deviceHwType)
DEVICE_TYPES = {
    "Airspy": {
        "settings": "airspySettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "AirspyHF": {
        "settings": "airspyHFSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "BladeRF2": {
        "settings": "bladeRF1OutputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "HackRF": {
        "settings": "hackRFInputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "LimeSDR": {
        "settings": "limeSdrInputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2SoftDecim",
    },
    "PlutoSDR": {
        "settings": "plutoSdrInputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "RTLSDR": {
        "settings": "rtlSdrSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "SDRplay1": {
        "settings": "sdrPlayV3Settings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "SDRplayV3": {
        "settings": "sdrPlaySettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2Decim",
    },
    "USRP": {
        "settings": "usrpInputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2SoftDecim",
    },
    "XTRX": {
        "settings": "xtrxInputSettings",
        "cf_key": "centerFrequency",
        "decim_key": "log2SoftDecim",
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
