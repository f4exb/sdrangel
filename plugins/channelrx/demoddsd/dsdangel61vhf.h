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

#ifndef INCLUDE_DSDANGEL61VHF_H
#define INCLUDE_DSDANGEL61VHF_H

#include "dsdangel61receiver.h"
#include "dsdangel61session.h"
#include "dsdblanker.h"
#include "dsdtonecanceller.h"

/**
 * The DSD Demodulator's revision 3 VHF branch (sdrbase/dsp/dv/ANGEL61.md section 5, plan item
 * C3): one profile's DSDAngel61Receiver (NB or WB) and a revision 3 session on its frames.
 *
 * **The rate request** (2.10, VHF, "as revision 2"): from transmissions of at least 13 frames (a
 * second), the least Es/N0 of the frames whose header decoded and the block error rate; up a tier
 * under 1% with the least Es/N0 3 dB over the next tier's threshold, down one above 3% or on two
 * consecutive lost frames, else as before. The thresholds are the built receiver's 1% points on AWGN
 * (C3, angel61_vhf_results.txt), as 2.10 asks.
 */
class DSDAngel61Vhf : public DSDAngel61Session
{
public:
    static const int MinimumFrames = 13;
    static constexpr float VhfUpgradeBler = 0.01f;
    static constexpr float VhfDowngradeBler = 0.03f;
    static const int VhfDowngradeConsecutive = 2;
    static constexpr float VhfUpgradeMarginDB = 3.0f;

    //! A tier's threshold (Es/N0 dB, 1% of frames lost on AWGN, the built receiver)
    static float tierThresholdDB(DVAngel61::Profile profile, int tier);
    //! The rate rule (2.10, VHF) on a transmission judged at `tier`. Public for the tests.
    static void judge(Correspondent& c, int tier, float bler, int maxConsecutiveLost, float minSnrDB);

    explicit DSDAngel61Vhf(DVAngel61::Profile profile);

    virtual void reset();
    virtual void process(const std::complex<float> *iq, int n);
    virtual void skip(int n);
    virtual bool isLocked() const { return m_receiver.getStatus().m_locked; }
    virtual int getAcquisitions() const { return m_receiver.getStatus().m_unlocks + (m_receiver.getStatus().m_locked ? 1 : 0); }
    virtual void setScopeEnabled(bool enable) { m_receiver.setScopeEnabled(enable); }
    virtual std::vector<std::complex<float>> takeScopeSymbols() { return m_receiver.takeScopeSymbols(); }
    const DSDAngel61Receiver& getReceiver() const { return m_receiver; }

protected:
    virtual void judgeRate(Correspondent& c, int tier, int64_t sample)
    {
        (void) sample;
        judge(c, tier, c.m_blockErrorRate, c.m_maxConsecutiveLost, c.m_minSnrDB);
    }

private:
    DSDAngel61Receiver m_receiver;
    DSDBlanker m_blanker;                  //!< impulses taken out before the receiver (DSD_ANGEL61_BLANKER=0: off)
    DSDToneCanceller m_canceller;          //!< ... and carriers (DSD_ANGEL61_CANCELLER=0: off)
    std::vector<std::complex<float>> m_blanked;
};

#endif // INCLUDE_DSDANGEL61VHF_H
