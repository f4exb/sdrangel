///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef DATVUDPSTREAM_H
#define DATVUDPSTREAM_H

#include <QUdpSocket>
#include <QHostAddress>
#include <QString>

class QString;

class DATVUDPStream
{
public:
    DATVUDPStream(int tsBlockSize);
    ~DATVUDPStream();

    void pushData(const char *chrData, int nbTSBlocks);
    void setActive(bool active) { m_active = active; }
    bool setAddress(const QString& address) { return m_address.setAddress(address); }
    void setPort(quint16 port) { m_port = port; }

    static const int m_tsBlocksPerFrame;

private:
    bool m_active;
    QUdpSocket m_udpSocket;
    QHostAddress m_address;
    quint16 m_port;
    int m_tsBlockSize;
    int m_tsBlockIndex;
    char *m_tsBuffer;
};

#endif // DATVUDPSTREAM_H