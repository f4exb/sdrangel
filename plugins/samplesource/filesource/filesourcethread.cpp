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
	m_fileBuf(0),
	m_convertBuf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
    m_samplerate(0),
	m_samplesize(0),
	m_samplebytes(0),
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

	if (m_fileBuf != 0) {
		free(m_fileBuf);
	}

	if (m_convertBuf != 0) {
		free(m_convertBuf);
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

void FileSourceThread::setSampleRateAndSize(int samplerate, quint32 samplesize)
{
	qDebug() << "FileSourceThread::setSampleRateAndSize:"
			<< " new rate:" << samplerate
			<< " new size:" << samplesize
			<< " old rate:" << m_samplerate
			<< " old size:" << m_samplesize;

	if ((samplerate != m_samplerate) || (samplesize != m_samplesize))
	{
		if (m_running) {
			stopWork();
		}

		m_samplerate = samplerate;
		m_samplesize = samplesize;
		m_samplebytes = m_samplesize > 16 ? sizeof(int32_t) : sizeof(int16_t);
        // TODO: implement FF and slow motion here. 2 corresponds to live. 1 is half speed, 4 is double speed
        m_chunksize = (m_samplerate * 2 * m_samplebytes * m_throttlems) / 1000;

        setBuffers(m_chunksize);
	}

	//m_samplerate = samplerate;
}

void FileSourceThread::setBuffers(std::size_t chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;
        int nbSamples = m_bufsize/(2 * m_samplebytes);

        if (m_fileBuf == 0)
        {
            qDebug() << "FileSourceThread::setBuffers: Allocate file buffer";
            m_fileBuf = (quint8*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "FileSourceThread::setBuffers: Re-allocate file buffer";
            quint8 *buf = m_fileBuf;
            m_fileBuf = (quint8*) realloc((void*) m_fileBuf, m_bufsize);
            if (!m_fileBuf) free(buf);
        }

        if (m_convertBuf == 0)
        {
            qDebug() << "FileSourceThread::setBuffers: Allocate conversion buffer";
            m_convertBuf = (quint8*) malloc(nbSamples*sizeof(Sample));
        }
        else
        {
            qDebug() << "FileSourceThread::setBuffers: Re-allocate conversion buffer";
            quint8 *buf = m_convertBuf;
            m_convertBuf = (quint8*) realloc((void*) m_convertBuf, nbSamples*sizeof(Sample));
            if (!m_convertBuf) free(buf);
        }

        qDebug() << "FileSourceThread::setBuffers: size: " << m_bufsize
                << " #samples: " << nbSamples;
    }
}

void FileSourceThread::run()
{
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
            m_chunksize = 2 * m_samplebytes * ((m_samplerate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
            m_throttleToggle = !m_throttleToggle;
            setBuffers(m_chunksize);
        }

		// read samples directly feeding the SampleFifo (no callback)
		m_ifstream->read(reinterpret_cast<char*>(m_fileBuf), m_chunksize);

        if (m_ifstream->eof())
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_ifstream->gcount());
            //m_sampleFifo->write(m_buf, m_ifstream->gcount());
            // TODO: handle loop playback situation
    		m_ifstream->clear();
            m_ifstream->seekg(sizeof(FileRecord::Header), std::ios::beg);
    		m_samplesCount = 0;
            //stopWork();
            //m_ifstream->close();
        }
        else
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_chunksize);
            //m_sampleFifo->write(m_buf, m_chunksize);
    		m_samplesCount += m_chunksize / (2 * m_samplebytes);
        }
	}
}

void FileSourceThread::writeToSampleFifo(const quint8* buf, qint32 nbBytes)
{
	if (m_samplesize == 16)
	{
		if (SDR_RX_SAMP_SZ == 16)
		{
			m_sampleFifo->write(buf, nbBytes);
		}
		else if (SDR_RX_SAMP_SZ == 24)
		{
			FixReal *convertBuf = (FixReal *) m_convertBuf;
			const int16_t *fileBuf = (int16_t *) buf;
			int nbSamples = nbBytes / (2 * m_samplebytes);

			for (int is = 0; is < nbSamples; is++)
			{
				convertBuf[2*is]   = fileBuf[2*is] << 8;
				convertBuf[2*is+1] = fileBuf[2*is+1] << 8;
			}

			m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
		}
	}
	else if (m_samplesize == 24)
	{
		if (SDR_RX_SAMP_SZ == 24)
		{
			m_sampleFifo->write(buf, nbBytes);
		}
		else if (SDR_RX_SAMP_SZ == 16)
		{
			FixReal *convertBuf = (FixReal *) m_convertBuf;
			const int32_t *fileBuf = (int32_t *) buf;
			int nbSamples = nbBytes / (2 * m_samplebytes);

			for (int is = 0; is < nbSamples; is++)
			{
				convertBuf[2*is]   = fileBuf[2*is] >> 8;
				convertBuf[2*is+1] = fileBuf[2*is+1] >> 8;
			}

			m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
		}
	}
}
