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

#ifndef INCLUDE_FILESINKTHREAD_H
#define INCLUDE_FILESINKTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QTimer>
#include <QElapsedTimer>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <stdint.h>

#include "dsp/inthalfbandfilter.h"
#include "dsp/interpolators.h"

#define FILESINK_THROTTLE_MS 50

class SampleSourceFifo;

class FileSinkThread : public QThread {
	Q_OBJECT

public:
	FileSinkThread(std::ofstream *samplesStream, SampleSourceFifo* sampleFifo, QObject* parent = 0);
	~FileSinkThread();

	void startWork();
	void stopWork();
	void setSamplerate(int samplerate);
	void setLog2Interpolation(int log2Interpolation);
    void setBuffer(std::size_t chunksize);
	bool isRunning() const { return m_running; }
    std::size_t getSamplesCount() const { return m_samplesCount; }
    void setSamplesCount(int samplesCount) { m_samplesCount = samplesCount; }

	void connectTimer(const QTimer& timer);

private:
	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	std::ofstream* m_ofstream;
	std::size_t m_bufsize;
	unsigned int m_samplesChunkSize;
	SampleSourceFifo* m_sampleFifo;
    std::size_t m_samplesCount;

	int m_samplerate;
	int m_log2Interpolation;
    int m_throttlems;
    int m_maxThrottlems;
    QElapsedTimer m_elapsedTimer;
    bool m_throttleToggle;

    Interpolators<qint16, SDR_TX_SAMP_SZ, 16> m_interpolators;
    int16_t *m_buf;

	void run();

private slots:
	void tick();
};

#endif // INCLUDE_FILESINKTHREAD_H
