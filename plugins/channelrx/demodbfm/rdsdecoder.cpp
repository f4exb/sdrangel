///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "rdsdecoder.h"

#include <QDebug>
#include <string.h>

const unsigned int RDSDecoder::offset_pos[5] = {0,1,2,3,2};
const unsigned int RDSDecoder::offset_word[5] = {252,408,360,436,848};
const unsigned int RDSDecoder::syndrome[5] = {383,14,303,663,748};

RDSDecoder::RDSDecoder()
{
	m_reg = 0;
	m_sync = NO_SYNC;
	m_presync = false;
	m_lastseenOffsetCounter = 0;
	m_bitCounter = 0;
	m_lastseenOffset = 0;
	m_wrongBlocksCounter   = 0;
	m_blocksCounter        = 0;
	m_blockBitCounter      = 0;
	m_blockNumber          = 0;
	m_groupAssemblyStarted = false;
	m_sync                 = SYNC;
	m_groupGoodBlocksCounter = 0;
	m_wrongBlocksCounter   = 0;
	m_goodBlock            = false;
	m_qua                  = 0.0f;
	memset(m_group, 0, 4*sizeof(int));
}

RDSDecoder::~RDSDecoder()
{
}

bool RDSDecoder::frameSync(bool bit)
{
	bool group_ready = false;
	unsigned int reg_syndrome;
	unsigned long bit_distance, block_distance;
	unsigned int block_calculated_crc, block_received_crc, checkword, dataword;

	m_reg = (m_reg<<1) | bit;

	switch (m_sync)
	{
	case NO_SYNC:
		reg_syndrome = calc_syndrome(m_reg,26);

		for (int j = 0; j < 5; j++)
		{
			if (reg_syndrome == syndrome[j])
			{
				if (!m_presync)
				{
					m_lastseenOffset = j;
					m_lastseenOffsetCounter = m_bitCounter;
					m_presync=true;
				}
				else
				{
					bit_distance = m_bitCounter - m_lastseenOffsetCounter;

					if (offset_pos[m_lastseenOffset] >= offset_pos[j])
					{
						block_distance = offset_pos[j] + 4 - offset_pos[m_lastseenOffset];
					}
					else
					{
						block_distance = offset_pos[j] - offset_pos[m_lastseenOffset];
					}

					if ((block_distance*26)!=bit_distance)
					{
						m_presync = false;
					}
					else
					{
						//qDebug("RDSDecoder::frameSync: Sync State Detected");
						enter_sync(j);
					}
				}

				break; //syndrome found, no more cycles
			}
		}
		break;

	case SYNC:
		// wait until 26 bits enter the buffer
		if (m_blockBitCounter < 25)
		{
			m_blockBitCounter++;
		}
		else
		{
			m_goodBlock = false;
			dataword = (m_reg>>10) & 0xffff;
			block_calculated_crc = calc_syndrome(dataword, 16);
			checkword = m_reg & 0x3ff;

			// manage special case of C or C' offset word
			if (m_blockNumber == 2)
			{
				block_received_crc = checkword ^ offset_word[m_blockNumber];

				if (block_received_crc==block_calculated_crc)
				{
					m_goodBlock = true;
				}
				else
				{
					block_received_crc=checkword^offset_word[4];

					if (block_received_crc==block_calculated_crc)
					{
						m_goodBlock = true;
					}
					else
					{
						m_wrongBlocksCounter++;
						m_goodBlock = false;
					}
				}
			}
			else
			{
				block_received_crc = checkword ^ offset_word[m_blockNumber];

				if (block_received_crc==block_calculated_crc)
				{
					m_goodBlock = true;
				}
				else
				{
					m_wrongBlocksCounter++;
					m_goodBlock = false;
				}
			}

			//done checking CRC
			if (m_blockNumber == 0 && m_goodBlock)
			{
				m_groupAssemblyStarted = true;
				m_groupGoodBlocksCounter = 1;
			}

			if (m_groupAssemblyStarted)
			{
				if (!m_goodBlock)
				{
					m_groupAssemblyStarted = false;
				}
				else
				{
					m_group[m_blockNumber] = dataword;
					m_groupGoodBlocksCounter++;
				}

				if (m_groupGoodBlocksCounter == 5)
				{
					group_ready = true; //decode_group(group); pass on to the group parser
				}
			}

			m_blockBitCounter = 0;
			m_blockNumber = (m_blockNumber + 1) % 4;
			m_blocksCounter++;

			// 1187.5 bps / 104 bits = 11.4 groups/sec, or 45.7 blocks/sec
			if (m_blocksCounter == 50)
			{
				if (m_wrongBlocksCounter > 35)
				{
					/*
					qDebug() << "RDSDecoder::frameSync: Lost Sync (Got " << m_wrongBlocksCounter
						<< " bad blocks on " << m_blocksCounter
						<< " total)";*/
					enter_no_sync();
				}
				else
				{
					/*
					qDebug() << "RDSDecoder::frameSync: Still Sync-ed (Got " << m_wrongBlocksCounter
						<< " bad blocks on " << m_blocksCounter
						<< " total)";*/
				}

				m_qua = 2.0 * (50 - m_wrongBlocksCounter);
				m_blocksCounter = 0;
				m_wrongBlocksCounter = 0;
			}
		}
		break;
	default:
		break;
	}

	m_bitCounter++;

	return group_ready;
}

////////////////////////// HELPER FUNTIONS /////////////////////////

void RDSDecoder::enter_sync(unsigned int sync_block_number)
{
	m_wrongBlocksCounter   = 0;
	m_blocksCounter        = 0;
	m_blockBitCounter      = 0;
	m_blockNumber          = (sync_block_number + 1) % 4;
	m_groupAssemblyStarted = false;
	m_sync                 = SYNC;
}

void RDSDecoder::enter_no_sync()
{
	m_presync = false;
	m_sync = NO_SYNC;
}

/**
 *  see Annex B, page 64 of the standard
 */
unsigned int RDSDecoder::calc_syndrome(unsigned long message, unsigned char mlen)
{
	unsigned long reg = 0;
	unsigned int i;
	const unsigned long poly = 0x5B9;
	const unsigned char plen = 10;

	for (i = mlen; i > 0; i--)
	{
		reg = (reg << 1) | ((message >> (i-1)) & 0x01);
		if (reg & (1 << plen)) reg = reg ^ poly;
	}

	for (i = plen; i > 0; i--)
	{
		reg = reg << 1;
		if (reg & (1<<plen)) {
			reg = reg ^ poly;
		}
	}

	return (reg & ((1<<plen)-1));	// select the bottom plen bits of reg
}

