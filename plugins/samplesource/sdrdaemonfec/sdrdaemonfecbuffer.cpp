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
        m_curNbRecovery(0),
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
	m_readNbBytes = 1;
    m_paramsCM256.BlockBytes = sizeof(ProtectedBlock); // never changes
    m_paramsCM256.OriginalCount = m_nbOriginalBlocks;  // never changes
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
        m_decoderSlots[i].m_totalCount = 0;
        m_decoderSlots[i].m_blockCount = 0;
        m_decoderSlots[i].m_originalCount = 0;
        m_decoderSlots[i].m_recoveryStartIndex = 0;
        m_decoderSlots[i].m_recoveryCount = 0;
        m_decoderSlots[i].m_decoded = false;
        m_decoderSlots[i].m_metaRetrieved = false;
        MetaDataFEC *metaData = (MetaDataFEC *) &m_decoderSlots[i].m_blockZero;
        metaData->init();
    }
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

    if (!m_decoderSlots[slotIndex].m_decoded)
    //if (m_decoderSlots[slotIndex].m_blockCount < m_nbOriginalBlocks)
    {
        qDebug() << "SDRdaemonFECBuffer::checkSlotData: incomplete frame:"
                << " m_blockCount: " << m_decoderSlots[slotIndex].m_blockCount
                << " m_recoveryCount: " << m_decoderSlots[slotIndex].m_recoveryCount;
    }

    if (m_decoderSlots[slotIndex].m_metaRetrieved) // meta data has been retrieved (direct or recovery)
    {
        MetaDataFEC *metaData = (MetaDataFEC *) &m_decoderSlots[slotIndex].m_blockZero;

        if (!(*metaData == m_currentMeta))
        {
            int sampleRate =  metaData->m_sampleRate;

            if (sampleRate > 0) {
                m_bufferLenSec = (float) m_framesNbBytes / (float) (sampleRate * m_iqSampleSize);
                m_balCorrLimit = sampleRate / 1000; // +/- 1 ms correction max per read
                m_readNbBytes = (sampleRate * m_iqSampleSize) / 20;
            }

            printMeta("SDRdaemonFECBuffer::checkSlotData: new meta", metaData); // print for change other than timestamp
        }

        m_currentMeta = *metaData; // renew current meta
    }
}

void SDRdaemonFECBuffer::initDecodeSlot(int slotIndex)
{
    // collect stats before voiding the slot
    m_curNbBlocks = m_decoderSlots[slotIndex].m_blockCount;
    m_curNbRecovery = m_decoderSlots[slotIndex].m_recoveryCount;
    m_avgNbBlocks(m_curNbBlocks);
    m_avgNbRecovery(m_curNbRecovery);
    // void the slot
    m_decoderSlots[slotIndex].m_totalCount = 0;
    m_decoderSlots[slotIndex].m_blockCount = 0;
    m_decoderSlots[slotIndex].m_originalCount = 0;
    m_decoderSlots[slotIndex].m_recoveryStartIndex = 0;
    m_decoderSlots[slotIndex].m_recoveryCount = 0;
    m_decoderSlots[slotIndex].m_decoded = false;
    m_decoderSlots[slotIndex].m_metaRetrieved = false;
    MetaDataFEC *metaData = (MetaDataFEC *) &m_decoderSlots[slotIndex].m_blockZero;
    metaData->init();
    memset((void *) m_frames[slotIndex].m_blocks, 0, (m_nbOriginalBlocks - 1) * sizeof(ProtectedBlock));
    memset((void *) m_decoderSlots[slotIndex].m_recoveryBlocks, 1, m_nbOriginalBlocks * sizeof(ProtectedBlock));
}

