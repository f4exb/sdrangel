/*
 * fcdtraits.cpp
 *
 *  Created on: Sep 5, 2015
 *      Author: f4exb
 */

#include "fcdtraits.h"

const char *fcd_traits<Pro>::alsaDeviceName = "hw:CARD=V10";
const char *fcd_traits<ProPlus>::alsaDeviceName = "hw:CARD=V20";

const char *fcd_traits<Pro>::interfaceIID = "org.osmocom.sdr.samplesource.fcdpro";
const char *fcd_traits<ProPlus>::interfaceIID = "org.osmocom.sdr.samplesource.fcdproplus";

const char *fcd_traits<Pro>::displayedName = "FunCube Dongle Pro";
const char *fcd_traits<ProPlus>::displayedName = "FunCube Dongle Pro+";

const char *fcd_traits<Pro>::pluginDisplayedName = "FunCube Pro Input";
const char *fcd_traits<ProPlus>::pluginDisplayedName = "FunCube Pro+ Input";

const char *fcd_traits<Pro>::pluginVersion = "---";
const char *fcd_traits<ProPlus>::pluginVersion = "---";
