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
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>
#include "sdrdaemonsourcebuffer.h"



const int SDRdaemonSourceBuffer::m_sampleSize = 2;
const int SDRdaemonSourceBuffer::m_iqSampleSize = 2 * m_sampleSize;

SDRdaemonSourceBuffer::SDRdaemonSourceBuffer(uint32_t throttlems) :
        m_decoderIndexHead(nbDecoderSlots/2),
        m_frameHead(0),
        m_curNbBlocks(0),
        m_minNbBlocks(256),
        m_curOriginalBlocks(0),
        m_minOriginalBlocks(128),
        m_curNbRecovery(0),
        m_maxNbRecovery(0),
        m_framesDecoded(true),
        m_readIndex(0),
        m_throttlemsNominal(throttlems),
        m_readBuffer(0),
        m_readSize(0),
        m_bufferLenSec(0.0f),
        m_nbReads(0),
        m_nbWrites(0),
        m_balCorrection(0),
	    m_balCorrLimit(0)
{
	m_currentMeta.init();
	m_framesNbBytes = nbDecoderSlots * sizeof(BufferFrame);
	m_wrDeltaEstimate = m_framesNbBytes / 2;
	m_tvOut_sec = 0;
	m_tvOut_usec = 0;
	m_readNbBytes = 1;
    m_paramsCM256.BlockBytes = sizeof(ProtectedBlock); // never changes
    m_paramsCM256.OriginalCount = m_nbOriginalBlocks;  // never changes

    if (!m_cm256.isInitialized()) {
        m_cm256_OK = false;
        qDebug() << "SDRdaemonSourceBuffer::SDRdaemonSourceBuffer: cannot initialize CM256 library";
    } else {
        m_cm256_OK = true;
    }
}

SDRdaemonSourceBuffer::~SDRdaemonSourceBuffer()
{
	if (m_readBuffer) {
		delete[] m_readBuffer;
	}
}

