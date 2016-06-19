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

#include <QDebug>
#include <cassert>
#include <cstring>
#include <cmath>
#include <lz4.h>
#include "sdrdaemonfecbuffer.h"


const int SDRdaemonFECBuffer::m_udpPayloadSize = 512;
const int SDRdaemonFECBuffer::m_sampleSize = 2;
const int SDRdaemonFECBuffer::m_iqSampleSize = 2 * m_sampleSize;
const int SDRdaemonFECBuffer::m_rawBufferLengthSeconds = 8; // should be even
const int SDRdaemonFECBuffer::m_rawBufferMinNbFrames = 50;

SDRdaemonFECBuffer::SDRdaemonFECBuffer(uint32_t throttlems) :
        m_frameHead(0),
        m_decoderSlotHead(nbDecoderSlots/2),
        m_curNbBlocks(0),
        m_curNbRecovery(0),
        m_throttlemsNominal(throttlems),
        m_readIndex(0),
        m_readBuffer(0),
        m_readSize(0),
        m_bufferLenSec(0.0f)
{
	m_currentMeta.init();
	m_framesNbBytes = nbDecoderSlots * sizeof(BufferFrame) * m_iqSampleSize;
	m_wrDeltaEstimate = m_framesNbBytes / 2;
}

SDRdaemonFECBuffer::~SDRdaemonFECBuffer()
{
	if (m_readBuffer) {
		delete[] m_readBuffer;
	}
}

void SDRdaemonFECBuffer::initDecoderSlotsAddresses()
{
    for (int i = 0; i < nbDecoderSlots; i++)
    {
        for (int j = 0; j < nbOriginalBlocks - 1; j++)
        {
            m_decoderSlots[i].m_originalBlockPtrs[j] = &m_frames[i].m_blocks[j];
        }
    }
}

void SDRdaemonFECBuffer::initDecodeAllSlots()
{
    for (int i = 0; i < nbDecoderSlots; i++)
    {
        m_decoderSlots[i].m_blockCount = 0;
        m_decoderSlots[i].m_recoveryCount = 0;
        m_decoderSlots[i].m_decoded = false;
        m_decoderSlots[i].m_blockZero.m_metaData.init();
    }
}

void SDRdaemonFECBuffer::initReadIndex()
{
    m_readIndex = ((m_decoderSlotHead + (nbDecoderSlots/2)) % nbDecoderSlots) * sizeof(BufferFrame);
    m_wrDeltaEstimate = m_framesNbBytes / 2;
}

void SDRdaemonFECBuffer::initDecodeSlot(int slotIndex)
{
    int pseudoWriteIndex = slotIndex * sizeof(BufferFrame);
    m_wrDeltaEstimate = pseudoWriteIndex - m_readIndex;
    // collect stats before voiding the slot
    m_curNbBlocks = m_decoderSlots[slotIndex].m_blockCount;
    m_curNbRecovery = m_decoderSlots[slotIndex].m_recoveryCount;
    m_avgNbBlocks(m_curNbBlocks);
    m_avgNbRecovery(m_curNbRecovery);
    // void the slot
    m_decoderSlots[slotIndex].m_blockCount = 0;
    m_decoderSlots[slotIndex].m_recoveryCount = 0;
    m_decoderSlots[slotIndex].m_decoded = false;
    m_decoderSlots[slotIndex].m_blockZero.m_metaData.init();
    memset((void *) m_decoderSlots[slotIndex].m_blockZero.m_samples, 0, samplesPerBlockZero * sizeof(Sample));
    memset((void *) m_frames[slotIndex].m_blocks, 0, (nbOriginalBlocks - 1) * samplesPerBlock * sizeof(Sample));
}

