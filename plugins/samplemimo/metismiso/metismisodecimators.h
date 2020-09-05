///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// Decimators adapters specific to Metis                                         //
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

#ifndef _METISMISO_METISMISODECIMATORS_H_
#define _METISMISO_METISMISODECIMATORS_H_

#include "dsp/decimators.h"

#include "metismisosettings.h"

class MetisMISODecimators
{
public:
    MetisMISODecimators();

    int decimate2(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex);
    int decimate4(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex);
    int decimate8(qint32 sampleI, qint32 sampleQ, SampleVector& result, unsigned int streamIndex);
    void resetCounters();

private:

    qint32 m_accumulators[MetisMISOSettings::m_maxReceivers][256*2];
    int m_counters[MetisMISOSettings::m_maxReceivers];
    Decimators<qint32, qint32, SDR_RX_SAMP_SZ, 24, true> m_decimatorsIQ[MetisMISOSettings::m_maxReceivers];
};


#endif // _METISMISO_METISMISODECIMATORS_H_
