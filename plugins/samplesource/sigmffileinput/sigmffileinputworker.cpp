///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "dsp/filerecord.h"
#include "dsp/samplesinkfifo.h"
#include "util/messagequeue.h"

#include "sigmffiledata.h"
#include "sigmffileconvert.h"
#include "sigmffileinputsettings.h"
#include "sigmffileinputworker.h"

MESSAGE_CLASS_DEFINITION(SigMFFileInputWorker::MsgReportEOF, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileInputWorker::MsgReportTrackChange, Message)

SigMFFileInputWorker::SigMFFileInputWorker(std::ifstream *samplesStream,
        SampleSinkFifo* sampleFifo,
        const QTimer& timer,
        MessageQueue *fileInputMessageQueue,
        QObject* parent) :
	QObject(parent),
	m_running(false),
    m_currentTrackIndex(0),
	m_ifstream(samplesStream),
	m_fileBuf(0),
	m_convertBuf(0),
	m_bufsize(0),
	m_chunksize(0),
	m_sampleFifo(sampleFifo),
	m_samplesCount(0),
	m_timer(timer),
	m_fileInputMessageQueue(fileInputMessageQueue),
    m_samplerate(48000),
    m_accelerationFactor(1),
	m_samplesize(16),
	m_samplebytes(2),
    m_throttlems(FILESOURCE_THROTTLE_MS),
    m_throttleToggle(false),
    m_sigMFConverter(nullptr)
{
}

SigMFFileInputWorker::~SigMFFileInputWorker()
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

void SigMFFileInputWorker::startWork()
{
	qDebug() << "SigMFFileInputWorker::startWork: ";

    if (m_ifstream->is_open())
    {
        qDebug() << "SigMFFileInputWorker::startWork: file stream open, starting...";
        m_elapsedTimer.start();
        connect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_running = true;
    }
    else
    {
        qDebug() << "SigMFFileInputWorker::startWork: file stream closed, not starting.";
    }
}

void SigMFFileInputWorker::stopWork()
{
	qDebug() << "SigMFFileInputWorker::stopWork";
	disconnect(&m_timer, SIGNAL(timeout()), this, SLOT(tick()));
	m_running = false;
}

void SigMFFileInputWorker::setMetaInformation(const SigMFFileMetaInfo *metaInfo, const QList<SigMFFileCapture> *captures)
{
    m_metaInfo = metaInfo;
    m_captures = captures;
    m_samplerate = m_metaInfo->m_coreSampleRate;
    m_samplesize = m_metaInfo->m_dataType.m_sampleBits;
    setConverter();
    setSampleRate();
}

void SigMFFileInputWorker::setTrackIndex(int trackIndex)
{
    m_currentTrackIndex = trackIndex;
    m_samplesCount = m_captures->at(m_currentTrackIndex).m_sampleStart;
    int sampleRate = m_captures->at(m_currentTrackIndex).m_sampleRate;

    if (sampleRate != m_samplerate)
    {
        m_samplerate = sampleRate;
        setSampleRate();
    }

    MsgReportTrackChange *message = MsgReportTrackChange::create(m_currentTrackIndex);
    m_fileInputMessageQueue->push(message);
}

void SigMFFileInputWorker::setAccelerationFactor(int accelerationFactor)
{
    m_accelerationFactor = accelerationFactor;
    setSampleRate();
}

void SigMFFileInputWorker::setSampleRate()
{
    bool running = m_running;

    if (running) {
        stopWork();
    }

    m_samplebytes = SigMFFileInputSettings::bitsToBytes(m_samplesize);
    m_chunksize = (m_accelerationFactor * m_samplerate * 2 * m_samplebytes * m_throttlems) / 1000;

    setBuffers(m_chunksize);

    if (running) {
        startWork();
    }
}

