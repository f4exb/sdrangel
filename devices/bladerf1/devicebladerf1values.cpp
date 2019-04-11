///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2017 Edouard Griffiths, F4EXB                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include "../bladerf1/devicebladerf1values.h"


unsigned int DeviceBladeRF1Bandwidths::m_nb_halfbw = 16;
unsigned int DeviceBladeRF1Bandwidths::m_halfbw[] = {
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

unsigned int DeviceBladeRF1Bandwidths::getBandwidth(unsigned int bandwidth_index)
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

unsigned int DeviceBladeRF1Bandwidths::getBandwidthIndex(unsigned int bandwidth)
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

unsigned int DeviceBladeRF1Bandwidths::getNbBandwidths()
{
    return DeviceBladeRF1Bandwidths::m_nb_halfbw;
}



