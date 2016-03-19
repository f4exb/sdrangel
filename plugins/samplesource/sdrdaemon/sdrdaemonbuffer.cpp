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

#include "sdrdaemonbuffer.h"

#include <QDebug>
#include <cassert>
#include <cstring>
#include <lz4.h>


const int SDRdaemonBuffer::m_udpPayloadSize = 512;
const int SDRdaemonBuffer::m_sampleSize = 2;
const int SDRdaemonBuffer::m_iqSampleSize = 2 * m_sampleSize;
const int SDRdaemonBuffer::m_rawBufferLengthSeconds = 8; // should be even

SDRdaemonBuffer::SDRdaemonBuffer(uint32_t throttlems) :
	m_throttlemsNominal(throttlems),
	m_rawSize(0),
	m_rawBuffer(0),
	m_sampleRateStream(0),
	m_sampleRate(0),
	m_sampleBytes(2),
	m_sampleBits(12),
	m_sync(false),
	m_syncLock(false),
	m_lz4(false),
	m_nbBlocks(0),
	m_bytesInBlock(0),
	m_dataCRC(0),
	m_inCount(0),
	m_lz4InCount(0),
	m_lz4InSize(0),
	m_lz4InBuffer(0),
	m_lz4OutBuffer(0),
	m_frameSize(0),
	m_nbLz4Decodes(0),
	m_nbLz4SuccessfulDecodes(0),
	m_nbLz4CRCOK(0),
	m_nbLastLz4SuccessfulDecodes(0),
	m_nbLastLz4CRCOK(0),
	m_writeIndex(0),
	m_readIndex(0),
	m_readSize(0),
	m_readBuffer(0),
    m_autoFollowRate(false),
    m_autoCorrBuffer(false),
    m_skewTest(false),
    m_skewCorrection(false),
    m_resetIndexes(false),
    m_readCount(0),
    m_writeCount(0),
    m_nbCycles(0),
    m_nbReads(0),
    m_balCorrection(0),
    m_balCorrLimit(0)
{
	m_currentMeta.init();
}

SDRdaemonBuffer::~SDRdaemonBuffer()
{
	if (m_rawBuffer) {
		delete[] m_rawBuffer;
	}

	if (m_lz4InBuffer) {
		delete[] m_lz4InBuffer;
	}

	if (m_lz4OutBuffer) {
		delete[] m_lz4OutBuffer;
	}

	if (m_readBuffer) {
		delete[] m_readBuffer;
	}
}

void SDRdaemonBuffer::updateBufferSize(uint32_t sampleRate)
{
	uint32_t rawSize = sampleRate * m_iqSampleSize * m_rawBufferLengthSeconds; // store worth of this seconds of samples at this sample rate

	if (rawSize != m_rawSize)
	{
		m_rawSize = rawSize;
        m_balCorrLimit = sampleRate / 50; // +/- 20 ms correction max per read

		if (m_rawBuffer) {
			delete[] m_rawBuffer;
		}

		m_rawBuffer = new uint8_t[m_rawSize];
        resetIndexes();

		qDebug() << "SDRdaemonBuffer::updateBufferSize:"
			<< " sampleRate: " << sampleRate
			<< " m_rawSize: " << m_rawSize;
	}
}

void SDRdaemonBuffer::updateLZ4Sizes(uint32_t frameSize)
{
	uint32_t maxInputSize = LZ4_compressBound(frameSize);

	if (m_lz4InBuffer) {
		delete[] m_lz4InBuffer;
	}

	m_lz4InBuffer = new uint8_t[maxInputSize];

	if (m_lz4OutBuffer) {
		delete[] m_lz4OutBuffer;
	}

	m_lz4OutBuffer = new uint8_t[frameSize];
}

void SDRdaemonBuffer::updateReadBufferSize(uint32_t length)
{
	if (m_readBuffer) {
		delete[] m_readBuffer;
	}

	m_readBuffer = new uint8_t[length];
}

