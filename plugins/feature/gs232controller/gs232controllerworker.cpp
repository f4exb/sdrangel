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

#include <algorithm>
#include <cmath>

#include <QDebug>
#include <QSerialPort>
#include <QRegularExpression>

#include "gs232controller.h"
#include "gs232controllerworker.h"
#include "gs232controllerreport.h"

MESSAGE_CLASS_DEFINITION(GS232ControllerWorker::MsgConfigureGS232ControllerWorker, Message)
MESSAGE_CLASS_DEFINITION(GS232ControllerReport::MsgReportAzAl, Message)

GS232ControllerWorker::GS232ControllerWorker() :
    m_msgQueueToFeature(nullptr),
    m_device(nullptr),
    m_serialPort(this),
    m_socket(this),
    m_pollTimer(this),
    m_lastAzimuth(-1.0f),
    m_lastElevation(-1.0f),
    m_spidSetOutstanding(false),
    m_spidSetSent(false),
    m_spidStatusSent(false),
    m_rotCtlDReadAz(false)
{
}

GS232ControllerWorker::~GS232ControllerWorker()
{
    qDebug() << "GS232ControllerWorker::~GS232ControllerWorker";
    stopWork();
    m_inputMessageQueue.clear();
}

void GS232ControllerWorker::startWork()
{
    qDebug() << "GS232ControllerWorker::startWork";
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(&m_serialPort, &QSerialPort::readyRead, this, &GS232ControllerWorker::readData);
    connect(&m_socket, &QTcpSocket::readyRead, this, &GS232ControllerWorker::readData);
    if (m_settings.m_connection == GS232ControllerSettings::TCP) {
        m_device = openSocket(m_settings);
    } else {
        m_device = openSerialPort(m_settings);
    }
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(update()));
    m_pollTimer.start(1000);
    // Handle any messages already on the queue
    handleInputMessages();
}

void GS232ControllerWorker::stopWork()
{
    qDebug() << "GS232ControllerWorker::stopWork";
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    if (m_device && m_device->isOpen()) {
        m_device->close();
    }
    disconnect(&m_serialPort, &QSerialPort::readyRead, this, &GS232ControllerWorker::readData);
    disconnect(&m_socket, &QTcpSocket::readyRead, this, &GS232ControllerWorker::readData);
    m_pollTimer.stop();
    disconnect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(update()));
}

