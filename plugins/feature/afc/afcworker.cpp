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

#include "afcreport.h"
#include "afcworker.h"

MESSAGE_CLASS_DEFINITION(AFCWorker::MsgConfigureAFCWorker, Message)
MESSAGE_CLASS_DEFINITION(AFCWorker::MsgPTT, Message)

AFCWorker::AFCWorker(WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_tx(false),
    m_mutex(QMutex::Recursive)
{
    qDebug("AFCWorker::AFCWorker");
	connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateHardware()));
}

AFCWorker::~AFCWorker()
{
    m_inputMessageQueue.clear();
}

void AFCWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool AFCWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
    return m_running;
}

void AFCWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = false;
}

void AFCWorker::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool AFCWorker::handleMessage(const Message& cmd)
{
    if (MsgConfigureAFCWorker::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureAFCWorker& cfg = (MsgConfigureAFCWorker&) cmd;
        qDebug() << "AFCWorker::handleMessage: MsgConfigureAFCWorker";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgPTT::match(cmd))
    {
        MsgPTT& cfg = (MsgPTT&) cmd;
        qDebug() << "AFCWorker::handleMessage: MsgPTT";

        sendPTT(cfg.getTx());

        return true;
    }
    else
    {
        return false;
    }
}

void AFCWorker::applySettings(const AFCSettings& settings, bool force)
{
    qDebug() << "AFCWorker::applySettings:"
            << " m_title: " << settings.m_title
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_rxDeviceSetIndex: " << settings.m_rxDeviceSetIndex
            << " m_txDeviceSetIndex: " << settings.m_txDeviceSetIndex
            << " m_rx2TxDelayMs: " << settings.m_rx2TxDelayMs
            << " m_tx2RxDelayMs: " << settings.m_tx2RxDelayMs
            << " force: " << force;
    m_settings = settings;
}

void AFCWorker::sendPTT(bool tx)
{
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
            AFCReport::MsgRadioState *msg = AFCReport::MsgRadioState::create(AFCReport::RadioIdle);
            m_msgQueueToGUI->push(msg);
        }
	}
}

void AFCWorker::updateHardware()
{
    SWGSDRangel::SWGSuccessResponse response;
    SWGSDRangel::SWGErrorResponse error;
    m_updateTimer.stop();
    m_mutex.unlock();

    if (turnDevice(true))
    {
        m_webAPIAdapterInterface->devicesetFocusPatch(
            m_tx ? m_settings.m_txDeviceSetIndex : m_settings.m_rxDeviceSetIndex, response, error);

        if (m_msgQueueToGUI)
        {
            AFCReport::MsgRadioState *msg = AFCReport::MsgRadioState::create(
                m_tx ? AFCReport::RadioTx : AFCReport::RadioRx
            );
            m_msgQueueToGUI->push(msg);
        }
    }
}

bool AFCWorker::turnDevice(bool on)
{
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
        return true;
    }
    else
    {
        qWarning("AFCWorker::turnDevice: error: %s", qPrintable(*error.getMessage()));
        return false;
    }
}
