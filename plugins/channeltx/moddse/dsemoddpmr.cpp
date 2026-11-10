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
#include <cstring>

#include <QDebug>

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvdpmr.h"
#include "dsp/dv/dvscrambler.h"

#include "dsemoddpmr.h"

// Frame sync words (TS 102 490 clause 6.1) as bits, MSB first
const uint8_t DSEModDPMR::m_fs1[DSEModDPMR::m_fs1Bits] = {
    0,1,0,1, 0,1,1,1, 1,1,1,1, 1,1,1,1, 0,1,0,1, 1,1,1,1, 0,1,1,1, 0,1,0,1, 1,1,0,1, 0,1,0,1, 0,1,1,1, 0,1,1,1 // 57FF5F75D577
};

const uint8_t DSEModDPMR::m_fs2[DSEModDPMR::m_fs2Bits] = {
    0,1,0,1, 1,1,1,1, 1,1,1,1, 0,1,1,1, 0,1,1,1, 1,1,0,1 // 5FF77D
};

const uint8_t DSEModDPMR::m_fs3[DSEModDPMR::m_fs3Bits] = {
    0,1,1,1, 1,1,0,1, 1,1,0,1, 1,1,1,1, 1,1,1,1, 0,1,0,1 // 7DDFF5
};

DSEModDPMR::DSEModDPMR() :
    m_state(StateIdle),
    m_stopRequested(false),
    m_colourCode(0),
    m_calledId(m_allCallId),
    m_ownId(1),
    m_format(CommFormatAllCall),
    m_stateFrameCount(0),
    m_frameNumber(0),
    m_voiceFrameCount(0)
{
    std::fill(m_hiCoded, m_hiCoded + m_hiCodedBits, 0);
    std::fill(&m_cchCoded[0][0], &m_cchCoded[0][0] + m_framesPerSuperframe * m_cchCodedBits, 0);
    std::fill(m_endCoded, m_endCoded + m_endCodedBits, 0);
}

DSEModDPMR::~DSEModDPMR()
{
}

// ---- Coding primitives ----------------------------------------------------------

void DSEModDPMR::crc7Append(uint8_t *bits, int nbBits)
{
    DVDPMR::crc7Append(bits, nbBits);
}

void DSEModDPMR::crc8Append(uint8_t *bits, int nbBits)
{
    DVDPMR::crc8Append(bits, nbBits);
}

void DSEModDPMR::hamming128Encode(const uint8_t *data8, uint8_t *code12)
{
    DVDPMR::hammingBlockEncode(data8, 1, code12);
}

void DSEModDPMR::interleave(const uint8_t *in, int nbBlocks, uint8_t *out)
{
    DVDPMR::interleave(in, nbBlocks, out);
}

void DSEModDPMR::scramblerSequence(uint8_t *seq, int nbBits)
{
    DVScrambler::dpmrSequence(seq, nbBits);
}

// ---- Field builders ---------------------------------------------------------------

void DSEModDPMR::valueToBits(uint32_t value, int nbBits, uint8_t *bits)
{
    for (int i = 0; i < nbBits; i++) {
        bits[i] = (value >> (nbBits - 1 - i)) & 1;
    }
}

void DSEModDPMR::buildHeaderInfo(int headerType, uint32_t calledId, uint32_t ownId, int mode, int format, uint8_t *hi72)
{
    // TS 102 490 table 5.2: HT(4) ID0+1(24) ID2+3(24) M(3) V(2) F(2) RES(2) CI(11)
    valueToBits(headerType, 4, &hi72[0]);
    valueToBits(calledId, 24, &hi72[4]);
    valueToBits(ownId, 24, &hi72[28]);
    valueToBits(mode, 3, &hi72[52]);
    valueToBits(0, 2, &hi72[55]);     // Version: 00 standard TCH content (table 5.15)
    valueToBits(format, 2, &hi72[57]);
    valueToBits(0, 2, &hi72[59]);     // Reserved (TS 102 658: emergency priority, PM)
    valueToBits(0, 11, &hi72[61]);    // Call information: normal header (clause 5.10.1)
}

void DSEModDPMR::encodeHeaderInfo(const uint8_t *hi72, uint8_t *coded120)
{
    DVDPMR::headerInfoEncode(hi72, coded120);
}

