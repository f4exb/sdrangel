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

#include <QDebug>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>

#include "adsbdemodworker.h"
#include "adsbdemodreport.h"

MESSAGE_CLASS_DEFINITION(ADSBDemodWorker::MsgConfigureADSBDemodWorker, Message)

ADSBDemodWorker::ADSBDemodWorker() :
    m_running(false),
    m_mutex(QMutex::Recursive)
{
    connect(&m_heartbeatTimer, SIGNAL(timeout()), this, SLOT(heartbeat()));
    connect(&m_socket, SIGNAL(readyRead()),this, SLOT(recv()));
    connect(&m_socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(&m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &ADSBDemodWorker::errorOccurred);
#else
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, &ADSBDemodWorker::errorOccurred);
#endif
}

ADSBDemodWorker::~ADSBDemodWorker()
{
    m_inputMessageQueue.clear();
}

void ADSBDemodWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool ADSBDemodWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (m_running) {
        return m_running;
    }

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_heartbeatTimer.start(60*1000);
    m_running = true;
    return m_running;
}

void ADSBDemodWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

    m_heartbeatTimer.stop();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void ADSBDemodWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool ADSBDemodWorker::handleMessage(const Message& message)
{
    if (MsgConfigureADSBDemodWorker::match(message))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureADSBDemodWorker& cfg = (MsgConfigureADSBDemodWorker&) message;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (ADSBDemodReport::MsgReportADSB::match(message))
    {
        ADSBDemodReport::MsgReportADSB& report = (ADSBDemodReport::MsgReportADSB&) message;
        handleADSB(report.getData(), report.getDateTime(), report.getPreambleCorrelation());
        return true;
    }
    else
    {
        return false;
    }
}

void ADSBDemodWorker::applySettings(const ADSBDemodSettings& settings, bool force)
{
    qDebug() << "ADSBDemodWorker::applySettings:"
            << " m_feedEnabled: " << settings.m_feedEnabled
            << " m_feedHost: " << settings.m_feedHost
            << " m_feedPort: " << settings.m_feedPort
            << " m_feedFormat: " << settings.m_feedFormat
            << " force: " << force;

    if ((settings.m_feedEnabled != m_settings.m_feedEnabled)
        || (settings.m_feedHost != m_settings.m_feedHost)
        || (settings.m_feedPort != m_settings.m_feedPort) || force)
    {
        // Close any existing connection
        if (m_socket.isOpen())
            m_socket.close();
        // Open connection
        if (settings.m_feedEnabled)
            m_socket.connectToHost(settings.m_feedHost, settings.m_feedPort);
    }

    m_settings = settings;
}

void ADSBDemodWorker::connected()
{
    qDebug() << "ADSBDemodWorker::connected " << m_settings.m_feedHost;
}

void ADSBDemodWorker::disconnected()
{
    qDebug() << "ADSBDemodWorker::disconnected";
}

void ADSBDemodWorker::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "ADSBDemodWorker::errorOccurred: " << socketError;
}

void ADSBDemodWorker::recv()
{
    // Not expecting to receiving anything from server
    qDebug() << "ADSBDemodWorker::recv";
    qDebug() << m_socket.readAll();
}

void ADSBDemodWorker::send(const char *data, int length)
{
    if (m_settings.m_feedEnabled)
    {
        // Reopen connection if it was lost
        if (!m_socket.isOpen())
            m_socket.connectToHost(m_settings.m_feedHost, m_settings.m_feedPort);
        // Send data
        m_socket.write(data, length);
    }
}

#define BEAST_ESC 0x1a

char *ADSBDemodWorker::escape(char *p, char c)
{
    *p++ = c;
    if (c == BEAST_ESC)
        *p++ = BEAST_ESC;
    return p;
}

// Forward ADS-B data in Beast binary format to specified server
// See: https://wiki.jetvision.de/wiki/Mode-S_Beast:Data_Output_Formats
void ADSBDemodWorker::handleADSB(QByteArray data, const QDateTime dateTime, float correlation)
{
    if (m_settings.m_feedFormat == ADSBDemodSettings::BeastBinary)
    {
        char beastBinary[2+6*2+1*2+14*2];
        int length;
        char *p = beastBinary;
        qint64 timestamp;
        unsigned char signalStrength;

        timestamp = dateTime.toMSecsSinceEpoch();

        if (correlation > 255)
           signalStrength = 255;
        if (correlation < 1)
           signalStrength = 1;
        else
           signalStrength = (unsigned char)correlation;

        *p++ = BEAST_ESC;
        *p++ = '3'; // Mode-S long

        p = escape(p, timestamp >> 56); // Big-endian timestamp
        p = escape(p, timestamp >> 48);
        p = escape(p, timestamp >> 32);
        p = escape(p, timestamp >> 24);
        p = escape(p, timestamp >> 16);
        p = escape(p, timestamp >> 8);
        p = escape(p, timestamp);

        p = escape(p, signalStrength);  // Signal strength

        for (int i = 0; i < data.length(); i++) // ADS-B data
            p = escape(p, data[i]);

        length = p - beastBinary;

        send(beastBinary, length);
    }
    else if (m_settings.m_feedFormat == ADSBDemodSettings::BeastHex)
    {
        QString beastHex = "*" + data.toHex() + ";\n";
        send(beastHex.toUtf8(), beastHex.size());
    }
}

// Periodically send heartbeat to keep connection alive
void ADSBDemodWorker::heartbeat()
{
    const char heartbeat[] = {BEAST_ESC, '1', 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Mode AC packet
    send(heartbeat, sizeof(heartbeat));
}
