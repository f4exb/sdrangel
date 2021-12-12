///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018-2019 Edouard Griffiths, F4EXB.                             //
//                                                                               //
// Remote sink channel (Rx) UDP sender thread                                    //
//                                                                               //
// SDRangel can work as a detached SDR front end. With this plugin it can        //
// sends the I/Q samples stream to another SDRangel instance via UDP.            //
// It is controlled via a Web REST API.                                          //
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

#ifndef REMOTEOUTPUT_REMOTEOUTPUTSENDER_H_
#define REMOTEOUTPUT_REMOTEOUTPUTSENDER_H_

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QHostAddress>

#include "cm256cc/cm256.h"

#include "util/message.h"
#include "util/messagequeue.h"

#include "remoteoutputfifo.h"

class RemoteDataFrame;
class CM256;
class QUdpSocket;

class RemoteOutputSender : public QObject {
    Q_OBJECT

public:
    RemoteOutputSender();
    ~RemoteOutputSender();

    RemoteDataFrame *getDataFrame();
    void setDestination(const QString& address, uint16_t port);

private:
    RemoteOutputFifo m_fifo;
    CM256 m_cm256;                       //!< CM256 library object
    CM256 *m_cm256p;
    bool m_cm256Valid;                   //!< true if CM256 library is initialized correctly

    QUdpSocket   *m_udpSocket;
    QString      m_remoteAddress;
    uint16_t     m_remotePort;
    QHostAddress m_remoteHostAddress;

    void sendDataFrame(RemoteDataFrame *dataFrame);

private slots:
    void handleData();
};

#endif // REMOTEOUTPUT_REMOTEOUTPUTSENDER_H_