void SDRdaemonSourceBuffer::initDecodeAllSlots()
{
    for (int i = 0; i < nbDecoderSlots; i++)
    {
        m_decoderSlots[i].m_blockCount = 0;
        m_decoderSlots[i].m_originalCount = 0;
        m_decoderSlots[i].m_recoveryCount = 0;
        m_decoderSlots[i].m_decoded = false;
        m_decoderSlots[i].m_metaRetrieved = false;
        resetOriginalBlocks(i);
        memset((void *) m_decoderSlots[i].m_recoveryBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
    }
}

void SDRdaemonSourceBuffer::initDecodeSlot(int slotIndex)
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
    memset((void *) m_decoderSlots[slotIndex].m_recoveryBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
}

void SDRdaemonSourceBuffer::initReadIndex()
{
    m_readIndex = ((m_decoderIndexHead + (nbDecoderSlots/2)) % nbDecoderSlots) * sizeof(BufferFrame);
    m_wrDeltaEstimate = m_framesNbBytes / 2;
    m_nbReads = 0;
    m_nbWrites = 0;
}

void SDRdaemonSourceBuffer::rwCorrectionEstimate(int slotIndex)
{
	if (m_nbReads >= 40) // check every ~1s as tick is ~50ms
	{
		int targetPivotSlot = (slotIndex + (nbDecoderSlots/2))  % nbDecoderSlots; // slot at half buffer opposite of current write slot
		int targetPivotIndex = targetPivotSlot * sizeof(BufferFrame);             // buffer index corresponding to start of above slot
		int normalizedReadIndex = (m_readIndex < targetPivotIndex ? m_readIndex + nbDecoderSlots * sizeof(BufferFrame) :  m_readIndex)
				- (targetPivotSlot * sizeof(BufferFrame)); // normalize read index so it is positive and zero at start of pivot slot
		int dBytes;
        int rwDelta = (m_nbReads * m_readNbBytes) - (m_nbWrites * sizeof(BufferFrame));

		if (normalizedReadIndex < (nbDecoderSlots/ 2) * (int) sizeof(BufferFrame)) // read leads
		{
			dBytes = - normalizedReadIndex - rwDelta;
		}
		else // read lags
		{
			dBytes = (nbDecoderSlots * sizeof(BufferFrame)) - normalizedReadIndex - rwDelta;
		}

        m_balCorrection = (m_balCorrection / 4) + (dBytes / (int) (m_iqSampleSize * m_nbReads)); // correction is in number of samples. Alpha = 0.25

        if (m_balCorrection < -m_balCorrLimit) {
            m_balCorrection = -m_balCorrLimit;
        } else if (m_balCorrection > m_balCorrLimit) {
            m_balCorrection = m_balCorrLimit;
        }

	    m_nbReads = 0;
	    m_nbWrites = 0;
	}
}

void SDRdaemonSourceBuffer::checkSlotData(int slotIndex)
{
    int pseudoWriteIndex = slotIndex * sizeof(BufferFrame);
    m_wrDeltaEstimate = pseudoWriteIndex - m_readIndex;
    m_nbWrites++;

    int rwDelayBytes = (m_wrDeltaEstimate > 0 ? m_wrDeltaEstimate : sizeof(BufferFrame) * nbDecoderSlots + m_wrDeltaEstimate);
    int sampleRate = m_currentMeta.m_sampleRate;

    if (sampleRate > 0)
    {
        int64_t ts = m_currentMeta.m_tv_sec * 1000000LL + m_currentMeta.m_tv_usec;
        ts -= (rwDelayBytes * 1000000LL) / (sampleRate * sizeof(SDRdaemonSample));
        m_tvOut_sec = ts / 1000000LL;
        m_tvOut_usec = ts - (m_tvOut_sec * 1000000LL);
    }

    if (!m_decoderSlots[slotIndex].m_decoded)
    {
        qDebug() << "SDRdaemonSourceBuffer::checkSlotData: incomplete frame:"
                << " m_blockCount: " << m_decoderSlots[slotIndex].m_blockCount
                << " m_recoveryCount: " << m_decoderSlots[slotIndex].m_recoveryCount;
    }
}

void SDRdaemonSourceBuffer::writeData(char *array)
{
    SuperBlock *superBlock = (SuperBlock *) array;
    int frameIndex = superBlock->header.frameIndex;
    int decoderIndex = frameIndex % nbDecoderSlots;

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
        initDecodeSlot(decoderIndex);      // collect stats and re-initialize current slot
    }

    // Block processing

    if (m_decoderSlots[decoderIndex].m_blockCount < m_nbOriginalBlocks) // not enough blocks to decode -> store data
    {
        int blockIndex = superBlock->header.blockIndex;
        int blockCount = m_decoderSlots[decoderIndex].m_blockCount;
        int recoveryCount = m_decoderSlots[decoderIndex].m_recoveryCount;
        m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Index = blockIndex;

        if (blockIndex == 0) // first block with meta
        {
            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
        }

        if (blockIndex < m_nbOriginalBlocks) // original data
        {
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) storeOriginalBlock(decoderIndex, blockIndex, superBlock->protectedBlock);
            m_decoderSlots[decoderIndex].m_originalCount++;
        }
        else // recovery data
        {
            m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount];
            m_decoderSlots[decoderIndex].m_recoveryCount++;
        }
    }

    m_decoderSlots[decoderIndex].m_blockCount++;

    if (m_decoderSlots[decoderIndex].m_blockCount == m_nbOriginalBlocks) // ready to decode
    {
        m_decoderSlots[decoderIndex].m_decoded = true;

        if (m_cm256_OK && (m_decoderSlots[decoderIndex].m_recoveryCount > 0)) // recovery data used => need to decode FEC
        {
            m_paramsCM256.BlockBytes = sizeof(ProtectedBlock); // never changes
            m_paramsCM256.OriginalCount = m_nbOriginalBlocks;  // never changes

            if (m_decoderSlots[decoderIndex].m_metaRetrieved) {
                m_paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks;
            } else {
                m_paramsCM256.RecoveryCount = m_decoderSlots[decoderIndex].m_recoveryCount;
            }

            if (m_cm256.cm256_decode(m_paramsCM256, m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks)) // CM256 decode
            {
                qDebug() << "SDRdaemonSourceBuffer::writeData: decode CM256 error:"
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
            }
            else
            {
                qDebug() << "SDRdaemonSourceBuffer::writeData: decode CM256 success:"
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;

                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = m_nbOriginalBlocks - m_decoderSlots[decoderIndex].m_recoveryCount + ir;
                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Index;
                    ProtectedBlock *recoveredBlock = (ProtectedBlock *) m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Block;

                    if (blockIndex == 0) // first block with meta
                    {
                        MetaDataFEC *metaData = (MetaDataFEC *) recoveredBlock;

                        boost::crc_32_type crc32;
                        crc32.process_bytes(metaData, 20);

                        if (crc32.checksum() == metaData->m_crc32)
                        {
                            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
                            printMeta("SDRdaemonSourceBuffer::writeData: recovered meta", metaData);
                        }
                        else
                        {
                            qDebug() << "SDRdaemonSourceBuffer::writeData: recovered meta: invalid CRC32";
                        }
                    }

                    storeOriginalBlock(decoderIndex, blockIndex, *recoveredBlock);

                    qDebug() << "SDRdaemonSourceBuffer::writeData: recovered block #" << blockIndex;
                } // restore missing blocks
            } // CM256 decode
        } // revovery

        if (m_decoderSlots[decoderIndex].m_metaRetrieved) // block zero with its meta data has been received
        {
            MetaDataFEC *metaData = getMetaData(decoderIndex);

            if (!(*metaData == m_currentMeta))
            {
                int sampleRate =  metaData->m_sampleRate;

                if (sampleRate > 0) {
                    m_bufferLenSec = (float) m_framesNbBytes / (float) (sampleRate * m_iqSampleSize);
                    m_balCorrLimit = sampleRate / 1000; // +/- 1 ms correction max per read
                    m_readNbBytes = (sampleRate * m_iqSampleSize) / 20;
                }

                printMeta("SDRdaemonSourceBuffer::writeData: new meta", metaData); // print for change other than timestamp
            }

            m_currentMeta = *metaData; // renew current meta
        } // check block 0
    } // decode
}

