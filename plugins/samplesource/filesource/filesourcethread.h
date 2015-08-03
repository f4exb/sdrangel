///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FILESOURCETHREAD_H
#define INCLUDE_FILESOURCETHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"

#define FILESOURCE_THROTTLE_MS 50

class FileSourceThread : public QThread {
	Q_OBJECT

public:
	FileSourceThread(std::ifstream *samplesStream, SampleFifo* sampleFifo, QObject* parent = NULL);
	~FileSourceThread();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
	bool isRunning() const { return m_running; }

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

	int m_samplerate;
	static const int m_rateDivider;

	void run();
	//void decimate1(SampleVector::iterator* it, const qint16* buf, qint32 len);
	//void callback(const qint16* buf, qint32 len);
private slots:
	void tick();
};

#endif // INCLUDE_FILESOURCETHREAD_H
