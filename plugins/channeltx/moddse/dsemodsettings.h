///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"
#include "dsp/cwkeyersettings.h"

class Serializable;
class Feature;

struct DSEModSettings
{
    enum DSEModMode
    {
        DSEModModeDStar,
        DSEModModeDMR,
        DSEModModeYSF,
        DSEModModeNXDN,
        DSEModModeNXDN96,   //!< NXDN at 4800 symbols/s (9600 bit/s, 12.5 kHz)
        DSEModModeDPMR,
        DSEModModeP25,
        DSEModModeP25P2,    //!< P25 Phase 2 TDMA at 6000 symbols/s (4FSK standing in for H-CPM)
        DSEModModeM17,      //!< M17: 4FSK at 4800 symbols/s with Codec2 3200 (https://spec.m17project.org)
        DSEModModeAngel61,  //!< Angel61 NB: QPSK / 16-QAM at 4800 symbols/s with V1, 6.25 kHz (sdrbase/dsp/dv/ANGEL61.md)
        DSEModModeAngel61WB, //!< Angel61 WB: QPSK / 16-QAM at 9600 symbols/s with V1 or Opus, 12.5 kHz
        DSEModModeAngel61HFNB, //!< Angel61 HF-NB: OFDM, 50 carriers in 2.5 kHz
        DSEModModeAngel61HFWB  //!< Angel61 HF-WB: OFDM, 110 carriers in 5.5 kHz
    };
    //! A stored or requested mode: out of range is D-STAR
    static DSEModMode modeOf(int value) { return (value < 0) || (value > (int) DSEModModeAngel61HFWB) ? DSEModModeDStar : (DSEModMode) value; }

    //! Any Angel61 profile (DSEModAngel61)
    static bool isAngel61(DSEModMode mode) { return isAngel61VHF(mode) || isAngel61HF(mode); }
    //! Angel61 NB or WB (linear)
    static bool isAngel61VHF(DSEModMode mode) { return (mode == DSEModModeAngel61) || (mode == DSEModModeAngel61WB); }
    //! Angel61 HF-NB or HF-WB (OFDM)
    static bool isAngel61HF(DSEModMode mode) { return (mode == DSEModModeAngel61HFNB) || (mode == DSEModModeAngel61HFWB); }

    //! The tier (ANGEL61.md 2.10). The values are kept from before tier 1 came between the others
    enum DSEModAngel61Rate
    {
        DSEModAngel61RateAuto,   //!< The destination's rate request when heard, else tier 0
        DSEModAngel61RateLow,    //!< Tier 0: NB V1 2.25k, WB 4.5k, HF-NB 1.5k, HF-WB 2.25k; QPSK
        DSEModAngel61RateHigh,   //!< Tier 2: NB V1 4.5k on QPSK, WB 12k, HF-NB 3.0k, HF-WB 6.0k on 16-QAM
        DSEModAngel61RateMedium, //!< Tier 1: NB V1 3.0k, WB 9.0k, HF-WB 4.5k on QPSK, HF-NB 2.25k on 16-QAM
        DSEModAngel61RateTier3   //!< Tier 3: NB V1 9.0k, WB Opus 18.4k, HF-NB V1 4.5k, HF-WB V1 9.0k; 16-QAM
    };

    enum DSEModInputAF
    {
        DSEModInputNone,
        DSEModInputTone,
        DSEModInputFile,
        DSEModInputAudio,
        DSEModInputCWTone,
        DSEModInputSilence
    };

    //! YSF's voice mode (the FICH's data type): V/D type 2 (DN, AMBE+2 2450 with its data channel,
    //! what radios send by default), V/D type 1 (AMBE+2 3600x2450, a larger data channel) or voice
    //! full rate (VW, IMBE 7200x4400)
    enum DSEModYSFMode
    {
        DSEModYSFModeVD2,
        DSEModYSFModeVD1,
        DSEModYSFModeVW
    };

    enum DSEModVocoderType
    {
        DSEModVocoderMbelib,      //!< Software encoder (silence frames if not built in)
        DSEModVocoderAMBEFeature  //!< DVSI device through the AMBE feature
    };

    struct AvailableAMBEFeature
    {
        int m_featureIndex = 0;
        Feature *m_feature = nullptr;

        AvailableAMBEFeature() = default;
        AvailableAMBEFeature(const AvailableAMBEFeature&) = default;
        AvailableAMBEFeature& operator=(const AvailableAMBEFeature&) = default;
    };

