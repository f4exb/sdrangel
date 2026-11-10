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

#include "dsp/dv/dvambe.h"

#include "dsemodysf.h"

// Frame sync D4 71 C9 63 4D as bits, MSB first (dsdcc SyncYSF: only the signs of the
// symbols are matched, giving the D5 75 DD 77 5D pattern quoted in dsd_sync.cpp)
const uint8_t DSEModYSF::m_syncBits[2 * DSEModYSF::m_syncDibits] = {
    1,1,0,1, 0,1,0,0, 0,1,1,1, 0,0,0,1, 1,1,0,0, 1,0,0,1, 0,1,1,0, 0,0,1,1, 0,1,0,0, 1,1,0,1
};

DSEModYSF::DSEModYSF(DataType dataType) :
    m_state(StateIdle),
    m_stopRequested(false),
    m_frameNumber(0),
    m_dataType(dataType),
    m_subHeaderSent(false),
    m_voiceFrameCount(0)
{
    std::memset(m_csd1, ' ', sizeof(m_csd1));
    std::memset(m_csd2, ' ', sizeof(m_csd2));
    std::memset(m_csd3, ' ', sizeof(m_csd3));
    std::memset(m_dt1, 0, sizeof(m_dt1));
    std::memset(m_dt2, 0, sizeof(m_dt2));
    std::memset(m_headerPayload, 0, sizeof(m_headerPayload));
}

DSEModYSF::~DSEModYSF()
{
}

DVYSF::FICH DSEModYSF::makeFICH(FrameInformation fi, int fn)
{
    DVYSF::FICH fich;
    fich.m_fi = (int) fi;
    fich.m_cs = 2;   // callsign type: not decoded by dsdcc (see NOTES-ysf.md)
    fich.m_cm = 0;   // group CQ
    fich.m_bn = 0;
    fich.m_bt = 0;
    fich.m_fn = fn & 7;
    fich.m_ft = m_framesPerSuperframe - 1;
    fich.m_dev = 0;  // wide
    fich.m_mr = 0;   // direct wave
    fich.m_voip = 0; // local
    fich.m_dt = (int) DTVoiceData2;
    fich.m_sql = 0;
    fich.m_sq = 0;
    return fich;
}

void DSEModYSF::formatCallsign(const QString& callsign, uint8_t *out)
{
    QByteArray bytes = callsign.trimmed().toUpper().toLatin1();
    std::memset(out, ' ', m_callsignLength);

    for (int i = 0; (i < bytes.size()) && (i < m_callsignLength); i++) {
        out[i] = (uint8_t) bytes.at(i);
    }
}

void DSEModYSF::buildCSD(const DSEModSettings& settings, uint8_t *csd1, uint8_t *csd2)
{
    // CSD1: destination (10) + source (10). CSD2: downlink (10) + uplink (10)
    // (dsdcc DSDYSF::processCSD1 / processCSD2 byte order)
    formatCallsign(settings.m_ysfDestination, &csd1[0]);
    formatCallsign(settings.m_ysfCallsign, &csd1[m_callsignLength]);
    formatCallsign(settings.m_ysfDownlink, &csd2[0]);
    formatCallsign(settings.m_ysfUplink, &csd2[m_callsignLength]);
}

void DSEModYSF::buildHeaderPayload(const uint8_t *csd1, const uint8_t *csd2, uint8_t *dibits)
{
    // dsdcc DSDYSF::processHeader: 36 dibit chunks alternating DCH1(0) DCH2(0) DCH1(1) ... DCH2(4)
    uint8_t dch1[DVYSF::m_headerDCHDibits];
    uint8_t dch2[DVYSF::m_headerDCHDibits];
    DVYSF::dchEncode(csd1, 20, dch1);
    DVYSF::dchEncode(csd2, 20, dch2);

    for (int chunk = 0; chunk < 5; chunk++)
    {
        std::copy(&dch1[m_dchChunkDibits * chunk], &dch1[m_dchChunkDibits * (chunk + 1)], &dibits[2 * m_dchChunkDibits * chunk]);
        std::copy(&dch2[m_dchChunkDibits * chunk], &dch2[m_dchChunkDibits * (chunk + 1)], &dibits[2 * m_dchChunkDibits * chunk + m_dchChunkDibits]);
    }
}

void DSEModYSF::buildVD2Payload(const uint8_t *dch, const uint8_t ambe49[5][DVYSF::m_ambeBits], uint8_t *dibits)
{
    // dsdcc DSDYSF::processVD2: five blocks of 20 DCH dibits followed by 52 voice dibits
    uint8_t dchDibits[DVYSF::m_vd2DCHDibits];
    DVYSF::dchEncode(dch, 10, dchDibits);

    for (int block = 0; block < m_voiceFramesPerFrame; block++)
    {
        uint8_t *p = &dibits[m_vd2BlockDibits * block];
        std::copy(&dchDibits[m_vd2DCHSliceDibits * block], &dchDibits[m_vd2DCHSliceDibits * (block + 1)], p);
        DVYSF::vd2VoiceEncode(ambe49[block], p + m_vd2DCHSliceDibits);
    }
}

