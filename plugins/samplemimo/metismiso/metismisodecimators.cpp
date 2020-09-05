///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "metismisodecimators.h"

MetisMISODecimators::MetisMISODecimators()
{
    resetCounters();
}

void MetisMISODecimators::resetCounters()
{
    std::fill(m_counters, m_counters+MetisMISOSettings::m_maxReceivers, 0);
}

int MetisMISODecimators::decimate2(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex)
{
    if (streamIndex < MetisMISOSettings::m_maxReceivers)
    {
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleI;
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleQ;

        if (m_counters[streamIndex] >= 8) 
        {
            SampleVector::iterator it = result.begin();
            m_decimatorsIQ[streamIndex].decimate2_cen(&it, m_accumulators[streamIndex], 8);
            m_counters[streamIndex] = 0;
            return 2; // 2 samples available
        }
    }

    return 0;
}

int MetisMISODecimators::decimate4(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex)
{
    if (streamIndex < MetisMISOSettings::m_maxReceivers)
    {
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleI;
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleQ;

        if (m_counters[streamIndex] >= 16) 
        {
            SampleVector::iterator it = result.begin();
            m_decimatorsIQ[streamIndex].decimate4_cen(&it, m_accumulators[streamIndex], 16);
            m_counters[streamIndex] = 0;
            return 2; // 2 samples available
        }
    }

    return 0;
}

int MetisMISODecimators::decimate8(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex)
{
    if (streamIndex < MetisMISOSettings::m_maxReceivers)
    {
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleI;
        m_accumulators[streamIndex][m_counters[streamIndex]++] = sampleQ;

        if (m_counters[streamIndex] >= 16) 
        {
            SampleVector::iterator it = result.begin();
            m_decimatorsIQ[streamIndex].decimate8_cen(&it, m_accumulators[streamIndex], 16);
            m_counters[streamIndex] = 0;
            return 1; // 1 sample available
        }
    }

    return 0;
}
