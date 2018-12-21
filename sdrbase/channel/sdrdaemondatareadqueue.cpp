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

#include "channel/sdrdaemondatablock.h"
#include "channel/sdrdaemondatareadqueue.h"

const uint32_t SDRDaemonDataReadQueue::MinimumMaxSize = 10;

SDRDaemonDataReadQueue::SDRDaemonDataReadQueue() :
        m_dataBlock(0),
        m_maxSize(MinimumMaxSize),
        m_blockIndex(1),
        m_sampleIndex(0),
        m_sampleCount(0),
        m_full(false)
{}

SDRDaemonDataReadQueue::~SDRDaemonDataReadQueue()
{
    SDRDaemonDataBlock* data;

    while ((data = pop()) != 0)
    {
        qDebug("SDRDaemonDataReadQueue::~SDRDaemonDataReadQueue: data block was still in queue");
        delete data;
    }
}

void SDRDaemonDataReadQueue::push(SDRDaemonDataBlock* dataBlock)
{
    if (length() >= m_maxSize)
    {
        qWarning("SDRDaemonDataReadQueue::push: queue is full");
        m_full = true; // stop filling the queue
        SDRDaemonDataBlock *data = m_dataReadQueue.takeLast();
        delete data;
    }

    if (m_full) {
        m_full = (length() > m_maxSize/2); // do not fill queue again before queue is half size
    }

    if (!m_full) {
        m_dataReadQueue.append(dataBlock);
    }
}

SDRDaemonDataBlock* SDRDaemonDataReadQueue::pop()
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

void SDRDaemonDataReadQueue::setSize(uint32_t size)
{
    if (size != m_maxSize) {
        m_maxSize = size < MinimumMaxSize ? MinimumMaxSize : size;
    }
}

void SDRDaemonDataReadQueue::readSample(Sample& s, bool scaleForTx)
{
    // depletion/repletion state
    if (m_dataBlock == 0)
    {
        if (length() >= m_maxSize/2)
        {
            qDebug("SDRDaemonDataReadQueue::readSample: initial pop new block: queue size: %u", length());
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
    uint32_t samplesPerBlock = SDRDaemonNbBytesPerBlock / sampleSize;

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

        if (m_blockIndex < SDRDaemonNbOrginalBlocks)
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
                qWarning("SDRDaemonDataReadQueue::readSample: try to pop new block but queue is empty");
            }

            if (length() > 0)
            {
                //qDebug("SDRDaemonDataReadQueue::readSample: pop new block: queue size: %u", length());
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



