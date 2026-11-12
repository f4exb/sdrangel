///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>

class Serializable;
class Feature;

struct DSDDemodSettings
{
    struct AvailableAMBEFeature
    {
        int m_featureIndex = 0;
        Feature *m_feature = nullptr;

        AvailableAMBEFeature() = default;
        AvailableAMBEFeature(const AvailableAMBEFeature&) = default;
        AvailableAMBEFeature& operator=(const AvailableAMBEFeature&) = default;
    };

    //! Protocols searched for: automatic detection or one protocol only
    enum Protocol
    {
        ProtocolAuto,
        ProtocolDMR,
        ProtocolDStar,
        ProtocolYSF,
        ProtocolNXDN48,
        ProtocolNXDN96,
        ProtocolDPMR,
        ProtocolP25,
        ProtocolP25P2,
        ProtocolM17,
        ProtocolAngel61,        //!< Angel61 NB (6.25 kHz; in Auto too)
        ProtocolAngel61WB,      //!< Angel61 WB (12.5 kHz; in Auto too)
        ProtocolAngel61HFNB,    //!< Angel61 HF-NB (OFDM, 2.5 kHz): selected only, never in Auto
        ProtocolAngel61HFWB,    //!< Angel61 HF-WB (OFDM, 5.5 kHz)
        NbProtocols
    };

    qint64 m_inputFrequencyOffset;
    Real  m_rfBandwidth;
    Real  m_volume;
    int  m_protocol;
    int m_p25p2Wacn;    //!< P25 Phase 2 scrambling: WACN (20 bits), system ID (12 bits) and NAC (12 bits) of the system
    int m_p25p2Sysid;
    int m_p25p2Nac;
    bool m_coherent;    //!< Coherent re-detection of the 4FSK bursts from the IQ (about 2 dB, echo equalisation)
    bool m_audioMute;
    bool m_slot1On;
    bool m_slot2On;
    bool m_tdmaStereo;
    int m_dmrColorCode; //!< Audio filter on the DMR or dPMR colour code or the NXDN RAN: -1 accepts any; otherwise 0..63 (DMR stops at 15)
    QString m_angel61FileDirectory; //!< Where Angel61 file transfers are written, empty: not stored
    QString m_codecModelDirectory;  //!< Angel61's V1 models (vocoscodec); empty for the default lookup (VocosCodec::defaultModelDirectory)
    int m_angel61FileMaxKB;         //!< The largest Angel61 file this station takes, 0 for no limit
    bool m_angel61FileConfirm;      //!< The operator confirms every Angel61 file offer before the file is kept (3.4)
    QString m_angel61Groups;        //!< Angel61 talkgroups played, "9, 23"; empty: every call (ANGEL61.md 2.6)
    bool m_contactReportEnabled;    //!< Opt-in RF contact reporting to angel61.sdrangel.org, with the station's position
    QString m_contactReportCallsign;
    quint32 m_rgbColor;
    QString m_title;
    bool m_highPassFilter;
    int m_traceLengthMutliplier; // x 50ms
    int m_traceStroke; // [0..255]
    int m_traceDecay; // [0..255]
    QString m_audioDeviceName;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;
    int m_ambeFeatureIndex;
    bool m_connectAMBE;

    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    DSDDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
    static const char *getProtocolName(int protocol);
    //! A stored or requested protocol: out of range is Auto
    static int protocolOf(int value) { return (value >= 0) && (value < NbProtocols) ? value : ProtocolAuto; }
};


#endif /* PLUGINS_CHANNELRX_DEMODDSD_DSDDEMODSETTINGS_H_ */
