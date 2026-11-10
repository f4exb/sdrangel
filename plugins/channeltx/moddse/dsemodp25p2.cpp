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

#include <cstring>

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvp25p2.h"

#include "dsemodp25p2.h"

DSEModP25P2::DSEModP25P2() :
    m_state(StateIdle),
    m_stopRequested(false),
    m_timeslot(0),
    m_superframe(0),
    m_voiceBurst(0),
    m_idleBursts(0),
    m_wacn(0),
    m_sysid(0),
    m_nac(0),
    m_voiceFrameCount(0)
{
    std::memset(m_mi, 0, sizeof(m_mi));
    std::memset(m_essB, 0, sizeof(m_essB));
    std::memset(m_essA, 0, sizeof(m_essA));
}

DSEModP25P2::~DSEModP25P2()
{
}

// ---- Static builders ----------------------------------------------------------------------

void DSEModP25P2::buildMacPTT(const DSEModSettings& settings, const uint8_t *mi9, uint8_t *pdu)
{
    // Opcode 1 in bits 7-5, the offset to the first 4V burst in bits 4-2: the voice starts at
    // the next burst of the channel
    std::memset(pdu, 0, 18);
    pdu[0] = (DVP25P2::MacPTT << 5) | (0 << 2);
    std::memcpy(pdu + 1, mi9, 9);
    pdu[10] = 0x80; // clear
    pdu[11] = 0x00; // key ID
    pdu[12] = 0x00;
    pdu[13] = (settings.m_p25SourceId >> 16) & 0xFF;
    pdu[14] = (settings.m_p25SourceId >> 8) & 0xFF;
    pdu[15] = settings.m_p25SourceId & 0xFF;
    pdu[16] = (settings.m_p25TalkGroup >> 8) & 0xFF;
    pdu[17] = settings.m_p25TalkGroup & 0xFF;
}

void DSEModP25P2::buildMacEndPTT(const DSEModSettings& settings, uint8_t *pdu)
{
    std::memset(pdu, 0, 18);
    pdu[0] = DVP25P2::MacEndPTT << 5;
    pdu[1] = (settings.m_p25Nac >> 8) & 0x0F; // colour code
    pdu[2] = settings.m_p25Nac & 0xFF;
    pdu[13] = (settings.m_p25SourceId >> 16) & 0xFF;
    pdu[14] = (settings.m_p25SourceId >> 8) & 0xFF;
    pdu[15] = settings.m_p25SourceId & 0xFF;
    pdu[16] = (settings.m_p25TalkGroup >> 8) & 0xFF;
    pdu[17] = settings.m_p25TalkGroup & 0xFF;
}

void DSEModP25P2::buildMacActive(uint8_t *pdu)
{
    // Offset 4: the 2V burst is four bursts after the SACCH... the field is informational here
    std::memset(pdu, 0, 21);
    pdu[0] = (DVP25P2::MacActive << 5) | (4 << 2);
}

void DSEModP25P2::buildMacIdle(uint8_t *pdu)
{
    std::memset(pdu, 0, 21);
    pdu[0] = DVP25P2::MacIdle << 5;
}

void DSEModP25P2::buildMacIdleNetworkStatus(uint32_t wacn, uint16_t sysid, uint16_t nac, uint8_t *pdu)
{
    buildMacIdle(pdu);
    uint8_t *m = pdu + 1;
    m[0] = 0x7B;                                    // Network Status Broadcast, abbreviated (11 octets)
    m[1] = 0;                                       // LRA
    m[2] = (uint8_t) (wacn >> 12);                  // WACN (20), system ID (12)
    m[3] = (uint8_t) (wacn >> 4);
    m[4] = (uint8_t) (((wacn & 0xF) << 4) | ((sysid >> 8) & 0xF));
    m[5] = (uint8_t) sysid;
    m[6] = 0;                                       // channel
    m[7] = 0;
    m[8] = 0;                                       // system service class
    m[9] = (uint8_t) ((nac >> 8) & 0xF);            // colour code (12)
    m[10] = (uint8_t) nac;
}

