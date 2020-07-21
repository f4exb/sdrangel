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

// Half-band centered decimators with Complex (i.e. omplex<float>) input/output
// Decimates by a power of two using centered half-band filters

#ifndef INCLUDE_DSP_DECIMATORC_H_
#define INCLUDE_DSP_DECIMATORC_H_

#include "dsp/dsptypes.h"
#include "dsp/inthalfbandfiltereof.h"
#include "export.h"

#define DECIMATORSXS_HB_FILTER_ORDER 64

class SDRBASE_API DecimatorC
{
public:
    DecimatorC();
    void setLog2Decim(unsigned int log2Decim);
    bool decimate(Complex c, Complex& cd);
    unsigned int getDecim() const { return m_decim; }

private:
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator2; // 1st stages
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator4; // 2nd stages
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator8; // 3rd stages
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator16; // 4th stages
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator32; // 5th stages
    IntHalfbandFilterEOF<DECIMATORSXS_HB_FILTER_ORDER, true> m_decimator64; // 6th stages
    unsigned int m_log2Decim;
    unsigned int m_decim;

    bool decimate2(Complex c, Complex& cd);
    bool decimate4(Complex c, Complex& cd);
    bool decimate8(Complex c, Complex& cd);
    bool decimate16(Complex c, Complex& cd);
    bool decimate32(Complex c, Complex& cd);
    bool decimate64(Complex c, Complex& cd);
};

#endif // INCLUDE_DSP_DECIMATORC_H_