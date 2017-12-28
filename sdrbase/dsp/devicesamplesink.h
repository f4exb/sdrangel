///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#ifndef SDRBASE_DSP_DEVICESAMPLESINK_H_
#define SDRBASE_DSP_DEVICESAMPLESINK_H_

#include <QtGlobal>

#include "samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/export.h"

namespace SWGSDRangel
{
    class SWGDeviceSettings;
    class SWGDeviceState;
}

class SDRANGEL_API DeviceSampleSink : public QObject {
	Q_OBJECT
public:
	DeviceSampleSink();
	virtual ~DeviceSampleSink();
	virtual void destroy() = 0;

    virtual void init() = 0;  //!< initializations to be done when all collaborating objects are created and possibly connected
	virtual bool start() = 0;
	virtual void stop() = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

	virtual const QString& getDeviceDescription() const = 0;
	virtual int getSampleRate() const = 0; //!< Sample rate exposed by the sink
	virtual quint64 getCenterFrequency() const = 0; //!< Center frequency exposed by the sink
    virtual void setCenterFrequency(qint64 centerFrequency) = 0;

	virtual bool handleMessage(const Message& message) = 0;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

    virtual int webapiSettingsPutPatch(
            bool force __attribute__((unused)), //!< true to force settings = put
            const QStringList& deviceSettingsKeys __attribute__((unused)),
            SWGSDRangel::SWGDeviceSettings& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

    virtual int webapiRun(bool run __attribute__((unused)),
            SWGSDRangel::SWGDeviceState& response __attribute__((unused)),
            QString& errorMessage)
    { errorMessage = "Not implemented"; return 501; }

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setMessageQueueToGUI(MessageQueue *queue) = 0; // pure virtual so that child classes must have to deal with this
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
	SampleSourceFifo* getSampleFifo() { return &m_sampleSourceFifo; }

protected slots:
	void handleInputMessages();

protected:
    SampleSourceFifo m_sampleSourceFifo;
	MessageQueue m_inputMessageQueue; //!< Input queue to the sink
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
};

#endif /* SDRBASE_DSP_DEVICESAMPLESINK_H_ */
