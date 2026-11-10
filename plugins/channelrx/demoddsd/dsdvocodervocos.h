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

#ifndef INCLUDE_DSDVOCODERVOCOS_H
#define INCLUDE_DSDVOCODERVOCOS_H

#include <string>
#include <vector>

#include "dsp/dv/vocoscodec.h"

#include "dsdprotocol.h"

/**
 * The V1 decoder of Angel61 revision 3 (sdrbase/dsp/dv/ANGEL61.md 3.1.1): vocoscodec's Vocos
 * decoder through the library loaded at run time (VocosCodec), its model set checked.
 *
 * A frame (DSDVoiceFrame::RateVocos) is a radio frame's packets, 6 on VHF and 18 on HF, decoded in
 * one call: frames early in the group see its end as lookahead, which is better and about four times
 * cheaper than a packet a call. Lost audio keeps its time: an erased frame is its packets reported
 * lost, and the codec conceals them, repeating the last features for 40 ms and fading over 40 ms to
 * comfort noise at the noise floor of the last second received. The decoder is reset when a new
 * transmission begins, and flushed (its last 60 ms of audio) when one ends.
 *
 * One instance per audio stream. Silent, with an error to show, when the library or the models are
 * missing or not V1's.
 */
class DSDVocoderVocos
{
public:
    static const int m_sampleRate = VocosCodec::SampleRate;
    static const int m_samplesPerPacket = VocosCodec::SamplesPerFrame;
    static const int m_maxSamples = 18 * VocosCodec::SamplesPerFrame + 1600;   //!< an HF frame, or a flush

    DSDVocoderVocos();
    ~DSDVocoderVocos();
    DSDVocoderVocos(const DSDVocoderVocos&) = delete;
    DSDVocoderVocos& operator=(const DSDVocoderVocos&) = delete;

    static bool isAvailable() { return VocosCodec::isAvailable(); }

    //! Where the models are (empty: VocosCodec's default). Another directory closes the decoder: the
    //! next decode loads that one's models, and a failure to load is tried again
    void setModelDirectory(const std::string& directory);
    //! A new transmission: the decoder starts afresh
    void reset();
    /**
     * Decode a frame into audio (float, full scale +-32767) at 24 kHz; `samples` is how many (320 a
     * packet, or a flush's). False when nothing could be produced (not V1, the library or the models
     * missing): the error says why.
     */
    bool decode(const DSDVoiceFrame& frame, float *audio, int& samples);
    //! Packets the concealment has filled in since the last reset
    int getConcealedPackets() const { return m_concealedPackets; }
    //! The concealment has faded to comfort noise: nothing of the speech is left
    bool isComfortNoise() const { return m_decoder.isOpen() && m_decoder.isComfortNoise(); }
    const std::string& getError() const { return m_error; }
    bool isOpen() const { return m_decoder.isOpen(); }

private:
    VocosCodec::Decoder m_decoder;
    std::string m_modelDirectory;
    std::string m_error;
    int m_concealedPackets;
    bool m_failed;               //!< the models could not be loaded: not retried every frame
    std::vector<int16_t> m_pcm;

    bool open();
};

#endif // INCLUDE_DSDVOCODERVOCOS_H
