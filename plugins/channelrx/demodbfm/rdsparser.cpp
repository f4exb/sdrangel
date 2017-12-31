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

#include "rdsparser.h"
#include "rdstmc.h"

#include <QDebug>
#include <string.h>
#include <sstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include "boost/format.hpp"


const unsigned int RDSParser::offset_pos[5] = {0,1,2,3,2};
const unsigned int RDSParser::offset_word[5] = {252,408,360,436,848};
const unsigned int RDSParser::syndrome[5] = {383,14,303,663,748};
const char * const RDSParser::offset_name[] = {"A","B","C","D","C'"};

/* page 77, Annex F in the standard */
const std::string RDSParser::pty_table[32] = {
	"None",
	"News",
	"Current Affairs",
	"Information",
	"Sport",
	"Education",
	"Drama",
	"Cultures",
	"Science",
	"Varied Speech",
	"Pop Music",
	"Rock Music",
	"Easy Listening",
	"Light Classics M",
	"Serious Classics",
	"Other Music",
	"Weather & Metr",
	"Finance",
	"Children’s Progs",
	"Social Affairs",
	"Religion",
	"Phone In",
	"Travel & Touring",
	"Leisure & Hobby",
	"Jazz Music",
	"Country Music",
	"National Music",
	"Oldies Music",
	"Folk Music",
	"Documentary",
	"Alarm Test",
	"Alarm-Alarm!"
};

/* page 71, Annex D, table D.1 in the standard */
const std::string RDSParser::pi_country_codes[15][5] = {
	{"DE","GR","MA","__","MD"},
	{"DZ","CY","CZ","IE","EE"},
	{"AD","SM","PL","TR","__"},
	{"IL","CH","VA","MK","__"},
	{"IT","JO","SK","__","__"},
	{"BE","FI","SY","__","UA"},
	{"RU","LU","TN","__","__"},
	{"PS","BG","__","NL","PT"},
	{"AL","DK","LI","LV","SI"},
	{"AT","GI","IS","LB","__"},
	{"HU","IQ","MC","__","__"},
	{"MT","GB","LT","HR","__"},
	{"DE","LY","YU","__","__"},
	{"__","RO","ES","SE","__"},
	{"EG","FR","NO","BY","BA"}
};

/* page 72, Annex D, table D.2 in the standard */
const std::string RDSParser::coverage_area_codes[16] = {
	"Local",
	"International",
	"National",
	"Supra-regional",
	"Regional 1",
	"Regional 2",
	"Regional 3",
	"Regional 4",
	"Regional 5",
	"Regional 6",
	"Regional 7",
	"Regional 8",
	"Regional 9",
	"Regional 10",
	"Regional 11",
	"Regional 12"
};

const std::string RDSParser::rds_group_acronyms[16] = {
	"BASIC",
	"PIN/SL",
	"RT",
	"AID",
	"CT",
	"TDC",
	"IH",
	"RP",
	"TMC",
	"EWS",
	"___",
	"___",
	"___",
	"___",
	"EON",
	"___"
};

const std::string RDSParser::rds_group_acronym_tags[16] = {
	"BAS",
	"PIN",
	"TXT",
	"AID",
	"TIM",
	"TDC",
	"IH_",
	"RP_",
	"TMC",
	"EWS",
	"___",
	"___",
	"___",
	"___",
	"EON",
	"___"
};

/* page 74, Annex E, table E.1 in the standard: that's the ASCII table!!! */

/* see page 84, Annex J in the standard */
const std::string RDSParser::language_codes[44] = {
	"N/A",
	"Albanian",
	"Breton",
	"Catalan",
	"Croatian",
	"Welsh",
	"Czech",
	"Danish",
	"German",
	"English",
	"Spanish",
	"Esperanto",
	"Estonian",
	"Basque",
	"Faroese",
	"French",
	"Frisian",
	"Irish",
	"Gaelic",
	"Galician",
	"Icelandic",
	"Italian",
	"Lappish",
	"Latin",
	"Latvian",
	"Luxembourgian",
	"Lithuanian",
	"Hungarian",
	"Maltese",
	"Dutch",
	"Norwegian",
	"Occitan",
	"Polish",
	"Portuguese",
	"Romanian",
	"Romansh",
	"Serbian",
	"Slovak",
	"Slovene",
	"Finnish",
	"Swedish",
	"Turkish",
	"Flemish",
	"Walloon"
};

/* see page 12 in ISO 14819-1 */
const std::string RDSParser::tmc_duration[8][2] =
{
	{"no duration given", "no duration given"},
	{"15 minutes", "next few hours"},
	{"30 minutes", "rest of the day"},
	{"1 hour", "until tomorrow evening"},
	{"2 hours", "rest of the week"},
	{"3 hours", "end of next week"},
	{"4 hours", "end of the month"},
	{"rest of the day", "long period"}
};

/* optional message content, data field lengths and labels
 * see page 15 in ISO 14819-1 */
