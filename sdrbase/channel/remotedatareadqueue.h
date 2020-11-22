///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Remote sink channel (Rx) data blocks to read queue                            //
//                                                                               //
// SDRangel can serve as a remote SDR front end that handles the interface       //
// with a physical device and sends or receives the I/Q samples stream via UDP   //
// to or from another SDRangel instance or any program implementing the same     //
// protocol. The remote SDRangel is controlled via its Web REST API.             //
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

#ifndef CHANNEL_REMOTEDATAREADQUEUE_H_
#define CHANNEL_REMOTEDATAREADQUEUE_H_

#include <QQueue>

#include "export.h"

class RemoteDataBlock;
struct Sample;

class SDRBASE_API RemoteDataReadQueue
{
public:
    RemoteDataReadQueue();
    ~RemoteDataReadQueue();

    void push(RemoteDataBlock* dataBlock); //!< push block on the queue
    RemoteDataBlock* pop();                //!< Pop block from the queue
    void readSample(Sample& s, bool scaleForTx = false); //!< Read sample from queue possibly scaling to Tx size
    uint32_t length() const { return m_dataReadQueue.size(); } //!< Returns queue length
    uint32_t size() const { return m_maxSize; } //!< Returns queue size (max length)
    void setSize(uint32_t size);              //!< Sets the queue size (max length)
    uint32_t readSampleCount() const { return m_sampleCount; } //!< Returns the absolute number of samples read

    static const uint32_t MinimumMaxSize;

private:
    QQueue<RemoteDataBlock*> m_dataReadQueue;
    RemoteDataBlock *m_dataBlock;
    uint32_t m_maxSize;
    uint32_t m_blockIndex;
    uint32_t m_sampleIndex;
    uint32_t m_sampleCount; //!< use a counter capped below 2^31 as it is going to be converted to an int in the web interface
    bool m_full; //!< full condition was hit

    inline void convertDataToSample(Sample& s, uint32_t blockIndex, uint32_t sampleIndex, bool scaleForTx)
    {
        int sampleSize = m_dataBlock->m_superBlocks[blockIndex].m_header.m_sampleBytes * 2; // I/Q sample size in data block
        int samplebits = m_dataBlock->m_superBlocks[blockIndex].m_header.m_sampleBits;      // I or Q sample size in bits
        int32_t iconv, qconv;

        if ((sizeof(Sample) == 4) && (sampleSize == 8)) // generally 24->16 bits
        {
            iconv = ((int32_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]))[0];
            qconv = ((int32_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+4]))[0];
            iconv >>= scaleForTx ? (SDR_TX_SAMP_SZ-SDR_RX_SAMP_SZ) : (samplebits-SDR_RX_SAMP_SZ);
            qconv >>= scaleForTx ? (SDR_TX_SAMP_SZ-SDR_RX_SAMP_SZ) : (samplebits-SDR_RX_SAMP_SZ);
            s.setReal(iconv);
            s.setImag(qconv);
        }
        else if ((sizeof(Sample) == 8) && (sampleSize == 4)) // generally 16->24 bits
        {
            iconv = ((int16_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]))[0];
            qconv = ((int16_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+2]))[0];
            iconv <<= scaleForTx ? (SDR_TX_SAMP_SZ-samplebits) : (SDR_RX_SAMP_SZ-samplebits);
            qconv <<= scaleForTx ? (SDR_TX_SAMP_SZ-samplebits) : (SDR_RX_SAMP_SZ-samplebits);
            s.setReal(iconv);
            s.setImag(qconv);
        }
        else if ((sampleSize == 4) || (sampleSize == 8)) // generally 16->16 or 24->24 bits
        {
            s = *((Sample*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]));
        }
        else // invalid size
        {
            s = Sample{0, 0};
        }
    }
};



#endif /* CHANNEL_REMOTEDATAREADQUEUE_H_ */
