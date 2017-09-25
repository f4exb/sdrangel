/*
 * fcdtraits.h
 *
 *  Created on: 3 Sep 2015
 *      Author: egriffiths
 */

#ifndef FCDLIB_FCDTRAITS_H_
#define FCDLIB_FCDTRAITS_H_

#include <inttypes.h>

typedef enum
{
    Pro,
    ProPlus
} fcd_type;

template <fcd_type FCDType>
struct fcd_traits
{
	static const uint16_t vendorId = 0x0;
	static const uint16_t productId = 0x0;
	static const int sampleRate = 48000;
	static const int convBufSize = (1<<11);
	static const int fcdBufSize = (1<<12);
	static const char *alsaDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template<>
struct fcd_traits<Pro>
{
	static const uint16_t vendorId = 0x04D8;
	static const uint16_t productId = 0xFB56;
	static const int sampleRate = 96000;
	static const int convBufSize = (1<<11);
	static const int fcdBufSize = (1<<12);
	static const char *alsaDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template<>
struct fcd_traits<ProPlus>
{
	static const uint16_t vendorId = 0x04D8;
	static const uint16_t productId = 0xFB31;
	static const int sampleRate = 192000;
	static const int convBufSize = (1<<12);
	static const int fcdBufSize = (1<<18);
	static const char *alsaDeviceName;
    static const char *hardwareID;
    static const char *interfaceIID;
    static const char *displayedName;
    static const char *pluginDisplayedName;
    static const char *pluginVersion;
    static const int64_t loLowLimitFreq;
    static const int64_t loHighLimitFreq;
};

template <fcd_type FCDType> const char *fcd_traits<FCDType>::alsaDeviceName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::hardwareID = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::interfaceIID = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::displayedName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::pluginDisplayedName = "";
template <fcd_type FCDType> const char *fcd_traits<FCDType>::pluginVersion = "---";

#endif /* FCDLIB_FCDTRAITS_H_ */
