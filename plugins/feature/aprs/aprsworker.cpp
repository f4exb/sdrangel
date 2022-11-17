///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"
#include "maincore.h"
#include "util/ax25.h"
#include "util/aprs.h"

#include "aprs.h"
#include "aprsworker.h"

MESSAGE_CLASS_DEFINITION(APRSWorker::MsgConfigureAPRSWorker, Message)

APRSWorker::APRSWorker(APRS *aprs, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_aprs(aprs),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_socket(this)
{
    connect(&m_socket, SIGNAL(readyRead()),this, SLOT(recv()));
    connect(&m_socket, SIGNAL(connected()), this, SLOT(connected()));
    connect(&m_socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
    connect(&m_socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error), this, &APRSWorker::errorOccurred);
#else
    connect(&m_socket, &QAbstractSocket::errorOccurred, this, &APRSWorker::errorOccurred);
#endif
}

APRSWorker::~APRSWorker()
{
    stopWork();
    m_inputMessageQueue.clear();
}

void APRSWorker::startWork()
{
    qDebug("APRSWorker::startWork");
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    // Handle any messages already on the queue
    handleInputMessages();
}

void APRSWorker::stopWork()
{
    qDebug("APRSWorker::stopWork");
    QMutexLocker mutexLocker(&m_mutex);
    // Close any existing connection
    if (m_socket.isOpen()) {
        m_socket.close();
    }
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void APRSWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool APRSWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureAPRSWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAPRSWorker& cfg = (MsgConfigureAPRSWorker&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        MainCore::MsgPacket& report = (MainCore::MsgPacket&) cmd;
        AX25Packet ax25;
        APRSPacket *aprs = new APRSPacket();
        if (ax25.decode(report.getPacket()))
        {
            if (aprs->decode(ax25))
            {
                // See: http://www.aprs-is.net/IGateDetails.aspx for gating rules
                if (!aprs->m_via.contains("TCPIP")
                    && !aprs->m_via.contains("TCPXX")
                    && !aprs->m_via.contains("NOGATE")
                    && !aprs->m_via.contains("RFONLY"))
                {
                    aprs->m_dateTime = report.getDateTime();
                    QString igateMsg = aprs->toTNC2(m_settings.m_igateCallsign);
                    send(igateMsg.toUtf8(), igateMsg.length());
                }
            }
        }
        return true;
    }
    else
    {
        return false;
    }
}

void APRSWorker::applySettings(const APRSSettings& settings, bool force)
{
    qDebug() << "APRSWorker::applySettings:"
            << " m_igateEnabled: " << settings.m_igateEnabled
            << " m_igateServer: " << settings.m_igateServer
            << " m_igatePort: " << settings.m_igatePort
            << " m_igateCallsign: " << settings.m_igateCallsign
            << " m_igateFilter: " << settings.m_igateFilter
            << " force: " << force;

    if ((settings.m_igateEnabled != m_settings.m_igateEnabled)
        || (settings.m_igateServer != m_settings.m_igateServer)
        || (settings.m_igatePort != m_settings.m_igatePort)
        || (settings.m_igateFilter != m_settings.m_igateFilter)
        || force)
    {
        // Close any existing connection
        if (m_socket.isOpen())
            m_socket.close();
        // Open connection
        if (settings.m_igateEnabled)
        {
            if (settings.m_igateServer.isEmpty())
            {
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(APRS::MsgReportWorker::create("IGate server name must be specified"));
            }
            else if (settings.m_igateCallsign.isEmpty())
            {
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(APRS::MsgReportWorker::create("IGate callsign must be specified"));
            }
            else if (settings.m_igatePasscode.isEmpty())
            {
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(APRS::MsgReportWorker::create("IGate passcode must be specified"));
            }
            else
            {
                qDebug() << "APRSWorker::applySettings: Connecting to " <<  settings.m_igateServer << ":" << settings.m_igatePort;
                m_socket.setSocketOption(QAbstractSocket::LowDelayOption, 1);
                m_socket.connectToHost(settings.m_igateServer, settings.m_igatePort);
            }
        }
    }

    m_settings = settings;
}

void APRSWorker::connected()
{
    qDebug() << "APRSWorker::connected " << m_settings.m_igateServer;
    m_loggedIn = false;
}

void APRSWorker::disconnected()
{
    qDebug() << "APRSWorker::disconnected";
    if (m_msgQueueToFeature)
        m_msgQueueToFeature->push(APRS::MsgReportWorker::create("Disconnected"));
}

void APRSWorker::errorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "APRSWorker::errorOccurred: " << socketError;
    if (m_msgQueueToFeature)
        m_msgQueueToFeature->push(APRS::MsgReportWorker::create(m_socket.errorString()));
}

void APRSWorker::recv()
{
    char buffer[2048];

    while (m_socket.readLine(buffer, sizeof(buffer)) > 0)
    {
        QString packet(buffer);
        qDebug() << "APRSWorker::recv: " << packet;
        if (packet.startsWith("#"))
        {
            if (!m_loggedIn)
            {
                // Log in with callsign and passcode
                QString login = QString("user %1 pass %2 vers SDRangel 6.4.0%3\r\n").arg(m_settings.m_igateCallsign).arg(m_settings.m_igatePasscode).arg(m_settings.m_igateFilter.isEmpty() ? "" : QString(" filter %1").arg(m_settings.m_igateFilter));
                send(login.toLatin1(), login.length());
                m_loggedIn = true;
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(APRS::MsgReportWorker::create("Connected"));
            }
            else if (packet.indexOf(QString("logresp %1 unverified").arg(m_settings.m_igateCallsign)) >= 0)
            {
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(APRS::MsgReportWorker::create("Invalid IGate callsign or passcode"));
            }
        }
        else if (packet.length() > 0)
        {
            // Forward to GUI
            if (getMessageQueueToGUI())
            {
                // Convert TNC2 to AX25 raw bytes
                QByteArray ax25Packet = APRSPacket::toByteArray(packet);
                getMessageQueueToGUI()->push(MainCore::MsgPacket::create(m_aprs, ax25Packet, QDateTime::currentDateTime()));
            }
        }
    }
}

void APRSWorker::send(const char *data, int length)
{
    if (m_settings.m_igateEnabled)
    {
        // Reopen connection if it was lost
        if (!m_socket.isOpen())
        {
            qDebug() << "APRSWorker::send: Reconnecting to " << m_settings.m_igateServer << ":" << m_settings.m_igatePort;
            m_socket.connectToHost(m_settings.m_igateServer, m_settings.m_igatePort);
        }
        // Send data
        qDebug() << "APRSWorker::send: " << QString(QByteArray(data, length));
        m_socket.write(data, length);
    }
}
