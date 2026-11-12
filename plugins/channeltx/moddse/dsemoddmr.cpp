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
#include <cmath>
#include <cstring>

#include <QDebug>

#include "dsp/dv/dvdmr.h"

#include "dsemodshaper.h"
#include "dsemoddmr.h"

// SYNC patterns (table 9.2) as bits, MSB first
const uint8_t DSEModDMR::m_syncVoiceBS[DSEModDMR::m_syncBits] = {
    0,1,1,1, 0,1,0,1, 0,1,0,1, 1,1,1,1, 1,1,0,1, 0,1,1,1, 1,1,0,1, 1,1,1,1, 0,1,1,1, 0,1,0,1, 1,1,1,1, 0,1,1,1 // 755FD7DF75F7
};

const uint8_t DSEModDMR::m_syncDataBS[DSEModDMR::m_syncBits] = {
    1,1,0,1, 1,1,1,1, 1,1,1,1, 0,1,0,1, 0,1,1,1, 1,1,0,1, 0,1,1,1, 0,1,0,1, 1,1,0,1, 1,1,1,1, 0,1,0,1, 1,1,0,1 // DFF57D75DF5D
};

const uint8_t DSEModDMR::m_syncVoiceMS[DSEModDMR::m_syncBits] = {
    0,1,1,1, 1,1,1,1, 0,1,1,1, 1,1,0,1, 0,1,0,1, 1,1,0,1, 1,1,0,1, 0,1,0,1, 0,1,1,1, 1,1,0,1, 1,1,1,1, 1,1,0,1 // 7F7D5DD57DFD
};

const uint8_t DSEModDMR::m_syncDataMS[DSEModDMR::m_syncBits] = {
    1,1,0,1, 0,1,0,1, 1,1,0,1, 0,1,1,1, 1,1,1,1, 0,1,1,1, 0,1,1,1, 1,1,1,1, 1,1,0,1, 0,1,1,1, 0,1,0,1, 0,1,1,1 // D5D7F77FD757
};

DSEModDMR::DSEModDMR() :
    m_state(StateIdle),
    m_stopRequested(false),
    m_direct(false),
    m_colorCode(1),
    m_voiceSlot(0),
    m_positionValid(false),
    m_sendPosition(false),
    m_latitude(0.0),
    m_longitude(0.0),
    m_positionError(7),
    m_gpsPending(false),
    m_gpsSuperframe(false),
    m_superframeCount(0),
    m_stateFrameCount(0),
    m_superframeIndex(0),
    m_cachLcssIndex(0),
    m_voiceFrameCount(0)
{
    std::fill(m_lc, m_lc + 9, 0);
    std::fill(m_vlcInfo, m_vlcInfo + m_infoBits, 0);
    std::fill(m_tlcInfo, m_tlcInfo + m_infoBits, 0);
    std::fill(m_idleInfo, m_idleInfo + m_infoBits, 0);
    std::fill(m_embeddedLC, m_embeddedLC + 128, 0);
    std::fill(m_embeddedGPS, m_embeddedGPS + 128, 0);
}

DSEModDMR::~DSEModDMR()
{
}

void DSEModDMR::setPosition(bool valid, double latitude, double longitude, float altitude,
    int speed, int course, int source)
{
    (void) altitude;   // the GPS Info LC has no altitude, course or speed
    (void) speed;
    (void) course;
    m_positionValid = valid;
    m_latitude = latitude;
    m_longitude = longitude;
    // The standard's accuracy classes: a GNSS fix is within 20 m, and a position typed into the
    // preferences has no accuracy to declare
    m_positionError = (source == 0) ? 1 : 7;
    // Rebuilt where the four fragments of a superframe cannot straddle two positions, which
    // would fail the embedded LC's checksum and cost the receiver that report
    m_gpsPending = true;
}

