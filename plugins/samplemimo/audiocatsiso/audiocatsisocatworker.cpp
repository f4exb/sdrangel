///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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
#include <QTimer>

#include "dsp/devicesamplesource.h"
#include "audiocatsisocatworker.h"

// Compatibility with all versions of Hamlib
#ifndef HAMLIB_FILPATHLEN
#define HAMLIB_FILPATHLEN FILPATHLEN
#endif

MESSAGE_CLASS_DEFINITION(AudioCATSISOCATWorker::MsgConfigureAudioCATSISOCATWorker, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISOCATWorker::MsgPollTimerConnect, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISOCATWorker::MsgSetRxSampleRate, Message)
MESSAGE_CLASS_DEFINITION(AudioCATSISOCATWorker::MsgReportFrequency, Message)

AudioCATSISOCATWorker::AudioCATSISOCATWorker(QObject* parent) :
    QObject(parent),
    m_inputMessageQueueToGUI(nullptr),
    m_inputMessageQueueToSISO(nullptr),
    m_running(false),
    m_connected(false),
    m_pollTimer(nullptr),
    m_ptt(false),
    m_frequency(0)
{
    rig_set_debug(RIG_DEBUG_ERR);
}

AudioCATSISOCATWorker::~AudioCATSISOCATWorker()
{
    stopWork();

    if (m_pollTimer) {
        delete m_pollTimer;
    }
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

    qint64 rxXlatedDeviceCenterFrequency = settings.m_rxCenterFrequency;
    rxXlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    rxXlatedDeviceCenterFrequency = rxXlatedDeviceCenterFrequency < 0 ? 0 : rxXlatedDeviceCenterFrequency;

    qint64 txXlatedDeviceCenterFrequency = settings.m_txCenterFrequency;
    txXlatedDeviceCenterFrequency -= settings.m_transverterMode ? settings.m_transverterDeltaFrequency : 0;
    txXlatedDeviceCenterFrequency = txXlatedDeviceCenterFrequency < 0 ? 0 : txXlatedDeviceCenterFrequency;

    if (settingsKeys.contains("rxCenterFrequency") ||
        settingsKeys.contains("transverterMode") ||
        settingsKeys.contains("transverterDeltaFrequency") || force)
    {
        if (!m_ptt)
        {
            qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                    rxXlatedDeviceCenterFrequency,
                    0,
                    settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) settings.m_fcPosRx,
                    m_rxSampleRate,
                    DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                    false);
            catSetFrequency(deviceCenterFrequency);
        }
    }

    if (settingsKeys.contains("txCenterFrequency") ||
        settingsKeys.contains("transverterMode") ||
        settingsKeys.contains("transverterDeltaFrequency") || force)
    {
        if (m_ptt) {
            catSetFrequency(txXlatedDeviceCenterFrequency);
        }
    }

    if (settingsKeys.contains("catPollingMs") || force)
    {
        if (m_pollTimer) {
            m_pollTimer->setInterval(settings.m_catPollingMs);
        }
    }

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
    else if (AudioCATSISOSettings::MsgPTT::match(message))
    {
        AudioCATSISOSettings::MsgPTT& cmd = (AudioCATSISOSettings::MsgPTT&) message;
        m_ptt = cmd.getPTT();
        catPTT(m_ptt);

        return true;
    }
    else if (MsgPollTimerConnect::match(message))
    {
        qDebug("AudioCATSISOCATWorker::handleMessage: MsgPollTimerConnect");
        m_pollTimer = new QTimer();
        connect(m_pollTimer, SIGNAL(timeout()), this, SLOT(pollingTick()));
        m_pollTimer->start(m_settings.m_catPollingMs);

        return true;
    }
    else if (MsgSetRxSampleRate::match(message))
    {
        MsgSetRxSampleRate& cmd = (MsgSetRxSampleRate&) message;
        m_rxSampleRate = cmd.getSampleRate();
        qDebug("AudioCATSISOCATWorker::handleMessage: MsgSetRxSampleRate: %d", m_rxSampleRate);

        if (m_settings.m_transverterMode && !m_ptt)
        {
            qint64 rxXlatedDeviceCenterFrequency = m_settings.m_rxCenterFrequency;
            rxXlatedDeviceCenterFrequency -= m_settings.m_transverterMode ? m_settings.m_transverterDeltaFrequency : 0;
            rxXlatedDeviceCenterFrequency = rxXlatedDeviceCenterFrequency < 0 ? 0 : rxXlatedDeviceCenterFrequency;
            qint64 deviceCenterFrequency = DeviceSampleSource::calculateDeviceCenterFrequency(
                    rxXlatedDeviceCenterFrequency,
                    0,
                    m_settings.m_log2Decim,
                    (DeviceSampleSource::fcPos_t) m_settings.m_fcPosRx,
                    m_rxSampleRate,
                    DeviceSampleSource::FrequencyShiftScheme::FSHIFT_STD,
                    false);
            catSetFrequency(deviceCenterFrequency);
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
    if (m_pollTimer)
    {
        disconnect(m_pollTimer, SIGNAL(timeout()), this, SLOT(pollingTick()));
        m_pollTimer->stop();
    }

    m_connected = false;
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

void AudioCATSISOCATWorker::catPTT(bool ptt)
{
    if (!m_connected) {
        return;
    }

    if (m_ptt)
    {
        if (m_settings.m_txCenterFrequency != m_frequency) {
            catSetFrequency(m_settings.m_txCenterFrequency);
        }
    }
    else
    {
        if (m_settings.m_rxCenterFrequency != m_frequency) {
            catSetFrequency(m_settings.m_rxCenterFrequency);
        }
    }

    int retcode = rig_set_ptt(m_rig, RIG_VFO_CURR, ptt ? RIG_PTT_ON : RIG_PTT_OFF);

    if (retcode != RIG_OK)
    {
        if (m_inputMessageQueueToGUI)
        {
            AudioCATSISOSettings::MsgCATReportStatus *msg = AudioCATSISOSettings::MsgCATReportStatus::create(
                AudioCATSISOSettings::MsgCATReportStatus::StatusError
            );
            m_inputMessageQueueToGUI->push(msg);
        }
    }
}

void AudioCATSISOCATWorker::catSetFrequency(uint64_t frequency)
{
    if (!m_connected) {
        return;
    }

    qDebug("AudioCATSISOCATWorker::catSetFrequency: %lu", frequency);
    int retcode = rig_set_freq(m_rig, RIG_VFO_CURR, frequency);

    if (retcode != RIG_OK)
    {
        m_frequency = frequency;

        if (m_inputMessageQueueToGUI)
        {
            AudioCATSISOSettings::MsgCATReportStatus *msg = AudioCATSISOSettings::MsgCATReportStatus::create(
                AudioCATSISOSettings::MsgCATReportStatus::StatusError
            );
            m_inputMessageQueueToGUI->push(msg);
        }
    }
}

void AudioCATSISOCATWorker::pollingTick()
{
    if (!m_connected) {
        return;
    }

    freq_t freq; // double
    int retcode = rig_get_freq(m_rig, RIG_VFO_CURR, &freq);

    if (m_settings.m_transverterMode) {
        freq += m_settings.m_transverterDeltaFrequency;
    }

    if (retcode == RIG_OK)
    {
        // qDebug("AudioCATSISOCATWorker::pollingTick: %f %lu", freq, m_frequency);
        if (m_frequency != freq)
        {
            qDebug("AudioCATSISOCATWorker::pollingTick: %lu", m_frequency);

            if (m_inputMessageQueueToSISO)
            {
                MsgReportFrequency *msgFreq = MsgReportFrequency::create(freq);
                m_inputMessageQueueToSISO->push(msgFreq);
            }

            m_frequency = freq;
        }

        if (m_inputMessageQueueToGUI)
        {
            AudioCATSISOSettings::MsgCATReportStatus *msgStatus =
                AudioCATSISOSettings::MsgCATReportStatus::create(AudioCATSISOSettings::MsgCATReportStatus::StatusConnected);
            m_inputMessageQueueToGUI->push(msgStatus);
        }
    }
    else
    {
        if (m_inputMessageQueueToGUI)
        {
            AudioCATSISOSettings::MsgCATReportStatus *msgStatus =
                AudioCATSISOSettings::MsgCATReportStatus::create(AudioCATSISOSettings::MsgCATReportStatus::StatusError);
            m_inputMessageQueueToGUI->push(msgStatus);
        }
    }
}