void SDRdaemonSourceBuffer::writeData0(char *array __attribute__((unused)), uint32_t length __attribute__((unused)))
{
// Kept as comments for the out of sync blocks algorithms
//    assert(length == m_udpPayloadSize);
//
//    bool dataAvailable = false;
//    SuperBlock *superBlock = (SuperBlock *) array;
//    int frameIndex = superBlock->header.frameIndex;
//    int decoderIndex = frameIndex % nbDecoderSlots;
//    int blockIndex = superBlock->header.blockIndex;
//
////    qDebug() << "SDRdaemonSourceBuffer::writeData:"
////            << " frameIndex: " << frameIndex
////            << " decoderIndex: " << decoderIndex
////            << " blockIndex: " << blockIndex;
//
//    if (m_frameHead == -1) // initial state
//    {
//        m_decoderIndexHead = decoderIndex; // new decoder slot head
//        m_frameHead = frameIndex;
//        initReadIndex(); // reset read index
//        initDecodeAllSlots(); // initialize all slots
//    }
//    else
//    {
//        int frameDelta = m_frameHead - frameIndex;
//
//        if (frameDelta < 0)
//        {
//            if (-frameDelta < nbDecoderSlots) // new frame head not too new
//            {
//                //qDebug() << "SDRdaemonSourceBuffer::writeData: new frame head (1): " << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                checkSlotData(decoderIndex);
//                dataAvailable = true;
//                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
//            }
//            else if (-frameDelta <= 65536 - nbDecoderSlots) // loss of sync start over
//            {
//                //qDebug() << "SDRdaemonSourceBuffer::writeData: loss of sync start over (1)" << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                initReadIndex(); // reset read index
//                initDecodeAllSlots(); // re-initialize all slots
//            }
//        }
//        else
//        {
//            if (frameDelta > 65536 - nbDecoderSlots) // new frame head not too new
//            {
//                //qDebug() << "SDRdaemonSourceBuffer::writeData: new frame head (2): " << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                checkSlotData(decoderIndex);
//                dataAvailable = true;
//                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
//            }
//            else if (frameDelta >= nbDecoderSlots) // loss of sync start over
//            {
//                //qDebug() << "SDRdaemonSourceBuffer::writeData: loss of sync start over (2)" << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                initReadIndex(); // reset read index
//                initDecodeAllSlots(); // re-initialize all slots
//            }
//        }
//    }
//
//    // decoderIndex should now be correctly set
//
}

uint8_t *SDRdaemonSourceBuffer::readData(int32_t length)
{
    uint8_t *buffer = (uint8_t *) m_frames;
    uint32_t readIndex = m_readIndex;

    m_nbReads++;

    // SEGFAULT FIX: arbitratily truncate so that it does not exceed buffer length
    if (length > framesSize) {
        length = framesSize;
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

        std::memcpy((void *) m_readBuffer, (const void *) &buffer[m_readIndex], m_framesNbBytes - m_readIndex); // copy end of buffer
        length -= m_framesNbBytes - m_readIndex;
        std::memcpy((void *) &m_readBuffer[m_framesNbBytes - m_readIndex], (const void *) buffer, length); // copy start of buffer
        m_readIndex = length;
        return m_readBuffer;
    }
}

void SDRdaemonSourceBuffer::printMeta(const QString& header, MetaDataFEC *metaData)
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