void DSEModP25P2::buildACCHBurst(bool fast, bool scrambled, const uint8_t *pdu, uint8_t *dibits)
{
    std::memset(dibits, 0, 160);
    int duid = fast ? (scrambled ? DVP25P2::DUIDFACCHScrambled : DVP25P2::DUIDFACCH)
                    : (scrambled ? DVP25P2::DUIDSACCHScrambled : DVP25P2::DUIDSACCH);
    uint8_t duidBits = DVP25P2::duidEncode(duid);

    for (int i = 0; i < 4; i++) {
        dibits[DVP25P2::m_duidDibits[i] - 20] = (duidBits >> (6 - 2 * i)) & 3;
    }

    uint8_t pduBits[168], coded[312];
    DVBits::bytesToBitsMSB(pdu, fast ? 18 : 21, pduBits);

    if (fast) {
        DVP25P2::facchEncode(pduBits, coded);
    } else {
        DVP25P2::sacchEncode(pduBits, coded);
    }

    int nbDibits = (fast ? DVP25P2::m_facchBits : DVP25P2::m_sacchBits) / 2;

    for (int k = 0; k < nbDibits; k++)
    {
        int d = fast ? DVP25P2::facchDibit(k) : DVP25P2::sacchDibit(k);
        dibits[d - 20] = (coded[2 * k] << 1) | coded[2 * k + 1];
    }
}

void DSEModP25P2::buildVoiceBurst(bool twoV, const uint8_t *frames, const uint8_t *essB, const uint8_t *essA, int essBIndex, uint8_t *dibits)
{
    std::memset(dibits, 0, 160);
    uint8_t duidBits = DVP25P2::duidEncode(twoV ? DVP25P2::DUID2V : DVP25P2::DUID4V);

    for (int i = 0; i < 4; i++) {
        dibits[DVP25P2::m_duidDibits[i] - 20] = (duidBits >> (6 - 2 * i)) & 3;
    }

    for (int c = 0; c < (twoV ? 2 : 4); c++)
    {
        uint8_t bits[72];
        DVBits::bytesToBitsMSB(frames + 9 * c, 9, bits);

        for (int k = 0; k < 36; k++) {
            dibits[DVP25P2::m_voiceDibits[c] - 20 + k] = (bits[2 * k] << 1) | bits[2 * k + 1];
        }
    }

    if (!twoV)
    {
        // ESS-B: four hexbits, three dibits each
        for (int i = 0; i < 4; i++)
        {
            uint8_t h = essB[4 * essBIndex + i];
            int d = DVP25P2::m_essBDibit - 20 + 3 * i;
            dibits[d] = (h >> 4) & 3;
            dibits[d + 1] = (h >> 2) & 3;
            dibits[d + 2] = h & 3;
        }
    }
    else
    {
        // ESS-A: 28 hexbits from dibit 94, the DUID dibit 142 skipped after hexbit 15
        int j = 0;

        for (int i = 0; i < 28; i++)
        {
            uint8_t h = essA[i];
            int d = DVP25P2::m_essBDibit - 20 + j;
            dibits[d] = (h >> 4) & 3;
            dibits[d + 1] = (h >> 2) & 3;
            dibits[d + 2] = h & 3;
            j += (i == 15) ? 4 : 3;
        }
    }
}

void DSEModP25P2::buildISCH(int timeslot, int superframe, uint8_t *dibits)
{
    uint64_t codeword;

    if (DVP25P2::timeslotHasSync(timeslot))
    {
        codeword = DVP25P2::m_syncISCH;
    }
    else
    {
        DVP25P2::ISCH isch;
        isch.m_channel = timeslot % 4;
        isch.m_location = timeslot / 4;
        isch.m_frame = (superframe & 4) ? 1 : 0;   // an ultraframe is four superframes; the last flags the end
        isch.m_count = superframe & 3;
        codeword = DVP25P2::ischEncode(isch);
    }

    for (int i = 0; i < 20; i++) {
        dibits[i] = (codeword >> (38 - 2 * i)) & 3;
    }
}

// ---- Transmission ---------------------------------------------------------------------------