void DSEModDMR::buildGPSLC(double latitude, double longitude, int positionError, uint8_t *lc)
{
    // Full LC: PF(1) R(1) FLCO(6) | FID(8) | reserved(4) position error(3) longitude(25) |
    // latitude(24). Both are two's complement fractions of the full range (ETSI TS 102 361-2
    // 7.1.1.4): the longitude of 2^25 over 360 degrees, the latitude of 2^24 over 180
    long longitudeBits = std::lround(longitude * (33554432.0 / 360.0));
    long latitudeBits = std::lround(latitude * (16777216.0 / 180.0));

    // +180 and +90 are the negative end of their fields, so they clamp to the last value below
    longitudeBits = std::max(-16777216L, std::min(16777215L, longitudeBits));
    latitudeBits = std::max(-8388608L, std::min(8388607L, latitudeBits));

    lc[0] = 0x08;   // FLCO 8: GPS Info
    lc[1] = 0x00;   // FID 0: the standard's own feature set
    lc[2] = (uint8_t) (((positionError & 7) << 1) | ((longitudeBits >> 24) & 1));
    lc[3] = (uint8_t) ((longitudeBits >> 16) & 0xff);
    lc[4] = (uint8_t) ((longitudeBits >> 8) & 0xff);
    lc[5] = (uint8_t) (longitudeBits & 0xff);
    lc[6] = (uint8_t) ((latitudeBits >> 16) & 0xff);
    lc[7] = (uint8_t) ((latitudeBits >> 8) & 0xff);
    lc[8] = (uint8_t) (latitudeBits & 0xff);
}

void DSEModDMR::updateEmbeddedGPS()
{
    uint8_t lc[9];
    buildGPSLC(m_latitude, m_longitude, m_positionError, lc);
    DVDMR::embeddedLCEncode(lc, m_embeddedGPS);
}

void DSEModDMR::buildLC(const DSEModSettings& settings, uint8_t *lc)
{
    // Full LC: PF(1) R(1) FLCO(6) | FID(8) | service options(8) | destination(24) | source(24)
    // FLCO 0 = group voice channel user, 3 = unit to unit voice channel user (ETSI TS 102 361-2)
    lc[0] = settings.m_dmrGroupCall ? 0x00 : 0x03;
    lc[1] = (uint8_t) settings.m_dmrFid;
    lc[2] = (uint8_t) settings.m_dmrServiceOptions;
    lc[3] = (settings.m_dmrDestinationId >> 16) & 0xff;
    lc[4] = (settings.m_dmrDestinationId >> 8) & 0xff;
    lc[5] = settings.m_dmrDestinationId & 0xff;
    lc[6] = (settings.m_dmrSourceId >> 16) & 0xff;
    lc[7] = (settings.m_dmrSourceId >> 8) & 0xff;
    lc[8] = settings.m_dmrSourceId & 0xff;
}

void DSEModDMR::vocoderFrameToBits(const uint8_t *frame, uint8_t *bits)
{
    for (int i = 0; i < m_voiceFrameBits; i++) {
        bits[i] = (frame[i / 8] >> (7 - (i % 8))) & 1;
    }
}

void DSEModDMR::buildDataBurst(int colorCode, int dataType, const uint8_t *info196, int tc, int cachLcss, const uint8_t *cachPayload, uint8_t *bits)
{
    uint8_t slotType[20];
    DVDMR::cachEncode(true, tc, cachLcss, cachPayload, &bits[0]);
    DVDMR::slotTypeEncode(colorCode, dataType, slotType);
    std::copy(info196, info196 + 98, &bits[24]);
    std::copy(slotType, slotType + 10, &bits[122]);
    std::copy(m_syncDataBS, m_syncDataBS + m_syncBits, &bits[132]);
    std::copy(slotType + 10, slotType + 20, &bits[180]);
    std::copy(info196 + 98, info196 + 196, &bits[190]);
}

void DSEModDMR::buildVoiceBurst(int colorCode, const uint8_t *vf1, const uint8_t *vf2, const uint8_t *vf3,
    const uint8_t *embedded, int embLcss, int tc, int cachLcss, const uint8_t *cachPayload, uint8_t *bits)
{
    DVDMR::cachEncode(true, tc, cachLcss, cachPayload, &bits[0]);
    std::copy(vf1, vf1 + 72, &bits[24]);
    std::copy(vf2, vf2 + 36, &bits[96]);

    if (embedded)
    {
        uint8_t emb[16];
        DVDMR::embEncode(colorCode, false, embLcss, emb);
        std::copy(emb, emb + 8, &bits[132]);
        std::copy(embedded, embedded + 32, &bits[140]);
        std::copy(emb + 8, emb + 16, &bits[172]);
    }
    else
    {
        std::copy(m_syncVoiceBS, m_syncVoiceBS + m_syncBits, &bits[132]);
    }

    std::copy(vf2 + 36, vf2 + 72, &bits[180]);
    std::copy(vf3, vf3 + 72, &bits[216]);
}