void SigMFFileInputWorker::setBuffers(std::size_t chunksize)
{
    if (chunksize > m_bufsize)
    {
        m_bufsize = chunksize;
        int nbSamples = m_bufsize/(2 * m_samplebytes);

        if (m_fileBuf == 0)
        {
            qDebug() << "FileInputThread::setBuffers: Allocate file buffer";
            m_fileBuf = (quint8*) malloc(m_bufsize);
        }
        else
        {
            qDebug() << "FileInputThread::setBuffers: Re-allocate file buffer";
            quint8 *buf = m_fileBuf;
            m_fileBuf = (quint8*) realloc((void*) m_fileBuf, m_bufsize);
            if (!m_fileBuf) free(buf);
        }

        if (m_convertBuf == 0)
        {
            qDebug() << "FileInputThread::setBuffers: Allocate conversion buffer";
            m_convertBuf = (quint8*) malloc(nbSamples*sizeof(Sample));
        }
        else
        {
            qDebug() << "FileInputThread::setBuffers: Re-allocate conversion buffer";
            quint8 *buf = m_convertBuf;
            m_convertBuf = (quint8*) realloc((void*) m_convertBuf, nbSamples*sizeof(Sample));
            if (!m_convertBuf) free(buf);
        }

        qDebug() << "FileInputThread::setBuffers: size: " << m_bufsize
                << " #samples: " << nbSamples;
    }
}

void SigMFFileInputWorker::tick()
{
	if (m_running)
	{
        qint64 throttlems = m_elapsedTimer.restart();

        if (throttlems != m_throttlems)
        {
            m_throttlems = throttlems;
            m_chunksize = 2 * m_samplebytes * ((m_samplerate * m_accelerationFactor * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000);
            m_throttleToggle = !m_throttleToggle;
            setBuffers(m_chunksize);
        }

		// read samples directly feeding the SampleFifo (no callback)

        if (m_samplesCount + m_chunksize > m_totalSamples) {
            m_ifstream->read(reinterpret_cast<char*>(m_fileBuf), m_totalSamples - m_samplesCount);
        } else {
    		m_ifstream->read(reinterpret_cast<char*>(m_fileBuf), m_chunksize);
        }

        if ((m_samplesCount + m_chunksize > m_totalSamples) || m_ifstream->eof())
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_ifstream->gcount()); // take what has been read
        	MsgReportEOF *message = MsgReportEOF::create();
        	m_fileInputMessageQueue->push(message);
        }
        else
        {
        	writeToSampleFifo(m_fileBuf, (qint32) m_chunksize);
    		m_samplesCount += m_chunksize / (2 * m_samplebytes);

            if ((m_currentTrackIndex + 1 < m_captures->size())
             && (m_samplesCount > m_captures->at(m_currentTrackIndex+1).m_sampleStart))
            {
                m_currentTrackIndex++;
                int sampleRate = m_captures->at(m_currentTrackIndex).m_sampleRate;

                if (sampleRate != m_samplerate)
                {
                    m_samplerate = sampleRate;
                    setSampleRate();
                }

                MsgReportTrackChange *message = MsgReportTrackChange::create(m_currentTrackIndex);
                m_fileInputMessageQueue->push(message);
            }
        }
	}
}

