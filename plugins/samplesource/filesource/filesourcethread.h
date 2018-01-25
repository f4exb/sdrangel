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
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "dsp/inthalfbandfilter.h"

#define FILESOURCE_THROTTLE_MS 50

class SampleSinkFifo;

class FileSourceThread : public QThread {
	Q_OBJECT

public:
	FileSourceThread(std::ifstream *samplesStream, SampleSinkFifo* sampleFifo, QObject* parent = NULL);
	~FileSourceThread();

	void startWork();
	void stopWork();
	void setSampleRateAndSize(int samplerate, quint32 samplesize);
    void setBuffers(std::size_t chunksize);
	bool isRunning() const { return m_running; }
	std::size_t getSamplesCount() const { return m_samplesCount; }
	void setSamplesCount(int samplesCount) { m_samplesCount = samplesCount; }

	void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	std::ifstream* m_ifstream;
	quint8  *m_fileBuf;
	quint8  *m_convertBuf;
	std::size_t m_bufsize;
	std::size_t m_chunksize;
	SampleSinkFifo* m_sampleFifo;
	std::size_t m_samplesCount;

	int m_samplerate;      //!< File I/Q stream original sample rate
	quint32 m_samplesize;  //!< File effective sample size in bits (I or Q). Ex: 16, 24.
	quint32 m_samplebytes; //!< Number of bytes used to store a I or Q sample. Ex: 2. 4.
    int m_throttlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

	void run();
	//void decimate1(SampleVector::iterator* it, const qint16* buf, qint32 len);
	void writeToSampleFifo(const quint8* buf, qint32 nbBytes);
private slots:
	void tick();
};

#endif // INCLUDE_FILESOURCETHREAD_H
