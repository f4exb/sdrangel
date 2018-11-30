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

#include <vector>
#include "dsp/dsptypes.h"
#include "export.h"

/** Phase-locked loop mainly for broadcadt FM stereo pilot. */
class SDRBASE_API PhaseLock
{
public:

    /** Expected pilot frequency (used for PPS events). */
    static const int pilot_frequency = 19000;

    /** Timestamp event produced once every 19000 pilot periods. */
    struct PpsEvent
    {
        quint64   pps_index;
        quint64   sample_index;
        double    block_position;
    };

    /**
     * Construct phase-locked loop.
     *
     * freq       :: 19 kHz center frequency relative to sample freq
     *               (0.5 is Nyquist)
     * bandwidth  :: bandwidth relative to sample frequency
     * minsignal  :: minimum pilot amplitude
     */
    PhaseLock(Real freq, Real bandwidth, Real minsignal);

    virtual ~PhaseLock()
    {}

    /**
     * Change phase locked loop parameters
     *
     * freq       :: 19 kHz center frequency relative to sample freq
     *               (0.5 is Nyquist)
     * bandwidth  :: bandwidth relative to sample frequency
     * minsignal  :: minimum pilot amplitude
     */
    void configure(Real freq, Real bandwidth, Real minsignal);

    /**
     * Process samples and extract 19 kHz pilot tone.
     * Generate phase-locked 38 kHz tone with unit amplitude.
     * Bufferized version with input and output vectors
     */
    void process(const std::vector<Real>& samples_in, std::vector<Real>& samples_out);

    /**
     * Process samples and track a pilot tone. Generate samples for single or multiple phase-locked
     * signals. Implement the processPhase virtual method to produce the output samples.
     * In flow version. Ex: Use 19 kHz stereo pilot tone to generate 38 kHz (stereo) and 57 kHz
     * pilots (see RDSPhaseLock class below).
     * This is the in flow version
     */
    void process(const Real& sample_in, Real *samples_out);
    void process(const Real& real_in, const Real& imag_in, Real *samples_out);

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

protected:
    Real    m_phase;
    Real    m_psin;
    Real    m_pcos;
    /**
     * Callback method to produce multiple outputs from the current phase value in m_phase
     * and/or the sin and cos values in m_psin and m_pcos
     */
    virtual void processPhase(Real *samples_out __attribute__((unused))) const {};

private:
    Real    m_minfreq, m_maxfreq;
    Real    m_phasor_b0, m_phasor_a1, m_phasor_a2;
    Real    m_phasor_i1, m_phasor_i2, m_phasor_q1, m_phasor_q2;
    Real    m_loopfilter_b0, m_loopfilter_b1;
    Real    m_loopfilter_x1;
    Real    m_freq;
    Real    m_minsignal;
    Real    m_pilot_level;
    int     m_lock_delay;
    int     m_lock_cnt;
    int     m_pilot_periods;
    quint64 m_pps_cnt;
    quint64 m_sample_cnt;
    std::vector<PpsEvent> m_pps_events;

    void process_phasor(Real& phasor_i, Real& phasor_q);
};

class SimplePhaseLock : public PhaseLock
{
public:
    SimplePhaseLock(Real freq, Real bandwidth, Real minsignal) :
        PhaseLock(freq, bandwidth, minsignal)
    {}

    virtual ~SimplePhaseLock()
    {}

protected:
    virtual void processPhase(Real *samples_out) const
    {
        samples_out[0] = m_psin; // f Pilot
        samples_out[1] = m_pcos; // f Pilot
    }
};

class StereoPhaseLock : public PhaseLock
{
public:
	StereoPhaseLock(Real freq, Real bandwidth, Real minsignal) :
		PhaseLock(freq, bandwidth, minsignal)
    {}

    virtual ~StereoPhaseLock()
    {}

protected:
    virtual void processPhase(Real *samples_out) const
    {
    	samples_out[0] = m_psin; // f Pilot
        // Generate double-frequency output.
        // sin(2*x) = 2 * sin(x) * cos(x)
    	samples_out[1] = 2.0 * m_psin * m_pcos; // 2f Pilot sin
        // cos(2*x) = 2 * cos(x) * cos(x) - 1
    	samples_out[2] = (2.0 * m_pcos * m_pcos) - 1.0; // 2f Pilot cos
    }
};


class RDSPhaseLock : public PhaseLock
{
public:
	RDSPhaseLock(Real freq, Real bandwidth, Real minsignal) :
		PhaseLock(freq, bandwidth, minsignal)
    {}

    virtual ~RDSPhaseLock()
    {}

protected:
    virtual void processPhase(Real *samples_out) const
    {
        samples_out[0] = m_psin; // Pilot signal (f)
        // Generate double-frequency output.
        // sin(2*x) = 2 * sin(x) * cos(x)
        samples_out[1] = 2.0 * m_psin * m_pcos; // Pilot signal (2f)
        // cos(2*x) = 2 * cos(x) * cos(x) - 1
    	samples_out[2] = (2.0 * m_pcos * m_pcos) - 1.0; // 2f Pilot cos
        samples_out[3] = m_phase; // Pilot phase
    }
};
