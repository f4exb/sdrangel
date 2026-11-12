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
#include <atomic>

#include <QMutex>

#include "dsemodvocoder.h"

// Silence frames as produced by the mbevocoder encoder for an all zero input.

// AMBE 3600x2400 (D-STAR) in D-STAR on-air order, LSB first per byte
const uint8_t DSEModVocoder::m_silence3600x2400[9] = {
    0x4E, 0x0F, 0x12, 0x18, 0xE4, 0x4E, 0x5F, 0xB7, 0xB9
};

// AMBE+2 3600x2450 in DMR on-air order, MSB first per byte
const uint8_t DSEModVocoder::m_silence3600x2450[9] = {
    0xDF, 0xE7, 0xA2, 0x75, 0x77, 0x14, 0x7C, 0x87, 0xBB
};

// IMBE 7200x4400 in P25 Phase 1 on-air order, MSB first per byte
const uint8_t DSEModVocoder::m_silenceIMBE7200x4400[18] = {
    0x78, 0x2D, 0x40, 0xBE, 0x32, 0xB5, 0x6E, 0x68, 0x56, 0xE5, 0x8A, 0x4D, 0x46, 0x00, 0xA1, 0x74, 0x53, 0x4F
};

uint8_t DSEModVocoder::m_silenceCodec23200[8] = { 0 };
uint8_t DSEModVocoder::m_silenceVocos[20] = { 0 };
uint8_t DSEModVocoder::m_silenceOpus80[184] = { 0 };

// The encoded silence frames are set by the first encoder of their rate to open, once: every
// channel's processor thread reads them, and one rewriting them while another copied one was a
// data race. Until set, zeros
namespace {
QMutex silenceMutex;
std::atomic<bool> silenceCodec2Set{false};
std::atomic<bool> silenceVocosSet{false};
std::atomic<bool> silenceOpusSet{false};
const uint8_t silenceUnset[184] = { 0 };
}

const uint8_t *DSEModVocoder::getSilenceFrame(Rate rate)
{
    switch (rate)
    {
    case Rate3600x2450:
        return m_silence3600x2450;
    case RateIMBE7200x4400:
        return m_silenceIMBE7200x4400;
    case RateCodec23200:
        return silenceCodec2Set.load(std::memory_order_acquire) ? m_silenceCodec23200 : silenceUnset;
    case RateVocos:
        return silenceVocosSet.load(std::memory_order_acquire) ? m_silenceVocos : silenceUnset;
    case RateOpus80:
        return silenceOpusSet.load(std::memory_order_acquire) ? m_silenceOpus80 : silenceUnset;
    case Rate3600x2400:
    default:
        return m_silence3600x2400;
    }
}

void DSEModVocoder::setSilenceFrame(Rate rate, const uint8_t *frame)
{
    QMutexLocker locker(&silenceMutex);

    if ((rate == RateCodec23200) && !silenceCodec2Set.load(std::memory_order_relaxed))
    {
        std::copy(frame, frame + 8, m_silenceCodec23200);
        silenceCodec2Set.store(true, std::memory_order_release);
    }
    else if ((rate == RateVocos) && !silenceVocosSet.load(std::memory_order_relaxed))
    {
        std::copy(frame, frame + 20, m_silenceVocos);
        silenceVocosSet.store(true, std::memory_order_release);
    }
    else if ((rate == RateOpus80) && !silenceOpusSet.load(std::memory_order_relaxed))
    {
        std::copy(frame, frame + 184, m_silenceOpus80);
        silenceOpusSet.store(true, std::memory_order_release);
    }
}

bool DSEModVocoder::dtmfFrequencies(char digit, float& low, float& high)
{
    static const char keys[4][4] = { {'1', '2', '3', 'A'}, {'4', '5', '6', 'B'}, {'7', '8', '9', 'C'}, {'*', '0', '#', 'D'} };
    static const float rows[4] = { 697.0f, 770.0f, 852.0f, 941.0f };
    static const float cols[4] = { 1209.0f, 1336.0f, 1477.0f, 1633.0f };

    for (int r = 0; r < 4; r++)
    {
        for (int c = 0; c < 4; c++)
        {
            if (keys[r][c] == digit)
            {
                low = rows[r];
                high = cols[c];
                return true;
            }
        }
    }

    return false;
}
