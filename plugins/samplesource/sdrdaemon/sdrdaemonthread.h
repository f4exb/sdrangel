///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_SDRDAEMONTHREAD_H
#define INCLUDE_SDRDAEMONTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"

#define SDRDAEMON_THROTTLE_MS 50

class SDRdaemonThread : public QThread {
	Q_OBJECT

public:
	SDRdaemonThread(std::ifstream *samplesStream, SampleFifo* sampleFifo, QObject* parent = NULL);
	~SDRdaemonThread();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
	bool isRunning() const { return m_running; }
	std::size_t getSamplesCount() const { return m_samplesCount; }

	void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	std::ifstream* m_ifstream;
	quint8  *m_buf;
	std::size_t m_bufsize;
	std::size_t m_chunksize;
	SampleFifo* m_sampleFifo;
	std::size_t m_samplesCount;

	int m_samplerate;
	static const int m_rateDivider;

	void run();
private slots:
	void tick();
};

#endif // INCLUDE_SDRDAEMONTHREAD_H
