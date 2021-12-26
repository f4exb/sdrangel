///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include "remoteinputbuffer.h"



RemoteInputBuffer::RemoteInputBuffer() :
        m_decoderSlots(nullptr),
        m_frames(nullptr),
        m_curNbBlocks(0),
        m_minNbBlocks(256),
        m_curOriginalBlocks(0),
        m_minOriginalBlocks(128),
        m_curNbRecovery(0),
        m_maxNbRecovery(0),
        m_framesDecoded(true),
        m_readIndex(0),
        m_readBuffer(0),
        m_readSize(0),
        m_bufferLenSec(0.0f),
        m_nbReads(0),
        m_nbWrites(0),
        m_balCorrection(0),
	    m_balCorrLimit(0)
{
	m_currentMeta.init();
    setNbDecoderSlots(16);
    m_decoderIndexHead = m_nbDecoderSlots/2;
	m_tvOut_sec = 0;
	m_tvOut_usec = 0;
	m_readNbBytes = 1;
    m_paramsCM256.BlockBytes = sizeof(RemoteProtectedBlock); // never changes
    m_paramsCM256.OriginalCount = RemoteNbOrginalBlocks;  // never changes

    if (!m_cm256.isInitialized()) {
        m_cm256_OK = false;
        qDebug() << "RemoteInputBuffer::RemoteInputBuffer: cannot initialize CM256 library";
    } else {
        m_cm256_OK = true;
    }

    std::fill(m_decoderSlots, m_decoderSlots + m_nbDecoderSlots, DecoderSlot());
    std::fill(m_frames, m_frames + m_nbDecoderSlots, BufferFrame());
}

RemoteInputBuffer::~RemoteInputBuffer()
{
	if (m_readBuffer) {
		delete[] m_readBuffer;
	}
    if (m_decoderSlots) {
        delete[] m_decoderSlots;
    }
    if (m_frames) {
        delete[] m_frames;
    }
}

void RemoteInputBuffer::setNbDecoderSlots(int nbDecoderSlots)
{
    m_nbDecoderSlots = nbDecoderSlots;
    m_framesSize = m_nbDecoderSlots * (RemoteNbOrginalBlocks - 1) * RemoteNbBytesPerBlock;
  	m_framesNbBytes = m_nbDecoderSlots * sizeof(BufferFrame);
    m_wrDeltaEstimate = m_framesNbBytes / 2;

    if (m_decoderSlots) {
        delete[] m_decoderSlots;
    }
    if (m_frames) {
        delete[] m_frames;
    }

    m_decoderSlots = new DecoderSlot[m_nbDecoderSlots];
    m_frames = new BufferFrame[m_nbDecoderSlots];
    m_frameHead = -1;
    initReadIndex();
}

void RemoteInputBuffer::setBufferLenSec(const RemoteMetaDataFEC& metaData)
{
    m_bufferLenSec = (float) m_framesNbBytes / (float) (metaData.m_sampleRate * metaData.m_sampleBytes * 2);
}

void RemoteInputBuffer::initDecodeAllSlots()
{
    for (int i = 0; i < m_nbDecoderSlots; i++)
    {
        m_decoderSlots[i].m_blockCount = 0;
        m_decoderSlots[i].m_originalCount = 0;
        m_decoderSlots[i].m_recoveryCount = 0;
        m_decoderSlots[i].m_decoded = false;
        m_decoderSlots[i].m_metaRetrieved = false;
        resetOriginalBlocks(i);
        memset((void *) m_decoderSlots[i].m_recoveryBlocks, 0, RemoteNbOrginalBlocks * sizeof(RemoteProtectedBlock));
    }
}

