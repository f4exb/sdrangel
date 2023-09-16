///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_REMOTECHANNELSINKSETTINGS_H_
#define INCLUDE_REMOTECHANNELSINKSETTINGS_H_

#include <QByteArray>
#include <QString>

class Serializable;

struct RemoteTCPSinkSettings
{
    enum Protocol {
        RTL0,                           // Compatible with rtl_tcp
        SDRA                            // SDRangel remote TCP protocol which extends rtl_tcp
    };

    qint32 m_channelSampleRate;
    qint32 m_inputFrequencyOffset;
    qint32 m_gain;                      // 10ths of a dB
    uint32_t m_sampleBits;
    QString m_dataAddress;
    uint16_t m_dataPort;
    enum Protocol m_protocol;
    quint32 m_rgbColor;
    QString m_title;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    Serializable *m_channelMarker;
    Serializable *m_rollupState;

    RemoteTCPSinkSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RemoteTCPSinkSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_REMOTECHANNELSINKSETTINGS_H_ */
