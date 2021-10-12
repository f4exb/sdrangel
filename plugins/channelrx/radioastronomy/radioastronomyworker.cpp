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

#include <cmath>

#include <QDebug>

#include "radioastronomy.h"
#include "radioastronomyworker.h"

MESSAGE_CLASS_DEFINITION(RadioAstronomyWorker::MsgConfigureRadioAstronomyWorker, Message)

RadioAstronomyWorker::RadioAstronomyWorker(RadioAstronomy* radioAstronomy) :
    m_radioAstronomy(radioAstronomy),
    m_msgQueueToChannel(nullptr),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive),
    m_sensorTimer(this)
{
    connect(&m_sensorTimer, SIGNAL(timeout()), this, SLOT(measureSensors()));
    m_sensorTimer.start((int)round(m_settings.m_sensorMeasurePeriod*1000.0));
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++) {
        m_session[i] = VI_NULL;
    }
}

RadioAstronomyWorker::~RadioAstronomyWorker()
{
    m_inputMessageQueue.clear();
    m_visa.closeDefault();
}

void RadioAstronomyWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool RadioAstronomyWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
    return m_running;
}

void RadioAstronomyWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void RadioAstronomyWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool RadioAstronomyWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureRadioAstronomyWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureRadioAstronomyWorker& cfg = (MsgConfigureRadioAstronomyWorker&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void RadioAstronomyWorker::applySettings(const RadioAstronomySettings& settings, bool force)
{
    qDebug() << "RadioAstronomyWorker::applySettings:"
            << " m_sensorEnabled[0]: " << settings.m_sensorEnabled[0]
            << " m_sensorDevice[0]: " << settings.m_sensorDevice[0]
            << " m_sensorInit[0]: " << settings.m_sensorInit[0]
            << " m_sensorMeasure[0]: " << settings.m_sensorMeasure[0]
            << " force: " << force;

    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++)
    {
        if (   (settings.m_sensorEnabled[i] != m_settings.m_sensorEnabled[i])
            || (settings.m_sensorEnabled[i] && (settings.m_sensorDevice[i] != m_settings.m_sensorDevice[i]))
            || force)
        {
            if (!settings.m_sensorEnabled[i] && (m_session[i] != VI_NULL))
            {
                m_visa.close(m_session[i]);
                m_session[i] = VI_NULL;
            }
            if (settings.m_sensorEnabled[i] && !settings.m_sensorDevice[i].trimmed().isEmpty())
            {
                m_visa.openDefault();
                m_session[i] = m_visa.open(settings.m_sensorDevice[i]);
            }
        }
        if (   (settings.m_sensorEnabled[i] && !m_settings.m_sensorEnabled[i])
            || (settings.m_sensorEnabled[i] && (settings.m_sensorInit[i] != m_settings.m_sensorInit[i]))
            || force)
        {
            if (m_session[i]) {
                m_visa.processCommands(m_session[i], settings.m_sensorInit[i]);
            }
        }
    }
    if ((settings.m_sensorMeasurePeriod != m_settings.m_sensorMeasurePeriod) || force) {
        m_sensorTimer.start((int)round(settings.m_sensorMeasurePeriod * 1000.0));
    }

    m_settings = settings;
}

void RadioAstronomyWorker::measureSensors()
{
    for (int i = 0; i < RADIOASTRONOMY_SENSORS; i++)
    {
        if (m_settings.m_sensorEnabled[i] && m_session[i])
        {
            QStringList results = m_visa.processCommands(m_session[i], m_settings.m_sensorMeasure[i]);
            if (results.size() >= 1)
            {
                double value = results[0].toDouble();
                if (getMessageQueueToGUI()) {
                    getMessageQueueToGUI()->push(RadioAstronomy::MsgSensorMeasurement::create(i, value));
                }
            }
            else
            {
                qDebug() << "RadioAstronomyWorker::measureSensors: No result for command " << m_settings.m_sensorMeasure[i];
            }
        }
    }
}
