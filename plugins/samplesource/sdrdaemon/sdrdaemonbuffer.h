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
#include "dsp/samplefifo.h"

class SDRdaemonBuffer
{
public:
#pragma pack(push, 1)
	struct MetaData
	{
        // critical data
		uint32_t m_centerFrequency;   //!< center frequency in kHz
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

	SDRdaemonBuffer(uint32_t rateDivider);
	~SDRdaemonBuffer();
	bool readMeta(char *array, uint32_t length);  //!< Attempt to read meta. Returns true if meta block
	void writeData(char *array, uint32_t length); //!< Write data into buffer.
	uint8_t *readDataChunk();                     //!< Read a chunk of data from buffer
	const MetaData& getCurrentMeta() const { return m_currentMeta; }
	void updateBlockCounts(uint32_t nbBytesReceived);
	bool isSync() const { return m_sync; }

	static const int m_udpPayloadSize;
	static const int m_sampleSize;
	static const int m_iqSampleSize;

private:
	void updateLZ4Sizes(uint32_t frameSize);
	void writeDataLZ4(const char *array, uint32_t length);
	void writeToRawBufferLZ4(const char *array, uint32_t originalLength);
	void writeToRawBufferUncompressed(const char *array, uint32_t length);
	void updateBufferSize(uint32_t sampleRate);
    void printMeta(MetaData *metaData);

	uint32_t m_rateDivider;  //!< Number of times per seconds the samples are fetched
	bool m_sync;             //!< Meta data acquired (Stream synchronized)
	bool m_lz4;              //!< Stream is compressed with LZ4
	MetaData m_currentMeta;  //!< Stored current meta data
	CRC64 m_crc64;           //!< CRC64 calculator

	uint32_t m_inCount;      //!< Current position of uncompressed input
    uint8_t *m_lz4InBuffer;  //!< Buffer for LZ4 compressed input
    uint32_t m_lz4InCount;   //!< Current position in LZ4 input buffer
    uint32_t m_lz4InSize;    //!< Size in bytes of the LZ4 input data
    uint8_t *m_lz4OutBuffer; //!< Buffer for LZ4 uncompressed output
    uint32_t m_frameSize;    //!< Size in bytes of one uncompressed frame
    uint32_t m_nbDecodes;
    uint32_t m_nbSuccessfulDecodes;
    uint32_t m_nbCRCOK;
    uint64_t m_dataCRC;

    uint32_t m_sampleRate;   //!< Current sample rate in Hz
	uint8_t  m_sampleBytes;  //!< Current number of bytes per I or Q sample
	uint8_t  m_sampleBits;   //!< Current number of effective bits per sample

	uint32_t m_writeIndex;   //!< Current write position in the raw samples buffer
	uint32_t m_readChunkIndex; //!< Current read chunk index in the raw samples buffer
	uint32_t m_rawSize;      //!< Size of the raw samples buffer in bytes
    uint8_t *m_rawBuffer;    //!< Buffer for raw samples obtained from UDP (I/Q not in a formal I/Q structure)
    uint32_t m_chunkSize;    //!< Size of a chunk of samples in bytes
    uint32_t m_bytesInBlock; //!< Number of bytes received in the current UDP block
    uint32_t m_nbBlocks;     //!< Number of UDP blocks received in the current frame
};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONBUFFER_H_ */
