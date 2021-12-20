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

class RemoteDataFrame;
struct Sample;

class SDRBASE_API RemoteDataReadQueue
{
public:
    RemoteDataReadQueue();
    ~RemoteDataReadQueue();

    void push(RemoteDataFrame* dataFrame); //!< push frame on the queue
    void readSample(Sample& s, bool isTx); //!< Read sample from queue
    uint32_t length() const { return m_dataReadQueue.size(); } //!< Returns queue length
    uint32_t size() const { return m_maxSize; } //!< Returns queue size (max length)
    void setSize(uint32_t size);              //!< Sets the queue size (max length)
    uint32_t readSampleCount() const { return m_sampleCount; } //!< Returns the absolute number of samples read

    static const uint32_t MinimumMaxSize;

private:
    QQueue<RemoteDataFrame*> m_dataReadQueue;
    RemoteDataFrame *m_dataFrame;
    uint32_t m_maxSize;
    uint32_t m_blockIndex;
    uint32_t m_sampleIndex;
    uint32_t m_sampleCount; //!< use a counter capped below 2^31 as it is going to be converted to an int in the web interface

    RemoteDataFrame* pop();                //!< Pop frame from the queue

    inline void convertDataToSample(Sample& s, uint32_t blockIndex, uint32_t sampleIndex, bool isTx)
    {
        int sampleSize = m_dataFrame->m_superBlocks[blockIndex].m_header.m_sampleBytes * 2; // I/Q sample size in data block
        int32_t iconv, qconv;

        if (sizeof(Sample) == sampleSize) // no conversion
        {
            s = *((Sample*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]));
        }
        else if (isTx)
        {
            if (sampleSize == 2) // 8 -> 16 bits
            {
                int8_t *buf = (int8_t*)  m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf;
                iconv = buf[sampleIndex*sampleSize] * (1<<8);
                qconv = buf[sampleIndex*sampleSize+1]  * (1<<8);
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else if (sampleSize == 4) // just convert types (always 16 bits wide)
            {
                iconv = *((int16_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]));
                qconv = *((int16_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+2]));
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else if (sampleSize == 8) // just convert types (always 16 bits wide)
            {
                iconv = *((int32_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize]));
                qconv = *((int32_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+4]));
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else // invalid
            {
                s = Sample{0, 0};
            }
        }
        else
        {
            if ((sampleSize == 2) && (sizeof(Sample) == 2)) // 8 -> 16 bits
            {
                int8_t *buf = (int8_t*) m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf;
                iconv = buf[sampleIndex*sampleSize] * (1<<8);
                qconv = buf[sampleIndex*sampleSize+1] * (1<<8);
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else if ((sampleSize == 2) && (sizeof(Sample) == 4)) // 8 -> 24 bits
            {
                int8_t *buf = (int8_t*) m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf;
                iconv = buf[sampleIndex*sampleSize] * (1<<16);
                qconv = buf[sampleIndex*sampleSize+1] * (1<<16);
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else if (sampleSize == 4) // 16 -> 24 bits
            {
                iconv = *((int16_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize])) * (1<<8);
                qconv = *((int16_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+2])) * (1<<8);
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else if (sampleSize == 8) // 24 -> 16 bits
            {
                iconv = *((int32_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize])) / (1<<8);
                qconv = *((int32_t*) &(m_dataFrame->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*sampleSize+4])) / (1<<8);
                s.setReal(iconv);
                s.setImag(qconv);
            }
            else // invalid
            {
                s = Sample{0, 0};
            }
        }
    }
};



#endif /* CHANNEL_REMOTEDATAREADQUEUE_H_ */
