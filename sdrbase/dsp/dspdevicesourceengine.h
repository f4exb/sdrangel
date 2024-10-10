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

#ifndef INCLUDE_DSPDEVICEENGINE_H
#define INCLUDE_DSPDEVICEENGINE_H

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/dsptypes.h"
#include "util/messagequeue.h"
#include "export.h"
#include "util/movingaverage.h"

class DeviceSampleSource;
class BasebandSampleSink;

class SDRBASE_API DSPDeviceSourceEngine : public QObject {
	Q_OBJECT

public:
	enum class State {
		StNotStarted,  //!< engine is before initialization
		StIdle,        //!< engine is idle
		StReady,       //!< engine is ready to run
		StRunning,     //!< engine is running
		StError        //!< engine is in error
	};

	DSPDeviceSourceEngine(uint uid, QObject* parent = nullptr);
	~DSPDeviceSourceEngine() final;

	uint getUID() const { return m_uid; }

	MessageQueue* getInputMessageQueue() { return &m_inputMessageQueue; }

	bool initAcquisition() const; //!< Initialize acquisition sequence
	bool startAcquisition(); //!< Start acquisition sequence
	void stopAcquistion();   //!< Stop acquisition sequence

	void setSource(DeviceSampleSource* source); //!< Set the sample source type
	void setSourceSequence(int sequence); //!< Set the sample source sequence in type
	DeviceSampleSource *getSource() { return m_deviceSampleSource; }

	void addSink(BasebandSampleSink* sink); //!< Add a sample sink
	void removeSink(BasebandSampleSink* sink); //!< Remove a sample sink

	void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection); //!< Configure DSP corrections

	State state() const { return m_state; } //!< Return DSP engine current state

	QString errorMessage() const; //!< Return the current error message
	QString sourceDeviceDescription() const; //!< Return the source device description

private:
	uint m_uid; //!< unique ID

	MessageQueue m_inputMessageQueue;  //<! Input message queue. Post here.

	State m_state;

	QString m_errorMessage;
	QString m_deviceDescription;

	DeviceSampleSource* m_deviceSampleSource;
	int m_sampleSourceSequence;

	using BasebandSampleSinks = std::list<BasebandSampleSink *>;
	BasebandSampleSinks m_basebandSampleSinks; //!< sample sinks within main thread (usually spectrum, file output)

	uint m_sampleRate;
	quint64 m_centerFrequency;
    bool m_realElseComplex;

	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;
	double m_iOffset;
	double m_qOffset;

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

    qint32 m_iRange;
	qint32 m_qRange;
	qint32 m_imbalance;

	void iqCorrections(SampleVector::iterator begin, SampleVector::iterator end, bool imbalanceCorrection);
	void dcOffset(SampleVector::iterator begin, SampleVector::iterator end);
	void imbalance(SampleVector::iterator begin, SampleVector::iterator end);
	void work(); //!< transfer samples from source to sinks if in running state

	State gotoIdle();     //!< Go to the idle state
	State gotoInit();     //!< Go to the acquisition init state from idle
	State gotoRunning();  //!< Go to the running state from ready state
	State gotoError(const QString& errorMsg); //!< Go to an error state
	void setState(State state);

	void handleSetSource(DeviceSampleSource* source); //!< Manage source setting
    bool handleMessage(const Message& cmd);

private slots:
	void handleData(); //!< Handle data when samples from source FIFO are ready to be processed
	void handleInputMessages(); //!< Handle input message queue

signals:
	void stateChanged();

	void acquistionStopped();
	void sampleSet();
	void sinkRemoved();
};

#endif // INCLUDE_DSPDEVICEENGINE_H