void SigMFFileInputWorker::setConverter()
{
    if (m_metaInfo->m_dataType.m_floatingPoint) // float
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_bigEndian)
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, true, true, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, true, true, false>();
                }
            }
            else
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, true, false, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, true, false, false>();
                }
            }
        }
        else
        {
            if (m_metaInfo->m_dataType.m_bigEndian) {
                m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, false, true, false>();
            } else {
                m_sigMFConverter = new SigMFConverter<float, SDR_RX_SAMP_SZ, 32, false, false, false>();
            }
        }
    }
    else if ((m_metaInfo->m_dataType.m_signed) && (m_samplesize == 8))  // i8
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_swapIQ) {
                m_sigMFConverter = new SigMFConverter<int8_t, SDR_RX_SAMP_SZ, 8, true, false, true>();
            } else {
                m_sigMFConverter = new SigMFConverter<int8_t, SDR_RX_SAMP_SZ, 8, true, false, false>();
            }
        }
        else
        {
            m_sigMFConverter = new SigMFConverter<int8_t, SDR_RX_SAMP_SZ, 8, false, false, false>();
        }
    }
    else if ((!m_metaInfo->m_dataType.m_signed) && (m_samplesize == 8))  // u8
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_swapIQ) {
                m_sigMFConverter = new SigMFConverter<uint8_t, SDR_RX_SAMP_SZ, 8, true, false, true>();
            } else {
                m_sigMFConverter = new SigMFConverter<uint8_t, SDR_RX_SAMP_SZ, 8, true, false, false>();
            }
        }
        else
        {
            m_sigMFConverter = new SigMFConverter<uint8_t, SDR_RX_SAMP_SZ, 8, false, false, false>();
        }
    }
    else if ((m_metaInfo->m_dataType.m_signed) && (m_samplesize == 16))  // i16
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_bigEndian)
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, true, true, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, true, true, false>();
                }
            }
            else
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, true, false, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, true, false, false>();
                }
            }
        }
        else
        {
            if (m_metaInfo->m_dataType.m_bigEndian) {
                m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, false, true, false>();
            } else {
                m_sigMFConverter = new SigMFConverter<int16_t, SDR_RX_SAMP_SZ, 16, false, false, false>();
            }
        }
    }
    else if ((!m_metaInfo->m_dataType.m_signed) && (m_samplesize == 16))  // u16
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_bigEndian)
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, true, true, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, true, true, false>();
                }
            }
            else
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, true, false, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, true, false, false>();
                }
            }
        }
        else
        {
            if (m_metaInfo->m_dataType.m_bigEndian) {
                m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, false, true, false>();
            } else {
                m_sigMFConverter = new SigMFConverter<uint16_t, SDR_RX_SAMP_SZ, 16, false, false, false>();
            }
        }
    }
    else if ((m_metaInfo->m_dataType.m_signed) && (m_samplesize == 24))  // i24 (SDRangel special)
    {
        m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 24, true, false, false>();
    }
    else if ((m_metaInfo->m_dataType.m_signed) && (m_samplesize == 32))  // i32
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_bigEndian)
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, true, true, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, true, true, false>();
                }
            }
            else
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, true, false, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, true, false, false>();
                }
            }
        }
        else
        {
            if (m_metaInfo->m_dataType.m_bigEndian) {
                m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, false, true, false>();
            } else {
                m_sigMFConverter = new SigMFConverter<int32_t, SDR_RX_SAMP_SZ, 32, false, false, false>();
            }
        }
    }
    else if ((!m_metaInfo->m_dataType.m_signed) && (m_samplesize == 32))  // u32
    {
        if (m_metaInfo->m_dataType.m_complex)
        {
            if (m_metaInfo->m_dataType.m_bigEndian)
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, true, true, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, true, true, false>();
                }
            }
            else
            {
                if (m_metaInfo->m_dataType.m_swapIQ) {
                    m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, true, false, true>();
                } else {
                    m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, true, false, false>();
                }
            }
        }
        else
        {
            if (m_metaInfo->m_dataType.m_bigEndian) {
                m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, false, true, false>();
            } else {
                m_sigMFConverter = new SigMFConverter<uint32_t, SDR_RX_SAMP_SZ, 32, false, false, false>();
            }
        }
    }
}

void SigMFFileInputWorker::writeToSampleFifo(const quint8* buf, qint32 nbBytes)
{
    if (!m_sigMFConverter)
    {
        qDebug("SigMFFileInputWorker::writeToSampleFifo: no converter - probably sample format is not supported");
        return;
    }

#if defined(__WINDOWS__) || (BYTE_ORDER == LITTLE_ENDIAN)
    if ((m_metaInfo->m_dataType.m_complex) && (!m_metaInfo->m_dataType.m_bigEndian) && (!m_metaInfo->m_dataType.m_swapIQ))
    {
        if ((m_samplesize == 16) && (SDR_RX_SAMP_SZ == 16))
        {
            m_sampleFifo->write(buf, nbBytes);
            return;
        }
        if ((m_samplesize == 24) && (SDR_RX_SAMP_SZ == 24))
        {
            m_sampleFifo->write(buf, nbBytes);
            return;
        }
    }
#endif
    int nbSamples = m_sigMFConverter->convert((FixReal *) m_convertBuf, buf, nbBytes);
    m_sampleFifo->write(m_convertBuf, nbSamples*sizeof(Sample));
}

