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

#include "udpsourceudphandler.h"

#include <QDebug>
#include <stdint.h>
#include <algorithm>

#include "udpsourcemsg.h"

MESSAGE_CLASS_DEFINITION(UDPSourceUDPHandler::MsgUDPAddressAndPort, Message)

UDPSourceUDPHandler::UDPSourceUDPHandler() :
    m_dataSocket(nullptr),
    m_dataAddress(QHostAddress::LocalHost),
    m_remoteAddress(QHostAddress::LocalHost),
    m_multicastAddress(QStringLiteral("224.0.0.1")),
    m_dataPort(9999),
    m_remotePort(0),
    m_dataConnected(false),
    m_multicast(false),
    m_udpDumpIndex(0),
    m_nbUDPFrames(m_minNbUDPFrames),
    m_nbAllocatedUDPFrames(m_minNbUDPFrames),
    m_writeFrameIndex(0),
    m_readFrameIndex(m_minNbUDPFrames/2),
    m_readIndex(0),
    m_rwDelta(m_minNbUDPFrames/2),
    m_d(0),
    m_autoRWBalance(true),
    m_feedbackMessageQueue(nullptr)
{
    m_udpBuf = new udpBlk_t[m_minNbUDPFrames];
    std::fill(m_udpDump, m_udpDump + m_udpBlockSize + 8192, 0);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()));
}

UDPSourceUDPHandler::~UDPSourceUDPHandler()
{
    stop();
    delete[] m_udpBuf;
}

void UDPSourceUDPHandler::start()
{
    qDebug("UDPSourceUDPHandler::start");

    if (!m_dataSocket) {
        m_dataSocket = new QUdpSocket(this);
    }

    if (!m_dataConnected)
    {
        if (m_dataSocket->bind(m_multicast ? QHostAddress::AnyIPv4 : m_dataAddress, m_dataPort, QUdpSocket::ShareAddress))
        {
            qDebug("UDPSourceUDPHandler::start: bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);

            if (m_multicast)
            {
                if (m_dataSocket->joinMulticastGroup(m_multicastAddress)) {
                    qDebug("UDPSourceUDPHandler::start: joined multicast group %s", qPrintable(m_multicastAddress.toString()));
                } else {
                    qDebug("UDPSourceUDPHandler::start: failed joining multicast group %s", qPrintable(m_multicastAddress.toString()));
                }
            }

            connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead())); // , Qt::QueuedConnection gets stuck since Qt 5.8.0
            m_dataConnected = true;
        }
        else
        {
            qWarning("UDPSourceUDPHandler::start: cannot bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);
            m_dataConnected = false;
        }
    }
}

void UDPSourceUDPHandler::stop()
{
    qDebug("UDPSourceUDPHandler::stop");

    if (m_dataConnected)
    {
        m_dataConnected = false;
        disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
    }

    if (m_dataSocket)
    {
        delete m_dataSocket;
        m_dataSocket = nullptr;
    }
}

void UDPSourceUDPHandler::dataReadyRead()
{
    while (m_dataSocket->hasPendingDatagrams() && m_dataConnected)
    {
        qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
        qint64 bytesRead = m_dataSocket->readDatagram(&m_udpDump[m_udpDumpIndex], pendingDataSize, &m_remoteAddress, &m_remotePort);

        if (bytesRead < 0)
        {
            qWarning("UDPSourceUDPHandler::dataReadyRead: UDP read error");
        }
        else
        {
            int udpDumpSize = m_udpDumpIndex + bytesRead;
            int udpDumpPtr = 0;

            while (udpDumpSize >= m_udpBlockSize)
            {
                moveData(&m_udpDump[udpDumpPtr]);
                udpDumpPtr += m_udpBlockSize;
                udpDumpSize -= m_udpBlockSize;
            }

            if (udpDumpSize > 0)
            {
                memcpy(m_udpDump, &m_udpDump[udpDumpPtr], udpDumpSize);
            }

            m_udpDumpIndex = udpDumpSize;
        }
    }
}

void UDPSourceUDPHandler::moveData(char *blk)
{
    memcpy(m_udpBuf[m_writeFrameIndex], blk, m_udpBlockSize);

    if (m_writeFrameIndex < m_nbUDPFrames - 1) {
        m_writeFrameIndex++;
    } else {
        m_writeFrameIndex = 0;
    }
}

void UDPSourceUDPHandler::readSample(qint16 &t)
{
    if (m_readFrameIndex == m_writeFrameIndex) // block until more writes
    {
        t = 0;
    }
    else
    {
        memcpy(&t, &m_udpBuf[m_readFrameIndex][m_readIndex], sizeof(qint16));
        advanceReadPointer((int) sizeof(qint16));
    }
}

