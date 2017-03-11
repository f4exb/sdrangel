///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
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

#include "devicebladerfvalues.h"


unsigned int DeviceBladeRFSampleRates::m_nb_rates = 23;
unsigned int DeviceBladeRFSampleRates::m_rates[] = {
        1536,
        1600,
        2000,
        2304,
        2400,
        3000,
        3072,
        3200,
        4608,
        4800,
        6000,
        6144,
        7680,
        9216,
        9600,
       10752,
       12288,
       18432,
       19200,
       24576,
       30720,
       36864,
       39936};

unsigned int DeviceBladeRFSampleRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates[rate_index];
    }
    else
    {
        return m_rates[0];
    }
}

unsigned int DeviceBladeRFSampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate/1000 == m_rates[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int DeviceBladeRFSampleRates::getNbRates()
{
    return DeviceBladeRFSampleRates::m_nb_rates;
}

unsigned int DeviceBladeRFBandwidths::m_nb_halfbw = 16;
unsigned int DeviceBladeRFBandwidths::m_halfbw[] = {
        750,
        875,
       1250,
       1375,
       1500,
       1920,
       2500,
       2750,
       3000,
       3500,
       4375,
       5000,
       6000,
       7000,
      10000,
      14000};

unsigned int DeviceBladeRFBandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_halfbw)
    {
        return m_halfbw[bandwidth_index] * 2;
    }
    else
    {
        return m_halfbw[0] * 2;
    }
}

unsigned int DeviceBladeRFBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_halfbw; i++)
    {
        if (bandwidth/2000 == m_halfbw[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int DeviceBladeRFBandwidths::getNbBandwidths()
{
    return DeviceBladeRFBandwidths::m_nb_halfbw;
}



