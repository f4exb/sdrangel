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

ADSBBeastServer::ADSBBeastServer()
{
}

void ADSBBeastServer::listen(quint16 port)
{
    QTcpServer::listen(QHostAddress::Any, port);
    qDebug() << "ADSBBeastServer listening on port " << serverPort();
}

void ADSBBeastServer::incomingConnection(qintptr socket)
{
    qDebug() << "ADSBBeastServer client connected";
    QTcpSocket *s = new QTcpSocket(this);
    connect(s, &QTcpSocket::readyRead, this, &ADSBBeastServer::readClient);
    connect(s, SIGNAL(disconnected()), this, SLOT(discardClient()));
    s->setSocketDescriptor(socket);
    m_clients.append(s);
}

void ADSBBeastServer::send(const char *data, int length)
{
    // Send frame to all clients
    for (auto client : m_clients) {
        client->write(data, length);
    }
}

void ADSBBeastServer::close()
{
    for (auto client : m_clients) {
        client->deleteLater();
    }
    m_clients.clear();
    QTcpServer::close();
}

void ADSBBeastServer::readClient()
{
    QTcpSocket *socket = (QTcpSocket *)sender();
    socket->readAll();
}

void ADSBBeastServer::discardClient()
{
    qDebug() << "ADSBBeastServer client disconnected";
    QTcpSocket *socket = (QTcpSocket*)sender();
    socket->deleteLater();
    m_clients.removeAll(socket);
}

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
    m_startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    m_heartbeatTimer.start(60*1000);
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
    m_running = true;
    return m_running;
}

void ADSBDemodWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);

    if (!m_running) {
        return;
    }

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
            << " m_exportClientEnabled: " << settings.m_exportClientEnabled
            << " m_exportClientHost: " << settings.m_exportClientHost
            << " m_exportClientPort: " << settings.m_exportClientPort
            << " m_exportClientFormat: " << settings.m_exportClientFormat
            << " m_exportServerEnabled: " << settings.m_exportServerEnabled
            << " m_exportServerPort: " << settings.m_exportServerPort
            << " m_logEnabled: " << settings.m_logEnabled
            << " m_logFilename: " << settings.m_logFilename
            << " force: " << force;

    if ((settings.m_feedEnabled != m_settings.m_feedEnabled)
        || (settings.m_exportClientEnabled != m_settings.m_exportClientEnabled)
        || (settings.m_exportClientHost != m_settings.m_exportClientHost)
        || (settings.m_exportClientPort != m_settings.m_exportClientPort)
        || force)
    {
        // Close any existing connection
        if (m_socket.isOpen()) {
            m_socket.close();
        }
        // Open connection
        if (settings.m_feedEnabled && settings.m_exportClientEnabled) {
            m_socket.connectToHost(settings.m_exportClientHost, settings.m_exportClientPort);
        }
    }

    if ((settings.m_feedEnabled != m_settings.m_feedEnabled)
        || (settings.m_exportServerEnabled != m_settings.m_exportServerEnabled)
        || (settings.m_exportServerPort != m_settings.m_exportServerPort)
        || force)
    {
        if (m_beastServer.isListening()) {
            m_beastServer.close();
        }
        if (settings.m_feedEnabled && settings.m_exportServerEnabled) {
            m_beastServer.listen(settings.m_exportServerPort);
        }
    }

    if ((settings.m_logEnabled != m_settings.m_logEnabled)
        || (settings.m_logFilename != m_settings.m_logFilename)
        || force)
    {
        if (m_logFile.isOpen())
        {
            m_logStream.flush();
            m_logFile.close();
        }
        if (settings.m_logEnabled && !settings.m_logFilename.isEmpty())
        {
            m_logFile.setFileName(settings.m_logFilename);
            if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
            {
                qDebug() << "ADSBDemodWorker::applySettings - Logging to: " << settings.m_logFilename;
                bool newFile = m_logFile.size() == 0;
                m_logStream.setDevice(&m_logFile);
                if (newFile)
                {
                    // Write header
                    m_logStream << "Date,Time,Data,Correlation\n";
                }
            }
            else
            {
                qDebug() << "ADSBDemodWorker::applySettings - Unable to open log file: " << settings.m_logFilename;
            }
        }
    }

    m_settings = settings;
}

