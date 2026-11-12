///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef INCLUDE_DSDANGEL61HF_H
#define INCLUDE_DSDANGEL61HF_H

#include "dsp/dv/dvangel61ofdm.h"

#include "dsdangel61session.h"
#include "dsdblanker.h"
#include "dsdtonecanceller.h"

/**
 * The DSD Demodulator's HF branch (sdrbase/dsp/dv/ANGEL61.md section 5, plan item C4): one
 * profile's OFDM receiver (DVAngel61OFDM::Receiver) and a revision 3 session on its frames (a data
 * group confirmed as its four frames, with its last).
 *
 * **The rate request** (2.10, HF): from transmissions of at least 5 frames (1.2 s), the mean SNR3k and
 * the block error rate; down a tier above 4% or on three consecutive lost frames, up one under 1% with
 * the mean SNR3k 3 dB over the next tier's poor-channel 10% point, never back up to a tier that failed
 * within its hold (1 minute, doubling on each repeated failure to 16).
 */
class DSDAngel61HF : public DSDAngel61Session
{
public:
    static constexpr int MinimumFrames = 5;

    //! The poor-channel 10% point (SNR3k, dB) of a tier, the raise rule's reference (2.9)
    static float poorThresholdDB(DVAngel61::Profile profile, int tier);
    /**
     * The rate rule (2.10, HF) on one judged transmission at `tier` ending at `sample`: updates the
     * correspondent's send tier and holds. Public for the tests.
     */
    static void judge(Correspondent& c, int tier, float bler, int maxConsecutiveLost, float meanSnrDB, int64_t sample);

    explicit DSDAngel61HF(DVAngel61::Profile profile);

    virtual void reset();
    virtual void process(const std::complex<float> *iq, int n);
    virtual bool isLocked() const { return m_receiver.locked(); }
    virtual int getAcquisitions() const { return m_receiver.acquisitions(); }
    virtual void setScopeEnabled(bool enable) { m_receiver.setScopeEnabled(enable); }
    virtual std::vector<std::complex<float>> takeScopeSymbols() { return m_receiver.takeScopeSymbols(); }
    int getDesignsRefused() const { return m_receiver.designsRefused(); }

protected:
    virtual void judgeRate(Correspondent& c, int tier, int64_t sample)
    {
        judge(c, tier, c.m_blockErrorRate, c.m_maxConsecutiveLost, c.m_meanSnrDB, sample);
    }

    DSDBlanker& getBlanker() { return m_blanker; }

private:
    DVAngel61OFDM::Receiver m_receiver;
    DSDBlanker m_blanker;                  //!< impulses taken out before the receiver (DSD_ANGEL61_BLANKER=0: off)
    DSDToneCanceller m_canceller;          //!< ... and carriers (DSD_ANGEL61_CANCELLER=0: off)
    std::vector<std::complex<float>> m_blanked;
    std::vector<DVAngel61OFDM::Receiver::Frame> m_frames;
};

#endif // INCLUDE_DSDANGEL61HF_H
