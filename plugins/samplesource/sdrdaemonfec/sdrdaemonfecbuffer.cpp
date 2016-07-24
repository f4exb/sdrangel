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


const int SDRdaemonFECBuffer::m_sampleSize = 2;
const int SDRdaemonFECBuffer::m_iqSampleSize = 2 * m_sampleSize;

SDRdaemonFECBuffer::SDRdaemonFECBuffer(uint32_t throttlems) :
        m_frameHead(0),
        m_decoderIndexHead(nbDecoderSlots/2),
        m_curNbBlocks(0),
        m_minNbBlocks(256),
        m_curNbRecovery(0),
        m_maxNbRecovery(0),
        m_framesDecoded(true),
        m_throttlemsNominal(throttlems),
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
	m_framesNbBytes = nbDecoderSlots * sizeof(BufferFrame);
	m_wrDeltaEstimate = m_framesNbBytes / 2;
	m_tvOut_sec = 0;
	m_tvOut_usec = 0;
	m_readNbBytes = 1;
    m_paramsCM256.BlockBytes = sizeof(ProtectedBlock); // never changes
    m_paramsCM256.OriginalCount = m_nbOriginalBlocks;  // never changes

    if (!m_cm256.isInitialized()) {
        m_cm256_OK = false;
        qDebug() << "SDRdaemonFECBuffer::SDRdaemonFECBuffer: cannot initialize CM256 library";
    } else {
        m_cm256_OK = true;
    }
}

SDRdaemonFECBuffer::~SDRdaemonFECBuffer()
{
	if (m_readBuffer) {
		delete[] m_readBuffer;
	}
}

