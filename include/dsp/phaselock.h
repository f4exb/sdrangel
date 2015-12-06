///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#include "dsp/dsptypes.h"

/** Phase-locked loop mainly for broadcadt FM stereo pilot. */
class PhaseLock
{
public:

    /** Expected pilot frequency (used for PPS events). */
    static const int pilot_frequency = 19000;

    /**
     * Construct phase-locked loop.
     *
     * freq       :: 19 kHz center frequency relative to sample freq
     *               (0.5 is Nyquist)
     * bandwidth  :: bandwidth relative to sample frequency
     * minsignal  :: minimum pilot amplitude
     */
    PhaseLock(Real freq, Real bandwidth, Real minsignal);

    /**
     * Process samples and extract 19 kHz pilot tone.
     * Generate phase-locked 38 kHz tone with unit amplitude.
     */
    void process(const std::vector<Real>& samples_in, std::vector<Real>& samples_out);

    /** Return true if the phase-locked loop is locked. */
    bool locked() const
    {
        return m_lock_cnt >= m_lock_delay;
    }

    /** Return detected amplitude of pilot signal. */
    Real get_pilot_level() const
    {
        return 2 * m_pilot_level;
    }

private:
    Real    m_minfreq, m_maxfreq;
    Real    m_phasor_b0, m_phasor_a1, m_phasor_a2;
    Real    m_phasor_i1, m_phasor_i2, m_phasor_q1, m_phasor_q2;
    Real    m_loopfilter_b0, m_loopfilter_b1;
    Real    m_loopfilter_x1;
    Real    m_freq, m_phase;
    Real    m_minsignal;
    Real    m_pilot_level;
    int     m_lock_delay;
    int     m_lock_cnt;
};
