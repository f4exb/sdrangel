///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2019 F4EXB                                                 //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_DSP_DEVICESAMPLESINK_H_
#define SDRBASE_DSP_DEVICESAMPLESINK_H_

#include <QtGlobal>

#include "samplesourcefifo.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "export.h"

namespace SWGSDRangel
{
    class SWGDeviceSettings;
    class SWGDeviceState;
    class SWGDeviceReport;
    class SWGDeviceActions;
}

class SDRBASE_API DeviceSampleSink : public QObject {
	Q_OBJECT
public:
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

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
    virtual void setSampleRate(int sampleRate) = 0; //!< For when the sink sample rate is set externally
	virtual quint64 getCenterFrequency() const = 0; //!< Center frequency exposed by the sink
    virtual void setCenterFrequency(qint64 centerFrequency) = 0;

	virtual bool handleMessage(const Message& message) = 0;

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGDeviceSettings& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiSettingsPutPatch(
            bool force, //!< true to force settings = put
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response,
            QString& errorMessage)
    {
        (void) force;
        (void) deviceSettingsKeys;
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage)
    {
        (void) run;
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage)
    {
        (void) response;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiActionsPost(
            const QStringList& deviceActionsKeys,
            SWGSDRangel::SWGDeviceActions& actions,
            QString& errorMessage)
    {
        (void) deviceActionsKeys;
        (void) actions;
        errorMessage = "Not implemented";
        return 501;
    }

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setMessageQueueToGUI(MessageQueue *queue) = 0; // pure virtual so that child classes must have to deal with this
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
	SampleSourceFifo* getSampleFifo() { return &m_sampleSourceFifo; }

    static qint64 calculateDeviceCenterFrequency(
            quint64 centerFrequency,
            qint64 transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode = false);

    static qint64 calculateCenterFrequency(
            quint64 deviceCenterFrequency,
            qint64 transverterDeltaFrequency,
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate,
            bool transverterMode = false);

    static qint32 calculateFrequencyShift(
            int log2Interp,
            fcPos_t fcPos,
            quint32 devSampleRate);

protected slots:
	void handleInputMessages();

protected:
    SampleSourceFifo m_sampleSourceFifo;
	MessageQueue m_inputMessageQueue; //!< Input queue to the sink
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
};

#endif /* SDRBASE_DSP_DEVICESAMPLESINK_H_ */
