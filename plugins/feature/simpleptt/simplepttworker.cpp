///////////////////////////////////////////////////////////////////////////////////
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

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"

#include "webapi/webapiadapterinterface.h"
#include "audio/audiodevicemanager.h"
#include "dsp/dspengine.h"
#include "util/db.h"

#include "simplepttreport.h"
#include "simplepttworker.h"

MESSAGE_CLASS_DEFINITION(SimplePTTWorker::MsgConfigureSimplePTTWorker, Message)
MESSAGE_CLASS_DEFINITION(SimplePTTWorker::MsgPTT, Message)

SimplePTTWorker::SimplePTTWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToGUI(nullptr),
    m_tx(false),
    m_audioFifo(12000),
    m_audioSampleRate(48000),
    m_voxLevel(1.0),
    m_voxHoldCount(0),
    m_voxState(false),
    m_updateTimer(this)
{
    m_audioReadBuffer.resize(16384);
    m_audioReadBufferFill = 0;
    qDebug("SimplePTTWorker::SimplePTTWorker");
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
}

SimplePTTWorker::~SimplePTTWorker()
{
    m_inputMessageQueue.clear();
    AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
    audioDeviceManager->removeAudioSource(&m_audioFifo);
}

void SimplePTTWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

void SimplePTTWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void SimplePTTWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void SimplePTTWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool SimplePTTWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureSimplePTTWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSimplePTTWorker& cfg = (MsgConfigureSimplePTTWorker&) cmd;
        qDebug() << "SimplePTTWorker::handleMessage: MsgConfigureSimplePTTWorker";

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (MsgPTT::match(cmd))
    {
        MsgPTT& cfg = (MsgPTT&) cmd;
        qDebug() << "SimplePTTWorker::handleMessage: MsgPTT tx:" << cfg.getTx();

        sendPTT(cfg.getTx());

        return true;
    }
    else
    {
        return false;
    }
}