void GS232ControllerWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool GS232ControllerWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureGS232ControllerWorker::match(cmd))
    {
        MsgConfigureGS232ControllerWorker& cfg = (MsgConfigureGS232ControllerWorker&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void GS232ControllerWorker::applySettings(const GS232ControllerSettings& settings, bool force)
{
    qDebug() << "GS232ControllerWorker::applySettings:"
            << " m_azimuth: " << settings.m_azimuth
            << " m_elevation: " << settings.m_elevation
            << " m_azimuthOffset: " << settings.m_azimuthOffset
            << " m_elevationOffset: " << settings.m_elevationOffset
            << " m_azimuthMin: " << settings.m_azimuthMin
            << " m_azimuthMax: " << settings.m_azimuthMax
            << " m_elevationMin: " << settings.m_elevationMin
            << " m_elevationMax: " << settings.m_elevationMax
            << " m_tolerance: " << settings.m_tolerance
            << " m_protocol: " << settings.m_protocol
            << " m_connection: " << settings.m_connection
            << " m_serialPort: " << settings.m_serialPort
            << " m_baudRate: " << settings.m_baudRate
            << " m_host: " << settings.m_host
            << " m_port: " << settings.m_port
            << " force: " << force;

    if (settings.m_connection != m_settings.m_connection)
    {
        if (m_device && m_device->isOpen()) {
            m_device->close();
        }
    }

    if (settings.m_connection == GS232ControllerSettings::TCP)
    {
        if ((settings.m_host != m_settings.m_host) || (settings.m_port != m_settings.m_port) || force) {
            m_device = openSocket(settings);
        }
    }
    else
    {
        if ((settings.m_serialPort != m_settings.m_serialPort) || force) {
            m_device = openSerialPort(settings);
        } else if ((settings.m_baudRate != m_settings.m_baudRate) || force) {
            m_serialPort.setBaudRate(settings.m_baudRate);
        }
    }

    // Apply offset then clamp

    float azimuth, elevation;
    settings.calcTargetAzEl(azimuth, elevation);

    // Don't set if within tolerance of last setting
    float azDiff = std::abs(azimuth - m_lastAzimuth);
    float elDiff = std::abs(elevation - m_lastElevation);

    if (((elDiff > settings.m_tolerance) || (m_lastElevation == -1) || force) && (settings.m_elevationMax != 0))
    {
        setAzimuthElevation(azimuth, elevation);
    }
    else if ((azDiff > settings.m_tolerance) || (m_lastAzimuth == -1) || force)
    {
        setAzimuth(azimuth);
    }

    m_settings = settings;
}

QIODevice *GS232ControllerWorker::openSerialPort(const GS232ControllerSettings& settings)
{
    qDebug() << "GS232ControllerWorker::openSerialPort: " << settings.m_serialPort;
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
    m_lastAzimuth = -1;
    m_lastElevation = -1;
    if (!settings.m_serialPort.isEmpty())
    {
        m_serialPort.setPortName(settings.m_serialPort);
        m_serialPort.setBaudRate(settings.m_baudRate);
        if (!m_serialPort.open(QIODevice::ReadWrite))
        {
            qCritical() << "GS232ControllerWorker::openSerialPort: Failed to open serial port " << settings.m_serialPort << ". Error: " << m_serialPort.error();
            m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Failed to open serial port %1: %2").arg(settings.m_serialPort).arg(m_serialPort.error())));
            return nullptr;
        }
        else
        {
            return &m_serialPort;
        }
    }
    else
    {
        return nullptr;
    }
}

QIODevice *GS232ControllerWorker::openSocket(const GS232ControllerSettings& settings)
{
    qDebug() << "GS232ControllerWorker::openSocket: " << settings.m_host << settings.m_port;
    if (m_socket.isOpen()) {
        m_socket.close();
    }
    m_lastAzimuth = -1;
    m_lastElevation = -1;
    m_socket.connectToHost(settings.m_host, settings.m_port);
    if (m_socket.waitForConnected(3000))
    {
        return &m_socket;
    }
    else
    {
        qCritical() << "GS232ControllerWorker::openSocket: Failed to connect to " << settings.m_host << settings.m_port;
        m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Failed to connect to %1:%2").arg(settings.m_host).arg(settings.m_port)));
        return nullptr;
    }
}

void GS232ControllerWorker::setAzimuth(float azimuth)
{
    if (m_settings.m_protocol == GS232ControllerSettings::GS232)
    {
        QString cmd = QString("M%1\r\n").arg((int)std::round(azimuth), 3, 10, QLatin1Char('0'));
        QByteArray data = cmd.toLatin1();
        m_device->write(data);
    }
    else
    {
        setAzimuthElevation(azimuth, m_lastElevation);
    }
    m_lastAzimuth = azimuth;
}

void GS232ControllerWorker::setAzimuthElevation(float azimuth, float elevation)
{
    if (m_settings.m_protocol == GS232ControllerSettings::GS232)
    {
        QString cmd = QString("W%1 %2\r\n").arg((int)std::round(azimuth), 3, 10, QLatin1Char('0')).arg((int)std::round(elevation), 3, 10, QLatin1Char('0'));
        QByteArray data = cmd.toLatin1();
        m_device->write(data);
    }
    else if (m_settings.m_protocol == GS232ControllerSettings::SPID)
    {
        if (!m_spidSetSent && !m_spidStatusSent)
        {
            QByteArray cmd(13, (char)0);

            cmd[0] = 0x57;  // Start
            int h = std::round((azimuth + 360.0f) * 2.0f);
            cmd[1] = 0x30 | (h / 1000);
            cmd[2] = 0x30 | ((h % 1000) / 100);
            cmd[3] = 0x30 | ((h % 100) / 10);
            cmd[4] = 0x30 | (h % 10);
            cmd[5] = 2; // 2 degree per impulse
            int v = std::round((elevation + 360.0f) * 2.0f);
            cmd[6] = 0x30 | (v / 1000);
            cmd[7] = 0x30 | ((v % 1000) / 100);
            cmd[8] = 0x30 | ((v % 100) / 10);
            cmd[9] = 0x30 | (v % 10);
            cmd[10] = 2; // 2 degree per impulse
            cmd[11] = 0x2f; // Set cmd
            cmd[12] = 0x20;  // End

            m_device->write(cmd);

            m_spidSetSent = true;
        }
        else
        {
            qDebug() << "GS232ControllerWorker::setAzimuthElevation: Not sent, waiting for status reply";
            m_spidSetOutstanding = true;
        }
    } else {
        QString cmd = QString("P %1 %2\n").arg(azimuth).arg(elevation);
        QByteArray data = cmd.toLatin1();
        m_device->write(data);
    }
    m_lastAzimuth = azimuth;
    m_lastElevation = elevation;
}

void GS232ControllerWorker::readData()
{
    char buf[1024];
    qint64 len;

    if (m_settings.m_protocol == GS232ControllerSettings::GS232)
    {
        while (m_device->canReadLine())
        {
            len = m_device->readLine(buf, sizeof(buf));
            if (len != -1)
            {
                QString response = QString::fromUtf8(buf, len);
                // MD-02 can return AZ=-00 EL=-00 and other negative angles
                QRegularExpression re("AZ=([-\\d]\\d\\d) *EL=([-\\d]\\d\\d)");
                QRegularExpressionMatch match = re.match(response);
                if (match.hasMatch())
                {
                    QString az = match.captured(1);
                    QString el = match.captured(2);
                    //qDebug() << "GS232ControllerWorker::readData read Az " << az << " El " << el;
                    m_msgQueueToFeature->push(GS232ControllerReport::MsgReportAzAl::create(az.toFloat(), el.toFloat()));
                }
                else if (response == "\r\n")
                {
                    // Ignore
                }
                else
                {
                    qWarning() << "GS232ControllerWorker::readData - unexpected GS-232 response \"" << response << "\"";
                    m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Unexpected GS-232 response: %1").arg(response)));
                }
            }
        }
    }
    else if (m_settings.m_protocol == GS232ControllerSettings::SPID)
    {
        while (m_device->bytesAvailable() >= 12)
        {
            len = m_device->read(buf, 12);
            if ((len == 12) && (buf[0] == 0x57))
            {
                double az;
                double el;
                az = buf[1] * 100.0 + buf[2] * 10.0 + buf[3] + buf[4] / 10.0 - 360.0;
                el = buf[6] * 100.0 + buf[7] * 10.0 + buf[8] + buf[9] / 10.0 - 360.0;
                //qDebug() << "GS232ControllerWorker::readData read Az " << az << " El " << el;
                m_msgQueueToFeature->push(GS232ControllerReport::MsgReportAzAl::create(az, el));
                if (m_spidStatusSent && m_spidSetSent) {
                    qDebug() << "GS232ControllerWorker::readData - m_spidStatusSent and m_spidSetSent set simultaneously";
                }
                if (m_spidStatusSent) {
                    m_spidStatusSent = false;
                }
                if (m_spidSetSent) {
                    m_spidSetSent = false;
                }
                if (m_spidSetOutstanding)
                {
                    m_spidSetOutstanding = false;
                    setAzimuthElevation(m_lastAzimuth, m_lastElevation);
                }
            }
            else
            {
                QByteArray bytes(buf, (int)len);
                qWarning() << "GS232ControllerWorker::readData - unexpected SPID rot2prog response \"" << bytes.toHex() << "\"";
                m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Unexpected SPID rot2prog response: %1").arg(bytes.toHex().data())));
            }
        }
    }
    else
    {
        while (m_device->canReadLine())
        {
            len = m_device->readLine(buf, sizeof(buf));
            if (len != -1)
            {
                QString response = QString::fromUtf8(buf, len).trimmed();
                QRegularExpression rprt("RPRT (-?\\d+)");
                QRegularExpressionMatch matchRprt = rprt.match(response);
                QRegularExpression decimal("(-?\\d+.\\d+)");
                QRegularExpressionMatch matchDecimal = decimal.match(response);
                if (matchRprt.hasMatch())
                {
                    // See rig_errcode_e in hamlib rig.h
                    const QStringList errors = {
                        "OK",
                        "Invalid parameter",
                        "Invalid configuration",
                        "No memory",
                        "Not implemented",
                        "Timeout",
                        "IO error",
                        "Internal error",
                        "Protocol error",
                        "Command rejected",
                        "Arg truncated",
                        "Not available",
                        "VFO not targetable",
                        "Bus error",
                        "Collision on bus",
                        "NULL rig handled or invalid pointer parameter",
                        "Invalid VFO",
                        "Argument out of domain of function"
                    };
                    int rprt = matchRprt.captured(1).toInt();
                    if (rprt != 0)
                    {
                        qWarning() << "GS232ControllerWorker::readData - rotctld error: " << errors[-rprt];
                        // Seem to get a lot of EPROTO errors from rotctld due to extra 00 char in response to GS232 C2 command
                        // E.g: ./rotctld.exe -m 603 -r com7 -vvvvv
                        // read_string(): RX 16 characters
                        // 0000    00 41 5a 3d 31 37 35 20 20 45 4c 3d 30 33 38 0d     .AZ=175  EL=038.
                        // So don't pass these to GUI for now
                        if (rprt != -8) {
                            m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("rotctld error: %1").arg(errors[-rprt])));
                        }
                    }
                    m_rotCtlDReadAz = false;
                }
                else if (matchDecimal.hasMatch() && !m_rotCtlDReadAz)
                {
                    m_rotCtlDAz = response;
                    m_rotCtlDReadAz = true;
                }
                else if (matchDecimal.hasMatch() && m_rotCtlDReadAz)
                {
                    QString az = m_rotCtlDAz;
                    QString el = response;
                    m_rotCtlDReadAz = false;
                    //qDebug() << "GS232ControllerWorker::readData read Az " << az << " El " << el;
                    m_msgQueueToFeature->push(GS232ControllerReport::MsgReportAzAl::create(az.toFloat(), el.toFloat()));
                }
                else
                {
                    qWarning() << "GS232ControllerWorker::readData - Unexpected rotctld response \"" << response << "\"";
                    m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Unexpected rotctld response: %1").arg(response)));
                }
            }
        }
    }
}

void GS232ControllerWorker::update()
{
    // Request current Az/El from controller
    if (m_device && m_device->isOpen())
    {
        if (m_settings.m_protocol == GS232ControllerSettings::GS232)
        {
            QByteArray cmd("C2\r\n");
            m_device->write(cmd);
        }
        else if (m_settings.m_protocol == GS232ControllerSettings::SPID)
        {
            // Don't send a new status command, if waiting for a previous reply
            if (!m_spidSetSent && !m_spidStatusSent)
            {
                // Status
                QByteArray cmd;
                cmd.append((char)0x57); // Start
                for (int i = 0; i < 10; i++) {
                    cmd.append((char)0x0);
                }
                cmd.append((char)0x1f); // Status
                cmd.append((char)0x20); // End
                m_device->write(cmd);
                m_spidStatusSent = true;
            }
        }
        else
        {
            QByteArray cmd("p\n");
            m_device->write(cmd);
        }
    }
}
