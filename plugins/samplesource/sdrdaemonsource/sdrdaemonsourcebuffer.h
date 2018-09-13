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

#ifndef PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCEBUFFER_H_
#define PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCEBUFFER_H_

#include <QString>
#include <QDebug>
#include <cstdlib>
#include "cm256.h"
#include "util/movingaverage.h"
#include "channel/sdrdaemondatablock.h"


#define SDRDAEMONSOURCE_UDPSIZE 512               // UDP payload size
#define SDRDAEMONSOURCE_NBORIGINALBLOCKS 128      // number of sample blocks per frame excluding FEC blocks
#define SDRDAEMONSOURCE_NBDECODERSLOTS 16         // power of two sub multiple of uint16_t size. A too large one is superfluous.

class SDRdaemonSourceBuffer
{
public:
	SDRdaemonSourceBuffer();
	~SDRdaemonSourceBuffer();

	// R/W operations
	void writeData(char *array); //!< Write data into buffer.
    void writeData0(char *array, uint32_t length); //!< Write data into buffer.
	uint8_t *readData(int32_t length);            //!< Read data from buffer

	// meta data
	const SDRDaemonMetaDataFEC& getCurrentMeta() const { return m_currentMeta; }

	// samples timestamp
	uint32_t getTVOutSec() const { return m_tvOut_sec; }
	uint32_t getTVOutUsec() const { return m_tvOut_usec; }

    // stats

	int getCurNbBlocks() const { return m_curNbBlocks; }
    int getCurOriginalBlocks() const { return m_curOriginalBlocks; }
    int getCurNbRecovery() const { return m_curNbRecovery; }
    float getAvgNbBlocks() const { return m_avgNbBlocks; }
    float getAvgOriginalBlocks() const { return m_avgOrigBlocks; }
    float getAvgNbRecovery() const { return m_avgNbRecovery; }

    int getMinNbBlocks()
    {
        int minNbBlocks = m_minNbBlocks;
        m_minNbBlocks = 256;
        return minNbBlocks;
    }

    int getMinOriginalBlocks()
    {
        int minOriginalBlocks = m_minOriginalBlocks;
        m_minOriginalBlocks = 128;
        return minOriginalBlocks;
    }

    int getMaxNbRecovery()
    {
        int maxNbRecovery = m_maxNbRecovery;
        m_maxNbRecovery = 0;
        return maxNbRecovery;
    }

    bool allFramesDecoded()
    {
        bool framesDecoded = m_framesDecoded;
        m_framesDecoded = true;
        return framesDecoded;
    }

    float getBufferLengthInSecs() const { return m_bufferLenSec; }
    int32_t getRWBalanceCorrection() const { return m_balCorrection; }

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
            return ret;
        }
        else
        {
            return 0; // default position
        }
    }

    static const int framesSize = SDRDAEMONSOURCE_NBDECODERSLOTS * (SDRDaemonNbOrginalBlocks - 1) * SDRDaemonNbBytesPerBlock;

private:
    static const int nbDecoderSlots = SDRDAEMONSOURCE_NBDECODERSLOTS;

#pragma pack(push, 1)
    struct BufferFrame
    {
        SDRDaemonProtectedBlock  m_blocks[SDRDaemonNbOrginalBlocks - 1];
    };
