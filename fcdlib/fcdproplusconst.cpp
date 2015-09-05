/*
 * fcdproplusconst.cpp
 *
 *  Created on: Sep 5, 2015
 *      Author: f4exb
 */

#include "fcdproplusconst.h"

const fcdproplus_rf_filter FCDProPlusConstants::rf_filters[] = {
		{FCDPROPLUS_TRF_0_4, "0-4M"},
		{FCDPROPLUS_TRF_4_8, "4-8M"},
		{FCDPROPLUS_TRF_8_16, "8-16M"},
		{FCDPROPLUS_TRF_16_32, "16-32M"},
		{FCDPROPLUS_TRF_32_75, "32-75M"},
		{FCDPROPLUS_TRF_75_125, "75-125M"},
		{FCDPROPLUS_TRF_125_250, "125-250M"},
		{FCDPROPLUS_TRF_145, "145M"},
		{FCDPROPLUS_TRF_410_875, "410-875M"},
		{FCDPROPLUS_TRF_435, "435M"},
		{FCDPROPLUS_TRF_875_2000, "875M-2G"}
};

int FCDProPlusConstants::fcdproplus_rf_filter_nb_values()
{
	return sizeof(rf_filters) / sizeof(fcdproplus_rf_filter);
}

const fcdproplus_if_filter FCDProPlusConstants::if_filters[] = {
		{FCDPROPLUS_TIF_200KHZ, "200k"},
		{FCDPROPLUS_TIF_300KHZ, "300k"},
		{FCDPROPLUS_TIF_600KHZ, "600k"},
		{FCDPROPLUS_TIF_1536KHZ, "1.5M"},
		{FCDPROPLUS_TIF_5MHZ, "5M"},
		{FCDPROPLUS_TIF_6MHZ, "6M"},
		{FCDPROPLUS_TIF_7MHZ, "7M"},
		{FCDPROPLUS_TIF_8MHZ, "8M"}
};

int FCDProPlusConstants::fcdproplus_if_filter_nb_values()
{
	return sizeof(if_filters) / sizeof(fcdproplus_if_filter);
}
