/*
 * fcdproconst.cpp
 *
 *  Created on: Sep 7, 2015
 *      Author: f4exb
 */

#include "fcdproconst.h"

const fcdpro_lna_gain FCDProConstants::lna_gains[] = {
		{FCDPRO_TLG_N5_0DB, "-5dB"},
		{FCDPRO_TLG_N2_5DB, "-2.5dB"},
		{FCDPRO_TLG_P0_0DB, "0dB"},
		{FCDPRO_TLG_P2_5DB, "2.5dB"},
		{FCDPRO_TLG_P5_0DB, "5dB"},
		{FCDPRO_TLG_P7_5DB, "7.5dB"},
		{FCDPRO_TLG_P10_0DB, "10dB"},
		{FCDPRO_TLG_P12_5DB, "12.5dB"},
		{FCDPRO_TLG_P15_0DB, "15dB"},
		{FCDPRO_TLG_P17_5DB, "17.5dB"},
		{FCDPRO_TLG_P20_0DB, "20dB"},
		{FCDPRO_TLG_P25_0DB, "25dB"},
		{FCDPRO_TLG_P30_0DB, "30dB"}
};

int FCDProConstants::fcdpro_lna_gain_nb_values()
{
	return sizeof(lna_gains) / sizeof(fcdpro_lna_gain);
}

const fcdpro_lna_enhance FCDProConstants::lna_enhances[] = {
		{FCDPRO_TLE_OFF, "Off"},
		{FCDPRO_TLE_0, "0"},
		{FCDPRO_TLE_1, "1"},
		{FCDPRO_TLE_2, "2"},
		{FCDPRO_TLE_3, "3"}
};

int FCDProConstants::fcdpro_lna_enhance_nb_values()
{
	return sizeof(lna_enhances) / sizeof(fcdpro_lna_enhance);
}

const fcdpro_band FCDProConstants::bands[] = {
		{FCDPRO_TB_VHF2, "VHF2"},
		{FCDPRO_TB_VHF3, "VHF3"},
		{FCDPRO_TB_UHF, "UHF"},
		{FCDPRO_TB_LBAND, "L"}
};

int FCDProConstants::fcdpro_band_nb_values()
{
	return sizeof(bands) / sizeof(fcdpro_band);
}

const fcdpro_rf_filter FCDProConstants::rf_filters[] = {
		// Band 0, VHF II
		{FCDPRO_TRF_LPF268MHZ, "LP268M"},
		{FCDPRO_TRF_LPF299MHZ, "LP299M"},
		// Band 1, VHF III
		{FCDPRO_TRF_LPF509MHZ, "LP509M"},
		{FCDPRO_TRF_LPF656MHZ, "LP656M"},
		// Band 2, UHF
		{FCDPRO_TRF_BPF360MHZ, "BP360M"},
		{FCDPRO_TRF_BPF380MHZ, "BP390M"},
		{FCDPRO_TRF_BPF405MHZ, "BP405M"},
		{FCDPRO_TRF_BPF425MHZ, "BP425M"},
		{FCDPRO_TRF_BPF450MHZ, "BP450M"},
		{FCDPRO_TRF_BPF475MHZ, "BP475M"},
		{FCDPRO_TRF_BPF505MHZ, "BP505M"},
		{FCDPRO_TRF_BPF540MHZ, "BP540M"},
		{FCDPRO_TRF_BPF575MHZ, "BP575M"},
		{FCDPRO_TRF_BPF615MHZ, "BP615M"},
		{FCDPRO_TRF_BPF670MHZ, "BP670M"},
		{FCDPRO_TRF_BPF720MHZ, "BP720M"},
		{FCDPRO_TRF_BPF760MHZ, "BP760M"},
		{FCDPRO_TRF_BPF840MHZ, "BP840M"},
		{FCDPRO_TRF_BPF890MHZ, "BP890M"},
		{FCDPRO_TRF_BPF970MHZ, "BP970M"},
		// Band 2, L band
		{FCDPRO_TRF_BPF1300MHZ, "BP1.3G"},
		{FCDPRO_TRF_BPF1320MHZ, "BP1.32G"},
		{FCDPRO_TRF_BPF1360MHZ, "BP1.36G"},
		{FCDPRO_TRF_BPF1410MHZ, "BP1.41G"},
		{FCDPRO_TRF_BPF1445MHZ, "BP1445M"},
		{FCDPRO_TRF_BPF1460MHZ, "BP1.46G"},
		{FCDPRO_TRF_BPF1490MHZ, "BP1.49G"},
		{FCDPRO_TRF_BPF1530MHZ, "BP1.53G"},
		{FCDPRO_TRF_BPF1560MHZ, "BP1.56G"},
		{FCDPRO_TRF_BPF1590MHZ, "BP1.59G"},
		{FCDPRO_TRF_BPF1640MHZ, "BP1.64G"},
		{FCDPRO_TRF_BPF1660MHZ, "BP1.66G"},
		{FCDPRO_TRF_BPF1680MHZ, "BP1.68G"},
		{FCDPRO_TRF_BPF1700MHZ, "BP1.7G"},
		{FCDPRO_TRF_BPF1720MHZ, "BP1.72G"},
		{FCDPRO_TRF_BPF1750MHZ, "BP1.75G"}
};

