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
#include <iostream>
#include "sdrdaemonbuffer.h"

SDRdaemonBuffer::SDRdaemonBuffer(uint32_t blockSize) :
	m_blockSize(blockSize),
	m_sync(false),
	m_lz4(false),
	m_inCount(0),
	m_lz4InBuffer(0),
	m_lz4InCount(0),
	m_lz4InSize(0),
	m_lz4OutBuffer(0),
	m_frameSize(0),
	m_nbDecodes(0),
	m_nbSuccessfulDecodes(0),
	m_nbCRCOK(0),
	m_dataCRC(0),
	m_sampleRate(1000000),
	m_sampleBytes(2),
	m_sampleBits(12),
	m_rawCount(0),
	m_readCount(0),
	m_rawSize(0),
	m_rawBuffer(0),
	m_frameBuffer(0),
	m_bytesInBlock(0),
	m_nbBlocks(0)
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

	if (m_frameBuffer) {
		delete[] m_frameBuffer;
	}
}

bool SDRdaemonBuffer::readMeta(char *array, uint32_t length)
{
	assert(length >= sizeof(MetaData) + 8);
	MetaData *metaData = (MetaData *) array;

	if (m_crc64.calculate_crc((uint8_t *)array, sizeof(MetaData) - 8) == metaData->m_crc)
	{
		memcpy((void *) &m_dataCRC, (const void *) &array[sizeof(MetaData)], 8);
		m_nbBlocks = 0;
		m_inCount = 0;

		if (!(m_currentMeta == *metaData))
		{
			std::cerr << "SDRdaemonBuffer::readMeta: ";
			printMeta(metaData);
		}

		m_currentMeta = *metaData;

		// sanity checks
		if (metaData->m_blockSize == m_blockSize) // sent blocksize matches given blocksize
		{
			m_sampleBytes = metaData->m_sampleBytes & 0x0F;
			uint32_t frameSize = 2 * 2 * metaData->m_nbSamples * metaData->m_nbBlocks;
			uint32_t sampleRate = metaData->m_sampleRate;

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

			if (frameSize != m_frameSize)
			{
				updateBufferSize(sampleRate, frameSize);
			}

			m_sampleRate = sampleRate;
			m_frameSize = frameSize;
			m_sync = true;
		}
		else
		{
			m_sync = false;
		}
	}

	return m_sync;
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
			writeDataUncompressed(array, length);
		}
	}
}

void SDRdaemonBuffer::writeDataUncompressed(const char *array, uint32_t length)
{
	if (m_inCount + length < m_frameSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_rawCount], (const void *) array, length);
		m_inCount += length;
	}
	else if (m_inCount < m_frameSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_rawCount], (const void *) array, m_frameSize - m_inCount); // copy rest of data in compressed Buffer
		m_inCount += m_frameSize - m_inCount;
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
        if (m_nbDecodes == 100)
        {
            std::cerr << "SDRdaemonBuffer::writeAndReadLZ4:"
               << " decoding: " << m_nbCRCOK
               << ":" << m_nbSuccessfulDecodes
               << "/" <<  m_nbDecodes
               << std::endl;

        	m_nbDecodes = 0;
        	m_nbSuccessfulDecodes = 0;
            m_nbCRCOK = 0;
        }

        uint64_t crc64 = m_crc64.calculate_crc(m_lz4InBuffer, m_lz4InSize);
        //uint64_t crc64 = 0x0123456789ABCDEF;

        if (memcmp(&crc64, &m_dataCRC, 8) == 0)
        {
            m_nbCRCOK++;
        }

        int compressedSize = LZ4_decompress_fast((const char*) m_lz4InBuffer, (char*) &m_rawBuffer[m_rawCount], m_frameSize);
        m_nbDecodes++;

        if (compressedSize == m_lz4InSize)
    	{
        	m_nbSuccessfulDecodes++;
    	}

        writeToRawBufferLZ4((const char *) m_lz4InBuffer, m_frameSize);

		m_lz4InCount = 0;
    }
}

void SDRdaemonBuffer::writeToRawBufferUncompressed(const char *array, uint32_t length)
{
	// TODO: handle the 1 byte per I or Q sample
	if (m_rawCount + length < m_rawSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_rawCount], (const void *) array, length);
		m_rawCount += length;
	}
	else
	{
		std::memcpy((void *) &m_rawBuffer[m_rawCount], (const void *) array, m_rawSize - m_rawCount);
		m_rawCount = length - (m_rawSize - m_rawCount);
		std::memcpy((void *) m_rawBuffer, (const void *) array, m_rawCount);
	}
}

void SDRdaemonBuffer::writeToRawBufferLZ4(const char *array, uint32_t length)
{
    int compressedSize = LZ4_decompress_fast((const char*) m_lz4InBuffer, (char*) m_lz4OutBuffer, m_frameSize);
    m_nbDecodes++;

    if (compressedSize == m_lz4InSize)
	{
    	m_nbSuccessfulDecodes++;
	}

    writeToRawBufferUncompressed((const char *) m_lz4OutBuffer, m_frameSize);
}

uint8_t *SDRdaemonBuffer::readData(uint32_t length)
{
	uint32_t readCount = m_readCount;

	if (m_readCount + length < m_rawSize)
	{
		return &m_rawBuffer[readCount];
		m_readCount += length;
	}
	else
	{
		std::memcpy((void *) m_frameBuffer, (const void *) &m_rawBuffer[readCount], m_rawSize - m_readCount); // read last bit from raw buffer
		m_readCount = length - (m_rawSize - m_readCount);
		std::memcpy((void *) &m_frameBuffer[m_rawSize - m_readCount], (const void *) m_frameBuffer, m_readCount); // read the rest at start of raw buffer
		return m_frameBuffer;
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

void SDRdaemonBuffer::updateBufferSize(uint32_t sampleRate, uint32_t frameSize)
{
	uint32_t nbFrames = ((sampleRate * 2 * 2) / frameSize) + 1; // store at least 1 second of samples

	std::cerr << "SDRdaemonBuffer::updateBufferSize:"
		<< " sampleRate: " << sampleRate
		<< " frameSize: " << frameSize
		<< " nbFrames: " << nbFrames
		<< std::endl;

	if (m_rawBuffer) {
		delete[] m_rawBuffer;
	}

	m_rawSize = nbFrames * frameSize;
	m_rawBuffer = new uint8_t[m_rawSize];

	if (m_frameBuffer) {
		delete[] m_frameBuffer;
	}

	m_frameBuffer = new uint8_t[frameSize];
}

void SDRdaemonBuffer::updateBlockCounts(uint32_t nbBytesReceived)
{
	m_nbBlocks += m_bytesInBlock + nbBytesReceived > m_blockSize ? 1 : 0;
	m_bytesInBlock = m_bytesInBlock + nbBytesReceived > m_blockSize ? nbBytesReceived : m_bytesInBlock + nbBytesReceived;
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

