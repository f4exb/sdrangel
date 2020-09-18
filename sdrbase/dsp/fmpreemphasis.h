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

#ifndef INCLUDE_DSP_FMPREEMPHASIS_H_
#define INCLUDE_DSP_FMPREEMPHASIS_H_

#include "dsp/dsptypes.h"
#include "export.h"

#define FMPREEMPHASIS_TAU_EU     50e-6f
#define FMPREEMPHASIS_TAU_US     75e-6f

/** FM preemphasis filter.
 * Amplifies frequencies above ~3.2k (tau=50e-6 in EU) or ~2.1k (tau=75e-6 in US)
 * at ~6dB per octave, and then flattens at 12k (highFreq).
 * Frequency response:
 *       highFreq
 *           -------
 *          /
 *         /
 * -------/
 *   1/(2*pi*tau)
 */
class SDRBASE_API FMPreemphasis
{
public:

    FMPreemphasis(int sampleRate, Real tau = FMPREEMPHASIS_TAU_EU, Real highFreq = 12000.0);

    void configure(int sampleRate, Real tau = FMPREEMPHASIS_TAU_EU, Real highFreq = 12000.0);

    Real filter(Real sampleIn);

private:
    Real m_z;                   // Delay element
    Real m_b0;                  // IIR numerator taps
    Real m_b1;
    Real m_a1;                  // IIR denominator taps
};

#endif /* INCLUDE_DSP_FMPREEMPHASIS_H_ */
