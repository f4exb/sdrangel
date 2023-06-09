///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include "audiocatsisocatworker.h"

MESSAGE_CLASS_DEFINITION(AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker, Message)

AudioCATSISOCATWorker::AudioCATSISOCATWorker(QObject* parent) :
    QObject(parent),
    m_inputMessageQueueToGUI(nullptr),
    m_running(false),
    m_connected(false)
{
}

AudioCATSISOCATWorker::~AudioCATSISOCATWorker()
{
    stopWork();
}

void AudioCATSISOCATWorker::startWork()
{
    if (m_running) {
        return;
    }

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()), Qt::QueuedConnection);
    m_running = true;
}

void AudioCATSISOCATWorker::stopWork()
{
    if (!m_running) {
        return;
    }

    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void AudioCATSISOCATWorker::applySettings(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "AudioCATSISOCATWorker::applySettings: "
        << " force:" << force
        << settings.getDebugString(settingsKeys, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

bool AudioCATSISOCATWorker::handleMessage(const Message& message)
{
    if (MsgConfigureAudioCATSISOCATWorker::match(message))
    {
        qDebug() << "AudioCATSISO::handleMessage: MsgConfigureAudioCATSISOCATWorker";
        MsgConfigureAudioCATSISOCATWorker& conf = (MsgConfigureAudioCATSISOCATWorker&) message;
        applySettings(conf.getSettings(), conf.getSettingsKeys(), conf.getForce());
        return true;
    }
    else if (AudioCATSISOSettings::MsgCATConnect::match(message))
    {
        AudioCATSISOSettings::MsgCATConnect& cmd = (AudioCATSISOSettings::MsgCATConnect&) message;

        if (cmd.getConnect()) {
            catConnect();
        } else {
            catDisconnect();
        }

        return true;
    }

    return false;
}

void AudioCATSISOCATWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

void AudioCATSISOCATWorker::catConnect()
{
    m_rig = rig_init(m_settings.m_hamlibModel);

    if (!m_rig)
    {
        m_connected = false;

        if (m_inputMessageQueueToGUI)
        {
            qCritical("AudioCATSISOCATWorker::catConnect: Unknown rig num: %u", m_settings.m_hamlibModel);
            qCritical("AudioCATSISOCATWorker::catConnect: Please check riglist.h");
            AudioCATSISOSettings::MsgCATReportStatus *msg = AudioCATSISOSettings::MsgCATReportStatus::create(
                AudioCATSISOSettings::MsgCATReportStatus::StatusError
            );
            m_inputMessageQueueToGUI->push(msg);
        }
    }
    else
    {
        qDebug("AudioCATSISOCATWorker::catConnect: rig initialized with num: %u", m_settings.m_hamlibModel);
    }

    m_rig->state.rigport.type.rig = RIG_PORT_SERIAL;
    m_rig->state.rigport.parm.serial.rate = AudioCATSISOSettings::m_catSpeeds[m_settings.m_catSpeedIndex];
    m_rig->state.rigport.parm.serial.data_bits = AudioCATSISOSettings::m_catDataBits[m_settings.m_catDataBitsIndex];
    m_rig->state.rigport.parm.serial.stop_bits = AudioCATSISOSettings::m_catStopBits[m_settings.m_catStopBitsIndex];
    m_rig->state.rigport.parm.serial.parity = RIG_PARITY_NONE;
    m_rig->state.rigport.parm.serial.handshake = (serial_handshake_e) AudioCATSISOSettings::m_catHandshakes[m_settings.m_catHandshakeIndex];
    strncpy(m_rig->state.rigport.pathname, m_settings.m_catDevicePath.toStdString().c_str() , HAMLIB_FILPATHLEN - 1);
    int retcode = rig_open(m_rig);
    AudioCATSISOSettings::MsgCATReportStatus *msg;

    if (retcode == RIG_OK)
    {
        m_connected = true;
        msg = AudioCATSISOSettings::MsgCATReportStatus::create(AudioCATSISOSettings::MsgCATReportStatus::StatusConnected);
    }
    else
    {
        m_connected = false;
        msg = AudioCATSISOSettings::MsgCATReportStatus::create(AudioCATSISOSettings::MsgCATReportStatus::StatusError);
    }

    if (m_inputMessageQueueToGUI) {
        m_inputMessageQueueToGUI->push(msg);
    } else {
        delete msg;
    }
}

void AudioCATSISOCATWorker::catDisconnect()
{
	rig_close(m_rig); /* close port */
	rig_cleanup(m_rig); /* if you care about memory */

    if (m_inputMessageQueueToGUI)
    {
        AudioCATSISOSettings::MsgCATReportStatus *msg = AudioCATSISOSettings::MsgCATReportStatus::create(
            AudioCATSISOSettings::MsgCATReportStatus::StatusNone
        );
        m_inputMessageQueueToGUI->push(msg);
    }
}
