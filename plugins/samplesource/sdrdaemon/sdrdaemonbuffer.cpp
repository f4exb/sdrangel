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

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include "sdrdaemonbuffer.h"

const int SDRdaemonBuffer::m_udpPayloadSize = 512;
const int SDRdaemonBuffer::m_sampleSize = 2;
const int SDRdaemonBuffer::m_iqSampleSize = 2 * m_sampleSize;

SDRdaemonBuffer::SDRdaemonBuffer(uint32_t rateDivider) :
	m_rateDivider(rateDivider),
	m_sync(false),
	m_syncLock(false),
	m_lz4(false),
	m_inCount(0),
	m_lz4InBuffer(0),
	m_lz4InCount(0),
	m_lz4InSize(0),
	m_lz4OutBuffer(0),
	m_frameSize(0),
	m_nbLz4Decodes(0),
	m_nbLz4SuccessfulDecodes(0),
	m_nbLz4CRCOK(0),
	m_nbLastLz4SuccessfulDecodes(0),
	m_nbLastLz4CRCOK(0),
	m_dataCRC(0),
	m_sampleRateStream(0),
	m_sampleRate(0),
	m_sampleBytes(2),
	m_sampleBits(12),
	m_writeIndex(0),
	m_readChunkIndex(0),
	m_rawSize(0),
	m_rawBuffer(0),
	m_chunkSize(0),
	m_bytesInBlock(0),
	m_nbBlocks(0),
	m_readCycles(0),
	m_lastWriteIndex(0),
	m_skewRateSum(0.0),
	m_skewRate(0.0),
	m_autoFollowRate(false)
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
}

bool SDRdaemonBuffer::readMeta(char *array, uint32_t length)
{
	assert(length >= sizeof(MetaData) + 8);
	MetaData *metaData = (MetaData *) array;

	if (m_crc64.calculate_crc((uint8_t *)array, sizeof(MetaData) - 8) == metaData->m_crc)
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
			std::cerr << "SDRdaemonBuffer::readMeta: ";
			printMeta(metaData);
		}

		m_currentMeta = *metaData;

		// sanity checks
		if (metaData->m_blockSize == m_udpPayloadSize) // sent blocksize matches given blocksize
		{
			m_sampleBytes = metaData->m_sampleBytes & 0x0F;
			uint32_t frameSize = m_iqSampleSize * metaData->m_nbSamples * metaData->m_nbBlocks;
			int sampleRate = metaData->m_sampleRate;

			if (m_autoFollowRate)
			{
				if (sampleRate != m_sampleRateStream)
				{
					m_sampleRateStream = sampleRate;
				}
				else
				{
					sampleRate = m_sampleRate;
				}

				sampleRate += sampleRate * m_skewRate;
				sampleRate = (sampleRate / m_rateDivider) * m_rateDivider;
			}
			else
			{
				m_sampleRateStream = sampleRate;
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

			if (sampleRate != m_sampleRate)
			{
				updateBufferSize(sampleRate);
			}

			m_sampleRate = sampleRate;
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
            std::cerr << "SDRdaemonBuffer::writeAndReadLZ4:"
               << " decoding: " << m_nbLz4CRCOK
               << ":" << m_nbLz4SuccessfulDecodes
               << "/" <<  m_nbLz4Decodes
               << std::endl;

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

void SDRdaemonBuffer::writeToRawBufferUncompressed(const char *array, uint32_t length)
{
	// TODO: handle the 1 byte per I or Q sample
	if (m_writeIndex + length < m_rawSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_writeIndex], (const void *) array, length);
		m_writeIndex += length;
	}
	else
	{
		std::memcpy((void *) &m_rawBuffer[m_writeIndex], (const void *) array, m_rawSize - m_writeIndex);
		length -= m_rawSize - m_writeIndex;
		std::memcpy((void *) m_rawBuffer, (const void *) &array[m_rawSize - m_writeIndex], length);
		m_writeIndex = length;
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

uint8_t *SDRdaemonBuffer::readDataChunk()
{
	// relies on the fact that we always have an integer number of chunks in the raw buffer
	if (m_readChunkIndex == m_rateDivider * 2) // go back to start of raw buffer
	{
		double oneCycleSkew = 0;

		if (m_readCycles > 0)
		{
			oneCycleSkew = (double) ((int) m_writeIndex - (int) m_lastWriteIndex) / (double) m_rawSize;
			m_skewRateSum += oneCycleSkew;
		}

		//qDebug("SDRdaemonBuffer::readDataChunk: %d / %d (%lf)", m_writeIndex, m_rawSize, oneCycleSkew);

		if (!m_autoFollowRate)
		{
			m_skewRate = 0.0;
		}
		else if (m_readCycles && ((m_writeIndex < m_rawSize / 10) || (m_rawSize - m_writeIndex < m_rawSize / 10)))
		{
			m_skewRate = m_skewRateSum / m_readCycles;
			if (m_skewRate > 0.2) {
				m_skewRate = 0.2;
			} else if (m_skewRate < -0.2) {
				m_skewRate = -0.2;
			}
			qDebug("SDRdaemonBuffer::readDataChunk: m_skewRate: %lf", m_skewRate);
		}

		m_readChunkIndex = 0; // go to start
		m_lastWriteIndex = m_writeIndex;
		m_readCycles++;
	}

	uint32_t readIndex = m_readChunkIndex;
	m_readChunkIndex++;
	return &m_rawBuffer[readIndex * m_chunkSize];
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

void SDRdaemonBuffer::updateBufferSize(uint32_t sampleRate)
{
	assert(sampleRate % m_rateDivider == 0); // make sure we get an integer number of samples in a chunk

	// Store 2 seconds long of samples so we have two one second long half buffers
	m_chunkSize = (sampleRate * m_iqSampleSize) / m_rateDivider;
	m_rawSize = m_chunkSize * m_rateDivider * 2;

	if (m_rawBuffer) {
		delete[] m_rawBuffer;
	}

	m_rawBuffer = new uint8_t[m_rawSize];

	m_writeIndex = 0;
	m_readChunkIndex = m_rateDivider;
	m_readCycles = 0;
	m_skewRateSum = 0;
	m_skewRate = 0;

	std::cerr << "SDRdaemonBuffer::updateBufferSize:"
		<< " sampleRate: " << sampleRate
		<< " m_chunkSize: " << m_chunkSize
		<< " m_rawSize: " << m_rawSize
		<< std::endl;
}

void SDRdaemonBuffer::updateBlockCounts(uint32_t nbBytesReceived)
{
	m_nbBlocks += m_bytesInBlock + nbBytesReceived > m_udpPayloadSize ? 1 : 0;
	m_bytesInBlock = m_bytesInBlock + nbBytesReceived > m_udpPayloadSize ? nbBytesReceived : m_bytesInBlock + nbBytesReceived;
}

void SDRdaemonBuffer::printMeta(MetaData *metaData)
{
	std::cerr
			<< "|" << metaData->m_centerFrequency
			<< ":" << metaData->m_sampleRate
			<< ":" << (int) (metaData->m_sampleBytes & 0xF)
			<< ":" << (int) metaData->m_sampleBits
			<< ":" << metaData->m_blockSize
			<< ":" << metaData->m_nbSamples
			<< "||" << metaData->m_nbBlocks
			<< ":" << metaData->m_nbBytes
	        << "|" << metaData->m_tv_sec
			<< ":" << metaData->m_tv_usec
			<< std::endl;
}

