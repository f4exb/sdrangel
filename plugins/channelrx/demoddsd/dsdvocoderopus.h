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

#ifndef INCLUDE_DSDVOCODEROPUS_H
#define INCLUDE_DSDVOCODEROPUS_H

#include "dsp/dv/dvopus.h"

#include "dsdprotocol.h"

/**
 * Opus decoder for Angel61 WB's codec family O1 (sdrbase/dsp/dv/ANGEL61.md 3.1.2), WB's tier 3: one
 * packet of 184 bytes per 80 ms frame to 3840 samples at 48 kHz. As with V1, lost audio keeps its
 * time: an erasure is reported to the codec, whose packet loss concealment extends the audio from
 * its own state and fades it out as the loss goes on. libopus is part of sdrbase: nothing is loaded
 * at run time and there are no models. One instance per audio stream.
 */
class DSDVocoderOpus
{
public:
    static const int m_maxSamplesPerFrame = DVOpus::FrameSamples;
    static const int m_sampleRate = DVOpus::SampleRate;
    //! Consecutive packets lost after which the concealment has faded out (240 ms)
    static const int m_fadedPackets = 3;

    DSDVocoderOpus();
    DSDVocoderOpus(const DSDVocoderOpus&) = delete;
    DSDVocoderOpus& operator=(const DSDVocoderOpus&) = delete;

    //! A new transmission: the decoder starts afresh
    void reset();
    /**
     * Decode one frame (a packet, or an erasure) to 3840 samples of audio (float, full scale +-32767):
     * DSDVoiceFrame::getAudioSamples(frame.m_rate).
     * Returns false when nothing could be produced (not the Opus rate, the decoder failed): the audio is then silence.
     * concealed: the frame was an erasure and the codec's packet loss concealment filled it.
     */
    bool decode(const DSDVoiceFrame& frame, float *audio, bool *concealed = nullptr);
    //! Packets the concealment has filled in since the last reset
    int getConcealedPackets() const { return m_concealedPackets; }
    //! Consecutive packets missing now (0 when the last one arrived)
    int getLostPackets() const { return m_lostPackets; }
    //! The concealment has faded out: nothing of the audio is left
    bool isComfortNoise() const { return m_lostPackets >= m_fadedPackets; }

private:
    DVOpus::Decoder m_decoder;
    int m_concealedPackets;
    int m_lostPackets;
};

#endif // INCLUDE_DSDVOCODEROPUS_H