const int RDSParser::optional_content_lengths[16] = {3,3,5,5,5,8,8,8,8,11,16,16,16,16,0,0};

const std::string RDSParser::label_descriptions[16] = {
	"Duration",
	"Control code",
	"Length of route affected",
	"Speed limit advice",
	"Quantifier",
	"Quantifier",
	"Supplementary information code",
	"Explicit start time",
	"Explicit stop time",
	"Additional event",
	"Detailed diversion instructions",
	"Destination",
	"RFU (Reserved for future use)",
	"Cross linkage to source of problem, or another route",
	"Separator",
	"RFU (Reserved for future use)"
};

RDSParser::RDSParser()
{
	clearAllFields();
}

RDSParser::~RDSParser()
{
}

void RDSParser::clearUpdateFlags()
{
	m_pi_updated = false;
	m_g0_updated = false;
	m_g0_af_updated = false;
	m_g1_updated = false;
	m_g2_updated = false;
	m_g3_updated = false;
	m_g4_updated = false;
	m_g5_updated = false;
	m_g6_updated = false;
	m_g7_updated = false;
	m_g8_updated = false;
	m_g9_updated = false;
	m_g10_updated = false;
	m_g11_updated = false;
	m_g12_updated = false;
	m_g13_updated = false;
	m_g14_updated = false;
	m_g14_data_available = false;
	m_g15_updated = false;
}

void RDSParser::clearAllFields()
{
	// PI data
	m_pi_count = 0;
	m_pi_program_identification = 0;
	m_pi_program_type = 0;
	m_pi_area_coverage_index = 0;
	m_pi_country_identification = 0;
	m_pi_traffic_program = false;

	// Group 00 data
	m_g0_count = 0;
	std::memset(m_g0_program_service_name, ' ', sizeof(m_g0_program_service_name));
	m_g0_program_service_name[sizeof(m_g0_program_service_name) - 1] = '\0';
	m_g0_psn_bitmap = 0;
	m_g0_traffic_announcement = false;
	m_g0_music_speech = false;
	m_g0_mono_stereo = false;
	m_g0_artificial_head = false;
	m_g0_compressed = false;
	m_g0_static_pty = false;
	m_g0_alt_freq.clear();

	// Group 01 data
	m_g1_count = 0;
	m_g1_country_page_index = -1;
	m_g1_country_index = -1;
	m_g1_language_index = -1;
	m_g1_pin_day = 0;
	m_g1_pin_hour = 0;
	m_g1_pin_minute = 0;

	// Group 02 data
	m_g2_count = 0;
	std::memset(m_g2_radiotext, ' ', sizeof(m_g2_radiotext));
	m_g2_radiotext[sizeof(m_g2_radiotext) - 1] = '\0';

	// Group 03 data
	m_g3_count = 0;
	m_g3_groupB = false;
	m_g3_appGroup = 0;
	m_g3_message = 0;
	m_g3_aid = 0;


	// Group 04 data
	m_g4_count = 0;
	m_g4_hours = 0;
	m_g4_minutes = 0;
	m_g4_seconds = 0;
	m_g4_year = 0;
	m_g4_month = 0;
	m_g4_day = 0;
	m_g4_local_time_offset = 0.0;

	// Group 05..07 data
	m_g5_count = 0;
	m_g6_count = 0;
	m_g7_count = 0;

	// Group 08 data
	m_g8_count = 0;
	m_g8_diversion_recommended = false;
	m_g8_dp_ci = 0;
	m_g8_sign = false;
	m_g8_extent = 0;
	m_g8_event = 0;
	m_g8_location = 0;
	m_g8_label_index = -1;
	m_g8_content = 0;

	// Group 09 data
	m_g9_varA = 0;
	m_g9_cA = 0;
	m_g9_dA = 0;
	m_g9_varB = 0;
	m_g9_dB = 0;

	// Group 10..13 data
	m_g9_count = 0;
	m_g10_count = 0;
	m_g11_count = 0;
	m_g12_count = 0;
	m_g13_count = 0;

	// Group 14
	m_g14_count = 0;
	m_g14_program_service_names.clear();
	m_g14_alt_freqs.clear();
	m_g14_mapped_freqs.clear();
	std::memset(m_g14_program_service_name, ' ', sizeof(m_g14_program_service_name));
	m_g14_program_service_name[8] = '\0';
	m_g14_alt_freq_set.clear();
	m_g14_mapped_freq_set.clear();
	m_g14_psn_counter = 0;

	// Group 15
	m_g15_count = 0;

	clearUpdateFlags();
}

