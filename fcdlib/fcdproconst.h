/*
 * fcdproconst.h
 *
 *  Created on: Sep 7, 2015
 *      Author: f4exb
 */

#ifndef FCDLIB_FCDPROCONST_H_
#define FCDLIB_FCDPROCONST_H_

#include <string>

#include "export.h"

typedef enum
{
	FCDPRO_TLG_N5_0DB=0,
	FCDPRO_TLG_N2_5DB=1,
	FCDPRO_TLG_P0_0DB=4,
	FCDPRO_TLG_P2_5DB=5,
	FCDPRO_TLG_P5_0DB=6,
	FCDPRO_TLG_P7_5DB=7,
	FCDPRO_TLG_P10_0DB=8,
	FCDPRO_TLG_P12_5DB=9,
	FCDPRO_TLG_P15_0DB=10,
	FCDPRO_TLG_P17_5DB=11,
	FCDPRO_TLG_P20_0DB=12,
	FCDPRO_TLG_P25_0DB=13,
	FCDPRO_TLG_P30_0DB=14
} fcdpro_lna_gain_value;

typedef struct
{
	fcdpro_lna_gain_value value;
	std::string label;
} fcdpro_lna_gain;

typedef enum
{
	FCDPRO_TLE_OFF=0,
	FCDPRO_TLE_0=1,
	FCDPRO_TLE_1=3,
	FCDPRO_TLE_2=5,
	FCDPRO_TLE_3=7
} fcdpro_lna_enhance_value;

typedef struct
{
	fcdpro_lna_enhance_value value;
	std::string label;
} fcdpro_lna_enhance;

typedef enum
{
	FCDPRO_TB_VHF2,
	FCDPRO_TB_VHF3,
	FCDPRO_TB_UHF,
	FCDPRO_TB_LBAND
} fcdpro_band_value;

typedef struct
{
	fcdpro_band_value value;
	std::string label;
} fcdpro_band;

typedef enum
{
	// Band 0, VHF II
	FCDPRO_TRF_LPF268MHZ=0,
	FCDPRO_TRF_LPF299MHZ=8,
	// Band 1, VHF III
	FCDPRO_TRF_LPF509MHZ=0,
	FCDPRO_TRF_LPF656MHZ=8,
	// Band 2, UHF
	FCDPRO_TRF_BPF360MHZ=0,
	FCDPRO_TRF_BPF380MHZ=1,
	FCDPRO_TRF_BPF405MHZ=2,
	FCDPRO_TRF_BPF425MHZ=3,
	FCDPRO_TRF_BPF450MHZ=4,
	FCDPRO_TRF_BPF475MHZ=5,
	FCDPRO_TRF_BPF505MHZ=6,
	FCDPRO_TRF_BPF540MHZ=7,
	FCDPRO_TRF_BPF575MHZ=8,
	FCDPRO_TRF_BPF615MHZ=9,
	FCDPRO_TRF_BPF670MHZ=10,
	FCDPRO_TRF_BPF720MHZ=11,
	FCDPRO_TRF_BPF760MHZ=12,
	FCDPRO_TRF_BPF840MHZ=13,
	FCDPRO_TRF_BPF890MHZ=14,
	FCDPRO_TRF_BPF970MHZ=15,
	// Band 2, L band
	FCDPRO_TRF_BPF1300MHZ=0,
	FCDPRO_TRF_BPF1320MHZ=1,
	FCDPRO_TRF_BPF1360MHZ=2,
	FCDPRO_TRF_BPF1410MHZ=3,
	FCDPRO_TRF_BPF1445MHZ=4,
	FCDPRO_TRF_BPF1460MHZ=5,
	FCDPRO_TRF_BPF1490MHZ=6,
	FCDPRO_TRF_BPF1530MHZ=7,
	FCDPRO_TRF_BPF1560MHZ=8,
	FCDPRO_TRF_BPF1590MHZ=9,
	FCDPRO_TRF_BPF1640MHZ=10,
	FCDPRO_TRF_BPF1660MHZ=11,
	FCDPRO_TRF_BPF1680MHZ=12,
	FCDPRO_TRF_BPF1700MHZ=13,
	FCDPRO_TRF_BPF1720MHZ=14,
	FCDPRO_TRF_BPF1750MHZ=15
} fcdpro_rf_filter_value;

typedef struct
{
	fcdpro_rf_filter_value value;
	std::string label;
} fcdpro_rf_filter;

