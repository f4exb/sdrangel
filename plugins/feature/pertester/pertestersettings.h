///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_PERTESTERSETTINGS_H_
#define INCLUDE_FEATURE_PERTESTERSETTINGS_H_

#include <QByteArray>
#include <QString>

#include "util/message.h"

class Serializable;

struct PERTesterSettings
{
    int m_packetCount;                  //!< How many packets to send
    float m_interval;                   //!< Interval between packets in seconds
    QString m_packet;                   //!< Contents of the packet
    QString m_txUDPAddress;             //!< UDP port of modulator to send packets too
    uint16_t m_txUDPPort;
    QString m_rxUDPAddress;             //!< UDP port to receive packets from demodulator on
    uint16_t m_rxUDPPort;
    int m_ignoreLeadingBytes;           //!< Number of bytes to ignore at start of received frame (E.g. header)
    int m_ignoreTrailingBytes;          //!< Number of bytes to ignore at end of received frame (E.g. CRC)
    enum Start {START_IMMEDIATELY, START_ON_AOS, START_ON_MID_PASS} m_start;    //!< When to start test
    QList<QString> m_satellites;        //!< Which satellites to start test on

    QString m_title;
    quint32 m_rgbColor;
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIFeatureSetIndex;
    uint16_t m_reverseAPIFeatureIndex;
    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;

    PERTesterSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }

    QByteArray serializeStringList(const QList<QString>& strings) const;
    void deserializeStringList(const QByteArray& data, QList<QString>& strings);

    void applySettings(const QStringList& settingsKeys, const PERTesterSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif // INCLUDE_FEATURE_PERTESTERSETTINGS_H_
