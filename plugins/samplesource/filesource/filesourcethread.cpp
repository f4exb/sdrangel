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

#include <stdio.h>
#include <errno.h>
#include "dsp/samplefifo.h"

#include "filesourcethread.h"

const int FileSourceThread::m_rateDivider = 1000/FILESOURCE_THROTTLE_MS;

FileSourceThread::FileSourceThread(std::ifstream *samplesStream, SampleFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_ifstream(samplesStream),
	m_buf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplerate(0)
{
}

FileSourceThread::~FileSourceThread()
{
	if (m_running) {
		stopWork();
	}

	if (m_buf != 0) {
		free(m_buf);
	}
}

void FileSourceThread::startWork()
{
	m_startWaitMutex.lock();
	start();
	while(!m_running)
		m_startWaiter.wait(&m_startWaitMutex, 100);
	m_startWaitMutex.unlock();
}

void FileSourceThread::stopWork()
{
	m_running = false;
	wait();
}

void FileSourceThread::setSamplerate(int samplerate)
{
	if (samplerate != m_samplerate)
	{
		if (m_running) {
			stopWork();
		}

		m_bufsize = (m_samplerate / m_rateDivider)*4;

		if (m_buf == 0)	{
			m_buf = (quint8*) malloc(m_bufsize);
		} else {
			m_buf = (quint8*) realloc((void*) m_buf, m_bufsize);
		}
	}

	m_samplerate = samplerate;
	m_chunksize = (m_samplerate / m_rateDivider)*4;
}

void FileSourceThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running)
	{
		sleep(1);
	}

	m_running = false;
}

void FileSourceThread::connectTimer(const QTimer& timer)
{
	std::cerr << "FileSourceThread::connectTimer" << std::endl;
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void FileSourceThread::tick()
{
	if (m_running)
	{
		// read samples directly feeding the SampleFifo (no callback)
		m_ifstream->read(reinterpret_cast<char*>(m_buf), m_chunksize);
		m_sampleFifo->write(m_buf, m_chunksize);
	}
}


/*
void FileSourceThread::decimate1(SampleVector::iterator* it, const qint16* buf, qint32 len)
{
	qint16 xreal, yimag;

	for (int pos = 0; pos < len; pos += 2) {
		xreal = buf[pos+0];
		yimag = buf[pos+1];
		Sample s( xreal * 16, yimag * 16 ); // shift by 4 bit positions (signed)
		**it = s;
		(*it)++;
	}
}

//  Decimate according to specified log2 (ex: log2=4 => decim=16)
void FileSourceThread::callback(const qint16* buf, qint32 len)
{
	SampleVector::iterator it = m_convertBuffer.begin();
	decimate1(&it, buf, len);
	m_sampleFifo->write(m_convertBuffer.begin(), it);
}
*/
