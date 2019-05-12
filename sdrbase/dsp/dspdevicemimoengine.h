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

#ifndef SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_
#define SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_

#include <QThread>

#include "util/messagequeue.h"
#include "util/syncmessenger.h"
#include "export.h"

class DeviceSampleMIMO;
class ThreadedBasebandSampleSource;
class ThreadedBasebandSampleSink;
class BasebandSampleSink;

class SDRBASE_API DSPDeviceMIMOEngine : public QThread {
	Q_OBJECT

public:
    class SetSampleMIMO : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        SetSampleMIMO(DeviceSampleMIMO* sampleMIMO) : Message(), m_sampleMIMO(sampleMIMO) { }
        DeviceSampleMIMO* getSampleMIMO() const { return m_sampleMIMO; }
    private:
        DeviceSampleMIMO* m_sampleMIMO;
    };

    class AddThreadedBasebandSampleSource : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddThreadedBasebandSampleSource(ThreadedBasebandSampleSource* threadedSampleSource, int index) :
            Message(),
            m_threadedSampleSource(threadedSampleSource),
            m_index(index)
        { }
        ThreadedBasebandSampleSource* getThreadedSampleSource() const { return m_threadedSampleSource; }
        int getIndex() const { return m_index; }
    private:
        ThreadedBasebandSampleSource* m_threadedSampleSource;
        int m_index;
    };

    class RemoveThreadedBasebandSampleSource : public Message {
	    MESSAGE_CLASS_DECLARATION

    public:
        RemoveThreadedBasebandSampleSource(ThreadedBasebandSampleSource* threadedSampleSource, int index) :
            Message(),
            m_threadedSampleSource(threadedSampleSource),
            m_index(index)
        { }
        ThreadedBasebandSampleSource* getThreadedSampleSource() const { return m_threadedSampleSource; }
        int getIndex() const { return m_index; }
    private:
        ThreadedBasebandSampleSource* m_threadedSampleSource;
        int m_index;
    };

    class AddThreadedBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddThreadedBasebandSampleSink(ThreadedBasebandSampleSink* threadedSampleSink, int index) :
            Message(),
            m_threadedSampleSink(threadedSampleSink),
            m_index(index)
        { }
        ThreadedBasebandSampleSink* getThreadedSampleSink() const { return m_threadedSampleSink; }
        int getIndex() const { return m_index; }
    private:
        ThreadedBasebandSampleSink* m_threadedSampleSink;
        int m_index;
    };

    class RemoveThreadedBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        RemoveThreadedBasebandSampleSink(ThreadedBasebandSampleSink* threadedSampleSink, int index) :
            Message(),
            m_threadedSampleSink(threadedSampleSink),
            m_index(index)
        { }
        ThreadedBasebandSampleSink* getThreadedSampleSink() const { return m_threadedSampleSink; }
        int getIndex() const { return m_index; }
    private:
        ThreadedBasebandSampleSink* m_threadedSampleSink;
        int m_index;
    };

    class AddBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddBasebandSampleSink(BasebandSampleSink* sampleSink, int index) :
            Message(),
            m_sampleSink(sampleSink),
            m_index(index)
        { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
        int getIndex() const { return m_index; }
    private:
        BasebandSampleSink* m_sampleSink;
        int m_index;
    };

    class RemoveBasebandSampleSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        RemoveBasebandSampleSink(BasebandSampleSink* sampleSink, int index) :
            Message(),
            m_sampleSink(sampleSink),
            m_index(index)
        { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
        int getIndex() const { return m_index; }
    private:
        BasebandSampleSink* m_sampleSink;
        int m_index;
    };

    class AddSpectrumSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        AddSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
    private:
        BasebandSampleSink* m_sampleSink;
    };

    class RemoveSpectrumSink : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        RemoveSpectrumSink(BasebandSampleSink* sampleSink) : Message(), m_sampleSink(sampleSink) { }
        BasebandSampleSink* getSampleSink() const { return m_sampleSink; }
    private:
        BasebandSampleSink* m_sampleSink;
    };

    class GetErrorMessage : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        void setErrorMessage(const QString& text) { m_errorMessage = text; }
        const QString& getErrorMessage() const { return m_errorMessage; }
    private:
        QString m_errorMessage;
    };

    class GetSinkDeviceDescription : public Message {
        MESSAGE_CLASS_DECLARATION
    public:
        void setDeviceDescription(const QString& text) { m_deviceDescription = text; }
        const QString& getDeviceDescription() const { return m_deviceDescription; }
    private:
        QString m_deviceDescription;
    };

	enum State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPDeviceMIMOEngine(uint32_t uid, QObject* parent = nullptr);
	~DSPDeviceMIMOEngine();

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	void start(); //!< This thread start
	void stop();  //!< This thread stop

	bool initProcess();  //!< Initialize process sequence
	bool startProcess(); //!< Start process sequence
	void stopProcess();  //!< Stop process sequence

	void setMIMO(DeviceSampleMIMO* mimo); //!< Set the sample MIMO type
	DeviceSampleMIMO *getMIMO() { return m_deviceSampleMIMO; }
	void setMIMOSequence(int sequence); //!< Set the sample MIMO sequence in type

	void addChannelSource(ThreadedBasebandSampleSource* source, int index = 0);    //!< Add a channel source that will run on its own thread
	void removeChannelSource(ThreadedBasebandSampleSource* source, int index = 0); //!< Remove a channel source that runs on its own thread
	void addChannelSink(ThreadedBasebandSampleSink* sink, int index = 0);          //!< Add a channel sink that will run on its own thread
	void removeChannelSink(ThreadedBasebandSampleSink* sink, int index = 0);       //!< Remove a channel sink that runs on its own thread

	void addAncillarySink(BasebandSampleSink* sink, int index = 0);    //!< Add an ancillary sink like a I/Q recorder
	void removeAncillarySink(BasebandSampleSink* sink, int index = 0); //!< Remove an ancillary sample sink

	void addSpectrumSink(BasebandSampleSink* spectrumSink);    //!< Add a spectrum vis baseband sample sink
	void removeSpectrumSink(BasebandSampleSink* spectrumSink); //!< Add a spectrum vis baseband sample sink

	State state() const { return m_state; } //!< Return DSP engine current state

	QString errorMessage(); //!< Return the current error message
	QString deviceDescription(); //!< Return the device description

