///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_CHANNELTX_MODDATV_DATVMODSETTINGS_H_
#define PLUGINS_CHANNELTX_MODDATV_DATVMODSETTINGS_H_

#include <QByteArray>
#include <QString>
#include <stdint.h>

class Serializable;

struct DATVModSettings
{
    enum DATVSource
    {
        SourceFile,
        SourceUDP
    };

    enum DVBStandard
    {
        DVB_S,
        DVB_S2
    };

    enum DATVModulation
    {
        BPSK,   // DVB-S
        QPSK,
        PSK8,   // DVB-S2
        APSK16, // DVB-S2
        APSK32  // DVB-S2
    };
    static const QStringList m_modulationStrings;

    enum DATVCodeRate
    {
        FEC12,
        FEC23,
        FEC34,
        FEC56,
        FEC78,  // DVB-S
        FEC45,  // DVB-S2
        FEC89,  // DVB-S2
        FEC910, // DVB-S2
        FEC14,  // DVB-S2
        FEC13,  // DVB-S2
        FEC25,  // DVB-S2
        FEC35   // DVB-S2
    };
    static const QStringList m_codeRateStrings;

    qint64 m_inputFrequencyOffset;              //!< Offset from baseband center frequency
    Real m_rfBandwidth;                         //!< Bandwidth of modulated signal

    DVBStandard m_standard;
    DATVModulation m_modulation;                //!< RF modulation type
    DATVCodeRate m_fec;                         //!< FEC code rate
    int m_symbolRate;                           //!< Symbol rate in symbols per second
    float m_rollOff;                            //!< 0.35, 0.25, 0.2

    DATVSource m_source;                        //!< Source of transport stream

    QString m_tsFileName;
    bool m_tsFilePlayLoop;                      //!< Play TS file in a loop
    bool m_tsFilePlay;                          //!< True to play TS file and false to pause

    QString m_udpAddress;                       //!< UDP to receive transport stream packets on
    int m_udpPort;                              //!< UDP port number

    bool m_channelMute;                         //!< Mute channel baseband output

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;

    int m_streamIndex;
    bool m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    DATVModSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    static DATVCodeRate mapCodeRate(const QString& string);
    static QString mapCodeRate(DATVCodeRate codeRate);
    static DATVModulation mapModulation(const QString& string);
    static QString mapModulation(DATVModulation modulation);

    static const int m_udpBufferSize = 5000000;

};

#endif /* PLUGINS_CHANNELTX_MODDATV_DATVMODSETTINGS_H_ */