void RDSParser::parseGroup(unsigned int *group)
{
	unsigned int group_type = (unsigned int)((group[1] >> 12) & 0xf);
	bool ab = (group[1] >> 11 ) & 0x1;

	/*
	qDebug() << "RDSParser::parseGroup:"
			<< " type: " << group_type << (ab ? 'B' :'A')
			<< " (" << rds_group_acronyms[group_type].c_str() << ")";*/

	m_pi_count++;
	m_pi_updated = true;

	m_pi_program_identification = group[0];                // "PI"
	m_pi_traffic_program        = (group[1] >> 10) & 0x01; // "TP"
	m_pi_program_type           = (group[1] >> 5) & 0x1f;  // "PTY"
	m_pi_country_identification = (m_pi_program_identification >> 12) & 0xf;
	m_pi_area_coverage_index = (m_pi_program_identification >> 8) & 0xf;

	/*
	std::string pistring = str(boost::format("%04X") % m_pi_program_identification);

	qDebug() << "RDSParser::parseGroup:"
			<< " PI:" << pistring.c_str()
			<< " - " << "PTY:" << pty_table[m_pi_program_type].c_str()
			<< " (country:" << (pi_country_codes[m_pi_country_identification - 1][0]).c_str()
			<< "/" << (pi_country_codes[m_pi_country_identification - 1][1]).c_str()
			<< "/" << (pi_country_codes[m_pi_country_identification - 1][2]).c_str()
			<< "/" << (pi_country_codes[m_pi_country_identification - 1][3]).c_str()
			<< "/" << (pi_country_codes[m_pi_country_identification - 1][4]).c_str()
			<< ", area:" << coverage_area_codes[m_pi_area_coverage_index].c_str()
			<< ", program:" << int(pi_program_reference_number) << ")";*/

	switch (group_type) {
		case 0:
			decode_type0(group, ab);
			break;
		case 1:
			decode_type1(group, ab);
			break;
		case 2:
			decode_type2(group, ab);
			break;
		case 3:
			decode_type3(group, ab);
			break;
		case 4:
			decode_type4(group, ab);
			break;
		case 5:
			decode_type5(group, ab);
			break;
		case 6:
			decode_type6(group, ab);
			break;
		case 7:
			decode_type7(group, ab);
			break;
		case 8:
			decode_type8(group, ab);
			break;
		case 9:
			decode_type9(group, ab);
			break;
		case 10:
			decode_type10(group, ab);
			break;
		case 11:
			decode_type11(group, ab);
			break;
		case 12:
			decode_type12(group, ab);
			break;
		case 13:
			decode_type13(group, ab);
			break;
		case 14:
			decode_type14(group, ab);
			break;
		case 15:
			decode_type15(group, ab);
			break;
	}

	/*
	#define HEX(a) std::hex << std::setfill('0') << std::setw(4) << long(a) << std::dec
	for(int i = 0; i < 4; i++) {
		dout << "  " << HEX(group[i]);
	}
	dout << std::endl;*/
}

/* BASIC TUNING: see page 21 of the standard */
void RDSParser::decode_type0(unsigned int *group, bool B)
{
	unsigned int af_code_1 = 0;
	unsigned int af_code_2 = 0;
	unsigned int  no_af    = 0;
	double af_1            = 0;
	double af_2            = 0;

	m_g0_count++;
	m_g0_updated = true;

	m_pi_traffic_program      = (group[1] >> 10) & 0x01;       // "TP"
	m_g0_traffic_announcement = (group[1] >>  4) & 0x01;       // "TA"
	m_g0_music_speech         = (group[1] >>  3) & 0x01;       // "MuSp"

	bool decoder_control_bit      = (group[1] >> 2) & 0x01; // "DI"
	unsigned char segment_address =  group[1] & 0x03;       // "DI segment"

	if (segment_address == 0)
	{
		std::memset(m_g0_program_service_name, ' ', sizeof(m_g0_program_service_name));
		m_g0_program_service_name[sizeof(m_g0_program_service_name) - 1] = '\0';
		m_g0_psn_bitmap = 0;
	}

	m_g0_program_service_name[segment_address * 2]     = (group[3] >> 8) & 0xff;
	m_g0_program_service_name[segment_address * 2 + 1] =  group[3]       & 0xff;
	m_g0_psn_bitmap |= 1<<segment_address;

	/* see page 41, table 9 of the standard */
	switch (segment_address)
	{
		case 0:
			m_g0_mono_stereo = decoder_control_bit;
			break;
		case 1:
			m_g0_artificial_head = decoder_control_bit;
			break;
		case 2:
			m_g0_compressed = decoder_control_bit;
			break;
		case 3:
			m_g0_static_pty = decoder_control_bit;
			break;
		default:
			break;
	}

	/* unused
	flagstring[0] = m_pi_traffic_program        ? '1' : '0';
	flagstring[1] = m_g0_traffic_announcement   ? '1' : '0';
	flagstring[2] = m_g0_music_speech           ? '1' : '0';
	flagstring[3] = m_g0_mono_stereo            ? '1' : '0';
	flagstring[4] = m_g0_artificial_head        ? '1' : '0';
	flagstring[5] = m_g0_compressed             ? '1' : '0';
	flagstring[6] = m_g0_static_pty             ? '1' : '0';*/

	static std::string af_string;

	if (!B)
	{ // type 0A
		af_code_1 = int(group[2] >> 8) & 0xff;
		af_code_2 = int(group[2])      & 0xff;
		af_1 = decode_af(af_code_1);
		af_2 = decode_af(af_code_2);

		if (af_1)
		{
            // @TODO: Find proper header or STL on OSX
#ifndef __APPLE__
            std::pair<std::_Rb_tree_const_iterator<double>, bool> res = m_g0_alt_freq.insert(af_1/1e3);
            m_g0_af_updated = m_g0_af_updated || res.second;
#endif
			no_af += 1;
		}

		if (af_2)
		{
            // @TODO: Find proper header or STL on OSX
#ifndef __APPLE__
            std::pair<std::_Rb_tree_const_iterator<double>, bool> res = m_g0_alt_freq.insert(af_2/1e3);
            m_g0_af_updated = m_g0_af_updated || res.second;
#endif
			no_af += 2;
		}

		/*
		std::string af1_string;
		std::string af2_string;

		// only AF1 => no_af==1, only AF2 => no_af==2, both AF1 and AF2 => no_af==3
		if(no_af)
		{
			if(af_1 > 80e3) {
				af1_string = str(boost::format("%2.2fMHz") % (af_1/1e3));
			} else if((af_1<2e3)&&(af_1>100)) {
				af1_string = str(boost::format("%ikHz") % int(af_1));
			}

			if(af_2 > 80e3) {
				af2_string = str(boost::format("%2.2fMHz") % (af_2/1e3));
			} else if ((af_2 < 2e3) && (af_2 > 100)) {
				af2_string = str(boost::format("%ikHz") % int(af_2));
			}
		}

		if(no_af == 1) {
			af_string = af1_string;
		} else if(no_af == 2) {
			af_string = af2_string;
		} else if(no_af == 3) {
			af_string = str(boost::format("%s, %s") % af1_string %af2_string);
		}*/
	}

	/*
	qDebug() << "RDSParser::decode_type0: "
			<< "\"" << std::string(m_g0_program_service_name, 8).c_str()
			<< "\" -" << (m_pi_traffic_program ? "TP" : "!TP")
			<< '-' << (m_g0_traffic_announcement ? "TA" : "!TA")
			<< '-' << (m_g0_music_speech ? "Music" : "Speech")
			<< '-' << (m_g0_mono_stereo ? "MONO" : "STEREO")
			<< " - AF:" << af_string.c_str();*/
}

