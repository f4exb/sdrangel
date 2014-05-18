///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#ifndef INCLUDE_DSPENGINE_H
#define INCLUDE_DSPENGINE_H

#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/dsptypes.h"
#include "dsp/fftwindow.h"
#include "dsp/samplefifo.h"
#include "audio/audiooutput.h"
#include "util/messagequeue.h"
#include "util/export.h"

class SampleSource;
class SampleSink;
class AudioFifo;

class SDRANGELOVE_API DSPEngine : public QThread {
	Q_OBJECT

public:
	enum State {
		StNotStarted,
		StIdle,
		StRunning,
		StError
	};

	DSPEngine(MessageQueue* reportQueue, QObject* parent = NULL);
	~DSPEngine();

	MessageQueue* getMessageQueue() { return &m_messageQueue; }

	void start();
	void stop();

	bool startAcquisition();
	void stopAcquistion();

	void setSource(SampleSource* source);

	void addSink(SampleSink* sink);
	void removeSink(SampleSink* sink);

	void addAudioSource(AudioFifo* audioFifo);
	void removeAudioSource(AudioFifo* audioFifo);

	void configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection);

	State state() const { return m_state; }

	QString errorMessage();
	QString deviceDescription();

private:
	MessageQueue m_messageQueue;
	MessageQueue* m_reportQueue;

	State m_state;

	QString m_errorMessage;
	QString m_deviceDescription;

	SampleSource* m_sampleSource;

	typedef std::list<SampleSink*> SampleSinks;
	SampleSinks m_sampleSinks;

	AudioOutput m_audioOutput;

	uint m_sampleRate;
	quint64 m_centerFrequency;

	bool m_dcOffsetCorrection;
	bool m_iqImbalanceCorrection;
	qint32 m_iOffset;
	qint32 m_qOffset;
	qint32 m_iRange;
	qint32 m_qRange;
	qint32 m_imbalance;

	void run();

	void dcOffset(SampleVector::iterator begin, SampleVector::iterator end);
	void imbalance(SampleVector::iterator begin, SampleVector::iterator end);
	void work();

	State gotoIdle();
	State gotoRunning();
	State gotoError(const QString& errorMsg);

	void handleSetSource(SampleSource* source);
	void generateReport();
	bool distributeMessage(Message* message);

private slots:
	void handleData();
	void handleMessages();
};

#endif // INCLUDE_DSPENGINE_H
