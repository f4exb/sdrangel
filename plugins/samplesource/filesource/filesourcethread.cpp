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
#include <assert.h>
#include <QDebug>

#include "dsp/filerecord.h"
#include "filesourcethread.h"
#include "dsp/samplesinkfifo.h"

FileSourceThread::FileSourceThread(std::ifstream *samplesStream, SampleSinkFifo* sampleFifo, QObject* parent) :
	QThread(parent),
	m_running(false),
	m_ifstream(samplesStream),
	m_buf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
    m_samplerate(0),
    m_throttlems(FILESOURCE_THROTTLE_MS),
    m_throttleToggle(false)
{
    assert(m_ifstream != 0);
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
	qDebug() << "FileSourceThread::startWork: ";

    if (m_ifstream->is_open())
    {
        qDebug() << "FileSourceThread::startWork: file stream open, starting...";
        m_startWaitMutex.lock();
        m_elapsedTimer.start();
        start();
        while(!m_running)
            m_startWaiter.wait(&m_startWaitMutex, 100);
        m_startWaitMutex.unlock();
    }
    else
    {
        qDebug() << "FileSourceThread::startWork: file stream closed, not starting.";
    }
}

void FileSourceThread::stopWork()
{
	qDebug() << "FileSourceThread::stopWork";
	m_running = false;
	wait();
}

void FileSourceThread::setSamplerate(int samplerate)
{
	qDebug() << "FileSourceThread::setSamplerate:"
			<< " new:" << samplerate
			<< " old:" << m_samplerate;

	if (samplerate != m_samplerate)
	{
		if (m_running) {
			stopWork();
		}

		m_samplerate = samplerate;
        // TODO: implement FF and slow motion here. 4 corresponds to live. 2 is half speed, 8 is doulbe speed
        m_chunksize = (m_samplerate * 4 * m_throttlems) / 1000;

        setBuffer(m_chunksize);
	}

	//m_samplerate = samplerate;
}

void FileSourceThread::setBuffer(std::size_t chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;

        if (m_buf == 0)
        {
            qDebug() << "FileSourceThread::setBuffer: Allocate buffer";
            m_buf = (quint8*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "FileSourceThread::setBuffer: Re-allocate buffer";
            quint8 *buf = m_buf;
            m_buf = (quint8*) realloc((void*) m_buf, m_bufsize);
            if (!m_buf) free(buf);
        }

        qDebug() << "FileSourceThread::setBuffer: size: " << m_bufsize
                << " #samples: " << (m_bufsize/4);
    }
}

void FileSourceThread::run()
{
	int res;

	m_running = true;
	m_startWaiter.wakeAll();

	while(m_running) // actual work is in the tick() function
	{
		sleep(1);
	}

	m_running = false;
}

void FileSourceThread::connectTimer(const QTimer& timer)
{
	qDebug() << "FileSourceThread::connectTimer";
	connect(&timer, SIGNAL(timeout()), this, SLOT(tick()));
}

void FileSourceThread::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_chunksize = 4 * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
            m_throttleToggle = !m_throttleToggle;
            setBuffer(m_chunksize);
        }

		// read samples directly feeding the SampleFifo (no callback)
		m_ifstream->read(reinterpret_cast<char*>(m_buf), m_chunksize);

        if (m_ifstream->eof())
        {
            m_sampleFifo->write(m_buf, m_ifstream->gcount());
            // TODO: handle loop playback situation
    		m_ifstream->clear();
            m_ifstream->seekg(sizeof(FileRecord::Header), std::ios::beg);
    		m_samplesCount = 0;
            //stopWork();
            //m_ifstream->close();
        }
        else
        {
            m_sampleFifo->write(m_buf, m_chunksize);
    		m_samplesCount += m_chunksize / 4;
        }
	}
}
