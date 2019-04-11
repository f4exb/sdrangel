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

#include <channel/remotedatablock.h>
#include <channel/remotedatareadqueue.h>

const uint32_t RemoteDataReadQueue::MinimumMaxSize = 10;

RemoteDataReadQueue::RemoteDataReadQueue() :
        m_dataBlock(0),
        m_maxSize(MinimumMaxSize),
        m_blockIndex(1),
        m_sampleIndex(0),
        m_sampleCount(0),
        m_full(false)
{}

RemoteDataReadQueue::~RemoteDataReadQueue()
{
    RemoteDataBlock* data;

    while ((data = pop()) != 0)
    {
        qDebug("RemoteDataReadQueue::~RemoteDataReadQueue: data block was still in queue");
        delete data;
    }
}

void RemoteDataReadQueue::push(RemoteDataBlock* dataBlock)
{
    if (length() >= m_maxSize)
    {
        qWarning("RemoteDataReadQueue::push: queue is full");
        m_full = true; // stop filling the queue
        RemoteDataBlock *data = m_dataReadQueue.takeLast();
        delete data;
    }

    if (m_full) {
        m_full = (length() > m_maxSize/2); // do not fill queue again before queue is half size
    }

    if (!m_full) {
        m_dataReadQueue.append(dataBlock);
    }
}

RemoteDataBlock* RemoteDataReadQueue::pop()
{
    if (m_dataReadQueue.isEmpty())
    {
        return 0;
    }
    else
    {
        m_blockIndex = 1;
        m_sampleIndex = 0;

        return m_dataReadQueue.takeFirst();
    }
}

void RemoteDataReadQueue::setSize(uint32_t size)
{
    if (size != m_maxSize) {
        m_maxSize = size < MinimumMaxSize ? MinimumMaxSize : size;
    }
}

void RemoteDataReadQueue::readSample(Sample& s, bool scaleForTx)
{
    // depletion/repletion state
    if (m_dataBlock == 0)
    {
        if (length() >= m_maxSize/2)
        {
            qDebug("RemoteDataReadQueue::readSample: initial pop new block: queue size: %u", length());
            m_blockIndex = 1;
            m_dataBlock = m_dataReadQueue.takeFirst();
            convertDataToSample(s, m_blockIndex, m_sampleIndex, scaleForTx);
            m_sampleIndex++;
            m_sampleCount++;
        }
        else
        {
            s = Sample{0, 0};
        }

        return;
    }

    int sampleSize = m_dataBlock->m_superBlocks[m_blockIndex].m_header.m_sampleBytes * 2;
    uint32_t samplesPerBlock = RemoteNbBytesPerBlock / sampleSize;

    if (m_sampleIndex < samplesPerBlock)
    {
        convertDataToSample(s, m_blockIndex, m_sampleIndex, scaleForTx);
        m_sampleIndex++;
        m_sampleCount++;
    }
    else
    {
        m_sampleIndex = 0;
        m_blockIndex++;

        if (m_blockIndex < RemoteNbOrginalBlocks)
        {
            convertDataToSample(s, m_blockIndex, m_sampleIndex, scaleForTx);
            m_sampleIndex++;
            m_sampleCount++;
        }
        else
        {
            delete m_dataBlock;
            m_dataBlock = 0;

            if (length() == 0) {
                qWarning("RemoteDataReadQueue::readSample: try to pop new block but queue is empty");
            }

            if (length() > 0)
            {
                //qDebug("RemoteDataReadQueue::readSample: pop new block: queue size: %u", length());
                m_blockIndex = 1;
                m_dataBlock = m_dataReadQueue.takeFirst();
                convertDataToSample(s, m_blockIndex, m_sampleIndex, scaleForTx);
                m_sampleIndex++;
                m_sampleCount++;
            }
            else
            {
                s = Sample{0, 0};
            }
        }
    }
}



