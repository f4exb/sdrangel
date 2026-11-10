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

#include <QDebug>

#include "dsp/dv/dvbits.h"
#include "dsp/dv/dvcrc.h"
#include "dsp/dv/dvnxdn.h"

#include "dsemodnxdn.h"

// Frame sync word 0xCDF59 (NXDN TS 1-A 4.5.1): symbols -3 +1 -3 +3 -3 -3 +3 +3 -1 +3, whose signs are
// dsdcc DSDSync::SyncNXDNRDCHFSW (3 1 3 1 3 3 1 1 3 1)
const uint8_t DSEModNXDN::m_fsw[DSEModNXDN::m_fswBits] = {
    1,1, 0,0, 1,1, 0,1, 1,1, 1,1, 0,1, 0,1, 1,0, 0,1
};

// Preamble: 4 repetitions of the 12 symbol pattern 0x57 0x75 0xFD. The last 9 symbols (- + - + + - - - +)
// followed by the FSW make dsdcc DSDSync::SyncNXDNRDCHFull (3 1 3 1 1 3 3 3 1 | 3 1 3 1 3 3 1 1 3 1)
const int8_t DSEModNXDN::m_preamble[DSEModNXDN::m_preambleSymbols] = {
    3, 3, 3, -3, 3, -3, 3, 3, -3, -3, -3, 3,
    3, 3, 3, -3, 3, -3, 3, 3, -3, -3, -3, 3,
    3, 3, 3, -3, 3, -3, 3, 3, -3, -3, -3, 3,
    3, 3, 3, -3, 3, -3, 3, 3, -3, -3, -3, 3
};

DSEModNXDN::DSEModNXDN(bool fast) :
    m_fast(fast),
    m_state(StateIdle),
    m_stopRequested(false),
    m_ran(1),
    m_superframeIndex(0),
    m_releaseCount(0),
    m_voiceFrameCount(0)
{
    std::fill(m_vcall, m_vcall + 9, 0);
    std::fill(m_txrel, m_txrel + 10, 0);
    std::fill(m_facch1TxRel, m_facch1TxRel + m_facch1Bits, 0);
}

DSEModNXDN::~DSEModNXDN()
{
}

void DSEModNXDN::buildMessage(int messageType, int callType, int sourceId, int destinationId, uint8_t *message)
{
    // Layout as parsed by dsdcc Message (nxdnmessage.cpp): type in the low 6 bits of byte 0,
    // CC option byte 1, call type in the top 3 bits of byte 2 (MSB set = individual) with the voice
    // call option in the low 5 bits (bit 0 = EFR full rate), source unit ID bytes 3-4, destination ID
    // bytes 5-6, cipher type (2 bits) and key ID (6 bits) byte 7, byte 8 unused
    message[0] = messageType & 0x3f;
    message[1] = 0x00; // CC option: not emergency, no priority, 4800 bit/s
    message[2] = (uint8_t) ((callType & 0x07) << 5); // voice call option 0: half duplex, 4800 bit/s EHR
    message[3] = (sourceId >> 8) & 0xff;
    message[4] = sourceId & 0xff;
    message[5] = (destinationId >> 8) & 0xff;
    message[6] = destinationId & 0xff;
    message[7] = 0x00; // no cipher, key ID 0
    message[8] = 0x00;
}

void DSEModNXDN::buildMessage(int messageType, const DSEModSettings& settings, uint8_t *message)
{
    buildMessage(messageType, settings.m_nxdnGroupCall ? CallTypeGroup : CallTypeIndividual,
        settings.m_nxdnSourceId & 0xffff, settings.m_nxdnDestinationId & 0xffff, message);
}

void DSEModNXDN::lichEncode(int rfChannelType, int functionalType, int option, int direction, uint8_t *bits)
{
    // 8 information bits (dsdcc DSDNXDN::processLICH): RF channel type (2), functional channel type (2),
    // option (2), direction (1), parity (1). dsdcc checks even parity over the first 6 bits plus the parity
    // bit (the direction bit is not included). Each information bit is sent in the MSB of one symbol
    // (dsdcc acquireLICH: bit = dibit >> 1). The LSB is set so that only +3/-3 symbols are used.
    uint8_t info[m_lichInfoBits];
    info[0] = (rfChannelType >> 1) & 1;
    info[1] = rfChannelType & 1;
    info[2] = (functionalType >> 1) & 1;
    info[3] = functionalType & 1;
    info[4] = (option >> 1) & 1;
    info[5] = option & 1;
    info[6] = direction & 1;
    info[7] = info[0] ^ info[1] ^ info[2] ^ info[3] ^ info[4] ^ info[5];

    for (int i = 0; i < m_lichInfoBits; i++)
    {
        bits[2 * i] = info[i];
        bits[2 * i + 1] = 1;
    }
}

