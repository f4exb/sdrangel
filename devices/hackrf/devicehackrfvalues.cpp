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

unsigned int HackRFSampleRates::m_rates_k[] = {2400, 3200, 4000, 4800, 5600, 6000, 6400, 8000, 9600, 12000, 12800, 16000, 18000, 19200, 20000};

unsigned int HackRFSampleRates::getRate(unsigned int rate_index)
{
    if (rate_index < m_nb_rates)
    {
        return m_rates_k[rate_index];
    }
    else
    {
        return m_rates_k[0];
    }
}

unsigned int HackRFSampleRates::getRateIndex(unsigned int rate)
{
    for (unsigned int i=0; i < m_nb_rates; i++)
    {
        if (rate == m_rates_k[i])
        {
            return i;
        }
    }

    return 0;
}

unsigned int HackRFBandwidths::m_bw_k[] = {1750, 2500, 3500, 5000, 5500, 6000, 7000, 8000, 9000, 10000, 12000, 14000, 15000, 20000, 24000, 28000};

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