void RemoteInputBuffer::initDecodeSlot(int slotIndex)
{
    // collect stats before voiding the slot

    m_curNbBlocks = m_decoderSlots[slotIndex].m_blockCount;
    m_curOriginalBlocks = m_decoderSlots[slotIndex].m_originalCount;
    m_curNbRecovery = m_decoderSlots[slotIndex].m_recoveryCount;
    m_avgNbBlocks(m_curNbBlocks);
    m_avgOrigBlocks(m_curOriginalBlocks);
    m_avgNbRecovery(m_curNbRecovery);
    m_framesDecoded = m_framesDecoded && m_decoderSlots[slotIndex].m_decoded;

    if (m_curNbBlocks < m_minNbBlocks) {
        m_minNbBlocks = m_curNbBlocks;
    }

    if (m_curOriginalBlocks < m_minOriginalBlocks) {
        m_minOriginalBlocks = m_curOriginalBlocks;
    }

    if (m_curNbRecovery > m_maxNbRecovery) {
        m_maxNbRecovery = m_curNbRecovery;
    }

    // void the slot

    m_decoderSlots[slotIndex].m_blockCount = 0;
    m_decoderSlots[slotIndex].m_originalCount = 0;
    m_decoderSlots[slotIndex].m_recoveryCount = 0;
    m_decoderSlots[slotIndex].m_decoded = false;
    m_decoderSlots[slotIndex].m_metaRetrieved = false;

    resetOriginalBlocks(slotIndex);
    memset((void *) m_decoderSlots[slotIndex].m_recoveryBlocks, 0, RemoteNbOrginalBlocks * sizeof(RemoteProtectedBlock));
}

void RemoteInputBuffer::initReadIndex()
{
    m_readIndex = ((m_decoderIndexHead + (m_nbDecoderSlots/2)) % m_nbDecoderSlots) * sizeof(BufferFrame);
    m_wrDeltaEstimate = m_framesNbBytes / 2;
    m_nbReads = 0;
    m_nbWrites = 0;
}

void RemoteInputBuffer::rwCorrectionEstimate(int slotIndex)
{
	if (m_nbReads >= 40) // check every ~1s as tick is ~50ms
	{
		int targetPivotSlot = (slotIndex + (m_nbDecoderSlots/2))  % m_nbDecoderSlots; // slot at half buffer opposite of current write slot
		int targetPivotIndex = targetPivotSlot * sizeof(BufferFrame);             // buffer index corresponding to start of above slot
		int normalizedReadIndex = (m_readIndex < targetPivotIndex ? m_readIndex + m_nbDecoderSlots * sizeof(BufferFrame) :  m_readIndex)
				- (targetPivotSlot * sizeof(BufferFrame)); // normalize read index so it is positive and zero at start of pivot slot
		int dBytes;
        int rwDelta = (m_nbReads * m_readNbBytes) - (m_nbWrites * sizeof(BufferFrame));

		if (normalizedReadIndex < (m_nbDecoderSlots/ 2) * (int) sizeof(BufferFrame)) // read leads
		{
			dBytes = - normalizedReadIndex - rwDelta;
		}
		else // read lags
		{
            int bufSize = (m_nbDecoderSlots * sizeof(BufferFrame));
			dBytes = bufSize - normalizedReadIndex - rwDelta;
		}

         // calculate exponential moving average on floating point for better accuracy (was int)
        double newCorrection = ((double) dBytes) / (((int) m_currentMeta.m_sampleBytes) * 2 * m_nbReads);
        m_balCorrection = 0.25*m_balCorrection + 0.75*newCorrection; // exponential average with alpha = 0.75 (original is wrong)
        //m_balCorrection = (m_balCorrection / 4) + (dBytes / (int) (m_currentMeta.m_sampleBytes * 2 * m_nbReads)); // correction is in number of samples. Alpha = 0.25

        if (m_balCorrection < -m_balCorrLimit) {
            m_balCorrection = -m_balCorrLimit;
        } else if (m_balCorrection > m_balCorrLimit) {
            m_balCorrection = m_balCorrLimit;
        }

	    m_nbReads = 0;
	    m_nbWrites = 0;
	}
}

