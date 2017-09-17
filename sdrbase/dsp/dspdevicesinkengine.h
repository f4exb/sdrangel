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

#ifndef SDRBASE_DSP_DSPDEVICESINKENGINE_H_
#define SDRBASE_DSP_DSPDEVICESINKENGINE_H_

#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <stdint.h>
#include <list>
#include <map>
#include "dsp/dsptypes.h"
#include "dsp/fftwindow.h"
#include "util/messagequeue.h"
#include "util/syncmessenger.h"
#include "util/export.h"

class DeviceSampleSink;
class BasebandSampleSource;
class ThreadedBasebandSampleSource;
class BasebandSampleSink;

class SDRANGEL_API DSPDeviceSinkEngine : public QThread {
	Q_OBJECT

public:
	enum State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPDeviceSinkEngine(uint32_t uid, QObject* parent = NULL);
	~DSPDeviceSinkEngine();

	uint32_t getUID() const { return m_uid; }

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	void start(); //!< This thread start
	void stop();  //!< This thread stop

	bool initGeneration(); //!< Initialize generation sequence
	bool startGeneration(); //!< Start generation sequence
	void stopGeneration();   //!< Stop generation sequence

	void setSink(DeviceSampleSink* sink); //!< Set the sample sink type
	DeviceSampleSink *getSink() { return m_deviceSampleSink; }
	void setSinkSequence(int sequence); //!< Set the sample sink sequence in type

	void addSource(BasebandSampleSource* source); //!< Add a baseband sample source
	void removeSource(BasebandSampleSource* source); //!< Remove a baseband sample source

	void addThreadedSource(ThreadedBasebandSampleSource* source); //!< Add a baseband sample source that will run on its own thread
	void removeThreadedSource(ThreadedBasebandSampleSource* source); //!< Remove a baseband sample source that runs on its own thread

	uint32_t getNumberOfSources() const { return m_basebandSampleSources.size() + m_threadedBasebandSampleSources.size(); }

	void addSpectrumSink(BasebandSampleSink* spectrumSink);    //!< Add a spectrum vis baseband sample sink
	void removeSpectrumSink(BasebandSampleSink* spectrumSink); //!< Add a spectrum vis baseband sample sink

	State state() const { return m_state; } //!< Return DSP engine current state

	QString errorMessage(); //!< Return the current error message
	QString sinkDeviceDescription(); //!< Return the sink device description

private:
	uint32_t m_uid; //!< unique ID

	MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.
	SyncMessenger m_syncMessenger;     //!< Used to process messages synchronously with the thread

	State m_state;

	QString m_errorMessage;
	QString m_deviceDescription;

	DeviceSampleSink* m_deviceSampleSink;
	int m_sampleSinkSequence;

	typedef std::list<BasebandSampleSource*> BasebandSampleSources;
	BasebandSampleSources m_basebandSampleSources; //!< baseband sample sources within main thread (usually file input)

	typedef std::list<ThreadedBasebandSampleSource*> ThreadedBasebandSampleSources;
	ThreadedBasebandSampleSources m_threadedBasebandSampleSources; //!< baseband sample sources on their own threads (usually channels)

	typedef std::map<BasebandSampleSource*, SampleVector::iterator> BasebandSampleSourcesIteratorMap;
	typedef std::pair<BasebandSampleSource*, SampleVector::iterator> BasebandSampleSourcesIteratorMapKV;
	BasebandSampleSourcesIteratorMap m_basebandSampleSourcesIteratorMap;

    typedef std::map<ThreadedBasebandSampleSource*, SampleVector::iterator> ThreadedBasebandSampleSourcesIteratorMap;
    typedef std::pair<ThreadedBasebandSampleSource*, SampleVector::iterator> ThreadedBasebandSampleSourcesIteratorMapKV;
    ThreadedBasebandSampleSourcesIteratorMap m_threadedBasebandSampleSourcesIteratorMap;

	BasebandSampleSink *m_spectrumSink;

	uint32_t m_sampleRate;
	quint64 m_centerFrequency;
	uint32_t m_multipleSourcesDivisionFactor;

	void run();
	void work(int nbWriteSamples); //!< transfer samples from beseband sources to sink if in running state

	State gotoIdle();     //!< Go to the idle state
	State gotoInit();     //!< Go to the acquisition init state from idle
	State gotoRunning();  //!< Go to the running state from ready state
	State gotoError(const QString& errorMsg); //!< Go to an error state

	void handleSetSink(DeviceSampleSink* sink); //!< Manage sink setting

private slots:
	void handleData(int nbSamples); //!< Handle data when samples have to be written to the sample FIFO
	void handleInputMessages(); //!< Handle input message queue
	void handleSynchronousMessages(); //!< Handle synchronous messages with the thread
	void handleForwardToSpectrumSink(int nbSamples);
};





#endif /* SDRBASE_DSP_DSPDEVICESINKENGINE_H_ */
