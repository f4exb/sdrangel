/*
 * fcdproplusconst.h
 *
 *  Created on: Sep 5, 2015
 *      Author: f4exb
 */

#ifndef FCDLIB_FCDPROPLUSCONST_H_
#define FCDLIB_FCDPROPLUSCONST_H_

#include <string>

#include "export.h"

typedef enum
{
	FCDPROPLUS_TRF_0_4,
	FCDPROPLUS_TRF_4_8,
	FCDPROPLUS_TRF_8_16,
	FCDPROPLUS_TRF_16_32,
	FCDPROPLUS_TRF_32_75,
	FCDPROPLUS_TRF_75_125,
	FCDPROPLUS_TRF_125_250,
	FCDPROPLUS_TRF_145,
	FCDPROPLUS_TRF_410_875,
	FCDPROPLUS_TRF_435,
	FCDPROPLUS_TRF_875_2000
} fcdproplus_rf_filter_value;

typedef enum
{
	FCDPROPLUS_TIF_200KHZ=0,
	FCDPROPLUS_TIF_300KHZ=1,
	FCDPROPLUS_TIF_600KHZ=2,
	FCDPROPLUS_TIF_1536KHZ=3,
	FCDPROPLUS_TIF_5MHZ=4,
	FCDPROPLUS_TIF_6MHZ=5,
	FCDPROPLUS_TIF_7MHZ=6,
	FCDPROPLUS_TIF_8MHZ=7
} fcdproplus_if_filter_value;

typedef struct
{
	fcdproplus_rf_filter_value value;
	std::string label;
} fcdproplus_rf_filter;

typedef struct
{
	fcdproplus_if_filter_value value;
	std::string label;
} fcdproplus_if_filter;

class FCDLIB_API FCDProPlusConstants
{
public:
	static const fcdproplus_rf_filter rf_filters[];
	static const fcdproplus_if_filter if_filters[];
	static int fcdproplus_rf_filter_nb_values();
	static int fcdproplus_if_filter_nb_values();
};


#endif /* FCDLIB_FCDPROPLUSCONST_H_ */