void DSEModNXDN::messageFragment(const uint8_t *message, int index, uint8_t *fragment)
{
    // dsdcc Message::setFromSACCH concatenates the 4 x 18 bit fragments into the 72 bit message
    for (int i = 0; i < m_sacchFragmentBits; i++)
    {
        int bit = index * m_sacchFragmentBits + i;
        fragment[i] = (message[bit / 8] >> (7 - (bit % 8))) & 1;
    }
}

void DSEModNXDN::sacchEncode(int structure, int ran, const uint8_t *fragment, uint8_t *bits)
{
    // dsdcc DSDNXDN::SACCH: byte 0 = structure (countdown, 2 bits) | RAN (6 bits), then 18 message bits
    uint8_t info[DVNXDN::m_sacchInfoBits];
    info[0] = (structure >> 1) & 1;
    info[1] = structure & 1;

    for (int i = 0; i < 6; i++) {
        info[2 + i] = (ran >> (5 - i)) & 1;
    }

    std::copy(fragment, fragment + m_sacchFragmentBits, info + 8);
    DVNXDN::sacchEncode(info, bits);
}

void DSEModNXDN::facch1Encode(const uint8_t *message, uint8_t *bits)
{
    uint8_t info[DVNXDN::m_facch1InfoBits];
    bytesToBits(message, DVNXDN::m_facch1InfoBits, info);
    DVNXDN::facch1Encode(info, bits);
}

void DSEModNXDN::vocoderFrameToBits(const uint8_t *frame, uint8_t *bits)
{
    // dsdcc DSDNXDN::processVoiceFrameEHR uses the DMR rW/rX/rY/rZ interleave: the VCH on-air order is
    // the DMR voice frame order so the DVSI (DMR on-air) frame bytes are sent as they are, MSB first
    bytesToBits(frame, m_vchBits, bits);
}

void DSEModNXDN::scramble(uint8_t *bits)
{
    // The MSB of symbol i (counted from the start of the LICH) is inverted when PN(9,5) bit i is set
    DVNXDN::scramble(bits);
}

void DSEModNXDN::buildFrame(const uint8_t *lich, const uint8_t *sacch, const uint8_t *payload, uint8_t *bits)
{
    std::copy(m_fsw, m_fsw + m_fswBits, bits);
    std::copy(lich, lich + m_lichBits, bits + m_fswBits);
    std::copy(sacch, sacch + m_sacchBits, bits + m_fswBits + m_lichBits);
    std::copy(payload, payload + m_payloadBits, bits + m_fswBits + m_lichBits + m_sacchBits);
    scramble(bits + m_fswBits);
}

void DSEModNXDN::start(const DSEModSettings& settings)
{
    m_ran = settings.m_nxdnRan & 0x3f;
    buildMessage(MessageTypeVCALL, settings, m_vcall);
    buildMessage(MessageTypeTXREL, settings, m_txrel);
    m_txrel[9] = 0;
    facch1Encode(m_txrel, m_facch1TxRel);

    if (m_fast) {
        m_vcall[2] |= 0x02; // the call option a 9600 bit/s EHR radio sends (bit 0, EFR, clear)
    }

    m_vcall[9] = 0;
    facch1Encode(m_vcall, m_facch1Vcall);

    m_stopRequested = false;
    m_superframeIndex = 0;
    m_releaseCount = 0;
    m_voiceFrameCount = 0;
    m_message.clear();
    m_userData.clear();
    m_dataBlocks = 0;
    m_dataFrame = 0;
    m_dataPadding = 0;
    m_settingsGroup = settings.m_nxdnGroupCall;
    m_settingsSource = settings.m_nxdnSourceId & 0xffff;
    m_settingsDestination = settings.m_nxdnDestinationId & 0xffff;
    m_state = StatePreamble;

    qDebug("DSEModNXDN::start: RAN: %d %s %d > %d", m_ran,
        settings.m_nxdnGroupCall ? "group" : "individual", settings.m_nxdnSourceId, settings.m_nxdnDestinationId);
}

void DSEModNXDN::stop()
{
    m_stopRequested = true;
}