void DSEModYSF::buildFrame(const DVYSF::FICH& fich, const uint8_t *payload, uint8_t *dibits)
{
    for (int i = 0; i < m_syncDibits; i++) {
        dibits[i] = (uint8_t) ((m_syncBits[2 * i] << 1) | m_syncBits[2 * i + 1]);
    }

    DVYSF::fichEncode(fich, &dibits[m_syncDibits]);
    std::copy(payload, payload + m_payloadDibits, &dibits[m_syncDibits + m_fichDibits]);
}

void DSEModYSF::appendDibitLevels(std::vector<int8_t>& levels, const uint8_t *dibits, int nbDibits)
{
    for (int i = 0; i < nbDibits; i++) {
        levels.push_back(dibitToLevel(dibits[i]));
    }
}

void DSEModYSF::start(const DSEModSettings& settings)
{
    buildCSD(settings, m_csd1, m_csd2);
    m_message.clear();
    m_subHeaderSent = false;
    std::memset(m_csd3, ' ', sizeof(m_csd3)); // rem1..rem4 blank
    std::memset(m_dt1, 0, sizeof(m_dt1));
    std::memset(m_dt2, 0, sizeof(m_dt2));
    buildHeaderPayload(m_csd1, m_csd2, m_headerPayload);

    m_stopRequested = false;
    m_frameNumber = 0;
    m_voiceFrameCount = 0;
    m_state = StatePreamble;

    qDebug() << "DSEModYSF::start:"
        << "src:" << QString::fromLatin1((const char *) &m_csd1[10], 10)
        << "dest:" << QString::fromLatin1((const char *) &m_csd1[0], 10)
        << "downlink:" << QString::fromLatin1((const char *) &m_csd2[0], 10)
        << "uplink:" << QString::fromLatin1((const char *) &m_csd2[10], 10);
}

void DSEModYSF::stop()
{
    m_stopRequested = true;
}

void DSEModYSF::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

const uint8_t *DSEModYSF::dchDataForFrame(int fn) const
{
    // dsdcc DSDYSF::processVD2: FN 0 dest, 1 src, 2 downlink, 3 uplink, 4 rem1+rem2, 5 rem3+rem4; 6 DT1 and 7 DT2 are not decoded
    switch (fn)
    {
    case 0: return &m_csd1[0];
    case 1: return &m_csd1[10];
    case 2: return &m_csd2[0];
    case 3: return &m_csd2[10];
    case 4: return &m_csd3[0];
    case 5: return &m_csd3[10];
    case 6: return m_dt1;
    case 7:
    default: return m_dt2;
    }
}

bool DSEModYSF::queueMessage(const std::string& text)
{
    if (text.empty() || (m_state == StateIdle) || !m_message.empty()) {
        return false;
    }

    // seq, "Ge_", radio model (unknown), then serial (6), two times (12 each), room (5), 10 unknown
    std::vector<uint8_t> m = { 0x00, 0x47, 0x65, 0x5F, 0x00 };
    const std::string fields = std::string(6, '0') + std::string(24, '0') + std::string(5, '0') + std::string(10, ' ');
    m.insert(m.end(), fields.begin(), fields.end());
    std::string body = text.substr(0, 80);
    body.resize(80, ' ');
    m.insert(m.end(), body.begin(), body.end());
    m.push_back(0x03);
    uint8_t sum = 0;

    for (uint8_t b : m) {
        sum += b;
    }

    m.push_back(sum);
    // Block 0 alone (up to 260 bytes: FT 7), zeros to the end of its last frame
    const int ft = dataFRFrames((int) m.size());
    m.resize(20 + 40 * (ft - 1), 0);
    m_message = m;
    return true;
}