void ADSBDemodWorker::connected()
{
    qDebug() << "ADSBDemodWorker::connected " << m_settings.m_exportClientHost;
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
    if (m_settings.m_feedEnabled && m_settings.m_exportClientEnabled)
    {
        // Reopen connection if it was lost
        if (!m_socket.isOpen()) {
            m_socket.connectToHost(m_settings.m_exportClientHost, m_settings.m_exportClientPort);
        }
        // Send data
        m_socket.write(data, length);
    }
}

#define BEAST_ESC 0x1a

char *ADSBDemodWorker::escape(char *p, char c)
{
    *p++ = c;
    if (c == BEAST_ESC) {
        *p++ = BEAST_ESC;
    }
    return p;
}

// Forward ADS-B data in Beast binary format to specified server
// See: https://wiki.jetvision.de/wiki/Mode-S_Beast:Data_Output_Formats
// Log to .csv file
void ADSBDemodWorker::handleADSB(QByteArray data, const QDateTime dateTime, float correlation)
{
    if (m_logFile.isOpen()) {
        m_logStream << dateTime.date().toString() << "," << dateTime.time().toString() << "," << data.toHex() << "," << correlation << "\n";
    }

    if (m_settings.m_feedEnabled && (m_settings.m_exportClientEnabled || m_settings.m_exportServerEnabled))
    {
        if ((m_settings.m_exportClientEnabled && (m_settings.m_exportClientFormat == ADSBDemodSettings::BeastBinary))
            || m_settings.m_exportServerEnabled)
        {
            char beastBinary[2+6*2+1*2+14*2];
            int length;
            char *p = beastBinary;
            qint64 timestamp;
            unsigned char signalStrength;

            // Timestamp seems to be 12MHz ticks since device started
            timestamp = (dateTime.toMSecsSinceEpoch() - m_startTime) * 12000;

            if (correlation > 255) {
               signalStrength = 255;
            } else if (correlation < 1) {
               signalStrength = 1;
            } else {
               signalStrength = (unsigned char)correlation;
            }

            *p++ = BEAST_ESC;
            *p++ = '3'; // Mode-S long

            p = escape(p, timestamp >> 48);  // Big-endian timestamp
            p = escape(p, timestamp >> 32);
            p = escape(p, timestamp >> 24);
            p = escape(p, timestamp >> 16);
            p = escape(p, timestamp >> 8);
            p = escape(p, timestamp);

            p = escape(p, signalStrength);  // Signal strength

            for (int i = 0; i < data.length(); i++) { // ADS-B data
                p = escape(p, data[i]);
            }

            length = p - beastBinary;

            if ((m_settings.m_exportClientEnabled) && (m_settings.m_exportClientFormat == ADSBDemodSettings::BeastBinary)) {
                send(beastBinary, length);
            }
            if (m_settings.m_exportServerEnabled) {
                m_beastServer.send(beastBinary, length);
            }
        }
        if (m_settings.m_exportClientEnabled && (m_settings.m_exportClientFormat == ADSBDemodSettings::BeastHex))
        {
            QString beastHex = "*" + data.toHex() + ";\n";
            send(beastHex.toUtf8(), beastHex.size());
        }
    }
}

// Periodically send heartbeat to keep connection alive
void ADSBDemodWorker::heartbeat()
{
    if (m_running)
    {
        const char heartbeat[] = {BEAST_ESC, '1', 0, 0, 0, 0, 0, 0, 0, 0, 0}; // Mode AC packet
        if (m_settings.m_exportClientEnabled) {
            send(heartbeat, sizeof(heartbeat));
        }
        if (m_settings.m_exportServerEnabled) {
            m_beastServer.send(heartbeat, sizeof(heartbeat));
        }
    }
}
