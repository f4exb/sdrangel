///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_GOERTZEL_H
#define INCLUDE_GOERTZEL_H

#include <complex>

#include "export.h"

// Goertzel filter for calculting discrete Fourier transform for a single frequency
// Implementation supports non-integer multiples of fundamental frequency, see:
// https://asp-eurasipjournals.springeropen.com/track/pdf/10.1186/1687-6180-2012-56.pdf
class SDRBASE_API Goertzel
{
public:
    Goertzel(double frequency, int sampleRate);
    void reset();
    void filter(double sample);
    std::complex<double> goertzel(double lastSample);
    double mag();
    double phase();
    int size() { return m_sampleCount; }

private:
    double m_s0, m_s1, m_s2;
    double m_a, m_b;
    std::complex<double> m_c, m_z;
    int m_sampleCount;
};

#endif // INCLUDE_GOERTZEL_H
