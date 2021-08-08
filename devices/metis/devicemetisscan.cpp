///////////////////////////////////////////////////////////////////////////////////
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

#include <QEventLoop>
#include <QTimer>
#include <QNetworkInterface>
#include <QSet>

#include "devicemetisscan.h"

void DeviceMetisScan::scan()
{
    m_scans.clear();

    if (m_udpSocket.bind(QHostAddress::AnyIPv4, 10001, QUdpSocket::ShareAddress))
    {
        connect(&m_udpSocket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    }
    else
    {
        qDebug() << "DeviceMetisScan::scan: cannot bind socket";
        return;
    }

    unsigned char buffer[63];
    buffer[0] = (unsigned char) 0xEF;
    buffer[1] = (unsigned char) 0XFE;
    buffer[2] = (unsigned char) 0x02;
    std::fill(&buffer[3], &buffer[63], 0);

    QList<QNetworkInterface> ifaces = QNetworkInterface::allInterfaces();
    QSet<QHostAddress> broadcastAddrs;

    for (int i = 0; i < ifaces.size(); i++)
    {
        QList<QNetworkAddressEntry> addrs = ifaces[i].addressEntries();

        for (int j = 0; j < addrs.size(); j++)
        {
            if ((addrs[j].ip().protocol() == QAbstractSocket::IPv4Protocol) && (addrs[j].broadcast().toString() != ""))
            {
                QHostAddress br = addrs[j].broadcast();
                broadcastAddrs.insert(br);
            }
        }
    }

    QSet<QHostAddress>::const_iterator brIt = broadcastAddrs.begin();

    for (; brIt != broadcastAddrs.end(); ++brIt)
    {
        if (m_udpSocket.writeDatagram((const char*) buffer, sizeof(buffer), *brIt, 1024) < 0)
        {
            qDebug("DeviceMetisScan::scan: discovery writeDatagram to %s failed: %s ",
                qPrintable(brIt->toString()), qPrintable(m_udpSocket.errorString()));
            continue;
        }
        else
        {
            qDebug("DeviceMetisScan::scan: discovery writeDatagram sent to %s", qPrintable(brIt->toString()));
        }
    }

    // wait for timeout before returning
    QEventLoop loop;
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    timer->start(500); // 500 ms timeout

    qDebug() << "DeviceMetisScan::scan: start 0.5 second timeout loop";
    // Execute the event loop here and wait for the timeout to trigger
    // which in turn will trigger event loop quit.
    loop.exec();

    disconnect(&m_udpSocket, SIGNAL(readyRead()), this, SLOT(readyRead()));
    m_udpSocket.close();
}

void DeviceMetisScan::enumOriginDevices(const QString& hardwareId, PluginInterface::OriginDevices& originDevices)
{
    scan();

    for (int i = 0; i < m_scans.size(); i++)
    {
        const DeviceScan& deviceInfo = m_scans.at(i);
        QString serial = QString("%1:%2_%3").arg(deviceInfo.m_address.toString()).arg(deviceInfo.m_port).arg(deviceInfo.m_serial);
        QString displayableName(QString("Metis[%1] %2").arg(i).arg(serial));
        originDevices.append(PluginInterface::OriginDevice(
            displayableName,
            hardwareId,
            serial,
            i, // sequence
            8, // Nb Rx
            1  // Nb Tx
        ));
    }
}

const DeviceMetisScan::DeviceScan* DeviceMetisScan::getDeviceAt(int index) const
{
    if (index < m_scans.size()) {
        return &m_scans.at(index);
    } else {
        return nullptr;
    }
}

void DeviceMetisScan::getSerials(QList<QString>& serials) const
{
    for (int i = 0; i < m_scans.size(); i++) {
        serials.append(m_scans.at(i).m_serial);
    }
}

void DeviceMetisScan::readyRead()
{
    QHostAddress metisAddress;
    quint16 metisPort;
    unsigned char buffer[1024];

    if (m_udpSocket.readDatagram((char*) &buffer, (qint64) sizeof(buffer), &metisAddress, &metisPort) < 0)
    {
        qDebug() << "DeviceMetisScan::readyRead: readDatagram failed " << m_udpSocket.errorString();
        return;
    }

    QString metisIP = QString("%1:%2").arg(metisAddress.toString()).arg(metisPort);

    if (buffer[0] == 0xEF && buffer[1] == 0xFE)
    {
        switch(buffer[2])
        {
            case 3:  // reply
                // should not happen on this port
                break;
            case 2:  // response to a discovery packet
            {
                QByteArray array((char *) &buffer[3], 6);
                QString serial = QString(array.toHex());
                m_scans.append(DeviceScan(
                    serial,
                    metisAddress,
                    metisPort
                ));
                m_serialMap.insert(serial, &m_scans.back());
                qDebug() << "DeviceMetisScan::readyRead: found Metis at:" << metisIP << "MAC:" << serial;
            }
                break;
            case 1:  // a data packet
                break;
        }
    }
    else
    {
        qDebug() << "DeviceMetisScan::readyRead: received invalid response to discovery";
    }
}