#pragma pack(pop)

    struct DecoderSlot
    {
        SDRDaemonProtectedBlock m_blockZero;                                       //!< First block of a frame. Has meta data.
        SDRDaemonProtectedBlock m_originalBlocks[SDRDaemonNbOrginalBlocks];        //!< Original blocks retrieved directly or by later FEC
        SDRDaemonProtectedBlock m_recoveryBlocks[SDRDaemonNbOrginalBlocks];        //!< Recovery blocks (FEC blocks) with max size
        CM256::cm256_block      m_cm256DescriptorBlocks[SDRDaemonNbOrginalBlocks]; //!< CM256 decoder descriptors (block addresses and block indexes)
        int                     m_blockCount;         //!< number of blocks received for this frame
        int                     m_originalCount;      //!< number of original blocks received
        int                     m_recoveryCount;      //!< number of recovery blocks received
        bool                    m_decoded;            //!< true if decoded
        bool                    m_metaRetrieved;      //!< true if meta data (block zero) was retrieved
    };

    SDRDaemonMetaDataFEC m_currentMeta;          //!< Stored current meta data
    CM256::cm256_encoder_params m_paramsCM256;          //!< CM256 decoder parameters block
    DecoderSlot          m_decoderSlots[nbDecoderSlots]; //!< CM256 decoding control/buffer slots
    BufferFrame          m_frames[nbDecoderSlots];       //!< Samples buffer
    int                  m_framesNbBytes;                //!< Number of bytes in samples buffer
    int                  m_decoderIndexHead;     //!< index of the current head frame slot in decoding slots
    int                  m_frameHead;            //!< index of the current head frame sent
    int                  m_curNbBlocks;          //!< (stats) instantaneous number of blocks received
    int                  m_minNbBlocks;          //!< (stats) minimum number of blocks received since last poll
    int                  m_curOriginalBlocks;    //!< (stats) instantanous number of original blocks received
    int                  m_minOriginalBlocks;    //!< (stats) minimum number of original blocks received since last poll
    int                  m_curNbRecovery;        //!< (stats) instantaneous number of recovery blocks used
    int                  m_maxNbRecovery;        //!< (stats) maximum number of recovery blocks used since last poll
    MovingAverageUtil<int, int, 10> m_avgNbBlocks;   //!< (stats) average number of blocks received
    MovingAverageUtil<int, int, 10> m_avgOrigBlocks; //!< (stats) average number of original blocks received
    MovingAverageUtil<int, int, 10> m_avgNbRecovery; //!< (stats) average number of recovery blocks used
    bool                 m_framesDecoded;        //!< [stats] true if all frames were decoded since last poll
    int                  m_readIndex;            //!< current byte read index in frames buffer
    int                  m_wrDeltaEstimate;      //!< Sampled estimate of write to read indexes difference
    uint32_t             m_tvOut_sec;            //!< Estimated returned samples timestamp (seconds)
    uint32_t             m_tvOut_usec;           //!< Estimated returned samples timestamp (microseconds)
    int                  m_readNbBytes;          //!< Nominal number of bytes per read (50ms)

    uint8_t* m_readBuffer;         //!< Read buffer to hold samples when looping back to beginning of raw buffer
    int      m_readSize;           //!< Read buffer size

    float    m_bufferLenSec;

    int      m_nbReads;       //!< Number of buffer reads since start of auto R/W balance correction period
    int      m_nbWrites;      //!< Number of buffer writes since start of auto R/W balance correction period
    int      m_balCorrection; //!< R/W balance correction in number of samples
    int      m_balCorrLimit;  //!< Correction absolute value limit in number of samples
    CM256    m_cm256;         //!< CM256 library
    bool     m_cm256_OK;      //!< CM256 library initialized OK

    inline SDRDaemonProtectedBlock* storeOriginalBlock(int slotIndex, int blockIndex, const SDRDaemonProtectedBlock& protectedBlock)
    {
        if (blockIndex == 0) {
            // m_decoderSlots[slotIndex].m_originalBlocks[0] = protectedBlock;
            // return &m_decoderSlots[slotIndex].m_originalBlocks[0];
            m_decoderSlots[slotIndex].m_blockZero = protectedBlock;
            return &m_decoderSlots[slotIndex].m_blockZero;
        } else {
            // m_decoderSlots[slotIndex].m_originalBlocks[blockIndex] = protectedBlock;
            // return &m_decoderSlots[slotIndex].m_originalBlocks[blockIndex];
            m_frames[slotIndex].m_blocks[blockIndex - 1] = protectedBlock;
            return &m_frames[slotIndex].m_blocks[blockIndex - 1];
        }
    }

    inline SDRDaemonProtectedBlock& getOriginalBlock(int slotIndex, int blockIndex)
    {
        if (blockIndex == 0) {
            // return m_decoderSlots[slotIndex].m_originalBlocks[0];
            return m_decoderSlots[slotIndex].m_blockZero;
        } else {
            // return m_decoderSlots[slotIndex].m_originalBlocks[blockIndex];
            return m_frames[slotIndex].m_blocks[blockIndex - 1];
        }
    }

    inline SDRDaemonMetaDataFEC *getMetaData(int slotIndex)
    {
        // return (MetaDataFEC *) &m_decoderSlots[slotIndex].m_originalBlocks[0];
        return (SDRDaemonMetaDataFEC *) &m_decoderSlots[slotIndex].m_blockZero;
    }

    inline void resetOriginalBlocks(int slotIndex)
    {
        // memset((void *) m_decoderSlots[slotIndex].m_originalBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
        memset((void *) &m_decoderSlots[slotIndex].m_blockZero, 0, sizeof(SDRDaemonProtectedBlock));
        memset((void *) m_frames[slotIndex].m_blocks, 0, (SDRDaemonNbOrginalBlocks - 1) * sizeof(SDRDaemonProtectedBlock));
    }

    void initDecodeAllSlots();
    void initReadIndex();
    void rwCorrectionEstimate(int slotIndex);
    void checkSlotData(int slotIndex);
    void initDecodeSlot(int slotIndex);

    static void printMeta(const QString& header, SDRDaemonMetaDataFEC *metaData);
};



#endif /* PLUGINS_SAMPLESOURCE_SDRDAEMONSOURCE_SDRDAEMONSOURCEBUFFER_H_ */