bool SDRdaemonBuffer::readMeta(char *array, uint32_t length)
{
	assert(length >= sizeof(MetaData) + 8);
	MetaData *metaData = (MetaData *) array;

	if (m_crc64.calculate_crc((uint8_t *) array, sizeof(MetaData) - 8) == metaData->m_crc)
	{
		// sync condition:
		if (m_currentMeta.m_blockSize > 0)
		{
			uint32_t nbBlocks = m_currentMeta.m_nbBytes / m_currentMeta.m_blockSize;
			m_syncLock = nbBlocks + (m_lz4 ? 2 : 1) == m_nbBlocks;
			//qDebug("SDRdaemonBuffer::readMeta: m_nbBlocks: %d:%d %s", nbBlocks, m_nbBlocks, (m_syncLock ? "locked" : "unlocked"));
		}
		else
		{
			m_syncLock = false;
		}

		memcpy((void *) &m_dataCRC, (const void *) &array[sizeof(MetaData)], 8);
		m_nbBlocks = 0;
		m_inCount = 0;

		if (!m_lz4 && !(m_currentMeta == *metaData))
		{
			printMeta(QString("SDRdaemonBuffer::readMeta"), metaData);
		}

		m_currentMeta = *metaData;

		// sanity checks
		if (metaData->m_blockSize == m_udpPayloadSize) // sent blocksize matches given blocksize
		{
			m_sampleBytes = metaData->m_sampleBytes & 0x0F;
			uint32_t frameSize = m_iqSampleSize * metaData->m_nbSamples * metaData->m_nbBlocks;
			int sampleRate = metaData->m_sampleRate;

            if (sampleRate != m_sampleRateStream) // change of nominal stream sample rate
			{
				updateBufferSize(sampleRate);
				m_sampleRateStream = sampleRate;
				m_sampleRate = sampleRate;
			}

            // auto skew rate compensation
            if (m_autoFollowRate)
            {
				if (m_skewCorrection)
				{
					int64_t deltaRate = (m_writeCount - m_readCount) / (m_nbCycles * m_rawBufferLengthSeconds * m_iqSampleSize);
					m_sampleRate = ((m_sampleRate + deltaRate) / m_iqSampleSize) * m_iqSampleSize; // ensure it is a multiple of the I/Q sample size
					resetIndexes();
				}
            }
            else
            {
            	m_sampleRate = sampleRate;
            }

            // Reset indexes if requested
            if (m_resetIndexes)
            {
                resetIndexes();
                m_resetIndexes = false;
            }

			if (metaData->m_sampleBytes & 0x10)
			{
				m_lz4 = true;
				m_lz4InSize = metaData->m_nbBytes; // compressed input size
				m_lz4InCount = 0;

				if (frameSize != m_frameSize)
				{
					updateLZ4Sizes(frameSize);
				}
			}
			else
			{
				m_lz4 = false;
			}

			m_frameSize = frameSize;
			m_sync = true;
		}
		else
		{
			m_sync = false;
		}

		return m_sync;
	}
	else
	{
		return false;
	}
}

void SDRdaemonBuffer::writeData(char *array, uint32_t length)
{
	if ((m_sync) && (m_nbBlocks > 0))
	{
		if (m_lz4)
		{
			writeDataLZ4(array, length);
		}
		else
		{
			writeToRawBufferUncompressed(array, length);
		}
	}
}

uint8_t *SDRdaemonBuffer::readData(int32_t length)
{
    // auto compensation calculations
    if (m_skewTest && ((m_readIndex + length) > (m_rawSize / 2)))
    {
        // auto follow sample rate calculation
        int dIndex = (m_readIndex - m_writeIndex > 0 ? m_readIndex - m_writeIndex : m_writeIndex - m_readIndex); // absolute delta
        m_skewCorrection = (dIndex < m_rawSize / 10); // close by 10%
        m_nbCycles++;
        // auto R/W balance calculation
        if ((m_nbReads > 5*m_rawBufferLengthSeconds) && m_autoCorrBuffer)
        {
            int32_t dBytes;

            if (m_readIndex > m_writeIndex) { // write leads
                dBytes = m_writeIndex; // positive from start of buffer
            } else { // read leads
                dBytes = m_writeIndex - (int32_t) m_rawSize; // negative from end of buffer
            }

            m_balCorrection += dBytes / (int32_t) (m_nbReads * m_iqSampleSize); // correction is in number of samples
            int32_t corrLimit = (int32_t) m_rawSize / (int32_t) (10 * m_rawBufferLengthSeconds * m_iqSampleSize);

            if (m_balCorrection < -m_balCorrLimit) {
                m_balCorrection = -m_balCorrLimit;
            } else if (m_balCorrection > m_balCorrLimit) {
                m_balCorrection = m_balCorrLimit;
            }

            m_nbReads = 0;
        }
        else
        {
            m_balCorrection = 0;
        }
        // un-arm
        m_skewTest = false;
    }

    m_readCount += length;
    m_nbReads++;

	if (m_readIndex + length < m_rawSize)
	{
		uint32_t readIndex = m_readIndex;
		m_readIndex += length;
		return &m_rawBuffer[readIndex];
	}
	else if (m_readIndex + length == m_rawSize)
	{
		uint32_t readIndex = m_readIndex;
		m_readIndex = 0;
        m_skewTest = true; // re-arm
		return &m_rawBuffer[readIndex];
	}
	else
	{
		if (length > m_readSize)
		{
			updateReadBufferSize(length);
			m_readSize = length;
		}

		std::memcpy((void *) m_readBuffer, (const void *) &m_rawBuffer[m_readIndex], m_rawSize - m_readIndex);
		length -= m_rawSize - m_readIndex;
		std::memcpy((void *) &m_readBuffer[m_rawSize - m_readIndex], (const void *) m_rawBuffer, length);
		m_readIndex = length;
        m_skewTest = true; // re-arm
        return m_readBuffer;
	}
}