void DSEModYSF::nextVoiceFrame(std::vector<int8_t>& levels)
{
    // V/D type 1 (MMDVMHost YSFPayload, dsdcc processVD1): five blocks of a 36 dibit slice of the
    // data channel (CSD1, CSD2, CSD3, then DT1 to DT5 by FN) and an AMBE+2 frame in DMR's order.
    // Voice full rate (VW): five IMBE frames in YSF's order, the first frame after the header (FN 0
    // with FT 1) CSD3, 36 dibits reserved and two of them
    uint8_t dibits[m_frameDibits];
    uint8_t payload[m_payloadDibits] = { 0 };
    uint8_t frame[DSEModVocoder::m_maxFrameBytes];
    DVYSF::FICH fich = makeFICH(FICommunication, m_frameNumber);
    fich.m_dt = (int) m_dataType;
    auto voiceDibits = [&](uint8_t *out, int nbBits) {
        pullVocoderFrame(frame);
        m_voiceFrameCount++;
        uint8_t bits[144];
        DSEModProtocol::bytesToBits(frame, nbBits, bits);
        uint8_t ysf[144];

        if (nbBits == 144) {
            DVYSF::imbeP25ToYSF(bits, ysf);
        } else {
            std::copy(bits, bits + nbBits, ysf);
        }

        for (int i = 0; i < nbBits / 2; i++) {
            out[i] = (uint8_t) ((ysf[2 * i] << 1) | ysf[2 * i + 1]);
        }
    };

    if (m_dataType == DTVoiceData1)
    {
        static const uint8_t zeros[20] = { 0 };
        const uint8_t *data = m_frameNumber == 0 ? m_csd1 : m_frameNumber == 1 ? m_csd2 : m_frameNumber == 2 ? m_csd3 : zeros;
        uint8_t dch[DVYSF::m_headerDCHDibits];
        DVYSF::dchEncode(data, 20, dch);

        for (int block = 0; block < m_voiceFramesPerFrame; block++)
        {
            std::copy(&dch[m_dchChunkDibits * block], &dch[m_dchChunkDibits * (block + 1)], &payload[2 * m_dchChunkDibits * block]);
            voiceDibits(&payload[2 * m_dchChunkDibits * block + m_dchChunkDibits], 72);
        }
    }
    else if (!m_subHeaderSent)
    {
        DVYSF::dchEncode(m_csd3, 20, payload);
        voiceDibits(&payload[216], 144);
        voiceDibits(&payload[288], 144);
        fich.m_fn = 0;
        fich.m_ft = 1;
        m_subHeaderSent = true;
    }
    else
    {
        for (int v = 0; v < m_voiceFramesPerFrame; v++) {
            voiceDibits(&payload[72 * v], 144);
        }
    }

    buildFrame(fich, payload, dibits);
    appendDibitLevels(levels, dibits, m_frameDibits);
    m_frameNumber = (m_frameNumber + 1) % m_framesPerSuperframe;

    if (m_stopRequested)
    {
        m_state = m_message.empty() ? StateTerminator : StateData;
        m_frameNumber = 0;
    }
}

bool DSEModYSF::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();
    uint8_t dibits[m_frameDibits];

    switch (m_state)
    {
    case StatePreamble:
        for (int i = 0; i < m_preambleDibits / 2; i++)
        {
            levels.push_back(3);
            levels.push_back(-3);
        }

        m_state = StateHeader;
        return true;

    case StateHeader:
    {
        DVYSF::FICH fich = makeFICH(FIHeader, 0);
        fich.m_dt = (int) m_dataType;
        buildFrame(fich, m_headerPayload, dibits);
        appendDibitLevels(levels, dibits, m_frameDibits);
        m_state = StateVoice;
        m_frameNumber = 0;
        return true;
    }

    case StateVoice:
    {
        if (m_dataType != DTVoiceData2)
        {
            nextVoiceFrame(levels);
            return true;
        }

        uint8_t frame[DSEModVocoder::m_maxFrameBytes];
        uint8_t ambe49[m_voiceFramesPerFrame][DVYSF::m_ambeBits];
        uint8_t payload[m_payloadDibits];

        for (int i = 0; i < m_voiceFramesPerFrame; i++)
        {
            pullVocoderFrame(frame);
            DVAMBE::bytesToParams(frame, ambe49[i]);
            m_voiceFrameCount++;
        }

        buildVD2Payload(dchDataForFrame(m_frameNumber), ambe49, payload);
        buildFrame(makeFICH(FICommunication, m_frameNumber), payload, dibits);
        appendDibitLevels(levels, dibits, m_frameDibits);
        m_frameNumber = (m_frameNumber + 1) % m_framesPerSuperframe;

        if (m_stopRequested)
        {
            m_state = m_message.empty() ? StateTerminator : StateData;
            m_frameNumber = 0;
        }

        return true;
    }

    case StateData:
    {
        // Data full rate (DT 1): two 20 byte data channels coded as the header's
        const int ft = dataFRFrames((int) m_message.size());
        uint8_t payload[m_payloadDibits];

        if (m_frameNumber == 0) {
            buildHeaderPayload(m_csd1, m_csd2, payload);
        } else if (m_frameNumber == 1) {
            buildHeaderPayload(m_csd3, m_message.data(), payload);
        } else {
            buildHeaderPayload(m_message.data() + 20 + 40 * (m_frameNumber - 2), m_message.data() + 40 + 40 * (m_frameNumber - 2), payload);
        }

        DVYSF::FICH fich = makeFICH(FICommunication, m_frameNumber);
        fich.m_dt = (int) DTDataFullRate;
        fich.m_ft = ft;
        buildFrame(fich, payload, dibits);
        appendDibitLevels(levels, dibits, m_frameDibits);

        if (++m_frameNumber > ft)
        {
            m_message.clear();
            m_state = StateTerminator;
        }

        return true;
    }

    case StateTerminator:
    {
        DVYSF::FICH fich = makeFICH(FITerminator, 0);
        fich.m_dt = (int) m_dataType;
        buildFrame(fich, m_headerPayload, dibits);
        appendDibitLevels(levels, dibits, m_frameDibits);
        m_state = StateIdle;
        m_stopRequested = false;
        return true;
    }

    case StateIdle:
    default:
        return false;
    }
}
