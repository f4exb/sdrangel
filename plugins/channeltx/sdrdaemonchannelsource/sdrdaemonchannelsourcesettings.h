///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon source channel (Tx) main settings                                   //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef CHANNEL_TX_SDRDAEMONCHANNELSOURCESETTINGS_H_
#define CHANNEL_TX_SDRDAEMONCHANNELSOURCESETTINGS_H_

#include <QString>
#include <QByteArray>

class Serializable;

struct SDRDaemonChannelSourceSettings
{
    QString  m_dataAddress; //!< Listening (local) data address
    uint16_t m_dataPort;    //!< Listening data port
    quint32 m_rgbColor;
    QString m_title;

    Serializable *m_channelMarker;

    SDRDaemonChannelSourceSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
};


#endif /* CHANNEL_TX_SDRDAEMONCHANNELSOURCESETTINGS_H_ */