void UDPSourceUDPHandler::readSample(AudioSample &a)
{
    if (m_readFrameIndex == m_writeFrameIndex) // block until more writes
    {
        a.l = 0;
        a.r = 0;
    }
    else
    {
        memcpy(&a, &m_udpBuf[m_readFrameIndex][m_readIndex], sizeof(AudioSample));
        advanceReadPointer((int) sizeof(AudioSample));
    }
}

void UDPSourceUDPHandler::readSample(Sample &s)
{
    if (m_readFrameIndex == m_writeFrameIndex) // block until more writes
    {
        s.m_real = 0;
        s.m_imag = 0;
    }
    else
    {
        Sample *u = (Sample *) &m_udpBuf[m_readFrameIndex][m_readIndex];
        s = *u;
        advanceReadPointer((int) sizeof(Sample));
    }
}

void UDPSourceUDPHandler::advanceReadPointer(int nbBytes)
{
    if (m_readIndex < m_udpBlockSize - 2*nbBytes)
    {
        m_readIndex += nbBytes;
    }
    else
    {
        m_readIndex = 0;

        if (m_readFrameIndex < m_nbUDPFrames - 1)
        {
            m_readFrameIndex++;
        }
        else
        {
            m_rwDelta = m_writeFrameIndex; // raw R/W delta estimate
            int nbUDPFrames2 = m_nbUDPFrames/2;
            float d = (m_rwDelta - nbUDPFrames2)/(float) m_nbUDPFrames;
            //qDebug("UDPSourceUDPHandler::advanceReadPointer: w: %02d d: %f", m_writeIndex, d);

            if ((d < -0.45) || (d > 0.45))
            {
                resetReadIndex();
            }
            else
            {
                float dd = d - m_d; // derivative
                float c = (d / 15.0) + (dd / 20.0); // damping and scaling
                c = c < -0.05 ? -0.05 : c > 0.05 ? 0.05 : c; // limit
                UDPSourceMessages::MsgSampleRateCorrection *msg = UDPSourceMessages::MsgSampleRateCorrection::create(c, d);

                if (m_autoRWBalance && m_feedbackMessageQueue) {
                    m_feedbackMessageQueue->push(msg);
                }

                m_readFrameIndex = 0;
                m_d = d;
            }
        }
    }
}

void UDPSourceUDPHandler::configureUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin)
{
    Message* msg = MsgUDPAddressAndPort::create(address, port, multicastAddress, multicastJoin);
    m_inputMessageQueue.push(msg);
}

void UDPSourceUDPHandler::applyUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin)
{
    qDebug() << "UDPSourceUDPHandler::applyUDPLink: "
        << " address: " << address
        << " port: " << port
        << " multicastAddress: " << multicastAddress
        << " multicastJoin: " << multicastJoin;

    bool addressOK = m_dataAddress.setAddress(address);

    if (!addressOK)
    {
        qWarning("UDPSourceUDPHandler::applyUDPLink: invalid address %s. Set to localhost.", address.toStdString().c_str());
        m_dataAddress = QHostAddress::LocalHost;
    }

    m_multicast = multicastJoin;
    addressOK = m_multicastAddress.setAddress(multicastAddress);

    if (!addressOK)
    {
        qWarning("UDPSourceUDPHandler::applyUDPLink: invalid multicast address %s. disabling multicast.", address.toStdString().c_str());
        m_multicast = false;
    }

    stop();
    m_dataPort = port;
    resetReadIndex();
    start();
}

void UDPSourceUDPHandler::resetReadIndex()
{
    m_readFrameIndex = (m_writeFrameIndex + (m_nbUDPFrames/2)) % m_nbUDPFrames;
    m_rwDelta = m_nbUDPFrames/2;
    m_readIndex = 0;
    m_d = 0.0f;
}

void UDPSourceUDPHandler::resizeBuffer(float sampleRate)
{
    int halfNbFrames = std::max((sampleRate / 375.0), (m_minNbUDPFrames / 2.0));
    qDebug("UDPSourceUDPHandler::resizeBuffer: nb_frames: %d", 2*halfNbFrames);

    if (2*halfNbFrames > m_nbAllocatedUDPFrames)
    {
        delete[] m_udpBuf;
        m_udpBuf = new udpBlk_t[2*halfNbFrames];
        m_nbAllocatedUDPFrames = 2*halfNbFrames;
    }

    m_nbUDPFrames = 2*halfNbFrames;
    m_writeFrameIndex = 0;

    resetReadIndex();
}

void UDPSourceUDPHandler::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

bool UDPSourceUDPHandler::handleMessage(const Message& cmd)
{
    if (UDPSourceUDPHandler::MsgUDPAddressAndPort::match(cmd))
    {
        UDPSourceUDPHandler::MsgUDPAddressAndPort& notif = (UDPSourceUDPHandler::MsgUDPAddressAndPort&) cmd;
        applyUDPLink(notif.getAddress(), notif.getPort(), notif.getMulticastAddress(), notif.getMulticastJoin());
        return true;
    }
    else
    {
        return false;
    }
}