int FCDProConstants::fcdpro_rf_filter_nb_values()
{
	return sizeof(rf_filters) / sizeof(fcdpro_rf_filter);
}

const fcdpro_mixer_gain FCDProConstants::mixer_gains[] = {
		{FCDPRO_TMG_P4_0DB, "0dB"},
		{FCDPRO_TMG_P12_0DB, "12dB"}
};

int FCDProConstants::fcdpro_mixer_gain_nb_values()
{
	return sizeof(mixer_gains) / sizeof(fcdpro_mixer_gain);
}

const fcdpro_bias_current FCDProConstants::bias_currents[] = {
		{FCDPRO_TBC_LBAND, "L"},
		{FCDPRO_TBC_1, "1"},
		{FCDPRO_TBC_2, "2"},
		{FCDPRO_TBC_VUBAND, "VU"}
};

int FCDProConstants::fcdpro_bias_current_nb_values()
{
	return sizeof(bias_currents) / sizeof(fcdpro_bias_current);
}

const fcdpro_mixer_filter FCDProConstants::mixer_filters[] = {
		{FCDPRO_TMF_27_0MHZ, "27M"},
		{FCDPRO_TMF_4_6MHZ, "4.6M"},
		{FCDPRO_TMF_4_2MHZ, "4.2M"},
		{FCDPRO_TMF_3_8MHZ, "3.8M"},
		{FCDPRO_TMF_3_4MHZ, "3.4M"},
		{FCDPRO_TMF_3_0MHZ, "3.0M"},
		{FCDPRO_TMF_2_7MHZ, "2.7M"},
		{FCDPRO_TMF_2_3MHZ, "2.3M"},
		{FCDPRO_TMF_1_9MHZ, "1.9M"}
};

int FCDProConstants::fcdpro_mixer_filter_nb_values()
{
	return sizeof(mixer_filters) / sizeof(fcdpro_mixer_filter);
}

const fcdpro_if_gain1 FCDProConstants::if_gains1[] = {
		{FCDPRO_TIG1_N3_0DB, "3dB"},
		{FCDPRO_TIG1_P6_0DB, "6dB"}
};

int FCDProConstants::fcdpro_if_gain1_nb_values()
{
	return sizeof(if_gains1) / sizeof(fcdpro_if_gain1);
}

const fcdpro_if_gain_mode FCDProConstants::if_gain_modes[] = {
		{FCDPRO_TIGM_LINEARITY, "Lin"},
		{FCDPRO_TIGM_SENSITIVITY, "Sens"}
};

int FCDProConstants::fcdpro_if_gain_mode_nb_values()
{
	return sizeof(if_gain_modes) / sizeof(fcdpro_if_gain_mode);
}

const fcdpro_if_rc_filter FCDProConstants::if_rc_filters[] = {
		{FCDPRO_TIRF_21_4MHZ, "21.4M"},
		{FCDPRO_TIRF_21_0MHZ, "21.0M"},
		{FCDPRO_TIRF_17_6MHZ, "17.6M"},
		{FCDPRO_TIRF_14_7MHZ, "14.7M"},
		{FCDPRO_TIRF_12_4MHZ, "12.4M"},
		{FCDPRO_TIRF_10_6MHZ, "18.6M"},
		{FCDPRO_TIRF_9_0MHZ, "9.0M"},
		{FCDPRO_TIRF_7_7MHZ, "7.7M"},
		{FCDPRO_TIRF_6_4MHZ, "6.4M"},
		{FCDPRO_TIRF_5_3MHZ, "5.3M"},
		{FCDPRO_TIRF_4_4MHZ, "4.4M"},
		{FCDPRO_TIRF_3_4MHZ, "3.4M"},
		{FCDPRO_TIRF_2_6MHZ, "2.6M"},
		{FCDPRO_TIRF_1_8MHZ, "1.8M"},
		{FCDPRO_TIRF_1_2MHZ, "1.2M"},
		{FCDPRO_TIRF_1_0MHZ, "1.0M"}
};

int FCDProConstants::fcdpro_if_rc_filter_nb_values()
{
	return sizeof(if_rc_filters) / sizeof(fcdpro_if_rc_filter);
}

const fcdpro_if_gain2 FCDProConstants::if_gains2[] = {
		{FCDPRO_TIG2_P0_0DB, "0dB"},
		{FCDPRO_TIG2_P3_0DB, "3dB"},
		{FCDPRO_TIG2_P6_0DB, "6dB"},
		{FCDPRO_TIG2_P9_0DB, "9dB"}
};