double RDSParser::decode_af(unsigned int af_code)
{
	static bool vhf_or_lfmf             = 0; // 0 = vhf, 1 = lf/mf
	double alt_frequency                = 0; // in kHz

	if ((af_code == 0) ||                          // not to be used
		( af_code == 205) ||                      // filler code
		((af_code >= 206) && (af_code <= 223)) || // not assigned
		( af_code == 224) ||                      // No AF exists
		( af_code >= 251))                        // not assigned
	{
		alt_frequency   = 0;
	}

	if ((af_code >= 225) && (af_code <= 249))      // VHF frequencies follow
	{
		alt_frequency   = 0;
		vhf_or_lfmf     = 1;
	}

	if (af_code == 250)                            // an LF/MF frequency follows
	{
		alt_frequency   = 0;
		vhf_or_lfmf     = 0;
	}

	if ((af_code > 0) && (af_code < 205) && vhf_or_lfmf) {
		alt_frequency = 100.0 * (af_code + 875);          // VHF (87.6-107.9MHz)
	}
	else if ((af_code > 0) && (af_code < 16) && !vhf_or_lfmf) {
		alt_frequency = 153.0 + (af_code - 1) * 9;        // LF (153-279kHz)
	}
	else if ((af_code > 15) && (af_code < 136) && !vhf_or_lfmf) {
		alt_frequency = 531.0 + (af_code - 16) * 9 + 531; // MF (531-1602kHz)
	}

	return alt_frequency;
}