typedef enum
{
	FCDPRO_TMG_P4_0DB=0,
	FCDPRO_TMG_P12_0DB=1
} fcdpro_mixer_gain_value;

typedef struct
{
	fcdpro_mixer_gain_value value;
	std::string label;
} fcdpro_mixer_gain;

typedef enum
{
	FCDPRO_TBC_LBAND=0,
	FCDPRO_TBC_1=1,
	FCDPRO_TBC_2=2,
	FCDPRO_TBC_VUBAND=3
} fcdpro_bias_current_value;

typedef struct
{
	fcdpro_bias_current_value value;
	std::string label;
} fcdpro_bias_current;

typedef enum
{
	FCDPRO_TMF_27_0MHZ=0,
	FCDPRO_TMF_4_6MHZ=8,
	FCDPRO_TMF_4_2MHZ=9,
	FCDPRO_TMF_3_8MHZ=10,
	FCDPRO_TMF_3_4MHZ=11,
	FCDPRO_TMF_3_0MHZ=12,
	FCDPRO_TMF_2_7MHZ=13,
	FCDPRO_TMF_2_3MHZ=14,
	FCDPRO_TMF_1_9MHZ=15
} fcdpro_mixer_filter_value;

typedef struct
{
	fcdpro_mixer_filter_value value;
	std::string label;
} fcdpro_mixer_filter;

typedef enum
{
	FCDPRO_TIG1_N3_0DB=0,
	FCDPRO_TIG1_P6_0DB=1
} fcdpro_if_gain1_value;

typedef struct
{
	fcdpro_if_gain1_value value;
	std::string label;
} fcdpro_if_gain1;

typedef enum
{
	FCDPRO_TIGM_LINEARITY=0,
	FCDPRO_TIGM_SENSITIVITY=1
} fcdpro_if_gain_mode_value;

typedef struct
{
	fcdpro_if_gain_mode_value value;
	std::string label;
} fcdpro_if_gain_mode;

typedef enum
{
	FCDPRO_TIRF_21_4MHZ=0,
	FCDPRO_TIRF_21_0MHZ=1,
	FCDPRO_TIRF_17_6MHZ=2,
	FCDPRO_TIRF_14_7MHZ=3,
	FCDPRO_TIRF_12_4MHZ=4,
	FCDPRO_TIRF_10_6MHZ=5,
	FCDPRO_TIRF_9_0MHZ=6,
	FCDPRO_TIRF_7_7MHZ=7,
	FCDPRO_TIRF_6_4MHZ=8,
	FCDPRO_TIRF_5_3MHZ=9,
	FCDPRO_TIRF_4_4MHZ=10,
	FCDPRO_TIRF_3_4MHZ=11,
	FCDPRO_TIRF_2_6MHZ=12,
	FCDPRO_TIRF_1_8MHZ=13,
	FCDPRO_TIRF_1_2MHZ=14,
	FCDPRO_TIRF_1_0MHZ=15
} fcdpro_if_rc_filter_value;

typedef struct
{
	fcdpro_if_rc_filter_value value;
	std::string label;
} fcdpro_if_rc_filter;

typedef enum
{
	FCDPRO_TIG2_P0_0DB=0,
	FCDPRO_TIG2_P3_0DB=1,
	FCDPRO_TIG2_P6_0DB=2,
	FCDPRO_TIG2_P9_0DB=3
} fcdpro_if_gain2_value;

typedef struct
{
	fcdpro_if_gain2_value value;
	std::string label;
} fcdpro_if_gain2;

typedef enum
{
	FCDPRO_TIG3_P0_0DB=0,
	FCDPRO_TIG3_P3_0DB=1,
	FCDPRO_TIG3_P6_0DB=2,
	FCDPRO_TIG3_P9_0DB=3
} fcdpro_if_gain3_value;

typedef struct
{
	fcdpro_if_gain3_value value;
	std::string label;
} fcdpro_if_gain3;

typedef enum
{
	FCDPRO_TIG4_P0_0DB=0,
	FCDPRO_TIG4_P1_0DB=1,
	FCDPRO_TIG4_P2_0DB=2
} fcdpro_if_gain4_value;

typedef struct
{
	fcdpro_if_gain4_value value;
	std::string label;
} fcdpro_if_gain4;