void SDRdaemonFECBuffer::writeData(char *array, uint32_t length)
{
    SuperBlock *superBlock = (SuperBlock *) array;
    int frameIndex = superBlock->header.frameIndex;
    int blockIndex = superBlock->header.blockIndex;
    int decoderIndex = frameIndex % nbDecoderSlots;

    if (m_frameHead == -1) // initial state
    {
        m_decoderIndexHead = decoderIndex; // new decoder slot head
        m_frameHead = frameIndex;
        initReadIndex(); // reset read index
        initDecodeAllSlots(); // initialize all slots
        m_blockIndex = 0;
    }
    else if (m_frameHead != frameIndex) // frame break => new frame starts
    {
        if (frameIndex != m_frameHead + 1)
        {
            qDebug() << "SDRdaemonFECBuffer::writeData: new frame problem start over: " << frameIndex << ":" << m_frameHead << ":" << blockIndex;
            m_decoderIndexHead = decoderIndex; // new decoder slot head
            m_frameHead = frameIndex;
            initReadIndex(); // reset read index
            initDecodeAllSlots(); // initialize all slots
        }
        else
        {
            m_decoderIndexHead = decoderIndex; // new decoder slot head
            m_frameHead = frameIndex;          // new frame head
            checkSlotData(decoderIndex);       // check slot before re-init
            rwCorrectionEstimate(decoderIndex);
            initDecodeSlot(decoderIndex);      // collect stats and re-initialize current slot
        }

        m_blockIndex = 0;
    }

    if (m_decoderSlots[decoderIndex].m_blockCount < m_nbOriginalBlocks) // not enough blocks to decode -> store data
    {
        int blockCount = m_decoderSlots[decoderIndex].m_blockCount;
        int recoveryCount = m_decoderSlots[decoderIndex].m_recoveryCount;
        m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Index = blockIndex;

        if (blockIndex == 0) // first block with meta
        {
            //qDebug() << "SDRdaemonFECBuffer::writeData(0): frameIndex: " << frameIndex << " blockIndex: " << blockIndex;
            m_decoderSlots[decoderIndex].m_blockZero = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_metaRetrieved = true;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_decoderSlots[decoderIndex].m_blockZero;
            m_decoderSlots[decoderIndex].m_originalCount++;
        }
        else if (blockIndex < m_nbOriginalBlocks) // original data
        {
            //qDebug() << "SDRdaemonFECBuffer::writeData(<128): frameIndex: " << frameIndex << " blockIndex: " << blockIndex;
            m_frames[decoderIndex].m_blocks[blockIndex - 1] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_frames[decoderIndex].m_blocks[blockIndex - 1];
            m_decoderSlots[decoderIndex].m_originalCount++;
        }
        else // recovery data
        {
            //qDebug() << "SDRdaemonFECBuffer::writeData(>=128): frameIndex: " << frameIndex << " blockCount: " << blockCount << " blockIndex: " << blockIndex;

            if (recoveryCount == 0)
            {
                m_decoderSlots[decoderIndex].m_recoveryStartIndex = blockCount;
            }

            m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount] = superBlock->protectedBlock;
            m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[blockCount].Block = (void *) &m_decoderSlots[decoderIndex].m_recoveryBlocks[recoveryCount];
            m_decoderSlots[decoderIndex].m_recoveryCount++;
        }

        if ((blockIndex > 0) && (blockIndex <= m_blockIndex))
        {
            qDebug() << "SDRdaemonFECBuffer::writeData: block out of sequence: " << blockIndex << ":" << m_blockIndex;
        }
    }

    m_decoderSlots[decoderIndex].m_blockCount++;
    m_blockIndex = blockIndex;

    if (m_decoderSlots[decoderIndex].m_blockCount == m_nbOriginalBlocks) // ready to decode
    {
        m_decoderSlots[decoderIndex].m_decoded = true;

        if (m_decoderSlots[decoderIndex].m_metaRetrieved) // block zero with its meta data has been received
        {
            m_currentMeta = *((MetaDataFEC *) &m_decoderSlots[decoderIndex].m_blockZero);
        }

        if (m_decoderSlots[decoderIndex].m_recoveryCount > 0) // recovery data used => need to decode FEC
        {
            m_paramsCM256.RecoveryCount = m_currentMeta.m_nbFECBlocks; // take last stored value for number of FEC blocks

            if (cm256_decode(m_paramsCM256, m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks)) // failure to decode
            {
                qDebug() << "SDRdaemonFECBuffer::writeData: CM256 decode error:"
                        << " BlockBytes: " << m_paramsCM256.BlockBytes
                        << " OriginalCount: " << m_paramsCM256.OriginalCount
                        << " RecoveryCount: " << m_paramsCM256.RecoveryCount
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;
            }
            else // success to decode
            {
                qDebug() << "SDRdaemonFECBuffer::writeData: CM256 decode success:"
                        << " m_recoveryStartIndex: " << m_decoderSlots[decoderIndex].m_recoveryStartIndex
                        << " m_originalCount: " << m_decoderSlots[decoderIndex].m_originalCount
                        << " m_recoveryCount: " << m_decoderSlots[decoderIndex].m_recoveryCount;

                for (int ir = 0; ir < m_decoderSlots[decoderIndex].m_recoveryCount; ir++) // recover lost blocks
                {
                    int blockIndex = m_decoderSlots[decoderIndex].m_cm256DescriptorBlocks[m_decoderSlots[decoderIndex].m_recoveryStartIndex+ir].Index;

                    if (blockIndex > 0) // ignore meta data for now
                    {
                        m_frames[decoderIndex].m_blocks[blockIndex - 1] =  m_decoderSlots[decoderIndex].m_recoveryBlocks[ir];
                    }

                    qDebug() << "SDRdaemonFECBuffer::writeData:"
                            << " recovered block #" << blockIndex
                            << " i[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].i
                            << " q[0]: " << m_decoderSlots[decoderIndex].m_recoveryBlocks[ir].samples[0].q;
                }
            } // recovery success
        } // revovery
    } // decode

    m_decoderSlots[decoderIndex].m_totalCount++;
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