void SDRdaemonFECBuffer::initDecodeAllSlots()
{
    for (int i = 0; i < nbDecoderSlots; i++)
    {
        m_decoderSlots[i].m_blockCount = 0;
        m_decoderSlots[i].m_originalCount = 0;
        m_decoderSlots[i].m_recoveryCount = 0;
        m_decoderSlots[i].m_decoded = false;
        m_decoderSlots[i].m_metaRetrieved = false;
        memset((void *) m_decoderSlots[i].m_originalBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
        memset((void *) m_decoderSlots[i].m_recoveryBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
    }
}

void SDRdaemonFECBuffer::initDecodeSlot(int slotIndex)
{
    // collect stats before voiding the slot

    m_curNbBlocks = m_decoderSlots[slotIndex].m_blockCount;
    m_curNbRecovery = m_decoderSlots[slotIndex].m_recoveryCount;
    m_avgNbBlocks(m_curNbBlocks);
    m_avgNbRecovery(m_curNbRecovery);
    m_framesDecoded = m_framesDecoded && m_decoderSlots[slotIndex].m_decoded;

    if (m_curNbBlocks < m_minNbBlocks) {
        m_minNbBlocks = m_curNbBlocks;
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
    memset((void *) m_decoderSlots[slotIndex].m_originalBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
    memset((void *) m_decoderSlots[slotIndex].m_recoveryBlocks, 0, m_nbOriginalBlocks * sizeof(ProtectedBlock));
}

void SDRdaemonFECBuffer::initReadIndex()
{
    m_readIndex = ((m_decoderIndexHead + (nbDecoderSlots/2)) % nbDecoderSlots) * sizeof(BufferFrame);
    m_wrDeltaEstimate = m_framesNbBytes / 2;
    m_nbReads = 0;
    m_nbWrites = 0;
}

void SDRdaemonFECBuffer::rwCorrectionEstimate(int slotIndex)
{
	if (m_nbReads >= 40) // check every ~1s as tick is ~50ms
	{
		int targetPivotSlot = (slotIndex + (nbDecoderSlots/2))  % nbDecoderSlots; // slot at half buffer opposite of current write slot
		int targetPivotIndex = targetPivotSlot * sizeof(BufferFrame);             // buffer index corresponding to start of above slot
		int normalizedReadIndex = (m_readIndex < targetPivotIndex ? m_readIndex + nbDecoderSlots * sizeof(BufferFrame) :  m_readIndex)
				- (targetPivotSlot * sizeof(BufferFrame)); // normalize read index so it is positive and zero at start of pivot slot
		int dBytes;
        int rwDelta = (m_nbReads * m_readNbBytes) - (m_nbWrites * sizeof(BufferFrame));

		if (normalizedReadIndex < (nbDecoderSlots/ 2) * sizeof(BufferFrame)) // read leads
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

        float rwRatio = (float) (m_nbWrites * sizeof(BufferFrame)) / (float)  (m_nbReads * m_readNbBytes);

//        qDebug() << "SDRdaemonFECBuffer::rwCorrectionEstimate: "
//                << " m_nbReads: " << m_nbReads
//                << " m_nbWrites: " << m_nbWrites
//                << " rwDelta: " << rwDelta
//                << " targetPivotSlot: " << targetPivotSlot
//                << " targetPivotIndex: " << targetPivotIndex
//                << " m_readIndex: " << m_readIndex
//                << " normalizedReadIndex: " << normalizedReadIndex
//                << " dBytes: " << dBytes
//                << " m_balCorrection: " << m_balCorrection;

		//m_balCorrection = dBytes / (int) (m_iqSampleSize * m_nbReads);
	    m_nbReads = 0;
	    m_nbWrites = 0;
	}
}

void SDRdaemonFECBuffer::checkSlotData(int slotIndex)
{
    int pseudoWriteIndex = slotIndex * sizeof(BufferFrame);
    m_wrDeltaEstimate = pseudoWriteIndex - m_readIndex;
    m_nbWrites++;

    int rwDelayBytes = (m_wrDeltaEstimate > 0 ? m_wrDeltaEstimate : sizeof(BufferFrame) * nbDecoderSlots + m_wrDeltaEstimate);
    int sampleRate = m_currentMeta.m_sampleRate;

    if (sampleRate > 0)
    {
        int64_t ts = m_currentMeta.m_tv_sec * 1000000LL + m_currentMeta.m_tv_usec;
        ts -= (rwDelayBytes * 1000000LL) / (sampleRate * sizeof(Sample));
        m_tvOut_sec = ts / 1000000LL;
        m_tvOut_usec = ts - (m_tvOut_sec * 1000000LL);
    }

    if (!m_decoderSlots[slotIndex].m_decoded)
    {
        qDebug() << "SDRdaemonFECBuffer::checkSlotData: incomplete frame:"
                << " m_blockCount: " << m_decoderSlots[slotIndex].m_blockCount
                << " m_recoveryCount: " << m_decoderSlots[slotIndex].m_recoveryCount;
    }

    // copy retrieved data to main buffer
    memcpy((void *) &m_frames[slotIndex].m_blocks[0], (const void *) &m_decoderSlots[slotIndex].m_originalBlocks[1], (m_nbOriginalBlocks - 1)*sizeof(ProtectedBlock));
}

void SDRdaemonFECBuffer::writeData(char *array, uint32_t length)
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
            m_decoderSlots[decoderIndex].m_originalBlocks[blockIndex] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_decoderSlots[decoderIndex].m_originalBlocks[blockIndex];
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
                qDebug() << "SDRdaemonFECBuffer::writeData: decode CM256 error:"
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
            }
            else
            {
                qDebug() << "SDRdaemonFECBuffer::writeData: decode CM256 success:"
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;

                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // restore missing blocks
                {
                    int recoveryIndex = m_nbOriginalBlocks - m_decoderSlots[decoderIndex].m_recoveryCount + ir;
                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Index;
                    ProtectedBlock *recoveredBlock = (ProtectedBlock *) m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[recoveryIndex].Block;

                    if (blockIndex == 0) // first block with meta
                    {
                        printMeta("SDRdaemonFECBuffer::writeData: recovered meta", (MetaDataFEC *) recoveredBlock);
//                        m_decoderSlots[decoderIndex].m_metaRetrieved = true;
                    }

                    m_decoderSlots[decoderIndex].m_originalBlocks[blockIndex] = *recoveredBlock;

                    qDebug() << "SDRdaemonFECBuffer::writeData: recovered block #" << blockIndex;
//                            << " i[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].i
//                            << " q[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].q
//                            << " i[1]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[1].i
//                            << " q[1]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[1].q
//                            << " i[2]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[2].i
//                            << " q[2]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[2].q;
                } // restore missing blocks
            } // CM256 decode
        } // revovery

        if (m_decoderSlots[decoderIndex].m_metaRetrieved) // block zero with its meta data has been received
        {
            MetaDataFEC *metaData = (MetaDataFEC *) &m_decoderSlots[decoderIndex].m_originalBlocks[0];

            if (!(*metaData == m_currentMeta))
            {
                int sampleRate =  metaData->m_sampleRate;

                if (sampleRate > 0) {
                    m_bufferLenSec = (float) m_framesNbBytes / (float) (sampleRate * m_iqSampleSize);
                    m_balCorrLimit = sampleRate / 1000; // +/- 1 ms correction max per read
                    m_readNbBytes = (sampleRate * m_iqSampleSize) / 20;
                }

                printMeta("SDRdaemonFECBuffer::writeData: new meta", metaData); // print for change other than timestamp
            }

            m_currentMeta = *metaData; // renew current meta
        } // check block 0
    } // decode
}

