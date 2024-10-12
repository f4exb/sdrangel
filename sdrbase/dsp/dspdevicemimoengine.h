///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2020, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_
#define SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_

#include <QObject>

#include "dsp/dsptypes.h"
#include "util/message.h"
#include "util/messagequeue.h"
#include "util/movingaverage.h"
#include "util/incrementalvector.h"
#include "export.h"

class DeviceSampleMIMO;
class BasebandSampleSink;
class BasebandSampleSource;
class MIMOChannel;

class SDRBASE_API DSPDeviceMIMOEngine : public QObject {
	Q_OBJECT

public:
    class SetSampleMIMO : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit SetSampleMIMO(DeviceSampleMIMO* sampleMIMO) : Message(), m_sampleMIMO(sampleMIMO) { }
        DeviceSampleMIMO* getSampleMIMO() const { return m_sampleMIMO; }
    private:
        DeviceSampleMIMO* m_sampleMIMO;
    };

    class AddBasebandSampleSource : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddBasebandSampleSource(BasebandSampleSource* sampleSource, unsigned int index) :
            Message(),
            m_sampleSource(sampleSource),
            m_index(index)
        { }
        BasebandSampleSource* getSampleSource() const { return m_sampleSource; }
        unsigned int getIndex() const { return m_index; }
    private:
        BasebandSampleSource* m_sampleSource;
        unsigned int m_index;
    };

    class RemoveBasebandSampleSource : public Message {
	    MESSAGE_CLASS_DECLARATION

    public:
        RemoveBasebandSampleSource(BasebandSampleSource* sampleSource, unsigned int index) :
            Message(),
            m_sampleSource(sampleSource),
            m_index(index)
        { }
        BasebandSampleSource* getSampleSource() const { return m_sampleSource; }
        unsigned int getIndex() const { return m_index; }
    private:
        BasebandSampleSource* m_sampleSource;
        unsigned int m_index;
    };

    class AddMIMOChannel : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit AddMIMOChannel(MIMOChannel* channel) :
            Message(),
            m_channel(channel)
        { }
        MIMOChannel* getChannel() const { return m_channel; }
    private:
        MIMOChannel* m_channel;
    };

    class RemoveMIMOChannel : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit RemoveMIMOChannel(MIMOChannel* channel) :
            Message(),
            m_channel(channel)
        { }
        MIMOChannel* getChannel() const { return m_channel; }
    private:
        MIMOChannel* m_channel;
    };

    class AddBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddBasebandSampleSink(BasebandSampleSink* sampleSink, unsigned int index) :
            Message(),
            m_sampleSink(sampleSink),
            m_index(index)
        { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
        unsigned int getIndex() const { return m_index; }
    private:
        BasebandSampleSink* m_sampleSink;
        unsigned int m_index;
    };

    class RemoveBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        RemoveBasebandSampleSink(BasebandSampleSink* sampleSink, unsigned int index) :
            Message(),
            m_sampleSink(sampleSink),
            m_index(index)
        { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
        unsigned int getIndex() const { return m_index; }
    private:
        BasebandSampleSink* m_sampleSink;
        unsigned int m_index;
    };

    class AddSpectrumSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit AddSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
    private:
        BasebandSampleSink* m_sampleSink;
    };

    class RemoveSpectrumSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit RemoveSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
    private:
        BasebandSampleSink* m_sampleSink;
    };

    class GetErrorMessage : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        explicit GetErrorMessage(unsigned int subsystemIndex) :
            m_subsystemIndex(subsystemIndex)
        {}
        void setErrorMessage(const QString& text) { m_errorMessage = text; }
        int getSubsystemIndex() const { return m_subsystemIndex; }
        const QString& getErrorMessage() const { return m_errorMessage; }
    private:
        int m_subsystemIndex;
        QString m_errorMessage;
    };

    class GetMIMODeviceDescription : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        void setDeviceDescription(const QString& text) { m_deviceDescription = text; }
        const QString& getDeviceDescription() const { return m_deviceDescription; }
    private:
        QString m_deviceDescription;
    };

    class ConfigureCorrection : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        ConfigureCorrection(bool dcOffsetCorrection, bool iqImbalanceCorrection, unsigned int index) :
            Message(),
            m_dcOffsetCorrection(dcOffsetCorrection),
            m_iqImbalanceCorrection(iqImbalanceCorrection),
            m_index(index)
        { }
        bool getDCOffsetCorrection() const { return m_dcOffsetCorrection; }
        bool getIQImbalanceCorrection() const { return m_iqImbalanceCorrection; }
        unsigned int getIndex() const { return m_index; }
    private:
        bool m_dcOffsetCorrection;
        bool m_iqImbalanceCorrection;
        unsigned int m_index;
    };

    class SetSpectrumSinkInput : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        SetSpectrumSinkInput(bool sourceElseSink, int index) :
            m_sourceElseSink(sourceElseSink),
            m_index(index)
        { }
        bool getSourceElseSink() const { return m_sourceElseSink; }
        int getIndex() const { return m_index; }
    private:
        bool m_sourceElseSink;
        int m_index;
    };

	enum class State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPDeviceMIMOEngine(uint32_t uid, QObject* parent = nullptr);
	~DSPDeviceMIMOEngine() override;

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	bool initProcess(int subsystemIndex);  //!< Initialize process sequence
	bool startProcess(int subsystemIndex); //!< Start process sequence
	void stopProcess(int subsystemIndex);  //!< Stop process sequence

	void setMIMO(DeviceSampleMIMO* mimo); //!< Set the sample MIMO type
	DeviceSampleMIMO *getMIMO() { return m_deviceSampleMIMO; }
	void setMIMOSequence(int sequence); //!< Set the sample MIMO sequence in type
    uint getUID() const { return m_uid; }

	void addChannelSource(BasebandSampleSource* source, int index = 0);            //!< Add a channel source
	void removeChannelSource(BasebandSampleSource* source, int index = 0);         //!< Remove a channel source
	void addChannelSink(BasebandSampleSink* sink, int index = 0);                  //!< Add a channel sink
	void removeChannelSink(BasebandSampleSink* sink, int index = 0);               //!< Remove a channel sink
    void addMIMOChannel(MIMOChannel *channel);                                     //!< Add a MIMO channel
    void removeMIMOChannel(MIMOChannel *channel);                                  //!< Remove a MIMO channel

	void addSpectrumSink(BasebandSampleSink* spectrumSink);    //!< Add a spectrum vis baseband sample sink
	void removeSpectrumSink(BasebandSampleSink* spectrumSink); //!< Add a spectrum vis baseband sample sink
    void setSpectrumSinkInput(bool sourceElseSink, int index);

	State state(int subsystemIndex) const //!< Return DSP engine current state
    {
        if (subsystemIndex == 0) {
            return m_stateRx;
        } else if (subsystemIndex == 1) {
            return m_stateTx;
        } else {
            return State::StNotStarted;
        }
    }

	QString errorMessage(int subsystemIndex) const; //!< Return the current error message
	QString deviceDescription() const; //!< Return the device description

   	void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection, int isource); //!< Configure source DSP corrections