void DSEModDMR::appendBits(std::vector<int8_t>& levels, const uint8_t *bits, int nbBits)
{
    // Dibit to deviation: 01 -> +3, 00 -> +1, 10 -> -1, 11 -> -3 (table 10.2)
    static const int8_t dibitLevels[4] = { 1, 3, -1, -3 };

    for (int i = 0; i + 1 < nbBits; i += 2) {
        levels.push_back(dibitLevels[(bits[i] << 1) | bits[i + 1]]);
    }
}

void DSEModDMR::start(const DSEModSettings& settings)
{
    m_colorCode = settings.m_dmrColorCode & 0x0f;
    m_voiceSlot = (settings.m_dmrSlot == 2) ? 1 : 0;
    m_direct = settings.m_dmrDirect;
    buildLC(settings, m_lc);

    uint8_t info[96];
    DVDMR::fullLCEncode(m_lc, DVDMR::m_maskVoiceLCHeader, info);
    DVDMR::bptcEncode(info, m_vlcInfo);
    DVDMR::fullLCEncode(m_lc, DVDMR::m_maskTerminatorWithLC, info);
    DVDMR::bptcEncode(info, m_tlcInfo);

    // Idle: pseudo random fill bits (a fixed LFSR sequence keeps the spectrum busy)
    uint16_t lfsr = 0xACE1;

    for (int i = 0; i < 96; i++)
    {
        info[i] = lfsr & 1;
        lfsr = (lfsr >> 1) ^ ((lfsr & 1) ? 0xB400 : 0);
    }

    DVDMR::bptcEncode(info, m_idleInfo);
    DVDMR::embeddedLCEncode(m_lc, m_embeddedLC);

    m_sendPosition = settings.m_sendPosition && m_positionValid;

    if (m_sendPosition)
    {
        updateEmbeddedGPS();
        m_gpsPending = false;
    }

    m_stopRequested = false;
    m_stateFrameCount = 0;
    m_superframeIndex = 0;
    m_cachLcssIndex = 0;
    m_superframeCount = 0;
    m_gpsSuperframe = false;
    m_voiceFrameCount = 0;
    m_state = m_direct ? StateHeader : StateStart; // an MS keys straight into the header, a BS wakes up with idle bursts

    qDebug("DSEModDMR::start: %s CC: %d slot: %d %s %d > %d", m_direct ? "MS" : "BS", m_colorCode, m_voiceSlot + 1,
        settings.m_dmrGroupCall ? "group" : "unit", settings.m_dmrSourceId, settings.m_dmrDestinationId);
}

void DSEModDMR::stop()
{
    m_stopRequested = true;
}

void DSEModDMR::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

int DSEModDMR::nextCachLcss()
{
    // Null short LC spread over 4 CACH bursts: first, continuation, continuation, last
    static const int lcss[4] = { LCSSFirst, LCSSContinuation, LCSSContinuation, LCSSLast };
    int value = lcss[m_cachLcssIndex];
    m_cachLcssIndex = (m_cachLcssIndex + 1) % 4;
    return value;
}

void DSEModDMR::buildIdleSlot(int tc, uint8_t *bits)
{
    buildDataBurst(m_colorCode, DataTypeIdle, m_idleInfo, tc, nextCachLcss(), nullptr, bits);
}

