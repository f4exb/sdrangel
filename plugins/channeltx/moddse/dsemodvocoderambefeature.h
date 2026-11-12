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

#ifndef PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERAMBEFEATURE_H_
#define PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERAMBEFEATURE_H_

#include <memory>

#include "dsemodvocoder.h"

#include <deque>
#include <utility>

class MbeEncoderPort;

class Feature;
class MessageQueue;

/**
 * Vocoder back end using a DVSI AMBE device through the AMBE feature plugin.
 *
 * Audio is sent to the feature synchronously (the feature copies it into the
 * device worker queue). Encoded frames come back asynchronously as
 * DSPReportMbeFrame messages on the reply queue given at construction, which
 * the owner must forward to handleMessage().
 */
class DSEModVocoderAMBEFeature : public DSEModVocoder
{
public:
    DSEModVocoderAMBEFeature(MessageQueue *replyQueue);
    virtual ~DSEModVocoderAMBEFeature();

    virtual bool open(Rate rate);
    virtual void close();
    virtual bool isReady() const { return (m_rate != RateNone) && canOpen(); }
    virtual bool canOpen() const;
    virtual void encode(uint32_t sequence, const int16_t *pcm);
    virtual bool handleMessage(const Message& msg);
    virtual int getLatencyFrames() const { return 4; } //!< ~60 ms device turnaround
    virtual const char *getName() const { return "AMBE feature"; }

    //! feature: identity only; port: what is called (asked of the feature in its own thread by the
    //! channel: here, on the processor's thread, the feature may be gone already)
    void setFeature(Feature *feature, std::shared_ptr<MbeEncoderPort> port);
    Feature *getFeature() const { return m_feature; }
    void setGain(int gain) { m_gain = gain; }

private:
    Feature *m_feature; //!< Identity only: never dereferenced after setFeature()
    std::shared_ptr<MbeEncoderPort> m_port;
    MessageQueue *m_replyQueue;
    int m_gain;
    //! The feature is given a tag of this vocoder's rather than the caller's sequence, and the
    //! tags this open is waiting for map back to it: a frame of an encode from before (the
    //! device's queue outlasting a transmission) has a tag no longer waited for
    uint32_t m_nextTag = 0;
    std::deque<std::pair<uint32_t, uint32_t>> m_pending;    //!< tag, sequence
    static constexpr int MaxPending = 256;

    void release();
};

#endif // PLUGINS_CHANNELTX_MODDSE_DSEMODVOCODERAMBEFEATURE_H_