bool DSEModNXDN::queueMessage(const std::string& text)
{
    // Up to 16 blocks of 20 bytes, four of them the CRC-32: 316 characters
    if (text.empty() || (m_state == StateIdle)) {
        return false;
    }

    m_message = text.substr(0, 316);
    return true;
}

void DSEModNXDN::abort()
{
    m_state = StateIdle;
    m_stopRequested = false;
}

void DSEModNXDN::buildVoiceFrame(uint8_t *bits)
{
    uint8_t lich[m_lichBits];
    uint8_t sacch[m_sacchBits];
    uint8_t payload[m_payloadBits];
    uint8_t fragment[m_sacchFragmentBits];
    uint8_t frame[DSEModVocoder::m_maxFrameBytes];

    lichEncode(RFChannelRDCH, FunctionalSACCHSuperframe, OptionStealNone, 0, lich);
    messageFragment(m_vcall, m_superframeIndex, fragment);
    sacchEncode(3 - m_superframeIndex, m_ran, fragment, sacch);

    for (int i = 0; i < 4; i++)
    {
        pullVocoderFrame(frame);
        vocoderFrameToBits(frame, payload + i * m_vchBits);
        m_voiceFrameCount++;
    }

    buildFrame(lich, sacch, payload, bits);
}

void DSEModNXDN::buildReleaseFrame(uint8_t *bits)
{
    uint8_t lich[m_lichBits];
    uint8_t sacch[m_sacchBits];
    uint8_t payload[m_payloadBits];
    uint8_t fragment[m_sacchFragmentBits];

    lichEncode(RFChannelRDCH, FunctionalSACCHSuperframe, OptionStealBoth, 0, lich);
    messageFragment(m_txrel, m_superframeIndex, fragment);
    sacchEncode(3 - m_superframeIndex, m_ran, fragment, sacch);
    std::copy(m_facch1TxRel, m_facch1TxRel + m_facch1Bits, payload);
    std::copy(m_facch1TxRel, m_facch1TxRel + m_facch1Bits, payload + m_facch1Bits);
    buildFrame(lich, sacch, payload, bits);
}

void DSEModNXDN::buildFrameUDCH(const uint8_t *lich, const uint8_t *udch, uint8_t *bits)
{
    // A user data channel frame: the LICH then the 348 UDCH bits over the SACCH and payload area
    std::copy(m_fsw, m_fsw + m_fswBits, bits);
    std::copy(lich, lich + m_lichBits, bits + m_fswBits);
    std::copy(udch, udch + DVNXDN::m_udchTxBits, bits + m_fswBits + m_lichBits);
    scramble(bits + m_fswBits);
}

void DSEModNXDN::buildDataHeaderFrame(uint8_t *bits)
{
    // SDCALL_REQ_HEADER in two FACCH1 (TS 1-A): the message's call type, source and destination,
    // no cipher, then the packet information: the number of the first block (they count down
    // to 0) and the padding of the last one
    uint8_t message[10];
    buildMessage(MessageTypeSDCALLHeader, m_settingsGroup ? CallTypeGroup : CallTypeIndividual, m_settingsSource, m_settingsDestination, message);
    uint8_t info[80];
    bytesToBits(message, 64, info);
    std::fill(info + 64, info + 80, 0);
    DVBits::putBits(info, 68, m_dataBlocks - 1, 4);
    DVBits::putBits(info, 72, m_dataPadding, 5);

    uint8_t facch[m_facch1Bits];
    DVNXDN::facch1Encode(info, facch);
    uint8_t lich[m_lichBits];
    uint8_t sacch[m_sacchBits];
    uint8_t payload[m_payloadBits];
    uint8_t fragment[m_sacchFragmentBits];
    lichEncode(RFChannelRDCH, FunctionalSACCHSuperframe, OptionStealBoth, 0, lich);
    messageFragment(m_txrel, m_superframeIndex, fragment);
    sacchEncode(3 - m_superframeIndex, m_ran, fragment, sacch);
    std::copy(facch, facch + m_facch1Bits, payload);
    std::copy(facch, facch + m_facch1Bits, payload + m_facch1Bits);
    buildFrame(lich, sacch, payload, bits);
}