void SigMFFileInputWorker::writeToSampleFifoBAK(const quint8* buf, qint32 nbBytes)
{
    if (m_metaInfo->m_dataType.m_floatingPoint) // FP assumes 32 bit floats (float) always
    {
        FixReal *convertBuf = (FixReal *) m_convertBuf;
        const float *fileBuf = (float *) buf;
        int nbSamples;

        if (m_metaInfo->m_dataType.m_complex)
        {
            nbSamples = nbBytes / (2 * m_samplebytes);
            for (int is = 0; is < nbSamples; is++)
            {
                convertBuf[2*is]   = fileBuf[2*is] * SDR_RX_SCALEF;
                convertBuf[2*is+1] = fileBuf[2*is+1] * SDR_RX_SCALEF;
            }
        }
        else
        {
            nbSamples = nbBytes / m_samplebytes;
            for (int is = 0; is < nbSamples; is++)
            {
                convertBuf[2*is]   = fileBuf[is] * SDR_RX_SCALEF;
                convertBuf[2*is+1] = 0;
            }
        }

        m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
    }
    else if (m_metaInfo->m_dataType.m_signed) // signed integers
    {
        if (m_samplesize == 8)
        {
            FixReal *convertBuf = (FixReal *) m_convertBuf;
            const int8_t *fileBuf = (int8_t *) buf;
            int nbSamples;

            if (m_metaInfo->m_dataType.m_complex)
            {
                nbSamples = nbBytes / (2 * m_samplebytes);
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[2*is];
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                    convertBuf[2*is+1] = fileBuf[2*is+1];
                    convertBuf[2*is+1] <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                }
            }
            else
            {
                nbSamples = nbBytes / m_samplebytes;
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[is];
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                    convertBuf[2*is+1] = 0;
                }
            }

            m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
        }
        else if (m_samplesize == 16)
        {
            if (SDR_RX_SAMP_SZ == 16)
            {
                if (m_metaInfo->m_dataType.m_complex)
                {
                    m_sampleFifo->write(buf, nbBytes);
                }
                else
                {
                    FixReal *convertBuf = (FixReal *) m_convertBuf;
                    const int16_t *fileBuf = (int16_t *) buf;
                    int nbSamples = nbBytes / m_samplebytes;

                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[2*is];
                        convertBuf[2*is+1] = 0;
                    }

                    m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
                }
            }
            else if (SDR_RX_SAMP_SZ == 24)
            {
                FixReal *convertBuf = (FixReal *) m_convertBuf;
                const int16_t *fileBuf = (int16_t *) buf;
                int nbSamples;

                if (m_metaInfo->m_dataType.m_complex)
                {
                    nbSamples = nbBytes / (2 * m_samplebytes);
                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[2*is] << 8;
                        convertBuf[2*is+1] = fileBuf[2*is+1] << 8;
                    }
                }
                else
                {
                    nbSamples = nbBytes / m_samplebytes;
                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[is] << 8;
                        convertBuf[2*is+1] = 0;
                    }
                }

                m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
            }
        }
        else if (m_samplesize == 24)
        {
            if (SDR_RX_SAMP_SZ == 24)
            {
                if (m_metaInfo->m_dataType.m_complex)
                {
                    m_sampleFifo->write(buf, nbBytes);
                }
                else
                {
                    FixReal *convertBuf = (FixReal *) m_convertBuf;
                    const int32_t *fileBuf = (int32_t *) buf;
                    int nbSamples = nbBytes / m_samplebytes;

                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[2*is];
                        convertBuf[2*is+1] = 0;
                    }

                    m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
                }
            }
            else if (SDR_RX_SAMP_SZ == 16)
            {
                FixReal *convertBuf = (FixReal *) m_convertBuf;
                const int32_t *fileBuf = (int32_t *) buf;
                int nbSamples;

                if (m_metaInfo->m_dataType.m_complex)
                {
                    nbSamples = nbBytes / (2 * m_samplebytes);
                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[2*is] >> 8;
                        convertBuf[2*is+1] = fileBuf[2*is+1] >> 8;
                    }
                }
                else
                {
                    nbSamples = nbBytes / m_samplebytes;
                    for (int is = 0; is < nbSamples; is++)
                    {
                        convertBuf[2*is]   = fileBuf[is] >> 8;
                        convertBuf[2*is+1] = 0;
                    }
                }

                m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
            }
        }
        if (m_samplesize == 32)
        {
            FixReal *convertBuf = (FixReal *) m_convertBuf;
            const int32_t *fileBuf = (int32_t *) buf;
            int nbSamples;

            if (m_metaInfo->m_dataType.m_complex)
            {
                nbSamples = nbBytes / (2 * m_samplebytes);
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[2*is] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16;
                    convertBuf[2*is+1] = fileBuf[2*is+1] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16;
                }
            }
            else
            {
                nbSamples = nbBytes / m_samplebytes;
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[is] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16;
                    convertBuf[2*is+1] = 0;
                }
            }

            m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
        }
    }
    else // unsigned integers
    {
        if (m_samplesize == 8)
        {
            FixReal *convertBuf = (FixReal *) m_convertBuf;
            const uint8_t *fileBuf = (uint8_t *) buf;
            int nbSamples;

            if (m_metaInfo->m_dataType.m_complex)
            {
                nbSamples = nbBytes / (2 * m_samplebytes);
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[2*is] - 128;
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                    convertBuf[2*is+1] = fileBuf[2*is+1] - 128;
                    convertBuf[2*is+1] <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                }
            }
            else
            {
                nbSamples = nbBytes / m_samplebytes;
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[is] - 128;
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 8 : 16;
                    convertBuf[2*is+1] = 0;
                }
            }

            m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
        }
        else if (m_samplesize == 16)
        {
            FixReal *convertBuf = (FixReal *) m_convertBuf;
            const uint16_t *fileBuf = (uint16_t *) buf;
            int nbSamples;

            if (m_metaInfo->m_dataType.m_complex)
            {
                nbSamples = nbBytes / (2 * m_samplebytes);
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[2*is] - 32768;
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 0 : 8;
                    convertBuf[2*is+1] = fileBuf[2*is+1] - 32768;
                    convertBuf[2*is+1] <<= (SDR_RX_SAMP_SZ == 16) ? 0 : 8;
                }
            }
            else
            {
                nbSamples = nbBytes / m_samplebytes;
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is]   = fileBuf[is] - 32768;
                    convertBuf[2*is]   <<= (SDR_RX_SAMP_SZ == 16) ? 0 : 8;
                    convertBuf[2*is+1] = 0;
                }
            }

            m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
        }
        else if (m_samplesize == 32)
        {
            FixReal *convertBuf = (FixReal *) m_convertBuf;
            const uint32_t *fileBuf = (uint32_t *) buf;
            int nbSamples;

            if (m_metaInfo->m_dataType.m_complex)
            {
                nbSamples = nbBytes / (2 * m_samplebytes);
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is] = (fileBuf[2*is] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16)
                        - ((SDR_RX_SAMP_SZ == 24) ? (1<<23) : (1<<15));
                    convertBuf[2*is+1] = (fileBuf[2*is+1] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16)
                        - ((SDR_RX_SAMP_SZ == 24) ? (1<<23) : (1<<15));;
                }
            }
            else
            {
                nbSamples = nbBytes / m_samplebytes;
                for (int is = 0; is < nbSamples; is++)
                {
                    convertBuf[2*is] = (fileBuf[is] >> (SDR_RX_SAMP_SZ == 24) ? 8 : 16)
                        - ((SDR_RX_SAMP_SZ == 24) ? (1<<23) : (1<<15));
                    convertBuf[2*is+1] = 0;
                }
            }

            m_sampleFifo->write((quint8*) convertBuf, nbSamples*sizeof(Sample));
        }
    }
}
