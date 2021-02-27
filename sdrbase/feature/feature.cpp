///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// API for features                                                              //
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

#include "util/uid.h"
#include "util/message.h"

#include "SWGDeviceState.h"

#include "feature.h"
#include "maincore.h"

Feature::Feature(const QString& uri, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_guiMessageQueue(nullptr),
	m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_name(uri),
	m_uri(uri),
    m_uid(UidCalculator::getNewObjectId())
{
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

void Feature::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()))
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

void Feature::handlePipeMessageQueue(MessageQueue* messageQueue)
{
    Message* message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

int Feature::webapiRunGet(
	SWGSDRangel::SWGDeviceState& response,
	QString& errorMessage) const
{
	(void) errorMessage;
	getFeatureStateStr(*response.getState());
	return 200;
}

void Feature::getFeatureStateStr(QString& stateStr) const
{
	switch(m_state)
	{
	case StNotStarted:
		stateStr = "notStarted";
		break;
	case StIdle:
		stateStr = "idle";
		break;
	case StRunning:
		stateStr = "running";
		break;
	case StError:
		stateStr = "error";
		break;
	default:
		stateStr = "notStarted";
		break;
	}
}
