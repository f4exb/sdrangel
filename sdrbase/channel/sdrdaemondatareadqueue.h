///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) data blocks to read queue                         //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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

#ifndef SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_
#define SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_

#include <QQueue>

class SDRDaemonDataBlock;
class Sample;

class SDRDaemonDataReadQueue
{
public:
    SDRDaemonDataReadQueue(uint32_t sampleSize);
    ~SDRDaemonDataReadQueue();

    void push(SDRDaemonDataBlock* dataBlock); //!< push block on the queue
    SDRDaemonDataBlock* pop();                //!< Pop block from the queue
    void readSample(Sample& s);               //!< Read sample from queue
    uint32_t length() const { return m_dataReadQueue.size(); } //!< Returns queue length
    uint32_t size() const { return m_maxSize; } //!< Returns queue size (max length)
    void setSize(uint32_t size);              //!< Sets the queue size (max length)
    uint32_t readSampleCount() const { return m_sampleCount; } //!< Returns the absolute number of samples read

    static const uint32_t MinimumMaxSize;

private:
    uint32_t m_sampleSize;
    QQueue<SDRDaemonDataBlock*> m_dataReadQueue;
    SDRDaemonDataBlock *m_dataBlock;
    uint32_t m_maxSize;
    uint32_t m_blockIndex;
    uint32_t m_sampleIndex;
    uint32_t m_sampleCount; //!< use a counter capped below 2^31 as it is going to be converted to an int in the web interface
    bool m_full; //!< full condition was hit

    inline void convertDataToSample(Sample& s, uint32_t blockIndex, uint32_t sampleIndex)
    {
        if (sizeof(Sample) == m_sampleSize)
        {
            s = *((Sample*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*m_sampleSize]));
        }
        else if ((sizeof(Sample) == 4) && (m_sampleSize == 8))
        {
            int32_t rp = *( (int32_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*m_sampleSize]) );
            int32_t ip = *( (int32_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*m_sampleSize+4]) );
            s.setReal(rp>>8);
            s.setImag(ip>>8);
        }
        else if ((sizeof(Sample) == 8) && (m_sampleSize == 4))
        {
            int32_t rp = *( (int16_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*m_sampleSize]) );
            int32_t ip = *( (int16_t*) &(m_dataBlock->m_superBlocks[blockIndex].m_protectedBlock.buf[sampleIndex*m_sampleSize+2]) );
                s.setReal(rp<<8);
                s.setImag(ip<<8);
        }
        else
        {
            s = Sample{0, 0};
        }
    }
};



#endif /* SDRDAEMON_CHANNEL_SDRDAEMONDATAREADQUEUE_H_ */
