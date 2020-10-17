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
            << " m_trackerDeviceSetIndex: " << settings.m_trackerDeviceSetIndex
            << " m_trackedDeviceSetIndex: " << settings.m_trackedDeviceSetIndex
            << " m_hasTargetFrequency: " << settings.m_hasTargetFrequency
            << " m_transverterTarget: " << settings.m_transverterTarget
            << " m_targetFrequency: " << settings.m_targetFrequency
            << " m_freqTolerance: " << settings.m_freqTolerance
            << " force: " << force;
    m_settings = settings;
}