    qint64 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    DSEModMode m_mode;
    bool m_transmit;            //!< PTT: start (header) / stop (end of transmission)
    bool m_invert;              //!< Invert symbol polarity
    DSEModInputAF m_modAFInput;
    float m_toneFrequency;
    float m_volumeFactor;
    bool m_channelMute;
    bool m_playLoop;
    QString m_audioDeviceName;
    DSEModVocoderType m_vocoder;
    int m_ambeFeatureIndex;     //!< Index of AMBE feature in feature set 0 (-1 for none)
    bool m_connectAMBE;         //!< Use the AMBE feature for encoding
    int m_vocoderGain;          //!< Encoder input gain (dB) for DVSI devices
    bool m_toneDetect;          //!< mbevocoder encoder: code single tones, DTMF and call progress tones as tone frames
    QString m_feedbackAudioDeviceName; //!< Where the operator hears what is being encoded
    float m_feedbackVolumeFactor;
    bool m_feedbackAudioEnable;
    bool m_spectrumAudio;       //!< Spectrum shows the speech input (encoder audio) instead of the modulated IQ
    bool m_noiseSuppress;       //!< mbevocoder encoder: noise suppression on the input
    QString m_dtmfSequence;     //!< DTMF digits sent by the DTMF send action when it carries none
    //! Send this station's own position in the modes that carry one: D-STAR as D-PRS slow data,
    //! DMR as a GPS Info embedded LC, Angel61 in its position slot
    bool m_sendPosition;
    // D-STAR
    QString m_dstarMyCall;
    QString m_dstarMySuffix;
    QString m_dstarUrCall;
    QString m_dstarRpt1;
    QString m_dstarRpt2;
    QString m_dstarText;
    int m_dstarFlag1;
    int m_dstarFlag2;
    int m_dstarFlag3;
    // DMR
    int m_dmrColorCode;
    int m_dmrSourceId;
    int m_dmrDestinationId;
    bool m_dmrGroupCall;
    int m_dmrSlot;
    bool m_dmrDirect;           //!< MS sourced single slot bursts (direct mode, simplex hotspot) instead of continuous base station              //!< 1 or 2
    int m_dmrFid;
    int m_dmrServiceOptions;
    // YSF
    QString m_ysfCallsign;      //!< 10 characters
    QString m_ysfDestination;   //!< 10 characters, "*****" for all
    QString m_ysfDownlink;
    QString m_ysfUplink;
    DSEModYSFMode m_ysfMode;
    // NXDN
    int m_nxdnRan;              //!< Radio access number 0..63
    int m_nxdnSourceId;         //!< 16 bit unit ID
    int m_nxdnDestinationId;    //!< 16 bit unit or group ID
    bool m_nxdnGroupCall;
    // dPMR
    int m_dpmrColorCode;        //!< 0..63
    int m_dpmrOwnId;            //!< 24 bit
    int m_dpmrCalledId;         //!< 24 bit, 16777215 (all ones) for all call
    // P25
    int m_p25Nac;               //!< 12 bit network access code
    int m_p25TalkGroup;         //!< 16 bit
    int m_p25SourceId;          //!< 24 bit
    int m_p25p2Wacn;            //!< P25 Phase 2 scrambling: 20 bit WACN and 12 bit system ID (the NAC is m_p25Nac)
    int m_p25p2Sysid;
    DSEModAngel61Rate m_angel61Rate;
    QString m_angel61Source;    //!< Own callsign (base-40, up to 9 characters: G4ABC-7, F/M7RCE/P)
    QString m_angel61Destination; //!< Destination callsign, CQ for all
    QString m_angel61Path;      //!< Repeater path: up to two callsigns separated by a comma, empty for none
    QString m_angel61Message;   //!< Text sent by the message action when it carries none
    QString m_angel61Demod;     //!< The DSD Demodulator the rate request is taken from ("R0:2"), empty for any of them
    QString m_angel61Symbol;    //!< APRS symbol (3.3): the table character and the code, e.g. "/-"
    QString m_angel61FileName;  //!< The file offered by the file transfer (3.4), empty for none
    // IP tunnelling (ANGEL61.md 3.5). Off until the operator turns it on, and it names one peer:
    // a datagram is only sent to that station and only accepted from it
    bool m_angel61Tunnel;
    QString m_angel61TunnelPeer;        //!< the callsign at the other end
    QString m_angel61TunnelInterface;   //!< the host interface to create (sdrangel0)
    QString m_angel61TunnelAddress;     //!< this end's address and prefix length, 10.255.0.1/32
    QString m_angel61TunnelRoutes;      //!< prefixes routed into the tunnel, comma separated
    QString m_angel61TunnelAllow;       //!< what may be sent: "icmp, tcp:22, udp:53"; empty for everything (3.5)
    QString m_angel61TunnelGroups;      //!< multicast groups sent all the same: "224.0.0.251, ff02::fb" (3.5)
    int m_angel61TunnelMTU;             //!< 576 at the low rate, 1280 at the high one (3.5)
    // M17
    QString m_m17Source;        //!< Own callsign (base-40, up to 9 characters)
    QString m_m17Destination;   //!< Destination callsign, empty or ALL for the broadcast
    int m_m17CAN;               //!< Channel access number, 0..15
    QString m_m17Message;       //!< Text sent as an SMS packet transmission
    QString m_codecModelDirectory; //!< Angel61's V1 models (vocoscodec); empty for the default lookup (VocosCodec::defaultModelDirectory)
    QString m_audioFileName;      //!< The speech file (.wav or .raw) of the file input, empty for none
    int m_frameSessionMaxDurationMs; //!< Maximum duration of an externally supplied gateway-frame session

    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_channelMarker;
    Serializable *m_spectrumGUI;
    Serializable *m_cwKeyerGUI;
    CWKeyerSettings m_cwKeyerSettings; //!< For standalone deserialize operation (without m_cwKeyerGUI)
    Serializable *m_rollupState;

    DSEModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setSpectrumGUI(Serializable *spectrumGUI) { m_spectrumGUI = spectrumGUI; }
    void setCWKeyerGUI(Serializable *cwKeyerGUI) { m_cwKeyerGUI = cwKeyerGUI; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    const CWKeyerSettings& getCWKeyerSettings() const { return m_cwKeyerSettings; }
    void setCWKeyerSettings(const CWKeyerSettings& cwKeyerSettings) { m_cwKeyerSettings = cwKeyerSettings; }
    void applySettings(const QStringList& settingsKeys, const DSEModSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force = false) const;

    static int getMbeRateIndex(DSEModMode mode);   //!< SerialDV::DVRate compatible index of the vocoder rate
    static const char* getModeName(DSEModMode mode);
};

#endif /* PLUGINS_CHANNELTX_MODDSE_DSEMODSETTINGS_H_ */
