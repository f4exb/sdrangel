///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <stdint.h>

#include "udpsinkmsg.h"
#include "udpsinkudphandler.h"

MESSAGE_CLASS_DEFINITION(UDPSinkUDPHandler::MsgUDPAddressAndPort, Message)

UDPSinkUDPHandler::UDPSinkUDPHandler() :
    m_dataSocket(0),
    m_dataAddress(QHostAddress::LocalHost),
    m_remoteAddress(QHostAddress::LocalHost),
    m_dataPort(9999),
    m_remotePort(0),
    m_dataConnected(false),
    m_udpDumpIndex(0),
    m_nbUDPFrames(m_minNbUDPFrames),
    m_nbAllocatedUDPFrames(m_minNbUDPFrames),
    m_writeIndex(0),
    m_readFrameIndex(m_minNbUDPFrames/2),
    m_readIndex(0),
    m_rwDelta(m_minNbUDPFrames/2),
    m_d(0),
    m_autoRWBalance(true),
    m_feedbackMessageQueue(0)
{
    m_udpBuf = new udpBlk_t[m_minNbUDPFrames];
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()));
}

UDPSinkUDPHandler::~UDPSinkUDPHandler()
{
    delete[] m_udpBuf;
}

void UDPSinkUDPHandler::start()
{
    qDebug("UDPSinkUDPHandler::start");

    if (!m_dataSocket)
    {
        m_dataSocket = new QUdpSocket(this);
    }

    if (!m_dataConnected)
    {

        if (m_dataSocket->bind(m_dataAddress, m_dataPort))
        {
            qDebug("UDPSinkUDPHandler::start: bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);
            connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()), Qt::QueuedConnection); // , Qt::QueuedConnection
            m_dataConnected = true;
        }
        else
        {
            qWarning("UDPSinkUDPHandler::start: cannot bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);
            m_dataConnected = false;
        }
    }
}

void UDPSinkUDPHandler::stop()
{
    qDebug("UDPSinkUDPHandler::stop");

    if (m_dataConnected)
    {
        m_dataConnected = false;
        disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
    }

    if (m_dataSocket)
    {
        delete m_dataSocket;
        m_dataSocket = 0;
    }
}

void UDPSinkUDPHandler::dataReadyRead()
{
    while (m_dataSocket->hasPendingDatagrams() && m_dataConnected)
    {
        qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
        qint64 bytesRead = m_dataSocket->readDatagram(&m_udpDump[m_udpDumpIndex], pendingDataSize, &m_remoteAddress, &m_remotePort);

        if (bytesRead < 0)
        {
            qWarning("UDPSinkUDPHandler::dataReadyRead: UDP read error");
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

void UDPSinkUDPHandler::moveData(char *blk)
{
    memcpy(m_udpBuf[m_writeIndex], blk, m_udpBlockSize);

    if (m_writeIndex < m_nbUDPFrames - 1) {
        m_writeIndex++;
    } else {
        m_writeIndex = 0;
    }
}

void UDPSinkUDPHandler::readSample(FixReal &t)
{
    if (m_readFrameIndex == m_writeIndex) // block until more writes
    {
        t = 0;
    }
    else
    {
        memcpy(&t, &m_udpBuf[m_readFrameIndex][m_readIndex], sizeof(FixReal));
        advanceReadPointer((int) sizeof(FixReal));
    }
}

void UDPSinkUDPHandler::readSample(Sample &s)
{
    if (m_readFrameIndex == m_writeIndex) // block until more writes
    {
        s.m_real = 0;
        s.m_imag = 0;
    }
    else
    {
        memcpy(&s, &m_udpBuf[m_readFrameIndex][m_readIndex], sizeof(Sample));
        advanceReadPointer((int) sizeof(Sample));
    }
}

void UDPSinkUDPHandler::advanceReadPointer(int nbBytes)
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
            m_rwDelta = m_writeIndex; // raw R/W delta estimate
            float d = (m_rwDelta - (m_nbUDPFrames/2))/(float) m_nbUDPFrames;
            //qDebug("UDPSinkUDPHandler::advanceReadPointer: w: %02d d: %f", m_writeIndex, d);

            if ((d < -0.45) || (d > 0.45))
            {
                resetReadIndex();
            }
            else
            {
                float dd = d - m_d; // derivative
                float c = (d / 15.0) + (dd / 20.0); // damping and scaling
                c = c < -0.05 ? -0.05 : c > 0.05 ? 0.05 : c; // limit
                UDPSinkMessages::MsgSampleRateCorrection *msg = UDPSinkMessages::MsgSampleRateCorrection::create(c, d);

                if (m_autoRWBalance && m_feedbackMessageQueue) {
                    m_feedbackMessageQueue->push(msg);
                }

                m_readFrameIndex = 0;
                m_d = d;
            }
        }
    }
}

void UDPSinkUDPHandler::configureUDPLink(const QString& address, quint16 port)
{
    Message* msg = MsgUDPAddressAndPort::create(address, port);
    m_inputMessageQueue.push(msg);
}

void UDPSinkUDPHandler::applyUDPLink(const QString& address, quint16 port)
{
    qDebug("UDPSinkUDPHandler::configureUDPLink: %s:%d", address.toStdString().c_str(), port);
    bool addressOK = m_dataAddress.setAddress(address);

    if (!addressOK)
    {
        qWarning("UDPSinkUDPHandler::configureUDPLink: invalid address %s. Set to localhost.", address.toStdString().c_str());
        m_dataAddress = QHostAddress::LocalHost;
    }

    stop();
    m_dataPort = port;
    resetReadIndex();
    start();
}

void UDPSinkUDPHandler::resetReadIndex()
{
    m_readFrameIndex = (m_writeIndex + (m_nbUDPFrames/2)) % m_nbUDPFrames;
    m_rwDelta = m_nbUDPFrames/2;
    m_readIndex = 0;
    m_d = 0.0f;
}

void UDPSinkUDPHandler::resizeBuffer(float sampleRate)
{
    int halfNbFrames = std::max((sampleRate / 375.0), (m_minNbUDPFrames / 2.0));
    qDebug("UDPSinkUDPHandler::resizeBuffer: nb_frames: %d", 2*halfNbFrames);

    if (2*halfNbFrames > m_nbAllocatedUDPFrames)
    {
        delete[] m_udpBuf;
        m_udpBuf = new udpBlk_t[2*halfNbFrames];
        m_nbAllocatedUDPFrames = 2*halfNbFrames;
    }

    m_nbUDPFrames = 2*halfNbFrames;
    m_writeIndex = 0;

    resetReadIndex();
}

void UDPSinkUDPHandler::handleMessages()
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

bool UDPSinkUDPHandler::handleMessage(const Message& cmd)
{
    if (UDPSinkUDPHandler::MsgUDPAddressAndPort::match(cmd))
    {
        UDPSinkUDPHandler::MsgUDPAddressAndPort& notif = (UDPSinkUDPHandler::MsgUDPAddressAndPort&) cmd;
        applyUDPLink(notif.getAddress(), notif.getPort());
        return true;
    }
    else
    {
        return false;
    }
}