void DSEModDPMR::buildCCH(int frameNumber, uint32_t calledId, uint32_t ownId, int mode, int format, const uint8_t *sld18, uint8_t *cch41)
{
    // TS 102 490 table 5.1 and clause 9.1: FN(2) ID(12) M(3) V(2) F(2) RES(2) SLD(18)
    static const int shifts[4] = { 12, 0, 12, 0 };
    uint32_t id = (frameNumber < 2) ? calledId : ownId;
    valueToBits(frameNumber & 3, 2, &cch41[0]);
    valueToBits((id >> shifts[frameNumber & 3]) & 0xFFF, 12, &cch41[2]);
    valueToBits(mode, 3, &cch41[14]);
    valueToBits(0, 2, &cch41[17]);    // Version
    valueToBits(format, 2, &cch41[19]);
    valueToBits(0, 2, &cch41[21]);    // Reserved

    if (sld18) {
        std::copy(sld18, sld18 + 18, &cch41[23]);
    } else {
        std::fill(&cch41[23], &cch41[41], 0);
    }
}

void DSEModDPMR::encodeCCH(const uint8_t *cch41, uint8_t *coded72)
{
    DVDPMR::cchEncode(cch41, coded72);
}

void DSEModDPMR::buildEndInfo(int endType, int ackRequest, int txWait, int status, uint8_t *end17)
{
    // TS 102 490 table 5.3: ET(2) ARQ(2) WAIT(4) STAT(5) RES(4)
    valueToBits(endType, 2, &end17[0]);
    valueToBits(ackRequest, 2, &end17[2]);
    valueToBits(txWait, 4, &end17[4]);
    valueToBits(status, 5, &end17[8]);
    valueToBits(0, 4, &end17[13]);
}

void DSEModDPMR::encodeEnd(const uint8_t *end17, uint8_t *coded72)
{
    DVDPMR::endEncode(end17, coded72);
}

uint32_t DSEModDPMR::colourCodeSequence(int colourCode)
{
    return DVDPMR::colourCodeSequence(colourCode);
}

void DSEModDPMR::colourCodeBits(int colourCode, uint8_t *bits24)
{
    valueToBits(colourCodeSequence(colourCode), m_ccBits, bits24);
}

int DSEModDPMR::colourCodeValue(int colourCode)
{
    uint32_t seq = colourCodeSequence(colourCode);
    int value = 0;

    for (int i = 0; i < 12; i++) {
        value = (value << 1) | ((seq >> (23 - 2 * i)) & 1); // MSB of the dibit: 1 for 10/11 (negative symbols)
    }

    return value;
}

void DSEModDPMR::vocoderFrameToBits(const uint8_t *frame, uint8_t *bits)
{
    for (int i = 0; i < m_tchBits; i++) {
        bits[i] = (frame[i / 8] >> (7 - (i % 8))) & 1;
    }
}

// ---- Frame builders ---------------------------------------------------------------

void DSEModDPMR::buildHeaderFrame(const uint8_t *hiCoded120, int colourCode, uint8_t *bits384)
{
    int pos = 0;

    for (int i = 0; i < m_preambleBits / 8; i++, pos += 8) {
        valueToBits(m_preambleByte, 8, &bits384[pos]);
    }

    std::copy(m_fs1, m_fs1 + m_fs1Bits, &bits384[pos]);
    pos += m_fs1Bits;
    std::copy(hiCoded120, hiCoded120 + m_hiCodedBits, &bits384[pos]);
    pos += m_hiCodedBits;
    colourCodeBits(colourCode, &bits384[pos]);
    pos += m_ccBits;
    std::copy(hiCoded120, hiCoded120 + m_hiCodedBits, &bits384[pos]);
}

void DSEModDPMR::buildPayloadFrame(int frameNumber, int colourCode, const uint8_t *cchCoded72, const uint8_t *tch4x72, uint8_t *bits384)
{
    if ((frameNumber & 1) == 0) {
        std::copy(m_fs2, m_fs2 + m_fs2Bits, &bits384[0]);
    } else {
        colourCodeBits(colourCode, &bits384[0]);
    }

    std::copy(cchCoded72, cchCoded72 + m_cchCodedBits, &bits384[24]);
    std::copy(tch4x72, tch4x72 + m_tchPerFrame * m_tchBits, &bits384[96]);
}

void DSEModDPMR::buildEndFrame(const uint8_t *endCoded72, uint8_t *bits96)
{
    std::copy(m_fs3, m_fs3 + m_fs3Bits, &bits96[0]);
    std::copy(endCoded72, endCoded72 + m_endCodedBits, &bits96[m_fs3Bits]);
}

// ---- Protocol state machine -----------------------------------------------------