void DSEModP25P2::start(const DSEModSettings& settings)
{
    m_settings = settings;
    m_wacn = settings.m_p25p2Wacn & 0xFFFFF;
    m_sysid = settings.m_p25p2Sysid & 0xFFF;
    m_nac = settings.m_p25Nac & 0xFFF;
    m_mask.resize(2 * DVP25P2::m_superframeDibits);
    DVP25P2::scramblerMask(m_wacn, m_sysid, m_nac, m_mask.data());
    std::memset(m_mi, 0, sizeof(m_mi));
    DVP25P2::essEncode(0x80, 0, m_mi, m_essB, m_essA);
    m_timeslot = 0;
    m_superframe = 0;
    m_voiceBurst = 0;
    m_idleBursts = 0;
    m_voiceFrameCount = 0;
    m_stopRequested = false;
    m_state = StatePTT;
}

void DSEModP25P2::stop()
{
    if (m_state != StateIdle) {
        m_stopRequested = true;
    }
}

void DSEModP25P2::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

void DSEModP25P2::scramble(int timeslot, uint8_t *dibits) const
{
    for (int d = 20; d < 180; d++)
    {
        bool isDuid = false;

        for (int i = 0; i < 4; i++) {
            isDuid = isDuid || (d == DVP25P2::m_duidDibits[i]);
        }

        if (isDuid) {
            continue;
        }

        int m = 2 * DVP25P2::maskDibit(timeslot, d);
        dibits[d - 20] ^= (m_mask[m] << 1) | m_mask[m + 1];
    }
}

void DSEModP25P2::appendBurst(std::vector<int8_t>& levels, int timeslot, const uint8_t *dibits) const
{
    uint8_t isch[20];
    buildISCH(timeslot, m_superframe, isch);

    for (int i = 0; i < 20; i++) {
        levels.push_back(dibitToLevel(isch[i]));
    }

    for (int i = 0; i < 160; i++) {
        levels.push_back(dibitToLevel(dibits[i]));
    }
}

bool DSEModP25P2::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    if (m_state == StateIdle) {
        return false;
    }

    int channel = DVP25P2::timeslotChannel(m_timeslot);
    uint8_t dibits[160];
    uint8_t pdu[21];

    if (channel == 1)
    {
        // The other channel: idle, unscrambled, with the network's identity
        buildMacIdleNetworkStatus(m_wacn, m_sysid, m_nac, pdu);
        buildACCHBurst(false, false, pdu, dibits);
    }
    else
    {
        switch (m_state)
        {
        case StatePTT:
            buildMacPTT(m_settings, m_mi, pdu);
            buildACCHBurst(true, true, pdu, dibits);
            scramble(m_timeslot, dibits);
            m_state = StateVoice;
            m_voiceBurst = 0;
            break;

        case StateVoice:
            if (m_voiceBurst < 5)
            {
                bool twoV = m_voiceBurst == 4;
                uint8_t frames[4 * 9];

                for (int c = 0; c < (twoV ? 2 : 4); c++)
                {
                    // Frame sources may fill up to the largest vocoder frame
                    uint8_t frame[DSEModVocoder::m_maxFrameBytes];

                    if (pullVocoderFrame(frame)) {
                        m_voiceFrameCount++;
                    }

                    std::memcpy(frames + 9 * c, frame, 9);
                }

                buildVoiceBurst(twoV, frames, m_essB, m_essA, m_voiceBurst, dibits);
            }
            else
            {
                buildMacActive(pdu);
                buildACCHBurst(false, true, pdu, dibits);
            }

            scramble(m_timeslot, dibits);

            if (++m_voiceBurst >= 6)
            {
                m_voiceBurst = 0;

                if (m_stopRequested) {
                    m_state = StateEndPTT;
                }
            }
            break;

        case StateEndPTT:
            buildMacEndPTT(m_settings, pdu);
            buildACCHBurst(true, true, pdu, dibits);
            scramble(m_timeslot, dibits);
            m_state = StateIdleBursts;
            m_idleBursts = 0;
            break;

        case StateIdleBursts:
            buildMacIdle(pdu);
            buildACCHBurst(false, false, pdu, dibits);

            if (++m_idleBursts >= 2)
            {
                m_state = StateIdle;
                m_stopRequested = false;
            }
            break;

        default:
            return false;
        }
    }

    appendBurst(levels, m_timeslot, dibits);

    if (++m_timeslot >= 12)
    {
        m_timeslot = 0;
        m_superframe++;
    }

    return true;
}