void DSEModNXDN::buildDataFrame(int block, uint8_t *bits)
{
    // SDCALL_REQ_USERDATA in a UDCH frame: the block counters (blocks left and this block's number,
    // both counting down to 0) then 20 bytes of user data
    int number = m_dataBlocks - 1 - block;
    uint8_t message[22] = { 0 };
    message[0] = MessageTypeSDCALLData;
    message[1] = (uint8_t) ((number << 4) | number);
    std::copy(m_userData.begin() + block * 20, m_userData.begin() + (block + 1) * 20, message + 2);

    uint8_t info[DVNXDN::m_udchInfoBits];
    info[0] = 0; // structure
    info[1] = 0;

    for (int i = 0; i < 6; i++) {
        info[2 + i] = (m_ran >> (5 - i)) & 1;
    }

    bytesToBits(message, 176, info + 8);
    uint8_t udch[DVNXDN::m_udchTxBits];
    DVNXDN::udchEncode(info, udch);
    uint8_t lich[m_lichBits];
    lichEncode(RFChannelRDCH, FunctionalUDCH, OptionStealNone, 0, lich);
    buildFrameUDCH(lich, udch, bits);
}

void DSEModNXDN::buildVcallFrame(uint8_t *bits)
{
    uint8_t lich[m_lichBits];
    uint8_t sacch[m_sacchBits];
    uint8_t payload[m_payloadBits];
    uint8_t fragment[m_sacchFragmentBits];

    lichEncode(RFChannelRDCH, FunctionalSACCHSuperframe, OptionStealBoth, 0, lich);
    messageFragment(m_vcall, m_superframeIndex, fragment);
    sacchEncode(3 - m_superframeIndex, m_ran, fragment, sacch);
    std::copy(m_facch1Vcall, m_facch1Vcall + m_facch1Bits, payload);
    std::copy(m_facch1Vcall, m_facch1Vcall + m_facch1Bits, payload + m_facch1Bits);
    buildFrame(lich, sacch, payload, bits);
}

bool DSEModNXDN::nextFrame(std::vector<int8_t>& levels)
{
    levels.clear();

    if (m_state == StateIdle) {
        return false;
    }

    if (m_state == StatePreamble)
    {
        levels.assign(m_preamble, m_preamble + m_preambleSymbols);
        m_state = StateVoice;
        m_superframeIndex = 0;
        return true;
    }

    uint8_t bits[m_frameBits];

    if (m_state == StateVoice)
    {
        // NXDN96: a VCALL frame then a voice frame, so that voice keeps real time
        if (m_fast && ((m_superframeIndex & 1) == 0)) {
            buildVcallFrame(bits);
        } else {
            buildVoiceFrame(bits);
        }

        m_superframeIndex++;

        if (m_superframeIndex == m_framesPerSuperframe)
        {
            m_superframeIndex = 0;

            if (m_stopRequested)
            {
                if (!m_message.empty())
                {
                    // The user data: the text, its CRC-32 (the register all ones over the bits,
                    // MSB first in the last four bytes) and the padding of the last block
                    m_userData.assign(m_message.begin(), m_message.end());
                    std::vector<uint8_t> dataBits(m_userData.size() * 8);
                    DVBits::bytesToBitsMSB(m_userData.data(), (int) m_userData.size(), dataBits.data());
                    uint32_t crc = DVCRC::nxdn32(dataBits.data(), (int) dataBits.size());
                    m_userData.push_back((uint8_t) (crc >> 24));
                    m_userData.push_back((uint8_t) (crc >> 16));
                    m_userData.push_back((uint8_t) (crc >> 8));
                    m_userData.push_back((uint8_t) crc);
                    m_dataBlocks = ((int) m_userData.size() + 19) / 20;
                    m_dataPadding = m_dataBlocks * 20 - (int) m_userData.size();
                    m_userData.resize(m_dataBlocks * 20, 0);
                    m_dataFrame = 0;
                    m_message.clear();
                    m_state = StateData;
                }
                else
                {
                    m_state = StateRelease;
                    m_releaseCount = 0;
                }
            }
        }
    }
    else if (m_state == StateData)
    {
        if (m_dataFrame == 0) {
            buildDataHeaderFrame(bits);
        } else {
            buildDataFrame(m_dataFrame - 1, bits);
        }

        m_superframeIndex = (m_superframeIndex + 1) % m_framesPerSuperframe;

        if (++m_dataFrame > m_dataBlocks)
        {
            m_state = StateRelease;
            m_releaseCount = 0;
            m_superframeIndex = 0;  // the release's TX_REL a whole SACCH superframe from its first fragment
        }
    }
    else // StateRelease
    {
        buildReleaseFrame(bits);
        m_superframeIndex = (m_superframeIndex + 1) % m_framesPerSuperframe;

        if (++m_releaseCount >= m_releaseFrames)
        {
            m_state = StateIdle;
            m_stopRequested = false;
        }
    }

    appendDibits(levels, bits, m_frameBits);
    return true;
}
