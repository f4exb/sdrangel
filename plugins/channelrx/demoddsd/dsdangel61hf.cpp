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

#include "dsdangel61hf.h"

float DSDAngel61HF::poorThresholdDB(DVAngel61::Profile profile, int tier)
{
    // 2.9's poor-channel 10% points (A2, the studies' receiver)
    static const float hfnb[4] = { 8.7f, 11.4f, 13.1f, 17.8f };
    static const float hfwb[4] = { 8.4f, 12.8f, 14.5f, 18.2f };
    tier = std::max(0, std::min(3, tier));
    return profile == DVAngel61::ProfileHFWB ? hfwb[tier] : hfnb[tier];
}

void DSDAngel61HF::judge(Correspondent& c, int tier, float bler, int maxConsecutiveLost, float meanSnrDB, int64_t sample)
{
    tier = std::max(0, std::min(3, tier));
    int next = tier;

    if ((bler > DowngradeBler) || (maxConsecutiveLost >= DowngradeConsecutive))
    {
        // The tier failed: down one, and no raise back to it for its hold, which doubles on each
        // repeated failure up to 16 minutes
        next = std::max(0, tier - 1);
        const int failures = std::min(c.m_holdFailures[tier], 4);
        c.m_holdFailures[tier]++;
        c.m_holdUntil[tier] = sample + (int64_t) std::min(HoldMaxMs, HoldStartMs << failures) * (SampleRate / 1000);
    }
    else
    {
        // A transmission at the tier ended without it being lowered: its hold returns to 1 minute
        c.m_holdFailures[tier] = 0;

        if ((tier < 3) && (bler < UpgradeBler)
            && (meanSnrDB >= poorThresholdDB(c.m_profile, tier + 1) + UpgradeMarginDB)
            && (sample >= c.m_holdUntil[tier + 1]))
        {
            next = tier + 1;
        }
    }

    c.m_sendTier = next;
    c.m_heardTier = tier;
}

DSDAngel61HF::DSDAngel61HF(DVAngel61::Profile profile) :
    DSDAngel61Session(profile, DVAngel61OFDM::FrameSamples, MinimumFrames),
    m_receiver(profile)
{
}

void DSDAngel61HF::reset()
{
    m_receiver.reset();
    m_blanker.reset();
    m_canceller.reset();
    m_frames.clear();
    DSDAngel61Session::reset();
}

void DSDAngel61HF::process(const std::complex<float> *iq, int n)
{
    m_frames.clear();

    // Impulses blanked first (DSDBlanker): through the FFT a click is every cell of a symbol
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
        m_receiver.process(m_blanked.data(), n, m_frames);
    }
    else
    {
        m_receiver.process(iq, n, m_frames);
    }

    for (const auto& frame : m_frames)
    {
        frameHeard(frame.m_headerOk, frame.m_snrDb, frame.m_frequencyHz, frame.m_headerOk ? &frame.m_header : nullptr);

        if (!frame.m_payloadOk) {
            continue;
        }

        if (frame.m_group)
        {
            // A data group: its four headers and one block, confirmed with its last frame
            const DVAngel61::Header& last = frame.m_groupHeaders[DVAngel61::GroupFrames - 1];
            confirmed(last, frame.m_groupHeaders[0].m_counter, DVAngel61::GroupFrames, frame.m_block.data(), frame.m_sample);
        }
        else
        {
            confirmed(frame.m_header, frame.m_header.m_counter, 1, frame.m_block.data(), frame.m_sample, frame.m_channelErrors, frame.m_codedBits);
        }
    }

    advance(n);
}
