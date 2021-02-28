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

#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>
#include <QSerialPort>
#include <QRegularExpression>

#include "gs232controller.h"
#include "gs232controllerworker.h"
#include "gs232controllerreport.h"

MESSAGE_CLASS_DEFINITION(GS232ControllerWorker::MsgConfigureGS232ControllerWorker, Message)
MESSAGE_CLASS_DEFINITION(GS232ControllerReport::MsgReportAzAl, Message)

GS232ControllerWorker::GS232ControllerWorker() :
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive),
    m_lastAzimuth(-1),
    m_lastElevation(-1)
{
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(update()));
    m_pollTimer.start(1000);
}

GS232ControllerWorker::~GS232ControllerWorker()
{
    m_inputMessageQueue.clear();
}

void GS232ControllerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool GS232ControllerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(&m_serialPort, &QSerialPort::readyRead, this, &GS232ControllerWorker::readSerialData);
    openSerialPort(m_settings);
    m_running = true;
    return m_running;
}

void GS232ControllerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    // Close serial port as USB/controller activity can create RFI
    if (m_serialPort.isOpen())
        m_serialPort.close();
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    disconnect(&m_serialPort, &QSerialPort::readyRead, this, &GS232ControllerWorker::readSerialData);
    m_running = false;
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
        QMutexLocker mutexLocker(&m_mutex);
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
            << " m_serialPort: " << settings.m_serialPort
            << " m_baudRate: " << settings.m_baudRate
            << " force: " << force;

    if ((settings.m_serialPort != m_settings.m_serialPort) || force)
    {
        openSerialPort(settings);
    }
    else if ((settings.m_baudRate != m_settings.m_baudRate) || force)
    {
        m_serialPort.setBaudRate(settings.m_baudRate);
    }

    // Apply offset then clamp

    int azimuth = settings.m_azimuth;
    azimuth += settings.m_azimuthOffset;
    azimuth = std::max(azimuth, settings.m_azimuthMin);
    azimuth = std::min(azimuth, settings.m_azimuthMax);

    int elevation = settings.m_elevation;
    elevation += settings.m_elevationOffset;
    elevation = std::max(elevation, settings.m_elevationMin);
    elevation = std::min(elevation, settings.m_elevationMax);

    if (((elevation != m_lastElevation) || force) && (settings.m_elevationMax != 0))
    {
        setAzimuthElevation(azimuth, elevation);
    }
    else if ((azimuth != m_lastAzimuth) || force)
    {
        setAzimuth(azimuth);
    }

    m_settings = settings;
}

void GS232ControllerWorker::openSerialPort(const GS232ControllerSettings& settings)
{
    if (m_serialPort.isOpen())
        m_serialPort.close();
    m_serialPort.setPortName(settings.m_serialPort);
    m_serialPort.setBaudRate(settings.m_baudRate);
    if (!m_serialPort.open(QIODevice::ReadWrite))
    {
        qCritical() << "GS232ControllerWorker::openSerialPort: Failed to open serial port " << settings.m_serialPort << ". Error: " << m_serialPort.error();
        if (m_msgQueueToFeature)
            m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Failed to open serial port %1: %2").arg(settings.m_serialPort).arg(m_serialPort.error())));
    }
    m_lastAzimuth = -1;
    m_lastElevation = -1;
}

void GS232ControllerWorker::setAzimuth(int azimuth)
{
   QString cmd = QString("M%1\r\n").arg(azimuth, 3, 10, QLatin1Char('0'));
   QByteArray data = cmd.toLatin1();
   m_serialPort.write(data);
   m_lastAzimuth = azimuth;
}

void GS232ControllerWorker::setAzimuthElevation(int azimuth, int elevation)
{
   QString cmd = QString("W%1 %2\r\n").arg(azimuth, 3, 10, QLatin1Char('0')).arg(elevation, 3, 10, QLatin1Char('0'));
   QByteArray data = cmd.toLatin1();
   m_serialPort.write(data);
   m_lastAzimuth = azimuth;
   m_lastElevation = elevation;
}

void GS232ControllerWorker::readSerialData()
{
    char buf[1024];
    qint64 len;

    while (m_serialPort.canReadLine())
    {
        len = m_serialPort.readLine(buf, sizeof(buf));
        if (len != -1)
        {
            QString response = QString::fromUtf8(buf, len);
            QRegularExpression re("AZ=(\\d\\d\\d)EL=(\\d\\d\\d)");
            QRegularExpressionMatch match = re.match(response);
            if (match.hasMatch())
            {
                QString az = match.captured(1);
                QString el = match.captured(2);
                //qDebug() << "GS232ControllerWorker::readSerialData read az " << az << " el " << el;
                if (getMessageQueueToGUI())
                    getMessageQueueToGUI()->push( GS232ControllerReport::MsgReportAzAl::create(az.toFloat(), el.toFloat()));
            }
            else if (response == "\r\n")
            {
                // Ignore
            }
            else
            {
                qDebug() << "GS232ControllerWorker::readSerialData - unexpected response " << response;
                if (m_msgQueueToFeature)
                    m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(QString("Unexpected GS-232 serial response: %1").arg(response)));
            }
        }
    }
}

void GS232ControllerWorker::update()
{
    // Request current Az/El from GS-232 controller
    if (m_serialPort.isOpen())
    {
        QByteArray cmd("C2\r\n");
        m_serialPort.write(cmd);
    }
}