void DSEModDPMR::start(const DSEModSettings& settings)
{
    m_colourCode = settings.m_dpmrColorCode & 63;
    m_calledId = settings.m_dpmrCalledId & 0xFFFFFF;
    m_ownId = settings.m_dpmrOwnId & 0xFFFFFF;
    m_format = (m_calledId == m_allCallId) ? CommFormatAllCall : CommFormatP2P;

    uint8_t hi[m_hiInfoBits];
    buildHeaderInfo(HeaderTypeCommStart, m_calledId, m_ownId, CommModeVoice, m_format, hi);
    encodeHeaderInfo(hi, m_hiCoded);

    for (int fn = 0; fn < m_framesPerSuperframe; fn++)
    {
        uint8_t cch[m_cchInfoBits];
        buildCCH(fn, m_calledId, m_ownId, CommModeVoice, m_format, nullptr, cch);
        encodeCCH(cch, m_cchCoded[fn]);
    }

    uint8_t end[m_endInfoBits];
    buildEndInfo(0, 0, 0, 0, end); // normal end, no ACK request, no Tx wait, status 0
    encodeEnd(end, m_endCoded);

    m_stopRequested = false;
    m_stateFrameCount = 0;
    m_frameNumber = 0;
    m_voiceFrameCount = 0;
    m_slowData.clear();
    m_state = (m_preambleFrames > 0) ? StatePreamble : StateHeader;

    qDebug("DSEModDPMR::start: CC: %d own: %u called: %u (%s)", m_colourCode, m_ownId, m_calledId,
        m_format == CommFormatAllCall ? "all call" : "peer to peer");
}

bool DSEModDPMR::queueMessage(const std::string& text)
{
    if (text.empty() || (m_state == StateIdle) || (m_slowData.size() + text.size() > (size_t) m_maxSlowDataBytes)) {
        return false;
    }

    for (size_t i = 0; i < text.size(); i++) {
        m_slowData.emplace_back((uint8_t) text[i], i == text.size() - 1);
    }

    return true;
}

void DSEModDPMR::stop()
{
    m_stopRequested = true;
}

void DSEModDPMR::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

bool DSEModDPMR::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    if (m_state == StateIdle) {
        return false;
    }

    uint8_t bits[m_frameBits];

    switch (m_state)
    {
    case StatePreamble:
        for (int i = 0; i < m_frameBits / 8; i++) {
            valueToBits(m_preambleByte, 8, &bits[8 * i]);
        }

        appendDibits(levels, bits, m_frameBits);

        if (++m_stateFrameCount >= m_preambleFrames)
        {
            m_state = StateHeader;
            m_stateFrameCount = 0;
        }
        break;

    case StateHeader:
        buildHeaderFrame(m_hiCoded, m_colourCode, bits);
        appendDibits(levels, bits, m_frameBits);
        m_state = StateVoice;
        m_frameNumber = 0;
        break;

    case StateVoice:
    {
        uint8_t frame[DSEModVocoder::m_maxFrameBytes];
        uint8_t tch[m_tchPerFrame * m_tchBits];

        for (int i = 0; i < m_tchPerFrame; i++)
        {
            pullVocoderFrame(frame);
            vocoderFrameToBits(frame, &tch[i * m_tchBits]);
            m_voiceFrameCount++;
        }

        if (m_slowData.empty())
        {
            buildPayloadFrame(m_frameNumber, m_colourCode, m_cchCoded[m_frameNumber], tch, bits);
        }
        else
        {
            // Voice + slow data: CONT1 (1) byte1 (8) CONT2 (1) byte2 (8), zeros after a message's end
            uint8_t sld[18] = { 0 };

            for (int k = 0; (k < 2) && !m_slowData.empty(); k++)
            {
                sld[9 * k] = m_slowData.front().second ? 1 : 0;
                valueToBits(m_slowData.front().first, 8, &sld[9 * k + 1]);
                m_slowData.pop_front();
            }

            uint8_t cch[m_cchInfoBits];
            uint8_t coded[m_cchCodedBits];
            buildCCH(m_frameNumber, m_calledId, m_ownId, CommModeVoiceSLD, m_format, sld, cch);
            encodeCCH(cch, coded);
            buildPayloadFrame(m_frameNumber, m_colourCode, coded, tch, bits);
        }

        appendDibits(levels, bits, m_frameBits);
        m_frameNumber++;

        if (m_frameNumber == m_framesPerSuperframe)
        {
            m_frameNumber = 0;

            if (m_stopRequested) { // a call is an integral number of superframes (clause 4.2.3)
                m_state = StateEnd;
            }
        }
        break;
    }

    case StateEnd:
    {
        uint8_t endBits[m_endFrameBits + m_tailBits];
        buildEndFrame(m_endCoded, endBits);

        // Trailing preamble pattern as sent by real radios after the end frame. It also gives
        // dsdcc the extra symbol it needs to leave its end frame state.
        for (int i = 0; i < m_tailBits / 8; i++) {
            valueToBits(m_preambleByte, 8, &endBits[m_endFrameBits + 8 * i]);
        }

        appendDibits(levels, endBits, m_endFrameBits + m_tailBits);
        m_state = StateIdle;
        m_stopRequested = false;
        break;
    }

    case StateIdle:
    default:
        return false;
    }

    return true;
}