void RemoteInputBuffer::checkSlotData(int slotIndex)
{
    int pseudoWriteIndex = slotIndex * sizeof(BufferFrame);
    m_wrDeltaEstimate = pseudoWriteIndex - m_readIndex;
    int rwDelayBytes = (m_wrDeltaEstimate > 0 ? m_wrDeltaEstimate : sizeof(BufferFrame) * m_nbDecoderSlots + m_wrDeltaEstimate);
    int sampleRate = m_currentMeta.m_sampleRate;

    if (sampleRate > 0)
    {
        int64_t ts = m_currentMeta.m_tv_sec * 1000000LL + m_currentMeta.m_tv_usec;
        ts -= (rwDelayBytes * 1000000LL) / (sampleRate * 2 * m_currentMeta.m_sampleBytes);
        m_tvOut_sec = ts / 1000000LL;
        m_tvOut_usec = ts - (m_tvOut_sec * 1000000LL);
    }

    if (!m_decoderSlots[slotIndex].m_decoded)
    {
        qDebug() << "RemoteInputBuffer::checkSlotData: incomplete frame:"
                << " slotIndex: " << slotIndex
                << " m_blockCount: " << m_decoderSlots[slotIndex].m_blockCount
                << " m_recoveryCount: " << m_decoderSlots[slotIndex].m_recoveryCount;
    }
}

void RemoteInputBuffer::writeData(char *array)
{
    RemoteSuperBlock *superBlock = (RemoteSuperBlock *) array;
    int frameIndex = superBlock->m_header.m_frameIndex;
    int decoderIndex = frameIndex % m_nbDecoderSlots;

    // frame break

    if (m_frameHead == -1) // initial state
    {
        m_decoderIndexHead = decoderIndex; // new decoder slot head
        m_frameHead = frameIndex;
        initReadIndex(); // reset read index
        initDecodeAllSlots(); // initialize all slots
    }
    else if (m_frameHead != frameIndex) // frame break => new frame starts
    {
        m_decoderIndexHead = decoderIndex; // new decoder slot head
        m_frameHead = frameIndex;          // new frame head
        checkSlotData(decoderIndex);       // check slot before re-init
        rwCorrectionEstimate(decoderIndex);
        m_nbWrites++;
        initDecodeSlot(decoderIndex);      // collect stats and re-initialize current slot
    }

    // Block processing

    if (m_decoderSlots[decoderIndex].m_blockCount < RemoteNbOrginalBlocks) // not enough blocks to decode -> store data
    {
        int blockIndex = superBlock->m_header.m_blockIndex;
        int blockCount = m_decoderSlots[decoderIndex].m_blockCount;
        int recoveryCount = m_decoderSlots[decoderIndex].m_recoveryCount;
        m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Index = blockIndex;

        if (blockIndex == 0) // first block with meta
        {
            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
        }

        if (blockIndex < RemoteNbOrginalBlocks) // original data
        {
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) storeOriginalBlock(decoderIndex, blockIndex, superBlock->m_protectedBlock);
            m_decoderSlots[decoderIndex].m_originalCount++;
        }
        else // recovery data
        {
            m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount] = superBlock->m_protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount];
            m_decoderSlots[decoderIndex].m_recoveryCount++;
        }
    }

    m_decoderSlots[decoderIndex].m_blockCount++;

    if (m_decoderSlots[decoderIndex].m_blockCount == RemoteNbOrginalBlocks) // ready to decode
    {
        m_decoderSlots[decoderIndex].m_decoded = true;

        if (m_cm256_OK && (m_decoderSlots[decoderIndex].m_recoveryCount > 0)) // recovery data used => need to decode FEC
        {
            m_paramsCM256.BlockBytes = sizeof(RemoteProtectedBlock); // never changes
            m_paramsCM256.OriginalCount = RemoteNbOrginalBlocks;  // never changes

            if (m_decoderSlots[decoderIndex].m_metaRetrieved) {
                m_paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            } else {
                m_paramsCM256.RecoveryCount = m_decoderSlots[decoderIndex].m_recoveryCount;
            }

            if (m_cm256.cm256_decode(m_paramsCM256, m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks)) // CM256 decode
            {
                qDebug() << "RemoteInputBuffer::writeData: decode CM256 error:"
                        << " decoderIndex: " << decoderIndex
                        << " m_blockCount: " << m_decoderSlots[decoderIndex].m_blockCount
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
            }
            else
            {
                qDebug() << "RemoteInputBuffer::writeData: decode CM256 success:"
                        << " decoderIndex: " << decoderIndex
                        << " m_blockCount: " << m_decoderSlots[decoderIndex].m_blockCount
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;

                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = RemoteNbOrginalBlocks - m_decoderSlots[decoderIndex].m_recoveryCount + ir;
                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Index;
                    RemoteProtectedBlock *recoveredBlock = (RemoteProtectedBlock *) m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Block;

                    if (blockIndex == 0) // first block with meta
                    {
                        RemoteMetaDataFEC *metaData = (RemoteMetaDataFEC *) recoveredBlock;

                        boost::crc_32_type crc32;
                        crc32.process_bytes(metaData, sizeof(RemoteMetaDataFEC)-4);

                        if (crc32.checksum() == metaData->m_crc32)
                        {
                            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
                            printMeta("RemoteInputBuffer::writeData: recovered meta", metaData);
                        }
                        else
                        {
                            qDebug() << "RemoteInputBuffer::writeData: recovered meta: invalid CRC32";
                        }
                    }

                    storeOriginalBlock(decoderIndex, blockIndex, *recoveredBlock);

                    qDebug() << "RemoteInputBuffer::writeData: recovered block #" << blockIndex;
                } // restore missing blocks
            } // CM256 decode
        } // recovery

        if (m_decoderSlots[decoderIndex].m_metaRetrieved) // block zero with its meta data has been received
        {
            RemoteMetaDataFEC *metaData = getMetaData(decoderIndex);

            if (!(*metaData == m_currentMeta))
            {
                uint32_t sampleRate =  metaData->m_sampleRate;

                if (sampleRate != 0)
                {
                    setBufferLenSec(*metaData);
                    m_balCorrLimit = sampleRate / 400; // +/- 5% correction max per read
                    m_readNbBytes = (sampleRate * metaData->m_sampleBytes * 2) / 20;
                }

                printMeta("RemoteInputBuffer::writeData: new meta", metaData); // print for change other than timestamp
            }

            m_currentMeta = *metaData; // renew current meta
        } // check block 0
    } // decode
}

