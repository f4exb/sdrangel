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

#include "dsdvocodervocos.h"

DSDVocoderVocos::DSDVocoderVocos() :
    m_concealedPackets(0),
    m_failed(false),
    m_pcm(m_maxSamples)
{
}

DSDVocoderVocos::~DSDVocoderVocos()
{
    m_decoder.close();
}

bool DSDVocoderVocos::open()
{
    if (m_decoder.isOpen()) {
        return true;
    }

    if (m_failed) {
        return false;
    }

    if (!m_decoder.open(m_modelDirectory))
    {
        m_error = m_decoder.getError();
        m_failed = true;    // not retried every frame; a new instance tries again
        return false;
    }

    m_error.clear();
    return true;
}

void DSDVocoderVocos::setModelDirectory(const std::string& directory)
{
    if (directory == m_modelDirectory) {
        return;
    }

    m_modelDirectory = directory;
    m_decoder.close();
    m_failed = false;
    m_error.clear();
}

void DSDVocoderVocos::reset()
{
    if (m_decoder.isOpen()) {
        m_decoder.reset();
    }

    m_concealedPackets = 0;
}

bool DSDVocoderVocos::decode(const DSDVoiceFrame& frame, float *audio, int& samples)
{
    samples = 0;

    if (!DSDVoiceFrame::isVocos(frame.m_rate) || !open()) {
        return false;
    }

    int n = 0;

    if (frame.m_flush)
    {
        // The end of the transmission: the decoder's last audio, then afresh for the next
        n = m_decoder.flush(m_pcm.data(), m_maxSamples);
    }
    else
    {
        const int packets = std::max(0, std::min(18, frame.m_packets));
        const uint8_t *pointers[18];
        int sizes[18];

        for (int p = 0; p < packets; p++)
        {
            // An erased frame is its packets lost: size 0, and the codec conceals them
            pointers[p] = frame.m_bytes + p * frame.m_packetBytes;
            sizes[p] = frame.m_erasure ? 0 : frame.m_packetBytes;
        }

        if (frame.m_erasure) {
            m_concealedPackets += packets;
        }

        n = packets ? m_decoder.decodePackets(pointers, sizes, packets, m_pcm.data(), m_maxSamples) : 0;

        if (packets && (n <= 0))
        {
            m_error = m_decoder.getError();
            return false;
        }
    }

    samples = std::max(0, n);

    for (int i = 0; i < samples; i++) {
        audio[i] = (float) m_pcm[i];
    }

    return true;
}
