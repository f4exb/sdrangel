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

#ifndef PLUGINS_SAMPLESOURCE_SDRDAEMONFEC_SDRDAEMONFECBUFFER_H_
#define PLUGINS_SAMPLESOURCE_SDRDAEMONFEC_SDRDAEMONFECBUFFER_H_

#include <QString>
#include <QDebug>
#include <cstdlib>
#include "cm256.h"
#include "util/movingaverage.h"


#define SDRDAEMONFEC_UDPSIZE 512            // UDP payload size
#define SDRDAEMONFEC_NBORIGINALBLOCKS 128   // number of sample blocks per frame excluding FEC blocks
#define SDRDAEMONFEC_NBDECODERSLOTS 16      // power of two sub multiple of uint16_t size. A too large one is superfluous.

class SDRdaemonFECBuffer
{
public:
#pragma pack(push, 1)
    struct MetaDataFEC
    {
        uint32_t m_centerFrequency;   //!<  4 center frequency in kHz
        uint32_t m_sampleRate;        //!<  8 sample rate in Hz
        uint8_t  m_sampleBytes;       //!<  9 MSB(4): indicators, LSB(4) number of bytes per sample
        uint8_t  m_sampleBits;        //!< 10 number of effective bits per sample
        uint8_t  m_nbOriginalBlocks;  //!< 11 number of blocks with original (protected) data
        uint8_t  m_nbFECBlocks;       //!< 12 number of blocks carrying FEC
        uint32_t m_tv_sec;            //!< 16 seconds of timestamp at start time of super-frame processing
        uint32_t m_tv_usec;           //!< 20 microseconds of timestamp at start time of super-frame processing

        bool operator==(const MetaDataFEC& rhs)
        {
            return (memcmp((const void *) this, (const void *) &rhs, 12) == 0); // Only the 12 first bytes are relevant
        }

        void init()
        {
            memset((void *) this, 0, sizeof(MetaDataFEC));
        }
    };

    struct Sample
    {
        uint16_t i;
        uint16_t q;
    };

    struct Header
    {
        uint16_t frameIndex;
        uint8_t  blockIndex;
        uint8_t  filler;
    };

    static const int samplesPerBlock = (SDRDAEMONFEC_UDPSIZE - sizeof(Header)) / sizeof(Sample);
    static const int samplesPerBlockZero = samplesPerBlock - (sizeof(MetaDataFEC) / sizeof(Sample));

    struct ProtectedBlock
    {
        Sample samples[samplesPerBlock];
    };

    struct SuperBlock
    {
        Header         header;
        ProtectedBlock protectedBlock;
    };

    struct ProtectedBlockZero
    {
        MetaDataFEC m_metaData;
        Sample      m_samples[samplesPerBlockZero];
    };

    struct SuperBlockZero
    {
        Header             header;
        ProtectedBlockZero protectedBlock;
    };
#pragma pack(pop)

	SDRdaemonFECBuffer(uint32_t throttlems);
	~SDRdaemonFECBuffer();

	// R/W operations
	void writeData(char *array, uint32_t length); //!< Write data into buffer.
	uint8_t *readData(int32_t length);            //!< Read data from buffer

	// meta data
	const MetaDataFEC& getCurrentMeta() const { return m_currentMeta; }
    const MetaDataFEC& getOutputMeta() const { return m_outputMeta; }

    // stats
    int getCurNbBlocks() const { return m_curNbBlocks; }
    int getCurNbRecovery() const { return m_curNbRecovery; }
    float getAvgNbBlocks() const { return m_avgNbBlocks; }
    float getAvgNbRecovery() const { return m_avgNbRecovery; }


    float getBufferLengthInSecs() const { return m_bufferLenSec; }

    /** Get buffer gauge value in % of buffer size ([-50:50])
     *  [-50:0] : write leads or read lags
     *  [0:50]  : read leads or write lags
     */
    inline int32_t getBufferGauge() const
    {
        if (m_framesNbBytes)
        {
            int32_t val = (m_wrDeltaEstimate * 100) / (int32_t) m_framesNbBytes;
            int32_t ret = val < 0 ? -val - 50 : 50 -val;
            int32_t rp  = (m_readIndex * 100) / (int32_t) m_framesNbBytes;
            //qDebug() << "getBufferLengthInSecs: " << val << ":" << ret << ":" << rp;
            // conversion: [-100:-50[ : read leads (+) / [-50:0[ : read lags (-) / [0:50[ : read leads (+) / [50:100{ : read lags (-)
            return ret;
        }
        else
        {
            return 0; // default position
        }
    }

    static const int m_udpPayloadSize = SDRDAEMONFEC_UDPSIZE;
    static const int m_nbOriginalBlocks = SDRDAEMONFEC_NBORIGINALBLOCKS;
	static const int m_sampleSize;
	static const int m_iqSampleSize;

private:
    static const int nbDecoderSlots = SDRDAEMONFEC_NBDECODERSLOTS;

#pragma pack(push, 1)
    struct BufferBlockZero
    {
        Sample m_samples[samplesPerBlockZero];
    };

    struct BufferFrame
    {
        BufferBlockZero m_blockZero;
        ProtectedBlock  m_blocks[m_nbOriginalBlocks - 1];
    };
#pragma pack(pop)

    struct DecoderSlot
    {
        ProtectedBlockZero   m_blockZero;
        ProtectedBlock*      m_originalBlockPtrs[m_nbOriginalBlocks];
        ProtectedBlock       m_recoveryBlocks[m_nbOriginalBlocks]; // max size
        cm256_block          m_cm256DescriptorBlocks[m_nbOriginalBlocks];
        int                  m_blockCount; //!< total number of blocks received for this frame
        int                  m_recoveryCount; //!< number of recovery blocks received
        bool                 m_decoded; //!< true if decoded
        bool                 m_metaRetrieved;
    };

    MetaDataFEC m_currentMeta;                   //!< Stored current meta data
    MetaDataFEC m_outputMeta;                    //!< Meta data corresponding to currently served frame
    cm256_encoder_params m_paramsCM256;          //!< CM256 decoder parameters block
    DecoderSlot          m_decoderSlots[nbDecoderSlots]; //!< CM256 decoding control/buffer slots
    BufferFrame          m_frames[nbDecoderSlots];       //!< Samples buffer
    int                  m_framesNbBytes;                //!< Number of bytes in samples buffer
    int                  m_decoderSlotHead;      //!< index of the current head frame slot in decoding slots
    int                  m_frameHead;            //!< index of the current head frame sent
    int                  m_curNbBlocks;          //!< (stats) instantaneous number of blocks received
    int                  m_curNbRecovery;        //!< (stats) instantaneous number of recovery blocks used
    MovingAverage<int, int, 10> m_avgNbBlocks;   //!< (stats) average number of blocks received
    MovingAverage<int, int, 10> m_avgNbRecovery; //!< (stats) average number of recovery blocks used
    int                  m_readIndex;            //!< current byte read index in frames buffer
    int                  m_wrDeltaEstimate;      //!< Sampled estimate of write to read indexes difference

	uint32_t m_throttlemsNominal;  //!< Initial throttle in ms
    uint8_t* m_readBuffer;         //!< Read buffer to hold samples when looping back to beginning of raw buffer
    uint32_t m_readSize;           //!< Read buffer size

    float    m_bufferLenSec;

    void initDecoderSlotsAddresses();
    void initDecodeAllSlots();
    void initReadIndex();
    void initDecodeSlot(int slotIndex);

    static void printMeta(const QString& header, MetaDataFEC *metaData);
};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMONFEC_SDRDAEMONFECBUFFER_H_ */