void RDSParser::decode_type1(unsigned int *group, bool B)
{
	int ecc    = 0;
	int paging = 0;
	char country_code           = (group[0] >> 12) & 0x0f;
	char radio_paging_codes     =  group[1]        & 0x1f;
	int variant_code            = (group[2] >> 12) & 0x7;
	unsigned int slow_labelling =  group[2]        & 0xfff;
	m_g1_pin_day    = (unsigned int)((group[3] >> 11) & 0x1f);
	m_g1_pin_hour   = (unsigned int)((group[3] >>  6) & 0x1f);
	m_g1_pin_minute = (unsigned int) (group[3]        & 0x3f);

	m_g1_count++;

	if (radio_paging_codes) {
		//qDebug() << "RDSParser::decode_type1: paging codes: " << int(radio_paging_codes) << " ";
	}

	if (m_g1_pin_day || m_g1_pin_hour || m_g1_pin_minute) {
		m_g1_updated = true;
		/*
		std::string s = str(boost::format("program item: %id, %i, %i ") % day % hour % minute);
		qDebug() << "RDSParser::decode_type1: " << s.c_str();*/
	}

	if (!B)
	{
		switch (variant_code)
		{
			case 0: // paging + ecc
				paging = (slow_labelling >> 8) & 0x0f;
				ecc    =  slow_labelling       & 0xff;
				if (paging) {
					//qDebug() << "RDSParser::decode_type1: " << "paging: " << paging << " ";
				}
				if ((ecc > 223) && (ecc < 229)) {
					m_g1_updated = true;
					m_g1_country_page_index = country_code - 1;
					m_g1_country_index = ecc - 224;
					/*
					qDebug() << "RDSParser::decode_type1: " << "extended country code: "
						<< (pi_country_codes[country_code-1][ecc-224]).c_str();*/
				} else {
					qDebug() << "RDSParser::decode_type1: " << "invalid extended country code: " << ecc;
				}
				break;
			case 1: // TMC identification
				//qDebug() << "RDSParser::decode_type1: TMC identification code received";
				break;
			case 2: // Paging identification
				//qDebug() << "RDSParser::decode_type1: Paging identification code received";
				break;
			case 3: // language codes
				if (slow_labelling < 44) {
					m_g1_updated = true;
					m_g1_language_index = slow_labelling;
					//qDebug() << "RDSParser::decode_type1: " << "language: " << language_codes[slow_labelling].c_str();
				} else {
					qDebug() << "RDSParser::decode_type1: " << "language: invalid language code " << slow_labelling;
				}
				break;
			default:
				break;
		}
	}
}

void RDSParser::decode_type2(unsigned int *group, bool B)
{
	unsigned char text_segment_address_code = group[1] & 0x0f;

	m_g2_updated = true;
	m_g2_count++;

	// when the A/B flag is toggled, flush your current radiotext
	if (radiotext_AB_flag != ((group[1] >> 4) & 0x01))
	{
		std::memset(m_g2_radiotext, ' ', sizeof(m_g2_radiotext));
		m_g2_radiotext[sizeof(m_g2_radiotext) - 1] = '\0';
	}

	radiotext_AB_flag = (group[1] >> 4) & 0x01;

	if (!B)
	{
		m_g2_radiotext[text_segment_address_code * 4    ] = (group[2] >> 8) & 0xff;
		m_g2_radiotext[text_segment_address_code * 4 + 1] =  group[2]       & 0xff;
		m_g2_radiotext[text_segment_address_code * 4 + 2] = (group[3] >> 8) & 0xff;
		m_g2_radiotext[text_segment_address_code * 4 + 3] =  group[3]       & 0xff;
	}
	else
	{
		m_g2_radiotext[text_segment_address_code * 2    ] = (group[3] >> 8) & 0xff;
		m_g2_radiotext[text_segment_address_code * 2 + 1] =  group[3]       & 0xff;
	}

	/*
	qDebug() << "RDSParser::decode_type2: " << "Radio Text " << (radiotext_AB_flag ? 'B' : 'A')
		<< ": " << std::string(m_g2_radiotext, sizeof(m_g2_radiotext)).c_str();*/
}

void RDSParser::decode_type3(unsigned int *group, bool B)
{
	if (B) {
		qDebug() << "RDSParser::decode_type3: type 3B not implemented yet";
		return;
	}

	int application_group = (group[1] >> 1) & 0xf;
	int group_type        =  group[1] & 0x1;
	int message           =  group[2];
	int aid               =  group[3];


	m_g3_updated = true;
	m_g3_count++;

	/*
	qDebug() << "RDSParser::decode_type3: aid group: " << application_group
		<< " " << (group_type ? 'B' : 'A');*/

	if ((application_group == 8) && (group_type == 0))
	{ // 8A
		int variant_code = (message >> 14) & 0x3;

		if (variant_code == 0)
		{
//			int ltn  = (message >> 6) & 0x3f; // location table number
//			bool afi = (message >> 5) & 0x1;  // alternative freq. indicator
//			bool M   = (message >> 4) & 0x1;  // mode of transmission
//			bool I   = (message >> 3) & 0x1;  // international
//			bool N   = (message >> 2) & 0x1;  // national
//			bool R   = (message >> 1) & 0x1;  // regional
//			bool U   =  message       & 0x1;  // urban

			/*
			qDebug() << "RDSParser::decode_type3: location table: " << ltn << " - "
				<< (afi ? "AFI-ON" : "AFI-OFF") << " - "
				<< (M   ? "enhanced mode" : "basic mode") << " - "
				<< (I   ? "international " : "")
				<< (N   ? "national " : "")
				<< (R   ? "regional " : "")
				<< (U   ? "urban" : "")
				<< " aid: " << aid;*/
		}
		else if (variant_code==1)
		{
//			int G   = (message >> 12) & 0x3;  // gap
//			int sid = (message >>  6) & 0x3f; // service identifier
//			int gap_no[4] = {3, 5, 8, 11};
			/*
			qDebug() << "RDSParser::decode_type3: gap: " << gap_no[G] << " groups, SID: "
				<< sid << " ";*/
		}
	}

	m_g3_groupB = group_type;
	m_g3_appGroup = application_group;
	m_g3_message = message;
	m_g3_aid = aid;

	//qDebug() << "RDSParser::decode_type3: message: " << message << " - aid: " << aid;
}