int FCDProConstants::fcdpro_if_gain2_nb_values()
{
	return sizeof(if_gains2) / sizeof(fcdpro_if_gain2);
}

const fcdpro_if_gain3 FCDProConstants::if_gains3[] = {
		{FCDPRO_TIG3_P0_0DB, "0dB"},
		{FCDPRO_TIG3_P3_0DB, "3dB"},
		{FCDPRO_TIG3_P6_0DB, "6dB"},
		{FCDPRO_TIG3_P9_0DB, "9dB"}
};

int FCDProConstants::fcdpro_if_gain3_nb_values()
{
	return sizeof(if_gains3) / sizeof(fcdpro_if_gain3);
}

const fcdpro_if_gain4 FCDProConstants::if_gains4[] = {
		{FCDPRO_TIG4_P0_0DB, "0dB"},
		{FCDPRO_TIG4_P1_0DB, "1dB"},
		{FCDPRO_TIG4_P2_0DB, "2dB"},
};

int FCDProConstants::fcdpro_if_gain4_nb_values()
{
	return sizeof(if_gains4) / sizeof(fcdpro_if_gain4);
}

const fcdpro_if_filter FCDProConstants::if_filters[] = {
		{FCDPRO_TIF_5_50MHZ, "5.5M"},
		{FCDPRO_TIF_5_30MHZ, "5.3M"},
		{FCDPRO_TIF_5_00MHZ, "5.0M"},
		{FCDPRO_TIF_4_80MHZ, "4.8M"},
		{FCDPRO_TIF_4_60MHZ, "4.6M"},
		{FCDPRO_TIF_4_40MHZ, "4.4M"},
		{FCDPRO_TIF_4_30MHZ, "4.3M"},
		{FCDPRO_TIF_4_10MHZ, "4.1M"},
		{FCDPRO_TIF_3_90MHZ, "3.9M"},
		{FCDPRO_TIF_3_80MHZ, "3.8M"},
		{FCDPRO_TIF_3_70MHZ, "3.7M"},
		{FCDPRO_TIF_3_60MHZ, "3.6M"},
		{FCDPRO_TIF_3_40MHZ, "3.4M"},
		{FCDPRO_TIF_3_30MHZ, "3.3M"},
		{FCDPRO_TIF_3_20MHZ, "3.2M"},
		{FCDPRO_TIF_3_10MHZ, "3.1M"},
		{FCDPRO_TIF_3_00MHZ, "3.0M"},
		{FCDPRO_TIF_2_95MHZ, "2.95M"},
		{FCDPRO_TIF_2_90MHZ, "2.9M"},
		{FCDPRO_TIF_2_80MHZ, "2.8M"},
		{FCDPRO_TIF_2_75MHZ, "2.75M"},
		{FCDPRO_TIF_2_70MHZ, "2.7M"},
		{FCDPRO_TIF_2_60MHZ, "2.6M"},
		{FCDPRO_TIF_2_55MHZ, "2.55M"},
		{FCDPRO_TIF_2_50MHZ, "2.5M"},
		{FCDPRO_TIF_2_45MHZ, "2.45M"},
		{FCDPRO_TIF_2_40MHZ, "2.4M"},
		{FCDPRO_TIF_2_30MHZ, "2.3M"},
		{FCDPRO_TIF_2_28MHZ, "2.28M"},
		{FCDPRO_TIF_2_24MHZ, "2.24M"},
		{FCDPRO_TIF_2_20MHZ, "2.2M"},
		{FCDPRO_TIF_2_15MHZ, "2.15M"}
};

int FCDProConstants::fcdpro_if_filter_nb_values()
{
	return sizeof(if_filters) / sizeof(fcdpro_if_filter);
}

const fcdpro_if_gain5 FCDProConstants::if_gains5[] = {
		{FCDPRO_TIG5_P3_0DB, "3dB"},
		{FCDPRO_TIG5_P6_0DB, "6dB"},
		{FCDPRO_TIG5_P9_0DB, "9dB"},
		{FCDPRO_TIG5_P12_0DB, "12dB"},
		{FCDPRO_TIG5_P15_0DB, "15dB"}
};

int FCDProConstants::fcdpro_if_gain5_nb_values()
{
	return sizeof(if_gains5) / sizeof(fcdpro_if_gain5);
}

const fcdpro_if_gain6 FCDProConstants::if_gains6[] = {
		{FCDPRO_TIG6_P3_0DB, "3dB"},
		{FCDPRO_TIG6_P6_0DB, "6dB"},
		{FCDPRO_TIG6_P9_0DB, "9dB"},
		{FCDPRO_TIG6_P12_0DB, "12dB"},
		{FCDPRO_TIG6_P15_0DB, "15dB"}
};

int FCDProConstants::fcdpro_if_gain6_nb_values()
{
	return sizeof(if_gains6) / sizeof(fcdpro_if_gain6);
}


