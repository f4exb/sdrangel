///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef PLUGINS_CHANNEL_BFM_RDSPARSER_H_
#define PLUGINS_CHANNEL_BFM_RDSPARSER_H_

#include <string>
#include <set>
#include <map>

class RDSParser
{
public:
	typedef std::map<unsigned int, std::string> psns_map_t;
	typedef std::pair<unsigned int, std::string> psns_map_kv_t;
	typedef std::map<unsigned int, std::set<double> > freqs_map_t;
	typedef std::pair<unsigned int, std::set<double> > freqs_map_kv_t;
	typedef std::set<double> freqs_set_t;

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
	char           m_g0_program_service_name[8+1];
	unsigned char  m_g0_psn_bitmap;
	bool           m_g0_traffic_announcement;
	bool           m_g0_music_speech;
	bool           m_g0_mono_stereo;
	bool           m_g0_artificial_head;
	bool           m_g0_compressed;
	bool           m_g0_static_pty;
	std::set<double> m_g0_alt_freq;

	// G1 data
	bool           m_g1_updated;
	unsigned int   m_g1_count;
	int            m_g1_country_page_index; //!< negative if not received
	int   		   m_g1_country_index;      //!< negative if not received
	int            m_g1_language_index;     //!< negative if not received
	unsigned int   m_g1_pin_day;
	unsigned int   m_g1_pin_hour;
	unsigned int   m_g1_pin_minute;

	// G2 data
	bool           m_g2_updated;
	unsigned int   m_g2_count;
	char           m_g2_radiotext[64+1];
	bool           m_radiotext_AB_flag;

	// G3 data
	bool           m_g3_updated;
	unsigned int   m_g3_count;
	bool           m_g3_groupB;
	unsigned int   m_g3_appGroup;
	unsigned int   m_g3_message;
	unsigned int   m_g3_aid;

	// G4 data
	bool           m_g4_updated;
	unsigned int   m_g4_count;
	unsigned int   m_g4_hours;
	unsigned int   m_g4_minutes;
	unsigned int   m_g4_seconds;
	unsigned int   m_g4_year;
	unsigned int   m_g4_month;
	unsigned int   m_g4_day;
	double         m_g4_local_time_offset;

    // G5..G7 data
	bool           m_g5_updated;
	bool           m_g6_updated;
	bool           m_g7_updated;
	unsigned int   m_g5_count;
	unsigned int   m_g6_count;
	unsigned int   m_g7_count;

	// G8 data
	bool           m_g8_updated;
	unsigned int   m_g8_count;
	bool           m_g8_diversion_recommended;
	unsigned int   m_g8_dp_ci;
	bool           m_g8_sign;
	unsigned int   m_g8_extent;
	unsigned int   m_g8_event;
	unsigned int   m_g8_location;
	int            m_g8_label_index; //!< negative if not received
	int            m_g8_content;

	// G9 data
	bool           m_g9_updated;
	unsigned int   m_g9_count;
	unsigned int   m_g9_varA;
	unsigned int   m_g9_cA;
	unsigned int   m_g9_dA;
	unsigned int   m_g9_varB;
	unsigned int   m_g9_dB;

	// G10..G13 data
	bool           m_g10_updated;
	bool           m_g11_updated;
	bool           m_g12_updated;
	bool           m_g13_updated;
	unsigned int   m_g10_count;
	unsigned int   m_g11_count;
	unsigned int   m_g12_count;
	unsigned int   m_g13_count;

	// G14 data
	bool           m_g14_updated;
	unsigned int   m_g14_count;
	psns_map_t     m_g14_program_service_names; //!< key: PI, value: PSN
	freqs_map_t    m_g14_alt_freqs;             //!< key: PI, value: set of alternate frequencies (MHz)
	freqs_map_t    m_g14_mapped_freqs;          //!< key: PI, value: set of mapped frequencies (MHz)
	char           m_g14_program_service_name[8+1];
	std::set<double> m_g14_alt_freq_set;
	std::set<double> m_g14_mapped_freq_set;
	unsigned int   m_g14_psn_counter;
	bool           m_g14_data_available;

	// G15 data
	bool           m_g15_updated;
	unsigned int   m_g15_count;

	// Static tables
	static const unsigned int offset_pos[5];
	static const unsigned int offset_word[5];
	static const unsigned int syndrome[5];
	static const char * const offset_name[];
	static const std::string pty_table[32];
	static const std::string pi_country_codes[15][5];
	static const std::string coverage_area_codes[16];
	static const std::string rds_group_acronyms[16];
	static const std::string rds_group_acronym_tags[16];
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

	bool           debug;
	bool           log;

};



#endif /* PLUGINS_CHANNEL_BFM_RDSPARSER_H_ */