void SimplePTTWorker::applySettings(const SimplePTTSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    qDebug() << "SimplePTTWorker::applySettings:" << settings.getDebugString(settingsKeys, force) << " force: " << force;

    if (settingsKeys.contains("audioDeviceName") || force)
    {
        QMutexLocker mlock(&m_mutex);
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getInputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->removeAudioSource(&m_audioFifo);
        audioDeviceManager->addAudioSource(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex);
        m_audioSampleRate = audioDeviceManager->getInputSampleRate(audioDeviceIndex);
        m_voxHoldCount = 0;
        m_voxState = false;
    }

    if (settingsKeys.contains("vox") || force)
    {
        QMutexLocker mlock(&m_mutex);
        m_voxHoldCount = 0;
        m_audioReadBufferFill = 0;
        m_voxState = false;

        if (m_msgQueueToGUI)
        {
            SimplePTTReport::MsgVox *msg = SimplePTTReport::MsgVox::create(false);
            m_msgQueueToGUI->push(msg);
        }

        if (settings.m_vox) {
            connect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        } else {
            disconnect(&m_audioFifo, SIGNAL(dataReady()), this, SLOT(handleAudio()));
        }
    }

    if (settingsKeys.contains("voxLevel") || force) {
        m_voxLevel = CalcDb::powerFromdB(settings.m_voxLevel);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

}

void SimplePTTWorker::sendPTT(bool tx)
{
    qDebug("SimplePTTWorker::sendPTT: %s", tx ? "tx" : "rx");
	if (!m_updateTimer.isActive())
	{
        bool switchedOff = false;
        m_mutex.lock();

        if (tx)
        {
            if (m_settings.m_rxDeviceSetIndex >= 0)
            {
                m_tx = false;
                switchedOff = turnDevice(false);
            }

            if (m_settings.m_txDeviceSetIndex >= 0)
            {
                m_tx = true;
                m_updateTimer.start(m_settings.m_rx2TxDelayMs);
            }
        }
        else
        {
            if (m_settings.m_txDeviceSetIndex >= 0)
            {
                m_tx = true;
                switchedOff = turnDevice(false);
            }

            if (m_settings.m_rxDeviceSetIndex >= 0)
            {
                m_tx = false;
                m_updateTimer.start(m_settings.m_tx2RxDelayMs);
            }
        }

        if (switchedOff && (m_msgQueueToGUI))
        {
            SimplePTTReport::MsgRadioState *msg = SimplePTTReport::MsgRadioState::create(SimplePTTReport::RadioIdle);
            m_msgQueueToGUI->push(msg);
        }
	}
}

void SimplePTTWorker::updateHardware()
{
    qDebug("SimplePTTWorker::updateHardware: m_tx: %s", m_tx ? "on" : "off");
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    m_updateTimer.stop();
    m_mutex.unlock();
    bool success = turnDevice(true);

    if (success && m_msgQueueToGUI)
    {
        SimplePTTReport::MsgRadioState *msg = SimplePTTReport::MsgRadioState::create(m_tx ? SimplePTTReport::RadioTx : SimplePTTReport::RadioRx);
        m_msgQueueToGUI->push(msg);
    }
}

bool SimplePTTWorker::turnDevice(bool on)
{
    qDebug("SimplePTTWorker::turnDevice %s: %s", m_tx ? "tx" : "rx", on ? "on" : "off");
    SWGSDRangel::SWGDeviceState response;
    SWGSDRangel::SWGErrorResponse error;
    int httpCode;

    if (on) {
        httpCode = m_webAPIAdapterInterface->devicesetDeviceRunPost(
            m_tx ? m_settings.m_txDeviceSetIndex : m_settings.m_rxDeviceSetIndex, response, error);
    } else {
        httpCode = m_webAPIAdapterInterface->devicesetDeviceRunDelete(
            m_tx ? m_settings.m_txDeviceSetIndex : m_settings.m_rxDeviceSetIndex, response, error);
    }

    if (httpCode/100 == 2)
    {
        qDebug("SimplePTTWorker::turnDevice: %s success", on ? "on" : "off");
        return true;
    }
    else
    {
        qWarning("SimplePTTWorker::turnDevice: error: %s", qPrintable(*error.getMessage()));
        return false;
    }
}

void SimplePTTWorker::handleAudio()
{
    unsigned int nbRead;
    QMutexLocker mlock(&m_mutex);

    while ((nbRead = m_audioFifo.read(reinterpret_cast<quint8*>(&m_audioReadBuffer[m_audioReadBufferFill]), 4096)) != 0)
    {
        if (m_audioReadBufferFill + nbRead + 4096 < m_audioReadBuffer.size())
        {
            m_audioReadBufferFill += nbRead;
        }
        else
        {
            bool voxState = m_voxState;

            for (unsigned int i = 0; i < m_audioReadBufferFill; i++)
            {
                std::complex<float> za{m_audioReadBuffer[i].l / 46334.0f, m_audioReadBuffer[i].r / 46334.0f};
                float magSq = std::norm(za);

                if (magSq > m_audioMagsqPeak) {
                    m_audioMagsqPeak = magSq;
                }

                if (magSq > m_voxLevel)
                {
                    voxState = true;
                    m_voxHoldCount = 0;
                }
                else
                {
                    if (m_voxHoldCount < (m_settings.m_voxHold * m_audioSampleRate) / 1000) {
                        m_voxHoldCount++;
                    } else {
                        voxState = false;
                    }
                }

                if (voxState != m_voxState)
                {
                    if (m_settings.m_voxEnable) {
                        sendPTT(voxState);
                    }

                    if (m_msgQueueToGUI)
                    {
                        SimplePTTReport::MsgVox *msg = SimplePTTReport::MsgVox::create(voxState);
                        m_msgQueueToGUI->push(msg);
                    }

                    m_voxState = voxState;
                }
            }

            m_audioReadBufferFill = 0;
        }
    }
}
