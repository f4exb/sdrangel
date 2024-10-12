///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2019, 2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#ifndef SDRBASE_DSP_DSPDEVICESINKENGINE_H_
#define SDRBASE_DSP_DSPDEVICESINKENGINE_H_

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>

#include <stdint.h>
#include <list>
#include <map>

#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "util/incrementalvector.h"
#include "export.h"

class DeviceSampleSink;
class BasebandSampleSource;
class BasebandSampleSink;

class SDRBASE_API DSPDeviceSinkEngine : public QObject {
	Q_OBJECT

public:
	enum class State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPDeviceSinkEngine(uint32_t uid, QObject* parent = nullptr);
	~DSPDeviceSinkEngine() final;

	uint32_t getUID() const { return m_uid; }

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	bool initGeneration(); //!< Initialize generation sequence
	bool startGeneration(); //!< Start generation sequence
	void stopGeneration();   //!< Stop generation sequence

	void setSink(DeviceSampleSink* sink); //!< Set the sample sink type
	DeviceSampleSink *getSink() { return m_deviceSampleSink; }
	void setSinkSequence(int sequence); //!< Set the sample sink sequence in type

	void addChannelSource(BasebandSampleSource* source);       //!< Add a baseband sample source
	void removeChannelSource(BasebandSampleSource* source);    //!< Remove a baseband sample source

	void addSpectrumSink(BasebandSampleSink* spectrumSink);    //!< Add a spectrum vis baseband sample sink
	void removeSpectrumSink(BasebandSampleSink* spectrumSink); //!< Add a spectrum vis baseband sample sink

	State state() const { return m_state; } //!< Return DSP engine current state

	QString errorMessage() const; //!< Return the current error message
	QString sinkDeviceDescription() const; //!< Return the sink device description

private:
	uint32_t m_uid; //!< unique ID

	MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.

	State m_state;

	QString m_errorMessage;
	QString m_deviceDescription;

	DeviceSampleSink* m_deviceSampleSink;
	int m_sampleSinkSequence;

	using BasebandSampleSources = std::list<BasebandSampleSource *>;
	BasebandSampleSources m_basebandSampleSources; //!< baseband sample sources within main thread (usually file input)

	BasebandSampleSink *m_spectrumSink;
    IncrementalVector<Sample> m_sourceSampleBuffer;
    IncrementalVector<Sample> m_sourceZeroBuffer;

	uint32_t m_sampleRate;
	quint64 m_centerFrequency;
    bool m_realElseComplex;
    unsigned int m_sumIndex; //!< channel index when summing channels

	void workSampleFifo(); //!< transfer samples from baseband sources to sink if in running state
    void workSamples(SampleVector& data, unsigned int iBegin, unsigned int iEnd);

	State gotoIdle();     //!< Go to the idle state
	State gotoInit();     //!< Go to the acquisition init state from idle
	State gotoRunning();  //!< Go to the running state from ready state
	State gotoError(const QString& errorMsg); //!< Go to an error state
	void setState(State state);

	void handleSetSink(const DeviceSampleSink* sink); //!< Manage sink setting
    bool handleMessage(const Message& cmd);

private slots:
	void handleData(); //!< Handle data when samples have to be written to the sample FIFO
	void handleInputMessages(); //!< Handle input message queue

signals:
	void stateChanged();

	void generationStopped();
	void sampleSet();
	void spectrumSinkRemoved();
};





#endif /* SDRBASE_DSP_DSPDEVICESINKENGINE_H_ */
