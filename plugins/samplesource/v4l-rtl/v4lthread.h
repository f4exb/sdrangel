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

#ifndef INCLUDE_V4LTHREAD_H
#define INCLUDE_V4LTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"

#define SAMPLERATE 1024000
#define FCD_BLOCKSIZE 4096

class V4LThread : public QThread {
	Q_OBJECT

public:
	V4LThread(SampleFifo* sampleFifo, double frequency, QObject* parent = NULL);
	~V4LThread();

	void stopWork();
	void OpenSource(const char *filename);
	void CloseSource();
	void set_sample_rate(double samp_rate);
	void set_center_freq(double freq);
	void set_bandwidth(double bandwidth);
	void set_tuner_gain(double gain);
	int  work(int n_items);
private:
	int fd;
	quint32 pixelformat;
	struct v4l_buffer *buffers;
	unsigned int n_buffers;
	void *recebuf_ptr;
	unsigned int recebuf_len;
	unsigned int recebuf_mmap_index;

	double centerFreq;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	SampleVector m_convertBuffer;
	SampleFifo* m_sampleFifo;

	void run();

};
#endif // INCLUDE_V4LTHREAD_H
