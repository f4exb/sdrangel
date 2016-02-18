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

SDRdaemonBuffer::SDRdaemonBuffer(uint32_t blockSize, uint32_t rateDivider) :
	m_blockSize(blockSize),
	m_rateDivider(rateDivider),
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
	m_writeCount(0),
	m_readCount(0),
	m_rawSize(0),
	m_rawBuffer(0),
	m_chunkBuffer(0),
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

	if (m_chunkBuffer) {
		delete[] m_chunkBuffer;
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

        int compressedSize = LZ4_decompress_fast((const char*) m_lz4InBuffer, (char*) &m_rawBuffer[m_writeCount], m_frameSize);
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
	if (m_writeCount + length < m_rawSize)
	{
		std::memcpy((void *) &m_rawBuffer[m_writeCount], (const void *) array, length);
		m_writeCount += length;
	}
	else
	{
		std::memcpy((void *) &m_rawBuffer[m_writeCount], (const void *) array, m_rawSize - m_writeCount);
		m_writeCount = length - (m_rawSize - m_writeCount);
		std::memcpy((void *) m_rawBuffer, (const void *) &array[m_rawSize - m_writeCount], m_writeCount);
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
	if (m_readCount + length < m_rawSize)
	{
		uint32_t readCount = m_readCount;
		m_readCount += length;
		return &m_rawBuffer[readCount];
	}
	else
	{
		uint32_t retLength = std::min(length, m_chunkSize);
		std::memcpy((void *) m_chunkBuffer, (const void *) &m_rawBuffer[m_readCount], m_rawSize - m_readCount); // read last bit from raw buffer
		m_readCount = retLength - (m_rawSize - m_readCount);
		std::memcpy((void *) &m_chunkBuffer[m_rawSize - m_readCount], (const void *) m_rawBuffer, m_readCount); // read the rest at start of raw buffer
		return m_chunkBuffer;
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

	if (m_chunkBuffer) {
		delete[] m_chunkBuffer;
	}

	m_chunkSize = (sampleRate * 2 * 2) / m_rateDivider;
	m_chunkBuffer = new uint8_t[m_chunkSize];
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

