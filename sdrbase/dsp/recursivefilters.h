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

#ifndef SDRBASE_DSP_RECURSIVEFILTERS_H_
#define SDRBASE_DSP_RECURSIVEFILTERS_H_

#include "export.h"

/**
 * \Brief: This is a second order bandpass filter using recursive method. r is in range ]0..1[ the higher the steeper the filter.
 * inspired by:http://www.ece.umd.edu/~tretter/commlab/c6713slides/FSKSlides.pdf
 */
class SDRBASE_API SecondOrderRecursiveFilter
{
public:
    SecondOrderRecursiveFilter(float samplingFrequency, float centerFrequency, float r);
    ~SecondOrderRecursiveFilter();

    void setFrequencies(float samplingFrequency, float centerFrequency);
    void setR(float r);
    short run(short sample);
    float run(float sample);

private:
    void init();

    float m_r;
    float m_frequencyRatio;
    float m_f;
    float m_v[3];
};



#endif /* SDRBASE_DSP_RECURSIVEFILTERS_H_ */
