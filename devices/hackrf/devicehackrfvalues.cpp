///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "devicehackrfvalues.h"

const unsigned int HackRFSampleRates::m_nb_rates = 22;
const unsigned int HackRFSampleRates::m_rates[HackRFSampleRates::m_nb_rates] = {
        2400000,
        2600000,
        3000000,
        3150000, // for PAL-M
        3200000,
        3250000, // For PAL-BGIL
        4000000,
        4333333, // for GSM
        4800000,
        5600000,
        6000000,
        6300000, // for PAL-M
        6400000,
        6500000, // For PAL-BGIL
        8000000,
        9600000,
       12000000,
       12800000,
       16000000,
       18000000,
       19200000,
       20000000};

unsigned int HackRFSampleRates::getRate(unsigned int rate_index)
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

unsigned int HackRFSampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates[i])
        {
            return i;
        }
    }

    return 0;
}

const unsigned int HackRFBandwidths::m_nb_bw = 16;
const unsigned int HackRFBandwidths::m_bw_k[HackRFBandwidths::m_nb_bw] = {
        1750,
        2500,
        3500,
        5000,
        5500,
        6000,
        7000,
        8000,
        9000,
       10000,
       12000,
       14000,
       15000,
       20000,
       24000,
       28000};

unsigned int HackRFBandwidths::getBandwidth(unsigned int bandwidth_index)
{
    if (bandwidth_index < m_nb_bw)
    {
        return m_bw_k[bandwidth_index];
    }
    else
    {
        return m_bw_k[0];
    }
}

unsigned int HackRFBandwidths::getBandwidthIndex(unsigned int bandwidth)
{
    for (unsigned int i=0; i < m_nb_bw; i++)
    {
        if (bandwidth == m_bw_k[i])
        {
            return i;
        }
    }

    return 0;
}