void SDRdaemonFECBuffer::writeData0(char *array, uint32_t length)
{
//    assert(length == m_udpPayloadSize);
//
//    bool dataAvailable = false;
//    SuperBlock *superBlock = (SuperBlock *) array;
//    int frameIndex = superBlock->header.frameIndex;
//    int decoderIndex = frameIndex % nbDecoderSlots;
//    int blockIndex = superBlock->header.blockIndex;
//
////    qDebug() << "SDRdaemonFECBuffer::writeData:"
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
//                //qDebug() << "SDRdaemonFECBuffer::writeData: new frame head (1): " << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                checkSlotData(decoderIndex);
//                dataAvailable = true;
//                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
//            }
//            else if (-frameDelta <= 65536 - nbDecoderSlots) // loss of sync start over
//            {
//                //qDebug() << "SDRdaemonFECBuffer::writeData: loss of sync start over (1)" << frameIndex << ":" << frameDelta << ":" << decoderIndex;
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
//                //qDebug() << "SDRdaemonFECBuffer::writeData: new frame head (2): " << frameIndex << ":" << frameDelta << ":" << decoderIndex;
//                m_decoderIndexHead = decoderIndex; // new decoder slot head
//                m_frameHead = frameIndex;
//                checkSlotData(decoderIndex);
//                dataAvailable = true;
//                initDecodeSlot(decoderIndex); // collect stats and re-initialize current slot
//            }
//            else if (frameDelta >= nbDecoderSlots) // loss of sync start over
//            {
//                //qDebug() << "SDRdaemonFECBuffer::writeData: loss of sync start over (2)" << frameIndex << ":" << frameDelta << ":" << decoderIndex;
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
//    int blockHead = m_decoderSlots[decoderIndex].m_blockCount;
//    int recoveryHead = m_decoderSlots[decoderIndex].m_recoveryCount;
//
//    if (blockHead < m_nbOriginalBlocks) // not enough blocks to decode -> store data
//    {
//        if (blockIndex == 0) // first block with meta
//        {
////            ProtectedBlockZero *blockZero = (ProtectedBlockZero *) &superBlock->protectedBlock;
////            m_decoderSlots[decoderIndex].m_blockZero = *blockZero;
//            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Block = (void *) &m_decoderSlots[decoderIndex].m_blockZero;
//            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
//        }
//        else if (blockIndex < m_nbOriginalBlocks) // normal block
//        {
//            m_frames[decoderIndex].m_blocks[blockIndex - 1] = superBlock->protectedBlock;
//            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Block = (void *) &m_frames[decoderIndex].m_blocks[blockIndex - 1];
//        }
//        else // redundancy block
//        {
//            m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryHead] = superBlock->protectedBlock;
//            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Block = (void *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryHead];
//            m_decoderSlots[decoderIndex].m_recoveryCount++;
//        }
//
//        m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockHead].Index = blockIndex;
//        m_decoderSlots[decoderIndex].m_blockCount++;
//    }
//    else if (!m_decoderSlots[decoderIndex].m_decoded) // ready to decode
//    {
//        if (m_decoderSlots[decoderIndex].m_recoveryCount > 0) // recovery data used
//        {
//            if (m_decoderSlots[decoderIndex].m_metaRetrieved) // block zero with its meta data has been received
//            {
////                m_paramsCM256.RecoveryCount = m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_nbFECBlocks;
//            }
//            else
//            {
//                m_paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks; // take last stored value for number of FEC blocks
//            }
//
//            if (cm256_decode(m_paramsCM256, m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks)) // failure to decode
//            {
//                qDebug() << "SDRdaemonFECBuffer::writeData: CM256 decode error:"
//                        << " BlockBytes: " << m_paramsCM256.BlockBytes
//                        << " OriginalCount: " << m_paramsCM256.OriginalCount
//                        << " RecoveryCount: " << m_paramsCM256.RecoveryCount
//                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
//            }
//            else // success to decode
//            {
//                int nbOriginalBlocks = m_decoderSlots[decoderIndex].m_blockCount - m_decoderSlots[decoderIndex].m_recoveryCount;
//
//                qDebug() << "SDRdaemonFECBuffer::writeData: CM256 decode success:"
//                        << " nbOriginalBlocks: " << nbOriginalBlocks
//                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
//
//                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // recover lost blocks
//                {
//                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[nbOriginalBlocks+ir].Index;
//
//                    if (blockIndex == 0)
//                    {
////                        ProtectedBlockZero *recoveredBlockZero = (ProtectedBlockZero *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[ir];
////                        printMeta("SDRdaemonFECBuffer::writeData: recovered meta", &recoveredBlockZero->m_metaData);
//                        // FEC does not work
////                        m_decoderSlots[decoderIndex].m_blockZero.m_metaData = recoveredBlockZero->m_metaData;
////                        m_decoderSlots[decoderIndex].m_metaRetrieved = true;
//                    }
//                    else
//                    {
//                        m_frames[decoderIndex].m_blocks[blockIndex - 1] =  m_decoderSlots[decoderIndex].m_recoveryBlocks[ir];
//                    }
//
//                    qDebug() << "SDRdaemonFECBuffer::writeData:"
//                            << " recovered block #" << blockIndex
//                            << " i[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].i
//                            << " q[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].q;
//                }
//            }
//        }
//
//        //printMeta("SDRdaemonFECBuffer::writeData", &m_decoderSlots[decoderIndex].m_blockZero.m_metaData);
//
////        if (m_decoderSlots[decoderIndex].m_metaRetrieved) // meta data has been retrieved (direct or recovery)
////        {
////            if (!(m_decoderSlots[decoderIndex].m_blockZero.m_metaData == m_currentMeta))
////            {
////                int sampleRate =  m_decoderSlots[decoderIndex].m_blockZero.m_metaData.m_sampleRate;
////
////                if (sampleRate > 0) {
////                    m_bufferLenSec = (float) m_framesNbBytes / (float) sampleRate;
////                }
////
////                printMeta("SDRdaemonFECBuffer::writeData: new meta", &m_decoderSlots[decoderIndex].m_blockZero.m_metaData); // print for change other than timestamp
////            }
////
////            m_currentMeta = m_decoderSlots[decoderIndex].m_blockZero.m_metaData; // renew current meta
////        }
////
//        m_decoderSlots[decoderIndex].m_decoded = true;
//    }
}

uint8_t *SDRdaemonFECBuffer::readData(int32_t length)
{
    uint8_t *buffer = (uint8_t *) m_frames;
    uint32_t readIndex = m_readIndex;

    m_nbReads++;

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
