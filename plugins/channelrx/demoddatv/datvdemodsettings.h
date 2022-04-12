///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#ifndef PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_
#define PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>
#include <vector>

class Serializable;

struct DATVDemodSettings
{
    enum dvb_version
    {
        DVB_S,
        DVB_S2
    };

    enum DATVModulation
    {
        BPSK,    // not DVB-S2
        QPSK,
        PSK8,
        APSK16,  // not DVB-S
        APSK32,  // not DVB-S
        APSK64E, // not DVB-S
        QAM16,   // not DVB-S2
        QAM64,   // not DVB-S2
        QAM256,  // not DVB-S2
        MOD_UNSET
    };

    enum DATVCodeRate
    {
        FEC12, // DVB-S
        FEC23, // DVB-S
        FEC46,
        FEC34, // DVB-S
        FEC56, // DVB-S
        FEC78, // DVB-S
        FEC45,
        FEC89,
        FEC910,
        FEC14,
        FEC13,
        FEC25,
        FEC35,
        RATE_UNSET
    };

    enum dvb_sampler
    {
        SAMP_NEAREST,
        SAMP_LINEAR,
        SAMP_RRC
    };

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_rfBandwidth;
    int m_centerFrequency;
    dvb_version m_standard;
    DATVModulation m_modulation;
    DATVCodeRate m_fec;
    bool m_softLDPC;
    QString m_softLDPCToolPath;
    int m_softLDPCMaxTrials;
    int m_maxBitflips;
    bool m_audioMute;
    QString m_audioDeviceName;
    int m_symbolRate;
    int m_notchFilters;
    bool m_allowDrift;
    bool m_fastLock;
    dvb_sampler m_filter;
    bool m_hardMetric;
    float m_rollOff;
    bool m_viterbi;
    int m_excursion;
    int m_audioVolume;
    bool m_videoMute;
    QString m_udpTSAddress;
    quint32 m_udpTSPort;
    bool m_udpTS;
    bool m_playerEnable;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    static const int m_softLDPCMaxMaxTrials = 50;

    DATVDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void debug(const QString& msg) const;
    bool isDifferent(const DATVDemodSettings& other); // true if a change of settings should trigger DVB framework config update
    void validateSystemConfiguration();

    static DATVModulation getModulationFromStr(const QString& str);
    static DATVCodeRate getCodeRateFromStr(const QString& str);
    static QString getStrFromModulation(const DATVModulation modulation);
    static QString getStrFromCodeRate(const DATVCodeRate codeRate);
    static void getAvailableModulations(dvb_version dvbStandard, std::vector<DATVModulation>& modulations);
    static void getAvailableCodeRates(dvb_version dvbStandard, DATVModulation modulation, std::vector<DATVCodeRate>& codeRates);
    static DATVDemodSettings::DATVCodeRate getCodeRateFromLeanDVBCode(int leanDVBCodeRate);
    static DATVDemodSettings::DATVModulation getModulationFromLeanDVBCode(int leanDVBModulation);
};

#endif // PLUGINS_CHANNELRX_DEMODATV_DATVDEMODSETTINGS_H_