void RDSParser::decode_type4(unsigned int *group, bool B)
{
	if (B)
	{
		qDebug() << "RDSParser::decode_type4: type 4B not implemented yet";
		return;
	}

	m_g4_updated = true;
	m_g4_count++;

	m_g4_hours   = ((group[2] & 0x1) << 4) | ((group[3] >> 12) & 0x0f);
	m_g4_minutes =  (group[3] >> 6) & 0x3f;
	m_g4_local_time_offset = .5 * (group[3] & 0x1f);

	if ((group[3] >> 5) & 0x1) {
		m_g4_local_time_offset *= -1;
	}

	double modified_julian_date = ((group[1] & 0x03) << 15) | ((group[2] >> 1) & 0x7fff);

	m_g4_year  = int((modified_julian_date - 15078.2) / 365.25);
	m_g4_month = int((modified_julian_date - 14956.1 - int(m_g4_year * 365.25)) / 30.6001);
	m_g4_day   =               modified_julian_date - 14956 - int(m_g4_year * 365.25) - int(m_g4_month * 30.6001);
	bool K = ((m_g4_month == 14) || (m_g4_month == 15)) ? 1 : 0;
	m_g4_year += K;
	m_g4_month -= 1 + K * 12;

	/*
	std::string time = str(boost::format("%02i.%02i.%4i, %02i:%02i (%+.1fh)")\
		% m_g4_day % m_g4_month % (1900 + m_g4_year) % m_g4_hours % m_g4_minutes % m_g4_local_time_offset);

	qDebug() << "RDSParser::decode_type4: Clocktime: " << time.c_str();*/

	//send_message(5,time);
}

void RDSParser::decode_type5(unsigned int *group __attribute__((unused)), bool B __attribute__((unused))) {
	qDebug() << "RDSParser::decode_type5: type5 not implemented yet";
	m_g5_updated = true;
	m_g5_count++;
}

void RDSParser::decode_type6(unsigned int *group __attribute__((unused)), bool B __attribute__((unused))) {
	qDebug() << "RDSParser::decode_type6: type 6 not implemented yet";
	m_g6_updated = true;
	m_g6_count++;
}

void RDSParser::decode_type7(unsigned int *group __attribute__((unused)), bool B __attribute__((unused))) {
	qDebug() << "RDSParser::decode_type7: type 7 not implemented yet";
	m_g7_updated = true;
	m_g7_count++;
}

void RDSParser::decode_type8(unsigned int *group, bool B)
{
	if (B)
	{
		qDebug() << "RDSParser::decode_type8: type 8B not implemented yet";
		return;
	}

	m_g8_updated = true;
	m_g8_count++;

	bool T = (group[1] >> 4) & 0x1; // 0 = user message, 1 = tuning info
	bool F = (group[1] >> 3) & 0x1; // 0 = multi-group, 1 = single-group
	bool D = (group[2] > 15) & 0x1; // 1 = diversion recommended
	m_g8_diversion_recommended = D;

	static unsigned long int free_format[4];
	static int no_groups = 0;

	if (T)
	{ // tuning info
		qDebug() << "RDSParser::decode_type8: #tuning info# ";
		int variant = group[1] & 0xf;

		if((variant > 3) && (variant < 10)) {
			qDebug() << "RDSParser::decode_type8: variant: " << variant << " - "
				<< group[2] << " " << group[3];
		} else {
			qDebug() << "RDSParser::decode_type8: invalid variant: " << variant;
		}

	}
	else if (F || D)
	{ // single-group or 1st of multi-group
		m_g8_dp_ci            =  group[1]        & 0x7;   // duration & persistence or continuity index
		m_g8_sign             = (group[2] >> 14) & 0x1;   // event direction, 0 = +, 1 = -
		m_g8_extent           = (group[2] >> 11) & 0x7;   // number of segments affected
		m_g8_event            =  group[2]        & 0x7ff; // event code, defined in ISO 14819-2
		m_g8_location         =  group[3];                // location code, defined in ISO 14819-3

		qDebug() << "RDSParser::decode_type8: #user msg# " << (D ? "diversion recommended, " : "");

		if (F) {
			qDebug() << "RDSParser::decode_type8: single-grp, duration:" << (tmc_duration[m_g8_dp_ci][0]).c_str();
		} else {
			qDebug() << "RDSParser::decode_type8: multi-grp, continuity index:" << m_g8_dp_ci;
		}

		int event_line = RDSTMC::get_tmc_event_code_index(m_g8_event, 1);

		qDebug() << "RDSParser::decode_type8: extent:" << (m_g8_sign ? "-" : "") << m_g8_extent + 1 << " segments"
			<< ", event" << m_g8_event << ":" << RDSTMC::get_tmc_events(event_line, 1).c_str()
			<< ", location:" << m_g8_location;

	}
	else
	{ // 2nd or more of multi-group
		unsigned int ci = group[1] & 0x7;          // countinuity index
		bool sg = (group[2] >> 14) & 0x1;          // second group
		unsigned int gsi = (group[2] >> 12) & 0x3; // group sequence

		qDebug() << "RDSParser::decode_type8: #user msg# multi-grp, continuity index:" << ci
			<< (sg ? ", second group" : "") << ", gsi:" << gsi;

		qDebug() << "RDSParser::decode_type8: free format: " << (group[2] & 0xfff) << " "
			<< group[3];
		// it's not clear if gsi=N-2 when gs=true

		if (sg) {
			no_groups = gsi;
		}

		free_format[gsi] = ((group[2] & 0xfff) << 12) | group[3];

		if (gsi == 0) {
			decode_optional_content(no_groups, free_format);
		}
	}
}

