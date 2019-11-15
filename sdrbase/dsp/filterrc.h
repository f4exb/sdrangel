///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef INCLUDE_DSP_FILTERRC_H_
#define INCLUDE_DSP_FILTERRC_H_

#include "dsp/dsptypes.h"
#include "export.h"

/** First order low-pass IIR filter for real-valued signals. */
class SDRBASE_API LowPassFilterRC
{
public:

    /**
     * Construct 1st order low-pass IIR filter.
     *
     * timeconst :: RC time constant in seconds (1 / (2 * PI * cutoff_freq)
     */
    LowPassFilterRC(Real timeconst);

    /**
     * Reconfigure filter with new time constant
     */
    void configure(Real timeconst);

    /** Process samples. */
    void process(const Real& sample_in, Real& sample_out);

private:
    Real m_timeconst;
    Real m_y1;
    Real m_a1;
    Real m_b0;
};

/** First order high-pass IIR filter for real-valued signals. */
class SDRBASE_API HighPassFilterRC
{
public:

    /**
     * Construct 1st order high-pass IIR filter.
     *
     * timeconst :: RC time constant in seconds (1 / (2 * PI * cutoff_freq)
     */
    HighPassFilterRC(Real timeconst);

    /**
     * Reconfigure filter with new time constant
     */
    void configure(Real timeconst);

    /** Process samples. */
    void process(const Real& sample_in, Real& sample_out);

private:
    Real m_timeconst;
    Real m_y1;
    Real m_a1;
    Real m_b0;
};

#endif /* INCLUDE_DSP_FILTERRC_H_ */
