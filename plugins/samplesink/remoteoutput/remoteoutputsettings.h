///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_REMOTEOUTPUT_REMOTEOUTPUTSETTINGS_H_
#define PLUGINS_REMOTEOUTPUT_REMOTEOUTPUTSETTINGS_H_

#include <QByteArray>
#include <QString>

struct RemoteOutputSettings {
    quint32 m_nbFECBlocks;
    quint32 m_nbTxBytes;
    QString m_apiAddress;
    quint16 m_apiPort;
    QString m_dataAddress;
    quint16 m_dataPort;
    quint32 m_deviceIndex;
    quint32 m_channelIndex;
    bool     m_useReverseAPI;
    QString  m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;

    RemoteOutputSettings();
    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const RemoteOutputSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* PLUGINS_REMOTEOUTPUT_REMOTEOUTPUTSETTINGS_H_ */