uint8_t *RemoteInputBuffer::readData(int32_t length)
{
    uint8_t *buffer = (uint8_t *) m_frames;
    uint32_t readIndex = m_readIndex;

    m_nbReads++;

    // SEGFAULT FIX: arbitratily truncate so that it does not exceed buffer length
    if (length > m_framesSize) {
        length = m_framesSize;
    }

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

        if (m_readBuffer)
        {
            std::memcpy((void *) m_readBuffer, (const void *) &buffer[m_readIndex], m_framesNbBytes - m_readIndex); // copy end of buffer
            length -= m_framesNbBytes - m_readIndex;
            std::memcpy((void *) &m_readBuffer[m_framesNbBytes - m_readIndex], (const void *) buffer, length); // copy start of buffer
            m_readIndex = length;
        }

        return m_readBuffer;
    }
}

void RemoteInputBuffer::printMeta(const QString& header, RemoteMetaDataFEC *metaData)
{
	qDebug() << header << ": "
            << "|" << metaData->m_centerFrequency
            << ":" << metaData->m_sampleRate
            << ":" << (int) (metaData->m_sampleBytes & 0xF)
            << ":" << (int) metaData->m_sampleBits
            << ":" << (int) metaData->m_nbOriginalBlocks
            << ":" << (int) metaData->m_nbFECBlocks
            << "|" << metaData->m_deviceIndex
            << ":" << metaData->m_channelIndex
            << "|" << metaData->m_tv_sec
            << ":" << metaData->m_tv_usec
            << "|";
}