void RDSParser::decode_optional_content(int no_groups, unsigned long int *free_format)
{
	int content_length = 0;
	int ff_pointer     = 0;

	for (int i = no_groups; i == 0; i--)
	{
		ff_pointer = 12 + 16;

		while(ff_pointer > 0)
		{
			ff_pointer -= 4;
			m_g8_label_index = (free_format[i] & (0xf << ff_pointer));
			content_length = optional_content_lengths[m_g8_label_index];
			ff_pointer -= content_length;
			m_g8_content = (free_format[i] & (int(std::pow(2, content_length) - 1) << ff_pointer));

			/*
			qDebug() << "RDSParser::decode_optional_content: TMC optional content (" << label_descriptions[m_g8_label_index].c_str()
				<< "):" << m_g8_content;*/
		}
	}
}

void RDSParser::decode_type9(unsigned int *group, bool B){

	if (B)
	{
		m_g9_varB = group[1] & 0x1f;
		m_g9_dB   = group[3];
	}
	else
	{
		m_g9_varA = group[1] & 0x1f;
		m_g9_cA   = group[2];
		m_g9_dA   = group[3];
	}

	m_g9_updated = true;
	m_g9_count++;
}

void RDSParser::decode_type10(unsigned int *group __attribute__((unused)), bool B __attribute__((unused)))
{
	qDebug() << "RDSParser::decode_type10: type 10 not implemented yet";
	m_g10_updated = true;
	m_g10_count++;
}

void RDSParser::decode_type11(unsigned int *group __attribute__((unused)), bool B __attribute__((unused)))
{
	qDebug() << "RDSParser::decode_type11: type 11 not implemented yet";
	m_g11_updated = true;
	m_g11_count++;
}

void RDSParser::decode_type12(unsigned int *group __attribute__((unused)), bool B __attribute__((unused)))
{
	qDebug() << "RDSParser::decode_type12: type 12 not implemented yet";
	m_g12_updated = true;
	m_g12_count++;
}

void RDSParser::decode_type13(unsigned int *group __attribute__((unused)), bool B __attribute__((unused)))
{
	qDebug() << "RDSParser::decode_type13: type 13 not implemented yet";
	m_g13_updated = true;
	m_g13_count++;
}