void DSEModDMR::buildVoiceSlot(uint8_t *bits)
{
    int tc = m_voiceSlot;

    switch (m_state)
    {
    case StateStart:
        buildIdleSlot(tc, bits);

        if (++m_stateFrameCount >= m_startFrames)
        {
            m_state = StateHeader;
            m_stateFrameCount = 0;
        }
        break;

    case StateHeader:
        buildDataBurst(m_colorCode, DataTypeVoiceLCHeader, m_vlcInfo, tc, nextCachLcss(), nullptr, bits);

        if (++m_stateFrameCount >= m_headerBursts)
        {
            m_state = StateVoice;
            m_stateFrameCount = 0;
            m_superframeIndex = 0;
        }
        break;

    case StateVoice:
    {
        uint8_t frame[3][DSEModVocoder::m_maxFrameBytes];
        uint8_t vf[3][m_voiceFrameBits];

        for (int i = 0; i < 3; i++)
        {
            pullVocoderFrame(frame[i]);
            vocoderFrameToBits(frame[i], vf[i]);
            m_voiceFrameCount++;
        }

        if (m_superframeIndex == 0)
        {
            buildVoiceBurst(m_colorCode, vf[0], vf[1], vf[2], nullptr, 0, tc, nextCachLcss(), nullptr, bits);
        }
        else if (m_superframeIndex < 5)
        {
            // The four fragments of one full LC: the voice LC, or the position when this is one
            // of the superframes that carries it (decided at burst B so that all four agree)
            static const int lcss[4] = { LCSSFirst, LCSSContinuation, LCSSContinuation, LCSSLast };
            int fragment = m_superframeIndex - 1;

            if (fragment == 0)
            {
                m_gpsSuperframe = m_sendPosition && ((m_superframeCount % m_positionIntervalSuperframes) == 1);

                if (m_gpsSuperframe && m_gpsPending)
                {
                    updateEmbeddedGPS();
                    m_gpsPending = false;
                }
            }

            const uint8_t *embedded = m_gpsSuperframe ? m_embeddedGPS : m_embeddedLC;
            buildVoiceBurst(m_colorCode, vf[0], vf[1], vf[2], &embedded[32 * fragment], lcss[fragment], tc, nextCachLcss(), nullptr, bits);
        }
        else
        {
            uint8_t nullEmbedded[32];
            std::fill(nullEmbedded, nullEmbedded + 32, 0); // null embedded message (annex D.1)
            buildVoiceBurst(m_colorCode, vf[0], vf[1], vf[2], nullEmbedded, LCSSSingle, tc, nextCachLcss(), nullptr, bits);
        }

        m_superframeIndex++;

        if (m_superframeIndex == m_burstsPerSuperframe)
        {
            m_superframeIndex = 0;
            m_superframeCount++;
        }

        if (m_stopRequested) // the terminator follows the last voice burst at any point of the superframe
        {
            m_state = StateTerminator;
            m_stateFrameCount = 0;
        }
        break;
    }

    case StateTerminator:
        buildDataBurst(m_colorCode, DataTypeTerminatorWithLC, m_tlcInfo, tc, nextCachLcss(), nullptr, bits);

        if (++m_stateFrameCount >= m_terminatorBursts)
        {
            m_state = m_direct ? StateIdle : StateHang; // an MS drops the carrier after the terminator
            m_stateFrameCount = 0;
            m_stopRequested = m_direct ? false : m_stopRequested;
        }
        break;

    case StateHang:
        buildIdleSlot(tc, bits);

        if (++m_stateFrameCount >= m_hangFrames)
        {
            m_state = StateIdle;
            m_stopRequested = false;
        }
        break;

    case StateIdle:
    default:
        buildIdleSlot(tc, bits);
        break;
    }
}

void DSEModDMR::applySync(uint8_t *bits, bool voice)
{
    const uint8_t *sync = voice ? m_syncVoiceMS : m_syncDataMS;
    std::copy(sync, sync + m_syncBits, &bits[m_cachBits + 108]);
}

bool DSEModDMR::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    if (m_state == StateIdle) {
        return false;
    }

    uint8_t bits[m_slotBits];

    if (m_direct)
    {
        // MS sourced (ETSI TS 102 361-1 clause 4.2.2): a single 264 bit burst per 60 ms TDMA frame with
        // the MS sync patterns and no CACH, the carrier off in the guard time and the other slot.
        // Voice bursts B to F carry EMB and embedded signalling instead of a sync: left as built.
        bool syncBurst = (m_state != StateVoice) || (m_superframeIndex == 0);
        bool voice = (m_state == StateVoice);
        buildVoiceSlot(bits);

        if (syncBurst) {
            applySync(bits, voice);
        }

        appendBits(levels, &bits[m_cachBits], m_burstBits);
        levels.insert(levels.end(), (m_slotBits - m_burstBits) / 2 + m_slotBits / 2, DSEModShaper::m_levelOff); // guard + other slot
        return true;
    }

    for (int slot = 0; slot < 2; slot++)
    {
        if (slot == m_voiceSlot) {
            buildVoiceSlot(bits);
        } else {
            buildIdleSlot(slot, bits);
        }

        appendBits(levels, bits, m_slotBits);
    }

    return true;
}
