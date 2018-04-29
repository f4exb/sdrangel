///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_DSP_DECIMATORSFI_H_
#define SDRBASE_DSP_DECIMATORSFI_H_

#include "dsp/inthalfbandfiltereof.h"
#include "export.h"

#define DECIMATORSFI_HB_FILTER_ORDER 64

/** Decimators with float input and integer output */
class SDRBASE_API DecimatorsFI
{
public:
    void decimate1(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate2_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate2_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate2_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate4_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate4_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate4_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate8_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate16_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate32_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_inf(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_sup(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);
    void decimate64_cen(SampleVector::iterator* it, const float* buf, qint32 nbIAndQ);

    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator2;  // 1st stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator4;  // 2nd stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator8;  // 3rd stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator16; // 4th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator32; // 5th stages
    IntHalfbandFilterEOF<DECIMATORSFI_HB_FILTER_ORDER> m_decimator64; // 6th stages
};



#endif /* SDRBASE_DSP_DECIMATORSFI_H_ */
