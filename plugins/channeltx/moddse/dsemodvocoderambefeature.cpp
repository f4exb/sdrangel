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

#include <QDebug>

#include "feature/feature.h"
#include "dsp/dspcommands.h"

#include "dsemodvocoderambefeature.h"

DSEModVocoderAMBEFeature::DSEModVocoderAMBEFeature(MessageQueue *replyQueue) :
    m_feature(nullptr),
    m_replyQueue(replyQueue),
    m_gain(0)
{
}

DSEModVocoderAMBEFeature::~DSEModVocoderAMBEFeature()
{
    release();
}

bool DSEModVocoderAMBEFeature::open(Rate rate)
{
    m_rate = rate;
    m_pending.clear();
    return m_feature != nullptr;
}

void DSEModVocoderAMBEFeature::close()
{
    release();
    m_rate = RateNone;
    m_pending.clear();
}

bool DSEModVocoderAMBEFeature::canOpen() const
{
    return m_port && m_port->isOpen();
}

void DSEModVocoderAMBEFeature::release()
{
    if (m_port)
    {
        DSPReleaseMbeClient msg(this);
        m_port->handleMessage(msg);
    }
}

void DSEModVocoderAMBEFeature::setFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port)
{
    if ((feature == m_feature) && (port == m_port)) {
        return;
    }

    release();
    m_feature = feature;
    m_port = feature ? port : nullptr;   // it stays valid after the feature is deleted

    qDebug("DSEModVocoderAMBEFeature::setFeature: %p port: %d", (void *) feature, m_port ? 1 : 0);
}

void DSEModVocoderAMBEFeature::encode(uint32_t sequence, const int16_t *pcm)
{
    if (!m_port || (m_rate == RateNone)) {
        return;
    }

    const uint32_t tag = m_nextTag++;
    DSPPushMbeAudio msg(pcm, (int) m_rate, m_gain, tag, this, m_replyQueue);

    if (!m_port->handleMessage(msg)) // audio is copied by the receiver; false: the feature is gone
    {
        deliver(sequence, getSilenceFrame(m_rate));
        return;
    }

    m_pending.emplace_back(tag, sequence);

    if ((int) m_pending.size() > MaxPending) {  // (a feature that stopped answering)
        m_pending.pop_front();
    }
}

bool DSEModVocoderAMBEFeature::handleMessage(const Message& msg)
{
    if (DSPReportMbeFrame::match(msg))
    {
        const DSPReportMbeFrame& report = (const DSPReportMbeFrame&) msg;

        if (report.getClient() != this) {
            return false;
        }

        auto it = std::find_if(m_pending.begin(), m_pending.end(),
            [&report](const std::pair<uint32_t, uint32_t>& p) { return p.first == report.getSequence(); });

        if (it == m_pending.end()) {
            return true;    // an earlier open's encode: dropped
        }

        // The frames the device skipped, in its order, as silence: the frame count keeps time
        while (m_pending.front().first != report.getSequence())
        {
            deliver(m_pending.front().second, getSilenceFrame(m_rate));
            m_pending.pop_front();
        }

        const uint32_t sequence = m_pending.front().second;
        m_pending.pop_front();

        if (report.isOK() && (report.getNbBytes() >= getFrameBytes(m_rate))) {
            deliver(sequence, report.getMbeFrame());
        } else {
            deliver(sequence, getSilenceFrame(m_rate));
        }

        return true;
    }

    return false;
}