void SDRdaemonBuffer::writeDataLZ4(const char *array, uint32_t length)
{
    if (m_lz4InCount + length < m_lz4InSize)
    {
    	std::memcpy((void *) &m_lz4InBuffer[m_lz4InCount], (const void *) array, length);
        m_lz4InCount += length;
    }
    else
    {
        std::memcpy((void *) &m_lz4InBuffer[m_lz4InCount], (const void *) array, m_lz4InSize - m_lz4InCount); // copy rest of data in compressed Buffer
        m_lz4InCount += length;
    }

    if (m_lz4InCount >= m_lz4InSize) // full input compressed block retrieved
    {
        if (m_nbLz4Decodes == 100)
        {
            qDebug() << "SDRdaemonBuffer::writeAndReadLZ4:"
               << " decoding: " << m_nbLz4CRCOK
               << ":" << m_nbLz4SuccessfulDecodes
               << "/" <<  m_nbLz4Decodes;

            m_nbLastLz4SuccessfulDecodes = m_nbLz4SuccessfulDecodes;
            m_nbLastLz4CRCOK = m_nbLz4CRCOK;
        	m_nbLz4Decodes = 0;
        	m_nbLz4SuccessfulDecodes = 0;
            m_nbLz4CRCOK = 0;
        }

        writeToRawBufferLZ4();
		m_lz4InCount = 0;
    }
}

void SDRdaemonBuffer::writeToRawBufferLZ4()
{
    uint64_t crc64 = m_crc64.calculate_crc(m_lz4InBuffer, m_lz4InSize);

    if (memcmp(&crc64, &m_dataCRC, 8) == 0)
    {
        m_nbLz4CRCOK++;
    }
    else
    {
    	return;
    }

    int compressedSize = LZ4_decompress_fast((const char*) m_lz4InBuffer, (char*) m_lz4OutBuffer, m_frameSize);
    m_nbLz4Decodes++;

    if (compressedSize == m_lz4InSize)
	{
    	m_nbLz4SuccessfulDecodes++;
    	writeToRawBufferUncompressed((const char *) m_lz4OutBuffer, m_frameSize);
	}
}

void SDRdaemonBuffer::writeToRawBufferUncompressed(const char *array, uint32_t length)
{
	// TODO: handle the 1 byte per I or Q sample
	if (m_writeIndex + length < m_rawSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_writeIndex], (const void *) array, length);
		m_writeIndex += length;
	}
	else if (m_writeIndex + length == m_rawSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_writeIndex], (const void *) array, length);
		m_writeIndex = 0;
	}
	else
	{
		std::memcpy((void *) &m_rawBuffer[m_writeIndex], (const void *) array, m_rawSize - m_writeIndex);
		length -= m_rawSize - m_writeIndex;
		std::memcpy((void *) m_rawBuffer, (const void *) &array[m_rawSize - m_writeIndex], length);
		m_writeIndex = length;
	}

    m_writeCount += length;
}

void SDRdaemonBuffer::resetIndexes()
{
    m_writeIndex = 0;
    m_readIndex = m_rawSize / 2;
    m_readCount = 0;
    m_writeCount = 0;
    m_nbCycles = 0;
    m_skewTest = false;
    m_skewCorrection = false;
    m_nbReads = 0;
    m_balCorrection = 0;
}

void SDRdaemonBuffer::updateBlockCounts(uint32_t nbBytesReceived)
{
	m_nbBlocks += m_bytesInBlock + nbBytesReceived > m_udpPayloadSize ? 1 : 0;
	m_bytesInBlock = m_bytesInBlock + nbBytesReceived > m_udpPayloadSize ? nbBytesReceived : m_bytesInBlock + nbBytesReceived;
}

void SDRdaemonBuffer::printMeta(const QString& header, MetaData *metaData)
{
	qDebug() << header << ": "
		<< "|" << metaData->m_centerFrequency
		<< ":" << metaData->m_sampleRate
		<< ":" << (int) (metaData->m_sampleBytes & 0xF)
		<< ":" << (int) metaData->m_sampleBits
		<< ":" << metaData->m_blockSize
		<< ":" << metaData->m_nbSamples
		<< "||" << metaData->m_nbBlocks
		<< ":" << metaData->m_nbBytes
		<< "|" << metaData->m_tv_sec
		<< ":" << metaData->m_tv_usec;
}

