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

#include <QString>
#include <cstdlib>

#include "util/CRC64.h"

class SDRdaemonBuffer
{
public:
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

	SDRdaemonBuffer(uint32_t throttlems);
	~SDRdaemonBuffer();

	bool readMeta(char *array, uint32_t length);  //!< Attempt to read meta. Returns true if meta block
	void writeData(char *array, uint32_t length); //!< Write data into buffer.
	uint8_t *readData(int32_t length);
	void updateBlockCounts(uint32_t nbBytesReceived);

	const MetaData& getCurrentMeta() const { return m_currentMeta; }
	uint32_t getSampleRateStream() const { return m_sampleRateStream; }
	uint32_t getSampleRate() const { return m_sampleRate; }
	bool isSync() const { return m_sync; }
	bool isSyncLocked() const { return m_syncLock; }
	uint32_t getFrameSize() const { return m_frameSize; }
	bool isLz4Compressed() const { return m_lz4; }
	float getCompressionRatio() const { return (m_frameSize > 0 ? (float) m_lz4InSize / (float) m_frameSize : 1.0); }
	uint32_t getLz4DataCRCOK() const { return m_nbLastLz4CRCOK; }
	uint32_t getLz4SuccessfulDecodes() const { return m_nbLastLz4SuccessfulDecodes; }
	void setAutoFollowRate(bool autoFollowRate) { m_autoFollowRate = autoFollowRate; }
    void setAutoCorrBuffer(bool autoCorrBuffer) { m_autoCorrBuffer = autoCorrBuffer; }
    void setResetIndexes() { m_resetIndexes = true; }
    int32_t getRWBalanceCorrection() const { return m_balCorrection; }

    /** Get buffer gauge value in % of buffer size ([-50:50])
     *  [-50:0] : write leads or read lags
     *  [0:50]  : read leads or write lags
     */
    inline int32_t getBufferGauge() const
    {
        if (m_rawSize)
        {
            int32_t val = ((m_writeIndex - m_readIndex) * 100) / (int32_t) m_rawSize;

            if (val < -50) {
                return val + 100; // read leads (positive)
            } else if (val < 50) {
                return val;       // read leads (positive) or write leads (negative)
            } else {
                return val - 100; // write leads (negative)
            }
        }
        else
        {
            return -50; // default position
        }
    }

	static const int m_udpPayloadSize;
	static const int m_sampleSize;
	static const int m_iqSampleSize;
	static const int m_rawBufferLengthSeconds;

private:
	void updateBufferSize(uint32_t sampleRate);
	void updateLZ4Sizes(uint32_t frameSize);
	void updateReadBufferSize(uint32_t length);
	void writeDataLZ4(const char *array, uint32_t length);
	void writeToRawBufferLZ4();
	void writeToRawBufferUncompressed(const char *array, uint32_t length);
    void resetIndexes();

    static void printMeta(const QString& header, MetaData *metaData);

	uint32_t m_throttlemsNominal;  //!< Initial throttle in ms
	uint32_t m_rawSize;            //!< Size of the raw samples buffer in bytes
    uint8_t  *m_rawBuffer;         //!< Buffer for raw samples obtained from UDP (I/Q not in a formal I/Q structure)
    uint32_t m_sampleRateStream;   //!< Current sample rate from the stream meta data
    uint32_t m_sampleRate;         //!< Current actual sample rate in Hz
	uint8_t  m_sampleBytes;        //!< Current number of bytes per I or Q sample
	uint8_t  m_sampleBits;         //!< Current number of effective bits per sample

	bool m_sync;             //!< Meta data acquired
	bool m_syncLock;         //!< Meta data expected (Stream synchronized)
	bool m_lz4;              //!< Stream is compressed with LZ4
	MetaData m_currentMeta;  //!< Stored current meta data
	CRC64 m_crc64;           //!< CRC64 calculator
    uint32_t m_nbBlocks;     //!< Number of UDP blocks received in the current frame
    uint32_t m_bytesInBlock; //!< Number of bytes received in the current UDP block
    uint64_t m_dataCRC;      //!< CRC64 of the data block
	uint32_t m_inCount;      //!< Current position of uncompressed input
    uint32_t m_lz4InCount;   //!< Current position in LZ4 input buffer
    uint32_t m_lz4InSize;    //!< Size in bytes of the LZ4 input data
    uint8_t *m_lz4InBuffer;  //!< Buffer for LZ4 compressed input
    uint8_t *m_lz4OutBuffer; //!< Buffer for LZ4 uncompressed output
    uint32_t m_frameSize;    //!< Size in bytes of one uncompressed frame
    uint32_t m_nbLz4Decodes;
    uint32_t m_nbLz4SuccessfulDecodes;
    uint32_t m_nbLz4CRCOK;
    uint32_t m_nbLastLz4SuccessfulDecodes;
    uint32_t m_nbLastLz4CRCOK;

	int32_t  m_writeIndex;   //!< Current write position in the raw samples buffer
	int32_t  m_readIndex;    //!< Current read position in the raw samples buffer
	uint32_t m_readSize;     //!< Read buffer size
	uint8_t  *m_readBuffer;  //!< Read buffer to hold samples when looping back to beginning of raw buffer

    bool     m_autoFollowRate; //!< Auto follow stream sample rate else stick with meta data sample rate
    bool     m_autoCorrBuffer; //!< Auto correct buffer read / write balance (attempt to ...)
    bool     m_skewTest;
    bool     m_skewCorrection; //!< Do a skew rate correction at next meta data reception
    bool     m_resetIndexes;   //!< Do a reset indexes at next meta data reception

    int64_t  m_readCount;    //!< Number of bytes read for auto skew compensation
    int64_t  m_writeCount;   //!< Number of bytes written for auto skew compensation
    uint32_t m_nbCycles;     //!< Number of buffer cycles since start of auto skew compensation byte counting

    uint32_t m_nbReads;      //!< Number of buffer reads since start of auto R/W balance correction period
    int32_t  m_balCorrection; //!< R/W balance correction in number of samples
};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMON_SDRDAEMONBUFFER_H_ */
