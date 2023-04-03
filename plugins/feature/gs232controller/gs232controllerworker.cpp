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
    m_controllerProtocol(nullptr)
//    m_spidSetOutstanding(false),
//    m_spidSetSent(false),
//    m_spidStatusSent(false),
//    m_rotCtlDReadAz(false)
{
}

GS232ControllerWorker::~GS232ControllerWorker()
{
    qDebug() << "GS232ControllerWorker::~GS232ControllerWorker";
    stopWork();
    m_inputMessageQueue.clear();
    delete m_controllerProtocol;
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
    if (m_device && m_device->isOpen())
    {
        m_device->close();
        m_device = nullptr;
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

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void GS232ControllerWorker::applySettings(const GS232ControllerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "GS232ControllerWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if ((settingsKeys.contains("protocol") && (settings.m_protocol != m_settings.m_protocol)) || force)
    {
        delete m_controllerProtocol;
        m_controllerProtocol = ControllerProtocol::create(settings.m_protocol);
        if (m_controllerProtocol)
        {
            m_controllerProtocol->setMessageQueue(m_msgQueueToFeature);
            m_controllerProtocol->setDevice(m_device);
        }
    }

    if (settingsKeys.contains("connection") )
    {
        if (m_device && m_device->isOpen())
        {
            m_device->close();
            m_device = nullptr;
        }
    }

    if (settings.m_connection == GS232ControllerSettings::TCP)
    {
        if (settingsKeys.contains("host") || settingsKeys.contains("port") || force)
        {
            m_device = openSocket(settings);
            if (m_controllerProtocol) {
                m_controllerProtocol->setDevice(m_device);
            }
        }
    }
    else
    {
        if (settingsKeys.contains("serialPort") || force)
        {
            m_device = openSerialPort(settings);
            if (m_controllerProtocol) {
                m_controllerProtocol->setDevice(m_device);
            }
        }
        else if (settingsKeys.contains("baudRate") || force)
        {
            m_serialPort.setBaudRate(settings.m_baudRate);
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (m_controllerProtocol) {
        m_controllerProtocol->applySettings(settings, settingsKeys, force);
    }

    if (m_device != nullptr)
    {
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
    }
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
    if (m_device && m_device->isOpen() && m_controllerProtocol) {
        m_controllerProtocol->setAzimuth(azimuth);
    }

    m_lastAzimuth = azimuth;
}

void GS232ControllerWorker::setAzimuthElevation(float azimuth, float elevation)
{
    if (m_device && m_device->isOpen() && m_controllerProtocol) {
        m_controllerProtocol->setAzimuthElevation(azimuth, elevation);
    }

    m_lastAzimuth = azimuth;
    m_lastElevation = elevation;
}

void GS232ControllerWorker::readData()
{
    if (m_controllerProtocol) {
        m_controllerProtocol->readData();
    }
}

void GS232ControllerWorker::update()
{
    // Request current Az/El from controller
    if (m_device && m_device->isOpen() && m_controllerProtocol) {
        m_controllerProtocol->update();
    }
}

