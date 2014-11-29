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

#ifndef INCLUDE_FCDTHREAD_H
#define INCLUDE_FCDTHREAD_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include "dsp/samplefifo.h"
#include "dsp/inthalfbandfilter.h"

#define SAMPLERATE 192000
#define BLOCKSIZE 8192

class FCDThread : public QThread {
	Q_OBJECT

public:
	FCDThread(SampleFifo* sampleFifo, QObject* parent = NULL);
	~FCDThread();

	void stopWork();
	void OpenSource(const char *filename);
	void CloseSource();
	void set_center_freq(double freq);
	int  work(int n_items);
private:
	int fd;
	struct fcd_buffer *buffers;
	unsigned int n_buffers;
	void *recebuf_ptr;
	unsigned int recebuf_len;
	unsigned int recebuf_mmap_index;

	QMutex m_startWaitMutex;
	QWaitCondition m_startWaiter;
	bool m_running;

	SampleVector m_convertBuffer;
	SampleFifo* m_sampleFifo;

	void run();

};
#endif // INCLUDE_FCDTHREAD_H
