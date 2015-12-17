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

#ifndef PLUGINS_CHANNEL_BFM_RDSDECODER_H_
#define PLUGINS_CHANNEL_BFM_RDSDECODER_H_

#include <map>

class RDSDecoder
{
public:
	RDSDecoder();
	~RDSDecoder();

	bool frameSync(bool bit);
	unsigned int *getGroup() { return m_group; }
	bool synced() const { return m_sync == SYNC; }

	float          m_qua;
	float          m_corr;

protected:
	unsigned int calc_syndrome(unsigned long message, unsigned char mlen);
	bool err_corr(unsigned int received_crc, unsigned int calculated_crc, unsigned int& symbol);
	void enter_sync(unsigned int sync_block_number);
	void enter_no_sync();

private:
	typedef std::map<unsigned int, unsigned int> ui_lut_t;
	unsigned long m_reg;
	enum { NO_SYNC, SYNC } m_sync;
	bool m_presync;
	unsigned long  m_lastseenOffsetCounter;
	unsigned long  m_bitCounter;
	unsigned char  m_lastseenOffset;
	unsigned int   m_blockBitCounter;
	unsigned int   m_wrongBlocksCounter;
	unsigned int   m_correctedBlocksCounter;
	unsigned int   m_blocksCounter;
	unsigned int   m_groupGoodBlocksCounter;
	unsigned char  m_blockNumber;
	bool           m_groupAssemblyStarted;
	bool           m_goodBlock;
	unsigned int   m_group[4];
	ui_lut_t m_error_lut;

	/* see page 59, Annex C, table C.1 in the standard
	 * offset word C' has been put at the end */
	static const unsigned int offset_pos[5];
	static const unsigned int offset_word[5];
	static const unsigned int syndrome[5];
};



#endif /* PLUGINS_CHANNEL_BFM_RDSDECODER_H_ */
