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
        m_dataFrame(nullptr),
        m_maxSize(MinimumMaxSize),
        m_blockIndex(1),
        m_sampleIndex(0),
        m_sampleCount(0)
{}

RemoteDataReadQueue::~RemoteDataReadQueue()
{
    RemoteDataFrame* data;

    while ((data = pop()) != nullptr)
    {
        qDebug("RemoteDataReadQueue::~RemoteDataReadQueue: data block was still in queue");
        delete data;
    }
}

void RemoteDataReadQueue::push(RemoteDataFrame* dataFrame)
{
    if (length() < m_maxSize) {
        m_dataReadQueue.enqueue(dataFrame);
    } else {
        qWarning("RemoteDataReadQueue::push: queue is full");
    }
}

RemoteDataFrame* RemoteDataReadQueue::pop()
{

    if (m_dataReadQueue.isEmpty())
    {
        return nullptr;
    }
    else
    {
        m_blockIndex = 1;
        m_sampleIndex = 0;
        return m_dataReadQueue.dequeue();
    }
}

void RemoteDataReadQueue::setSize(uint32_t size)
{
    if (size != m_maxSize) {
        m_maxSize = size < MinimumMaxSize ? MinimumMaxSize : size;
    }
}

void RemoteDataReadQueue::readSample(Sample& s, bool isTx)
{
    // depletion/repletion state
    if (m_dataFrame == nullptr)
    {
        m_dataFrame = pop();

        if (m_dataFrame)
        {
            qDebug("RemoteDataReadQueue::readSample: initial pop new frame: queue size: %u", length());
            m_blockIndex = 1;
            m_sampleIndex = 0;
            convertDataToSample(s, m_blockIndex, m_sampleIndex, isTx);
            m_sampleIndex++;
        }
        else
        {
            s = Sample{0, 0};
        }

        m_sampleCount++;

        return;
    }

    int sampleSize = m_dataFrame->m_superBlocks[m_blockIndex].m_header.m_sampleBytes * 2;
    uint32_t samplesPerBlock = RemoteNbBytesPerBlock / sampleSize;

    if (m_sampleIndex < samplesPerBlock)
    {
        convertDataToSample(s, m_blockIndex, m_sampleIndex, isTx);
        m_sampleIndex++;
        m_sampleCount++;
    }
    else
    {
        m_sampleIndex = 0;
        m_blockIndex++;

        if (m_blockIndex < RemoteNbOrginalBlocks)
        {
            convertDataToSample(s, m_blockIndex, m_sampleIndex, isTx);
            m_sampleIndex++;
            m_sampleCount++;
        }
        else
        {
            delete m_dataFrame;
            m_dataFrame = nullptr;

            m_dataFrame = pop();

            if (m_dataFrame)
            {
                m_blockIndex = 1;
                m_sampleIndex = 0;
                convertDataToSample(s, m_blockIndex, m_sampleIndex, isTx);
                m_sampleIndex++;
                m_sampleCount++;
            }
            else
            {
                qWarning("RemoteDataReadQueue::readSample: try to pop new block but queue is empty");
                s = Sample{0, 0};
            }
        }
    }
}
