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

#include "dsemodgatewaymonitor.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include <QtEndian>

#include "dsp/dv/dvbits.h"

bool DSEModGatewayMonitor::extract(const DSEModTxSession::Frame& frame,
    std::array<DSDVoiceFrame, 3>& voices, int& count, QString& error)
{
    count = 0;

    if ((frame.format != "dstar-voice") && (frame.format != "dmr-burst")) {
        return true;
    }

    if ((frame.format == "dstar-voice") && (frame.type == "end")) {
        return true;
    }

    if ((frame.format == "dmr-burst") && (frame.type != "voice") && (frame.type != "voice-sync")) {
        return true;
    }

    if (frame.format == "dstar-voice")
    {
        if (frame.data.size() != 12)
        {
            error = "Invalid D-STAR gateway voice frame";
            return false;
        }

        voices[0].m_rate = DSDVoiceFrame::RateAMBE3600x2400;
        std::memcpy(voices[0].m_bytes, frame.data.constData(), 9); // D-STAR bits are LSB first
        count = 1;
    }
    else
    {
        if (frame.data.size() != 33)
        {
            error = "Invalid DMR gateway voice burst";
            return false;
        }

        uint8_t bits[264];
        DVBits::bytesToBitsMSB(reinterpret_cast<const uint8_t*>(frame.data.constData()), 33, bits);
        // The 48 middle bits are sync or embedded signalling, not AMBE voice. These are the
        // same three 72-bit spans emitted by DSDProtocolDMR::emitVoiceFrame.
        const int first[3] = {0, 72, 192};
        const int second[3] = {-1, 156, -1};

        for (int i = 0; i < 3; ++i)
        {
            uint8_t voiceBits[72];
            const int firstLength = (i == 1) ? 36 : 72;
            std::copy(bits + first[i], bits + first[i] + firstLength, voiceBits);

            if (second[i] >= 0) {
                std::copy(bits + second[i], bits + second[i] + 36, voiceBits + firstLength);
            }

            voices[i].m_rate = DSDVoiceFrame::RateAMBE3600x2450;
            DVBits::bitsToBytesMSB(voiceBits, 72, voices[i].m_bytes);
        }

        count = 3;
    }

    for (int i = 0; i < count; ++i) {
        std::fill(voices[i].m_llr, voices[i].m_llr + 72, 127.0f); // gateway bytes are hard decisions
    }

    return true;
}

bool DSEModGatewayMonitor::decode(const DSEModTxSession::Frame& frame, QByteArray& pcm, QString& error)
{
    pcm.clear();
    std::array<DSDVoiceFrame, 3> voices;
    int count = 0;

    if (!extract(frame, voices, count, error)) {
        return false;
    }

    if (!count) {
        return true;
    }

    if (!isAvailable())
    {
        error = "DMR/D-STAR audio feedback requires mbevocoder";
        return false;
    }

    pcm.resize(count * DSDVocoderMbelib::m_samplesPerFrame * 2);

    for (int i = 0; i < count; ++i)
    {
        float audio[DSDVocoderMbelib::m_samplesPerFrame];
        DSDVocoderMbelib::Result result;

        if (!m_vocoder.decode(voices[i], audio, result))
        {
            pcm.clear();
            error = "AMBE gateway voice decode failed";
            return false;
        }

        for (int j = 0; j < DSDVocoderMbelib::m_samplesPerFrame; ++j)
        {
            const qint16 sample = qint16(std::lround(std::max(-32768.0f, std::min(32767.0f, audio[j]))));
            qToLittleEndian<qint16>(sample,
                reinterpret_cast<uchar*>(pcm.data() + (i * DSDVocoderMbelib::m_samplesPerFrame + j) * 2));
        }
    }

    return true;
}
