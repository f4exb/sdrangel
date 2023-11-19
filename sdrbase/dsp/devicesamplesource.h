///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#ifndef INCLUDE_SAMPLESOURCE_H
#define INCLUDE_SAMPLESOURCE_H

#include <QtGlobal>
#include <QByteArray>

#include "samplesinkfifo.h"
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

class SDRBASE_API DeviceSampleSource : public QObject {
	Q_OBJECT
public:
    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

    typedef enum {
        FSHIFT_STD = 0, // Standard Rx independent
        FSHIFT_TXSYNC   // Follows same scheme as Tx
    } FrequencyShiftScheme;

	DeviceSampleSource();
	virtual ~DeviceSampleSource();
	virtual void destroy() = 0;

	virtual void init() = 0;  //!< initializations to be done when all collaborating objects are created and possibly connected
	virtual bool start() = 0;
	virtual void stop() = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

	virtual const QString& getDeviceDescription() const = 0;
	virtual int getSampleRate() const = 0; //!< Sample rate exposed by the source
    virtual void setSampleRate(int sampleRate) = 0; //!< For when the source sample rate is set externally
	virtual quint64 getCenterFrequency() const = 0; //!< Center frequency exposed by the source
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
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceActions& actions,
            QString& errorMessage)
    {
        (void) deviceSettingsKeys;
        (void) actions;
        errorMessage = "Not implemented";
        return 501;
    }

	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
	virtual void setMessageQueueToGUI(MessageQueue *queue) = 0; // pure virtual so that child classes must have to deal with this
	MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }
    SampleSinkFifo* getSampleFifo() { return &m_sampleFifo; }

    static qint64 calculateDeviceCenterFrequency(
            quint64 centerFrequency,
            qint64 transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode = false
    );

    static qint64 calculateCenterFrequency(
            quint64 deviceCenterFrequency,
            qint64 transverterDeltaFrequency,
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme,
            bool transverterMode = false
    );

    static qint32 calculateFrequencyShift(
            int log2Decim,
            fcPos_t fcPos,
            quint32 devSampleRate,
            FrequencyShiftScheme frequencyShiftScheme
    );

protected slots:
	void handleInputMessages();

protected:
    SampleSinkFifo m_sampleFifo;
	MessageQueue m_inputMessageQueue; //!< Input queue to the source
	MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
};

#endif // INCLUDE_SAMPLESOURCE_H