private:
    struct SourceCorrection
    {
        bool m_dcOffsetCorrection = false;
        bool m_iqImbalanceCorrection = false;
        double m_iOffset = 0;
        double m_qOffset = 0;
        int m_iRange = 1 << 16;
        int m_qRange = 1 << 16;
        int m_imbalance = 65536;
        MovingAverageUtil<int32_t, int64_t, 1024> m_iBeta;
        MovingAverageUtil<int32_t, int64_t, 1024> m_qBeta;
#if IMBALANCE_INT
        // Fixed point DC + IQ corrections
        MovingAverageUtil<int64_t, int64_t, 128> m_avgII;
        MovingAverageUtil<int64_t, int64_t, 128> m_avgIQ;
        MovingAverageUtil<int64_t, int64_t, 128> m_avgPhi;
        MovingAverageUtil<int64_t, int64_t, 128> m_avgII2;
        MovingAverageUtil<int64_t, int64_t, 128> m_avgQQ2;
        MovingAverageUtil<int64_t, int64_t, 128> m_avgAmp;
#else
        // Floating point DC + IQ corrections
        MovingAverageUtil<float, double, 128> m_avgII;
        MovingAverageUtil<float, double, 128> m_avgIQ;
        MovingAverageUtil<float, double, 128> m_avgII2;
        MovingAverageUtil<float, double, 128> m_avgQQ2;
        MovingAverageUtil<double, double, 128> m_avgPhi;
        MovingAverageUtil<double, double, 128> m_avgAmp;
#endif
        SourceCorrection()
        {
            m_iBeta.reset();
            m_qBeta.reset();
            m_avgAmp.reset();
            m_avgII.reset();
            m_avgII2.reset();
            m_avgIQ.reset();
            m_avgPhi.reset();
            m_avgQQ2.reset();
            m_iBeta.reset();
            m_qBeta.reset();
        }
    };

	uint32_t m_uid; //!< unique ID
    State m_stateRx;
    State m_stateTx;

    QString m_errorMessageRx;
    QString m_errorMessageTx;
	QString m_deviceDescription;

	DeviceSampleMIMO* m_deviceSampleMIMO;
	int m_sampleMIMOSequence;

    MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.

	using BasebandSampleSinks = std::list<BasebandSampleSink *>;
	std::vector<BasebandSampleSinks> m_basebandSampleSinks; //!< ancillary sample sinks on main thread (per input stream)
    std::map<int, bool> m_rxRealElseComplex; //!< map of real else complex indicators for device sources (by input stream)
	using BasebandSampleSources = std::list<BasebandSampleSource *>;
	std::vector<BasebandSampleSources> m_basebandSampleSources; //!< channel sample sources (per output stream)
    std::map<int, bool> m_txRealElseComplex; //!< map of real else complex indicators for device sinks (by input stream)
    std::vector<IncrementalVector<Sample>> m_sourceSampleBuffers;
    std::vector<IncrementalVector<Sample>> m_sourceZeroBuffers;
    unsigned int m_sumIndex;            //!< channel index when summing channels

    using MIMOChannels = std::list<MIMOChannel *>;
    MIMOChannels m_mimoChannels; //!< MIMO channels

    std::vector<SourceCorrection> m_sourcesCorrections;

    BasebandSampleSink *m_spectrumSink; //!< The spectrum sink
    bool m_spectrumInputSourceElseSink; //!< Source else sink stream to be used as spectrum sink input
    unsigned int m_spectrumInputIndex;  //!< Index of the stream to be used as spectrum sink input

    void workSampleSinkFifos(); //!< transfer samples of all sink streams (sync mode)
    void workSampleSinkFifo(unsigned int streamIndex); //!< transfer samples of one sink stream (async mode)
    void workSamplesSink(const SampleVector::const_iterator& vbegin, const SampleVector::const_iterator& vend, unsigned int streamIndex);
    void workSampleSourceFifos(); //!< transfer samples of all source streams (sync mode)
    void workSampleSourceFifo(unsigned int streamIndex); //!< transfer samples of one source stream (async mode)
    void workSamplesSource(SampleVector& data, unsigned int iBegin, unsigned int iEnd, unsigned int streamIndex);

	State gotoIdle(int subsystemIndex);     //!< Go to the idle state
	State gotoInit(int subsystemIndex);     //!< Go to the acquisition init state from idle
	State gotoRunning(int subsystemIndex);  //!< Go to the running state from ready state
	State gotoError(int subsystemIndex, const QString& errorMsg); //!< Go to an error state
	void setStateRx(State state);
	void setStateTx(State state);

    void handleSetMIMO(DeviceSampleMIMO* mimo); //!< Manage MIMO device setting
    void iqCorrections(SampleVector::iterator begin, SampleVector::iterator end, int isource, bool imbalanceCorrection);
    bool handleMessage(const Message& cmd);

private slots:
	void handleDataRxSync();           //!< Handle data when Rx samples have to be processed synchronously
	void handleDataRxAsync(int streamIndex); //!< Handle data when Rx samples have to be processed asynchronously
	void handleDataTxSync();           //!< Handle data when Tx samples have to be processed synchronously
	void handleDataTxAsync(int streamIndex); //!< Handle data when Tx samples have to be processed asynchronously
	void handleInputMessages();        //!< Handle input message queue

signals:
	void stateChanged();

    void acquisitionStopped();
    void sampleSet();
    void generationStopped();
    void spectrumSinkRemoved();
};

#endif // SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_
