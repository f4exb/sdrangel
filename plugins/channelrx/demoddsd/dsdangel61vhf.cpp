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

#include <algorithm>

#include <cstdlib>

#include "dsdangel61vhf.h"

float DSDAngel61Vhf::tierThresholdDB(DVAngel61::Profile profile, int tier)
{
    // The built receiver's 1% points on AWGN (C3): NB 2.25k, 3.0k, 4.5k QPSK, 9.0k 16-QAM; WB 4.5k,
    // 9.0k QPSK, 12k 16-QAM, Opus 16-QAM
    static const float nb[4] = { 2.5f, 3.4f, 5.7f, 10.3f };
    static const float wb[4] = { 1.9f, 3.4f, 6.6f, 8.5f };
    tier = std::max(0, std::min(3, tier));
    return profile == DVAngel61::ProfileWB ? wb[tier] : nb[tier];
}

void DSDAngel61Vhf::judge(Correspondent& c, int tier, float bler, int maxConsecutiveLost, float minSnrDB)
{
    // As revision 2's session: one step from the previous judgement (the send tier, back to 0 when it
    // has expired or was of another profile), whatever tier the transmission used
    const int previous = std::max(0, std::min(3, c.m_sendTier));
    int next = previous;

    if ((previous < 3) && (bler < VhfUpgradeBler) && (minSnrDB >= tierThresholdDB(c.m_profile, previous + 1) + VhfUpgradeMarginDB)) {
        next = previous + 1;
    } else if ((bler > VhfDowngradeBler) || (maxConsecutiveLost >= VhfDowngradeConsecutive)) {
        next = std::max(0, previous - 1);
    }

    c.m_sendTier = next;
    c.m_heardTier = std::max(0, std::min(3, tier));
}

DSDAngel61Vhf::DSDAngel61Vhf(DVAngel61::Profile profile) :
    DSDAngel61Session(profile, DSDAngel61Receiver::layout(profile).frameSamples(), MinimumFrames),
    m_receiver(profile)
{
}

void DSDAngel61Vhf::reset()
{
    m_receiver.reset();
    m_blanker.reset();
    m_canceller.reset();
    DSDAngel61Session::reset();
}

void DSDAngel61Vhf::skip(int n)
{
    m_receiver.skip(n);
    m_blanker.reset();
    m_canceller.reset();
    advance(n);
}

void DSDAngel61Vhf::process(const std::complex<float> *iq, int n)
{
    // Impulses blanked first (DSDBlanker)
    static const bool blanker = !getenv("DSD_ANGEL61_BLANKER") || (atoi(getenv("DSD_ANGEL61_BLANKER")) != 0);
    m_blanker.setEnabled(blanker);

    static const bool canceller = DSDToneCanceller::enabledByDefault();
    m_canceller.setEnabled(canceller);
    m_canceller.setBand(DSDToneCanceller::halfWidthHz((int) getProfile()));

    // ... and carriers in the channel cancelled (DSDToneCanceller)
    if (m_blanker.isEnabled() || m_canceller.isEnabled())
    {
        m_blanked.assign(iq, iq + n);
        m_blanker.process(m_blanked.data(), n);
        m_canceller.process(m_blanked.data(), n);
        m_receiver.process(m_blanked.data(), n);
    }
    else
    {
        m_receiver.process(iq, n);
    }

    // The receiver gives a frame when its header decoded; its SNR is the known symbols' Es/N0
    for (const DSDAngel61Receiver::Frame& frame : m_receiver.takeFrames())
    {
        frameHeard(true, frame.m_snrDB, frame.m_frequencyHz, &frame.m_header);

        if (frame.m_payloadOk) {
            confirmed(frame.m_header, frame.m_header.m_counter, 1, frame.m_block.data(), frame.m_sample, frame.m_channelErrors, frame.m_codedBits);
        }
    }

    advance(n);
}
