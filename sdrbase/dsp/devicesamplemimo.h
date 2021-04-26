///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
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

#ifndef SDRBASE_DSP_DEVICESAMPLEMIMO_H_
#define SDRBASE_DSP_DEVICESAMPLEMIMO_H_

#include <vector>

#include "samplemififo.h"
#include "samplemofifo.h"
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

class SDRBASE_API DeviceSampleMIMO : public QObject {
	Q_OBJECT
public:
    enum MIMOType //!< Type of MIMO
    {
        MIMOAsynchronous,    //!< All streams are asynchronous (false MIMO)
        MIMOHalfSynchronous, //!< MI + MO (synchronous inputs on one side and synchronous outputs on the other side)
        MIMOFullSynchronous, //!< True MIMO (all streams synchronous)
    };

    typedef enum {
        FC_POS_INFRA = 0,
        FC_POS_SUPRA,
        FC_POS_CENTER
    } fcPos_t;

	DeviceSampleMIMO();
	virtual ~DeviceSampleMIMO();
	virtual void destroy() = 0;

    virtual void init() = 0;  //!< initializations to be done when all collaborating objects are created and possibly connected
	virtual bool startRx() = 0;
	virtual void stopRx() = 0;
	virtual bool startTx() = 0;
	virtual void stopTx() = 0;

    virtual QByteArray serialize() const = 0;
    virtual bool deserialize(const QByteArray& data) = 0;

	virtual const QString& getDeviceDescription() const = 0;

	virtual int getSinkSampleRate(int index) const = 0;                     //!< Sample rate exposed by the sink at index
    virtual void setSinkSampleRate(int sampleRate, int index) = 0;          //!< For when the sink sample rate is set externally
	virtual quint64 getSinkCenterFrequency(int index) const = 0;            //!< Center frequency exposed by the sink at index
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index) = 0;

	virtual int getSourceSampleRate(int index) const = 0;                   //!< Sample rate exposed by the source at index
    virtual void setSourceSampleRate(int sampleRate, int index) = 0;        //!< For when the source sample rate is set externally
	virtual quint64 getSourceCenterFrequency(int index) const = 0;          //!< Center frequency exposed by the source at index
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index) = 0;

    virtual quint64 getMIMOCenterFrequency() const = 0; //!< Unique center frequency for preset identification or any unique reference
    virtual unsigned int getMIMOSampleRate() const = 0; //!< Unique sample rate for any unique reference

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
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage)
    {
        (void) response;
        (void) subsystemIndex;
        errorMessage = "Not implemented";
        return 501;
    }

    virtual int webapiRun(bool run,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage)
    {
        (void) run;
        (void) subsystemIndex;
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

    MIMOType getMIMOType() const { return m_mimoType; }
	MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }
    virtual void setMessageQueueToGUI(MessageQueue *queue) = 0; // pure virtual so that child classes must have to deal with this
    MessageQueue *getMessageQueueToGUI() { return m_guiMessageQueue; }

    unsigned int getNbSourceFifos() const { return m_sampleMOFifo.getNbStreams(); } //!< Get the number of Tx FIFOs
    unsigned int getNbSinkFifos() const { return m_sampleMIFifo.getNbStreams(); }   //!< Get the number of Rx FIFOs
    SampleMIFifo* getSampleMIFifo() { return &m_sampleMIFifo; }
    SampleMOFifo* getSampleMOFifo() { return &m_sampleMOFifo; }
    // Streams and FIFOs are in opposed source/sink type whick makes it confusing when stream direction is involved:
    //   Rx: source stream -> sink FIFO    -> channel sinks
    //   Tx: sink stream   <- source FIFO  <- channel sources
    unsigned int getNbSourceStreams() const { return m_sampleMIFifo.getNbStreams(); } //!< Commodity function same as getNbSinkFifos (Rx or source streams)
    unsigned int getNbSinkStreams() const { return m_sampleMOFifo.getNbStreams(); }   //!< Commodity function same as getNbSourceFifos (Tx or sink streams)

protected slots:
	void handleInputMessages();

protected:
    MIMOType m_mimoType;
    SampleMIFifo m_sampleMIFifo;      //!< Multiple Input FIFO
    SampleMOFifo m_sampleMOFifo;      //!< Multiple Output FIFO
	MessageQueue m_inputMessageQueue; //!< Input queue to the sink
    MessageQueue *m_guiMessageQueue;  //!< Input message queue to the GUI
};

#endif // SDRBASE_DSP_DEVICESAMPLEMIMO_H_
