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

#ifndef PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONBUFFER_H_
#define PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONBUFFER_H_

#include <stdint.h>
#include <cstring>
#include <cstddef>
#include <lz4.h>
#include "util/CRC64.h"

class SDRdaemonBuffer
{
public:
#pragma pack(push, 1)
	struct MetaData
	{
        // critical data
		uint64_t m_centerFrequency;   //!< center frequency in Hz
		uint32_t m_sampleRate;        //!< sample rate in Hz
		uint8_t  m_sampleBytes;       //!< MSB(4): indicators, LSB(4) number of bytes per sample
		uint8_t  m_sampleBits;        //!< number of effective bits per sample
		uint16_t m_blockSize;         //!< payload size
		uint32_t m_nbSamples;         //!< number of samples in a hardware block
        // end of critical data
		uint16_t m_nbBlocks;          //!< number of hardware blocks in the frame
		uint32_t m_nbBytes;           //!< total number of bytes in the frame
		uint32_t m_tv_sec;            //!< seconds of timestamp at start time of frame processing
		uint32_t m_tv_usec;           //!< microseconds of timestamp at start time of frame processing
		uint64_t m_crc;               //!< 64 bit CRC of the above

		bool operator==(const MetaData& rhs)
		{
		    return (memcmp((const void *) this, (const void *) &rhs, 20) == 0); // Only the 20 first bytes are relevant (critical)
		}

		void init()
		{
			memset((void *) this, 0, sizeof(MetaData));
		}

		void operator=(const MetaData& rhs)
		{
			memcpy((void *) this, (const void *) &rhs, sizeof(MetaData));
		}
	};
#pragma pack(pop)

	SDRdaemonBuffer(std::size_t blockSize);
	~SDRdaemonBuffer();
	bool writeAndRead(uint8_t *array, std::size_t length, uint8_t *data, std::size_t& dataLength);
	const MetaData& getCurrentMeta() const { return m_currentMeta; }

private:
	bool writeAndReadLZ4(uint8_t *array, std::size_t length, uint8_t *data, std::size_t& dataLength);
	void updateSizes(MetaData *metaData);
    void printMeta(MetaData *metaData);

	std::size_t m_blockSize; //!< UDP block (payload) size
	bool m_sync;             //!< Meta data acquired (Stream synchronized)
	bool m_lz4;              //!< Stream is compressed with LZ4
	MetaData m_currentMeta;  //!< Stored current meta data
	CRC64 m_crc64;           //!< CRC64 calculator
	uint8_t *m_buf;          //!< UDP block buffer

    uint8_t *m_lz4InBuffer;           //!< Buffer for LZ4 compressed input
    uint32_t m_lz4InCount;            //!< Current position in LZ4 input buffer
    uint32_t m_lz4InSize;             //!< Size in bytes of the LZ4 input data
    uint8_t *m_lz4OutBuffer;          //!< Buffer for LZ4 uncompressed output
    uint32_t m_lz4OutSize;            //!< Size in bytes of the LZ4 output data (original uncomressed data)
    uint32_t m_nbDecodes;
    uint32_t m_nbSuccessfulDecodes;
    uint32_t m_nbCRCOK;
    uint64_t m_dataCRC;

};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONBUFFER_H_ */
