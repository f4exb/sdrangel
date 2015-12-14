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

#ifndef PLUGINS_CHANNEL_BFM_RDSPARSER_H_
#define PLUGINS_CHANNEL_BFM_RDSPARSER_H_

#include <string>
#include <set>

class RDSParser
{
public:
	RDSParser();
	~RDSParser();

	/**
	 * Parses a group retrieved by the decoder
	 */
	void parseGroup(unsigned int *group);

	void clearAllFields();
	void clearUpdateFlags();

	// PI data
	bool           m_pi_updated;
	unsigned int   m_pi_count;
	unsigned int   m_pi_program_identification;
	unsigned char  m_pi_program_type;
	bool           m_pi_traffic_program;
	int            m_pi_country_identification;
	int            m_pi_area_coverage_index;

	// G0 data
	bool           m_g0_updated;
	bool           m_g0_af_updated;
	unsigned int   m_g0_count;
	char           m_g0_program_service_name[9];
	bool           m_g0_traffic_announcement;
	bool           m_g0_music_speech;
	bool           m_g0_mono_stereo;
	bool           m_g0_artificial_head;
	bool           m_g0_compressed;
	bool           m_g0_static_pty;
	std::set<double> m_g0_alt_freq;

	// Static tables
	static const unsigned int offset_pos[5];
	static const unsigned int offset_word[5];
	static const unsigned int syndrome[5];
	static const char * const offset_name[];
	static const std::string pty_table[32];
	static const std::string pi_country_codes[15][5];
	static const std::string coverage_area_codes[16];
	static const std::string rds_group_acronyms[16];
	static const std::string language_codes[44];
	static const std::string tmc_duration[8][2];
	static const int optional_content_lengths[16];
	static const std::string label_descriptions[16];

private:
	double decode_af(unsigned int);
	void decode_optional_content(int, unsigned long int *);
	void decode_type0( unsigned int* group, bool B);
	void decode_type1( unsigned int* group, bool B);
	void decode_type2( unsigned int* group, bool B);
	void decode_type3( unsigned int* group, bool B);
	void decode_type4( unsigned int* group, bool B);
	void decode_type5( unsigned int* group, bool B);
	void decode_type6( unsigned int* group, bool B);
	void decode_type7( unsigned int* group, bool B);
	void decode_type8( unsigned int* group, bool B);
	void decode_type9( unsigned int* group, bool B);
	void decode_type10(unsigned int* group, bool B);
	void decode_type11(unsigned int* group, bool B);
	void decode_type12(unsigned int* group, bool B);
	void decode_type13(unsigned int* group, bool B);
	void decode_type14(unsigned int* group, bool B);
	void decode_type15(unsigned int* group, bool B);

	unsigned char  pi_country_identification;
	unsigned char  pi_program_reference_number;
	char           radiotext[64+1];

	bool           radiotext_AB_flag;
	bool           debug;
	bool           log;

};



#endif /* PLUGINS_CHANNEL_BFM_RDSPARSER_H_ */