private:
	uint32_t m_uid; //!< unique ID
    State m_state;

    QString m_errorMessage;
	QString m_deviceDescription;

	DeviceSampleMIMO* m_deviceSampleMIMO;
	int m_sampleMIMOSequence;

    MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.
	SyncMessenger m_syncMessenger;     //!< Used to process messages synchronously with the thread

	typedef std::list<BasebandSampleSink*> BasebandSampleSinks;
	std::vector<BasebandSampleSinks> m_basebandSampleSinks; //!< ancillary sample sinks on main thread (per input stream)

	typedef std::list<ThreadedBasebandSampleSink*> ThreadedBasebandSampleSinks;
	std::vector<ThreadedBasebandSampleSinks> m_threadedBasebandSampleSinks; //!< channel sample sinks on their own thread (per input stream)

	typedef std::list<ThreadedBasebandSampleSource*> ThreadedBasebandSampleSources;
	std::vector<ThreadedBasebandSampleSources> m_threadedBasebandSampleSources; //!< channel sample sources on their own threads (per output stream)

    std::vector<uint32_t> m_sourceStreamSampleRates; //!< device sources streams (Rx) sample rates
    std::vector<uint32_t> m_sinkStreamSampleRates;   //!< device sink streams (Tx) sample rates

    BasebandSampleSink *m_spectrumSink; //!< The spectrum sink

  	void run();
	void work(int nbWriteSamples, int nbReadSamples); //!< transfer samples if in running state

	State gotoIdle();     //!< Go to the idle state
	State gotoInit();     //!< Go to the acquisition init state from idle
	State gotoRunning();  //!< Go to the running state from ready state
	State gotoError(const QString& errorMsg); //!< Go to an error state

private slots:
	void handleData(int nbSamples);    //!< Handle data when samples have to be written to the sample FIFO
	void handleInputMessages();        //!< Handle input message queue
	void handleSynchronousMessages();  //!< Handle synchronous messages with the thread
	void handleForwardToSpectrumSink(int nbSamples);
};

#endif // SDRBASE_DSP_DSPDEVICEMIMOENGINE_H_