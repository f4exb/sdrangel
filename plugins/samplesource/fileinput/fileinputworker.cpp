///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2020 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
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
#include <QDebug>
#ifdef ANDROID
#include <QFile>
#endif

#include "fileinputworker.h"
#include "dsp/samplesinkfifo.h"
#include "util/messagequeue.h"

MESSAGE_CLASS_DEFINITION(FileInputWorker::MsgReportEOF, Message)

FileInputWorker::FileInputWorker(
#ifdef ANDROID
        QFile *samplesStream,
#else
        std::ifstream *samplesStream,
#endif
        SampleSinkFifo* sampleFifo,
        const QTimer& timer,
        MessageQueue *fileInputMessageQueue,
        QObject* parent) :
	QObject(parent),
	m_running(false),
	m_ifstream(samplesStream),
	m_fileBuf(nullptr),
	m_convertBuf(nullptr),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_timer(timer),
	m_fileInputMessageQueue(fileInputMessageQueue),
    m_samplerate(0),
	m_samplesize(0),
	m_samplebytes(0),
    m_throttlems(FILESOURCE_THROTTLE_MS),
    m_throttleToggle(false)
{
}

FileInputWorker::~FileInputWorker()
{
	if (m_running) {
		stopWork();
	}

	if (m_fileBuf) {
		free(m_fileBuf);
	}

	if (m_convertBuf) {
		free(m_convertBuf);
	}
}

void FileInputWorker::startWork()
{
	qDebug() << "FileInputThread::startWork: ";

#ifdef ANDROID
    if (m_ifstream->isOpen())
#else
    if (m_ifstream->is_open())
#endif
    {
        qDebug() << "FileInputThread::startWork: file stream open, starting...";
        m_elapsedTimer.start();
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
		m_running = true;
    }
    else
    {
        qDebug() << "FileInputThread::startWork: file stream closed, not starting.";
    }
}

void FileInputWorker::stopWork()
{
	qDebug() << "FileInputThread::stopWork";
	disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_running = false;
}

void FileInputWorker::setSampleRateAndSize(int samplerate, quint32 samplesize)
{
	qDebug() << "FileInputThread::setSampleRateAndSize:"
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
        m_chunksize = (m_samplerate * 2 * m_samplebytes * m_throttlems) / 1000;

        setBuffers(m_chunksize);
	}

	//m_samplerate = samplerate;
}

void FileInputWorker::setBuffers(std::size_t chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;
        int nbSamples = m_bufsize/(2 * m_samplebytes);

        if (!m_fileBuf)
        {
            qDebug() << "FileInputThread::setBuffers: Allocate file buffer";
            m_fileBuf = (quint8*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "FileInputThread::setBuffers: Re-allocate file buffer";
            quint8 *buf = m_fileBuf;
            m_fileBuf = (quint8*) realloc((void*) m_fileBuf, m_bufsize);

            if (!m_fileBuf) {
                free(buf);
            }
        }

        if (!m_convertBuf)
        {
            qDebug() << "FileInputThread::setBuffers: Allocate conversion buffer";
            m_convertBuf = (quint8*) malloc(nbSamples*sizeof(Sample));
        }
        else
        {
            qDebug() << "FileInputThread::setBuffers: Re-allocate conversion buffer";
            quint8 *buf = m_convertBuf;
            m_convertBuf = (quint8*) realloc((void*) m_convertBuf, nbSamples*sizeof(Sample));

            if (!m_convertBuf) {
                free(buf);
            }
        }

        qDebug() << "FileInputThread::setBuffers: size: " << m_bufsize
                << " #samples: " << nbSamples;
    }
}

void FileInputWorker::tick()
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


#ifdef ANDROID
		// read samples directly feeding the SampleFifo (no callback)
		qint64 bytesRead = m_ifstream->read(reinterpret_cast<char*>(m_fileBuf), m_chunksize);

        if (m_ifstream->atEnd())
        {
            writeToSampleFifo(m_fileBuf, (qint32) bytesRead);
            MsgReportEOF *message = MsgReportEOF::create();
            m_fileInputMessageQueue->push(message);
        }
        else
        {
            writeToSampleFifo(m_fileBuf, (qint32) m_chunksize);
            m_samplesCount += m_chunksize / (2 * m_samplebytes);
        }
#else
		// read samples directly feeding the SampleFifo (no callback)
		m_ifstream->read(reinterpret_cast<char*>(m_fileBuf), m_chunksize);

        if (m_ifstream->eof())
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_ifstream->gcount());
        	MsgReportEOF *message = MsgReportEOF::create();
        	m_fileInputMessageQueue->push(message);
        }
        else
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_chunksize);
    		m_samplesCount += m_chunksize / (2 * m_samplebytes);
        }
#endif
	}
}

void FileInputWorker::writeToSampleFifo(const quint8* buf, qint32 nbBytes)
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