void RDSParser::decode_type14(unsigned int *group, bool B)
{
	char variant_code        = group[1] & 0x0f;
	unsigned int information = group[2];
	unsigned int pi_on       = group[3];

	bool ta_on = 0;
	double af_1 = 0;
	double af_2 = 0;

	m_g14_updated = true;
	m_g14_count++;

	if (!B)
	{
		switch (variant_code)
		{
			case 0: // PS(ON)
			case 1: // PS(ON)
			case 2: // PS(ON)
			case 3: // PS(ON)
			{
				m_g14_program_service_name[variant_code * 2    ] = (information >> 8) & 0xff;
				m_g14_program_service_name[variant_code * 2 + 1] =  information       & 0xff;
				m_g14_psn_counter++;
				//qDebug() << "RDSParser::decode_type14: PS(ON): \"" << std::string(m_g14_program_service_name, 8).c_str() << "\"";
				break;
			}
			case 4: // AF
			{
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 100.0 * ((information & 0xff) + 875);
				m_g14_alt_freq_set.insert(af_1/1000.0);
				m_g14_alt_freq_set.insert(af_2/1000.0);
				/*
				std::string s = str(boost::format("AF:%3.2fMHz %3.2fMHz") % (af_1/1000) % (af_2/1000));
				qDebug() << "RDSParser::decode_type14: " << s.c_str();*/
				break;
			}
			case 5: // mapped frequencies
			case 6: // mapped frequencies
			case 7: // mapped frequencies
			case 8: // mapped frequencies
			{
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 100.0 * ((information & 0xff) + 875);
				m_g14_mapped_freq_set.insert(af_2/1000.0);
				/*
				std::string s = str(boost::format("TN:%3.2fMHz - ON:%3.2fMHz") % (af_1/1000) % (af_2/1000));
				qDebug() << "RDSParser::decode_type14: " << s.c_str();*/
				break;
			}
			case 9: // mapped frequencies (AM)
			{
				af_1 = 100.0 * (((information >> 8) & 0xff) + 875);
				af_2 = 9.0 * ((information & 0xff) - 16) + 531;
				m_g14_mapped_freq_set.insert(af_2/1000.0);
				/*
				std::string s = str(boost::format("TN:%3.2fMHz - ON:%ikHz") % (af_1/1000) % int(af_2));
				qDebug() << "RDSParser::decode_type14: " << s.c_str();*/
				break;
			}
			case 10: // unallocated
				break;
			case 11: // unallocated
				break;
			case 12: // linkage information
			{
				if (m_g14_psn_counter == 4)
				{
					//qDebug("RDSParser::decode_type14: m_g14_psn_updated: %d", m_g14_psn_counter);
					std::pair<psns_map_t::iterator, bool> ret = m_g14_program_service_names.insert(psns_map_kv_t(pi_on, std::string(m_g14_program_service_name)));
					std::memset(m_g14_program_service_name, ' ', sizeof(m_g14_program_service_name));
					m_g14_program_service_name[8] = '\0';
					m_g14_psn_counter = 0;
					m_g14_data_available = ret.second;
				}

				if (m_g14_alt_freq_set.size() > 0)
				{
					//qDebug("RDSParser::decode_type14: m_g14_alt_freq_set updated");

					std::pair<freqs_map_t::iterator, bool> retMap;
					std::pair<freqs_set_t::iterator, bool> retSet;
					bool updated = false;

					freqs_map_t::iterator mIt = m_g14_alt_freqs.find(pi_on);

					if (mIt == m_g14_alt_freqs.end()) // key does not exist yet => insert the whole set
					{
						retMap = m_g14_alt_freqs.insert(freqs_map_kv_t(pi_on, m_g14_alt_freq_set));
						updated |= retMap.second;
					}
					else // merge sets
					{
						freqs_set_t::iterator sIt = m_g14_alt_freq_set.begin();
						const freqs_set_t::iterator sItEnd = m_g14_alt_freq_set.end();

						for (; sIt != sItEnd; ++sIt)
						{
							retSet = (mIt->second).insert(*sIt);
							updated |= retSet.second;
						}
					}

					m_g14_alt_freq_set.clear();
					m_g14_data_available |= updated;
				}

				if (m_g14_mapped_freq_set.size() > 0)
				{
					//qDebug("RDSParser::decode_type14: m_g14_mapped_freq_set updated");

					std::pair<freqs_map_t::iterator, bool> retMap;
					std::pair<freqs_set_t::iterator, bool> retSet;
					bool updated = false;

					freqs_map_t::iterator mIt = m_g14_mapped_freqs.find(pi_on);

					if (mIt == m_g14_mapped_freqs.end()) // key does not exist yet => insert the whole set
					{
						retMap = m_g14_mapped_freqs.insert(freqs_map_kv_t(pi_on, m_g14_mapped_freq_set));
						updated |= retMap.second;
					}
					else // merge sets
					{
						freqs_set_t::iterator sIt = m_g14_mapped_freq_set.begin();
						const freqs_set_t::iterator sItEnd = m_g14_mapped_freq_set.end();

						for (; sIt != sItEnd; ++sIt)
						{
							retSet = (mIt->second).insert(*sIt);
							updated |= retSet.second;
						}
					}

					m_g14_mapped_freq_set.clear();
					m_g14_data_available |= updated;
				}

				/*
				std::string s = str(boost::format("Linkage information: %x%x") % ((information >> 8) & 0xff) % (information & 0xff));
				qDebug() << "RDSParser::decode_type14: " << s.c_str();*/
				break;
			}
			case 13: // PTY(ON), TA(ON)
			{
				ta_on = information & 0x01;
				//qDebug() << "RDSParser::decode_type14: PTY(ON):" << pty_table[int(pty_on)].c_str();
				if(ta_on) {
					qDebug() << "RDSParser::decode_type14:  - TA";
				}
				break;
			}
			case 14: // PIN(ON)
			{
				/*
				std::string s = str(boost::format("PIN(ON):%x%x") % ((information >> 8) & 0xff) % (information & 0xff));
				qDebug() << "RDSParser::decode_type14: " << s.c_str();*/
				break;
			}
			case 15: // Reserved for broadcasters use
				break;
			default:
				//qDebug() << "RDSParser::decode_type14: invalid variant code:" << variant_code;
				break;
		}
	}

	/*
	if (pi_on)
	{
		std::string pistring = str(boost::format("%04X") % pi_on);
		qDebug() << "RDSParser::decode_type14: PI(ON):" << pistring.c_str();

		if (tp_on) {
			qDebug() << "RDSParser::decode_type14: TP(ON)";
		}
	}*/
}

void RDSParser::decode_type15(unsigned int *group __attribute__((unused)), bool B __attribute__((unused)))
{
	qDebug() << "RDSParser::decode_type5: type 15 not implemented yet";
	m_g15_updated = true;
	m_g15_count++;
}