void SDRdaemonFECBuffer::writeData(char *array, uint32_t length)
{
    assert(length == udpSize);

    bool dataAvailable = false;
    SuperBlock *superBlock = (SuperBlock *) array;
    int frameIndex = superBlock->header.frameIndex;
    int decoderIndex = frameIndex % nbDecoderSlots;

    if (m_frameHead == -1) // initial state
    {
        m_decoderSlotHead = decoderIndex; // new decoder slot head
        m_frameHead = frameIndex;
        initReadIndex(); // reset read index
        initDecodeAllSlots(); // initialize all slots
    }
    else
    {
        int frameDelta = m_frameHead - frameIndex;

        if (frameDelta < 0)
        {
            if (-frameDelta < nbDecoderSlots) // new frame head not too new
            {
                m_decoderSlotHead = decoderIndex; // new decoder slot head
                m_frameHead = frameIndex;
                dataAvailable = true;
                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
            }
            else if (-frameDelta <= sizeof(uint16_t) - nbDecoderSlots) // loss of sync start over
            {
                m_decoderSlotHead = frameIndex % nbDecoderSlots; // new decoder slot head
                decoderIndex = m_decoderSlotHead;
                m_frameHead = frameIndex;
                initReadIndex(); // reset read index
                initDecodeAllSlots(); // re-initialize all slots
            }
        }
        else
        {
            if (frameDelta > sizeof(uint16_t) - nbDecoderSlots) // new frame head not too new
            {
                m_decoderSlotHead = decoderIndex; // new decoder slot head
                m_frameHead = frameIndex;
                dataAvailable = true;
                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
            }
            else if (frameDelta >= nbDecoderSlots) // loss of sync start over
            {
                m_decoderSlotHead = frameIndex % nbDecoderSlots; // new decoder slot head
                decoderIndex = m_decoderSlotHead;
                m_frameHead = frameIndex;
                initReadIndex(); // reset read index
                initDecodeAllSlots(); // re-initialize all slots
            }
        }
    }

    // decoderIndex should now be correctly set

    int blockIndex = superBlock->header.blockIndex;
    int blockHead = m_decoderSlots[decoderIndex].m_blockCount;

    if (blockHead < nbOriginalBlocks) // not enough blocks to decode -> store data
    {
        if (blockIndex == 0) // first block with meta
        {
            SuperBlockZero *superBlockZero = (SuperBlockZero *) array;
            m_decoderSlots[decoderIndex].m_blockZero = superBlockZero->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Block = (void *) &m_decoderSlots[decoderIndex].m_blockZero;
            memcpy((void *) m_frames[decoderIndex].m_blockZero.m_samples,
                    (const void *) m_decoderSlots[decoderIndex].m_blockZero.m_samples,
                    samplesPerBlockZero * sizeof(Sample));
        }
        else if (blockIndex < nbOriginalBlocks) // normal block
        {
            m_frames[decoderIndex].m_blocks[blockIndex - 1] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Block = (void *) &m_frames[decoderIndex].m_blocks[blockIndex - 1];
        }
        else // redundancy block
        {
            m_decoderSlots[decoderIndex].m_recoveryBlocks[m_decoderSlots[decoderIndex].m_recoveryCount] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_recoveryCount++;
        }

        m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Index = blockIndex;
        m_decoderSlots[decoderIndex].m_blockCount++;
    }
    else if (!m_decoderSlots[decoderIndex].m_decoded) // ready to decode
    {
        if (m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_nbFECBlocks < 0) // block zero has not been received
        {
            m_paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks; // take last value for number of FEC blocks
        }
        else
        {
            m_paramsCM256.RecoveryCount = m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_nbFECBlocks;
        }

        if (m_decoderSlots[decoderIndex].m_recoveryCount > 0) // recovery data used
        {
            if (cm256_decode(m_paramsCM256, m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks)) // failure to decode
            {
                qDebug("SDRdaemonFECBuffer::writeAndRead: CM256 decode error");
            }
            else // success to decode
            {
                int nbOriginalBlocks = m_decoderSlots[decoderIndex].m_blockCount - m_decoderSlots[decoderIndex].m_recoveryCount;

                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // recover lost blocks
                {
                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[nbOriginalBlocks+ir].Index;

                    if (blockIndex == 0)
                    {
                        ProtectedBlockZero *recoveredBlockZero = (ProtectedBlockZero *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[ir];
                        m_decoderSlots[decoderIndex].m_blockZero.m_metaData = recoveredBlockZero->m_metaData;
                        memcpy((void *) m_frames[decoderIndex].m_blockZero.m_samples,
                                (const void *) recoveredBlockZero->m_samples,
                                samplesPerBlockZero * sizeof(Sample));
                    }
                    else
                    {
                        m_frames[decoderIndex].m_blocks[blockIndex - 1] =  m_decoderSlots[decoderIndex].m_recoveryBlocks[ir];
                    }
                }
            }
        }

        if (m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_nbFECBlocks >= 0) // meta data valid
        {
            if (!(m_decoderSlots[decoderIndex].m_blockZero.m_metaData == m_currentMeta))
            {
                int sampleRate =  m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_sampleRate;

                if (sampleRate > 0) {
                    m_bufferLenSec = (float) m_framesNbBytes / (float) sampleRate;
                }

                printMeta("SDRdaemonFECBuffer::writeData", &m_decoderSlots[decoderIndex].m_blockZero.m_metaData); // print for change other than timestamp
            }

            m_currentMeta = m_decoderSlots[decoderIndex].m_blockZero.m_metaData; // renew current meta
        }

        m_decoderSlots[decoderIndex].m_decoded = true;
    }
}

uint8_t *SDRdaemonFECBuffer::readData(int32_t length)
{
    uint8_t *buffer = (uint8_t *) m_frames;
    uint32_t readIndex = m_readIndex;

    if (m_readIndex + length < m_framesNbBytes) // ends before buffer bound
    {
        m_readIndex += length;
        return &buffer[readIndex];
    }
    else if (m_readIndex + length == m_framesNbBytes) // ends at buffer bound
    {
        m_readIndex = 0;
        return &buffer[readIndex];
    }
    else // ends after buffer bound
    {
        if (length > m_readSize) // reallocate composition buffer if necessary
        {
            if (m_readBuffer) {
                delete[] m_readBuffer;
            }

            m_readBuffer = new uint8_t[length];
            m_readSize = length;
        }

        std::memcpy((void *) m_readBuffer, (const void *) &buffer[m_readIndex], m_framesNbBytes - m_readIndex); // copy end of buffer
        length -= m_framesNbBytes - m_readIndex;
        std::memcpy((void *) &m_readBuffer[m_framesNbBytes - m_readIndex], (const void *) buffer, length); // copy start of buffer
        m_readIndex = length;
        return m_readBuffer;
    }
}

void SDRdaemonFECBuffer::printMeta(const QString& header, MetaDataFEC *metaData)
{
	qDebug() << header << ": "
            << "|" << metaData->m_centerFrequency
            << ":" << metaData->m_sampleRate
            << ":" << (int) (metaData->m_sampleBytes & 0xF)
            << ":" << (int) metaData->m_sampleBits
            << ":" << (int) metaData->m_nbOriginalBlocks
            << ":" << (int) metaData->m_nbFECBlocks
            << "|" << metaData->m_tv_sec
            << ":" << metaData->m_tv_usec
            << "|";
}