typedef enum
{
	FCDPRO_TIF_5_50MHZ=0,
	FCDPRO_TIF_5_30MHZ=1,
	FCDPRO_TIF_5_00MHZ=2,
	FCDPRO_TIF_4_80MHZ=3,
	FCDPRO_TIF_4_60MHZ=4,
	FCDPRO_TIF_4_40MHZ=5,
	FCDPRO_TIF_4_30MHZ=6,
	FCDPRO_TIF_4_10MHZ=7,
	FCDPRO_TIF_3_90MHZ=8,
	FCDPRO_TIF_3_80MHZ=9,
	FCDPRO_TIF_3_70MHZ=10,
	FCDPRO_TIF_3_60MHZ=11,
	FCDPRO_TIF_3_40MHZ=12,
	FCDPRO_TIF_3_30MHZ=13,
	FCDPRO_TIF_3_20MHZ=14,
	FCDPRO_TIF_3_10MHZ=15,
	FCDPRO_TIF_3_00MHZ=16,
	FCDPRO_TIF_2_95MHZ=17,
	FCDPRO_TIF_2_90MHZ=18,
	FCDPRO_TIF_2_80MHZ=19,
	FCDPRO_TIF_2_75MHZ=20,
	FCDPRO_TIF_2_70MHZ=21,
	FCDPRO_TIF_2_60MHZ=22,
	FCDPRO_TIF_2_55MHZ=23,
	FCDPRO_TIF_2_50MHZ=24,
	FCDPRO_TIF_2_45MHZ=25,
	FCDPRO_TIF_2_40MHZ=26,
	FCDPRO_TIF_2_30MHZ=27,
	FCDPRO_TIF_2_28MHZ=28,
	FCDPRO_TIF_2_24MHZ=29,
	FCDPRO_TIF_2_20MHZ=30,
	FCDPRO_TIF_2_15MHZ=31
} fcdpro_if_filter_value;

typedef struct
{
	fcdpro_if_filter_value value;
	std::string label;
} fcdpro_if_filter;

typedef enum
{
	FCDPRO_TIG5_P3_0DB=0,
	FCDPRO_TIG5_P6_0DB=1,
	FCDPRO_TIG5_P9_0DB=2,
	FCDPRO_TIG5_P12_0DB=3,
	FCDPRO_TIG5_P15_0DB=4
} fcdpro_if_gain5_value;

typedef struct
{
	fcdpro_if_gain5_value value;
	std::string label;
} fcdpro_if_gain5;

typedef enum
{
	FCDPRO_TIG6_P3_0DB=0,
	FCDPRO_TIG6_P6_0DB=1,
	FCDPRO_TIG6_P9_0DB=2,
	FCDPRO_TIG6_P12_0DB=3,
	FCDPRO_TIG6_P15_0DB=4
} fcdpro_if_gain6_value;

typedef struct
{
	fcdpro_if_gain6_value value;
	std::string label;
} fcdpro_if_gain6;

class FCDLIB_API FCDProConstants
{
public:
	static const fcdpro_lna_gain lna_gains[];
	static const fcdpro_lna_enhance lna_enhances[];
	static const fcdpro_band bands[];
	static const fcdpro_rf_filter rf_filters[];
	static const fcdpro_mixer_gain mixer_gains[];
	static const fcdpro_bias_current bias_currents[];
	static const fcdpro_mixer_filter mixer_filters[];
	static const fcdpro_if_gain_mode if_gain_modes[];
	static const fcdpro_if_rc_filter if_rc_filters[];
	static const fcdpro_if_filter if_filters[];
	static const fcdpro_if_gain1 if_gains1[];
	static const fcdpro_if_gain2 if_gains2[];
	static const fcdpro_if_gain3 if_gains3[];
	static const fcdpro_if_gain4 if_gains4[];
	static const fcdpro_if_gain5 if_gains5[];
	static const fcdpro_if_gain6 if_gains6[];

	static int fcdpro_lna_gain_nb_values();
	static int fcdpro_lna_enhance_nb_values();
	static int fcdpro_band_nb_values();
	static int fcdpro_rf_filter_nb_values();
	static int fcdpro_mixer_gain_nb_values();
	static int fcdpro_bias_current_nb_values();
	static int fcdpro_mixer_filter_nb_values();
	static int fcdpro_if_gain_mode_nb_values();
	static int fcdpro_if_rc_filter_nb_values();
	static int fcdpro_if_filter_nb_values();
	static int fcdpro_if_gain1_nb_values();
	static int fcdpro_if_gain2_nb_values();
	static int fcdpro_if_gain3_nb_values();
	static int fcdpro_if_gain4_nb_values();
	static int fcdpro_if_gain5_nb_values();
	static int fcdpro_if_gain6_nb_values();
};

#endif /* FCDLIB_FCDPROCONST_H_ */
