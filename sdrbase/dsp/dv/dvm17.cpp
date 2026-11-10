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

#include "dvm17.h"
#include "dvbits.h"
#include "dvblockcode.h"
#include "dvconvolutional.h"

const double DVM17::RrcAlpha = 0.5;

// The link setup frame is punctured 61 bits at a time, a stream frame 12 and a packet frame 8
const int8_t DVM17::m_puncture1[61] = {
    1,
    1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,
    1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,
    1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1,  1, 0, 1, 1
};

const int8_t DVM17::m_puncture2[12] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0 };

const int8_t DVM17::m_puncture3[8] = { 1, 1, 1, 1, 1, 1, 1, 0 };

//! The randomiser: 46 bytes exclusive ored over the 368 payload bits
static const uint8_t m_randomiser[46] = {
    0xd6, 0xb5, 0xe2, 0x30, 0x82, 0xff, 0x84, 0x62,
    0xba, 0x4e, 0x96, 0x90, 0xd8, 0x98, 0xdd, 0x5d,
    0x0c, 0xc8, 0x52, 0x43, 0x91, 0x1d, 0xf8, 0x6e,
    0x68, 0x2f, 0x35, 0xda, 0x14, 0xea, 0xcd, 0x76,
    0x19, 0x8d, 0xd5, 0x80, 0xd1, 0x33, 0x87, 0x13,
    0x57, 0x18, 0x2d, 0x29, 0x78, 0xc3
};

int8_t DVM17::dibitToLevel(int dibit)
{
    switch (dibit & 3)
    {
    case 1: return 3;
    case 0: return 1;
    case 2: return -1;
    default: return -3;
    }
}

int DVM17::levelToDibit(int8_t level)
{
    if (level >= 2) {
        return 1;
    } else if (level >= 0) {
        return 0;
    } else if (level >= -2) {
        return 2;
    }

    return 3;
}

void DVM17::syncLevels(int syncWord, int8_t *levels)
{
    for (int i = 0; i < SyncSymbols; i++) {
        levels[i] = dibitToLevel((syncWord >> (14 - 2 * i)) & 3);
    }
}

uint64_t DVM17::encodeCallsign(const std::string& callsign)
{
    // Base-40 over up to 9 characters, most significant first, as M17 defines it
    static const char *alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";
    std::string text;

    for (char c : callsign)
    {
        char u = (char) ((c >= 'a') && (c <= 'z') ? c - 'a' + 'A' : c);

        if (std::strchr(alphabet, u) && (u != 0)) {
            text += u;
        } else {
            return 0;
        }
    }

    if (text.empty() || (text == "ALL") || (text == "CQ") || (text == "BROADCAST")) {
        return CallsignBroadcast;
    }

    if (text.size() > 9) {
        return 0;
    }

    uint64_t code = 0;

    for (int i = (int) text.size() - 1; i >= 0; i--)
    {
        const char *at = std::strchr(alphabet, text[i]);
        code = code * 40 + (uint64_t) (at - alphabet);
    }

    return code;
}

std::string DVM17::decodeCallsign(uint64_t code)
{
    static const char *alphabet = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-/.";

    if (code == CallsignBroadcast) {
        return "ALL";
    }

    if ((code == 0) || (code >= 0xEE6B28000000ull)) {  // 40^9: not a callsign
        return std::string();
    }

    std::string text;

    while (code > 0)
    {
        text += alphabet[code % 40];
        code /= 40;
    }

    return text;
}

uint16_t DVM17::crc16(const uint8_t *bytes, int nbBytes)
{
    uint16_t crc = 0xFFFF;

    for (int i = 0; i < nbBytes; i++)
    {
        crc ^= (uint16_t) (bytes[i] << 8);

        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t) ((crc << 1) ^ 0x5935) : (uint16_t) (crc << 1);
        }
    }

    return crc;
}

void DVM17::packLSF(const LSF& lsf, uint8_t *bytes)
{
    for (int i = 0; i < 6; i++)
    {
        bytes[i] = (uint8_t) (lsf.m_destination >> (40 - 8 * i));
        bytes[6 + i] = (uint8_t) (lsf.m_source >> (40 - 8 * i));
    }

    bytes[12] = (uint8_t) (lsf.m_type >> 8);
    bytes[13] = (uint8_t) lsf.m_type;
    std::memcpy(bytes + 14, lsf.m_meta, 14);
    uint16_t crc = crc16(bytes, 28);
    bytes[28] = (uint8_t) (crc >> 8);
    bytes[29] = (uint8_t) crc;
}

bool DVM17::unpackLSF(const uint8_t *bytes, LSF& lsf)
{
    if (crc16(bytes, LSFBytes) != 0) {
        return false;   // the CRC is over the whole frame, its own field included
    }

    lsf.m_destination = 0;
    lsf.m_source = 0;

    for (int i = 0; i < 6; i++)
    {
        lsf.m_destination = (lsf.m_destination << 8) | bytes[i];
        lsf.m_source = (lsf.m_source << 8) | bytes[6 + i];
    }

    lsf.m_type = (uint16_t) ((bytes[12] << 8) | bytes[13]);
    std::memcpy(lsf.m_meta, bytes + 14, 14);
    return true;
}

int DVM17::encodePunctured(const uint8_t *bits, int nbBits, const int8_t *pattern, int patternLength, uint8_t *out)
{
    std::vector<uint8_t> in(bits, bits + nbBits);
    in.resize(nbBits + FlushBits, 0);
    std::vector<uint8_t> coded(2 * in.size());
    DVConvolutional::k5().encode(in.data(), (int) in.size(), coded.data());
    int kept = 0;

    for (int i = 0; i < (int) coded.size(); i++)
    {
        if (pattern[i % patternLength]) {
            out[kept++] = coded[i];
        }
    }

    return kept;
}

bool DVM17::decodePunctured(const float *llr, int nbCoded, int nbBits, const int8_t *pattern, int patternLength,
    uint8_t *bits, int *cost)
{
    // The bits the transmitter dropped are unknown to the decoder: a zero LLR
    std::vector<float> full(2 * (nbBits + FlushBits), 0.0f);
    int taken = 0;

    for (int i = 0; (i < (int) full.size()) && (taken < nbCoded); i++)
    {
        if (pattern[i % patternLength]) {
            full[i] = llr[taken++];
        }
    }

    std::vector<uint8_t> decoded(nbBits + FlushBits, 0);
    int errors = DVConvolutional::k5().decodeSoft(full.data(), (int) decoded.size(), decoded.data(), true);

    if (cost) {
        *cost = errors;
    }

    std::copy(decoded.begin(), decoded.begin() + nbBits, bits);
    return taken == nbCoded;
}

void DVM17::interleave(const uint8_t *in, uint8_t *out)
{
    for (int i = 0; i < PayloadBits; i++) {
        out[(45 * i + 92 * i * i) % PayloadBits] = in[i];
    }
}

void DVM17::deinterleave(const float *in, float *out)
{
    for (int i = 0; i < PayloadBits; i++) {
        out[i] = in[(45 * i + 92 * i * i) % PayloadBits];
    }
}

void DVM17::randomise(uint8_t *bits, int nbBits)
{
    for (int i = 0; i < nbBits; i++)
    {
        if ((m_randomiser[(i / 8) % 46] >> (7 - (i % 8))) & 1) {
            bits[i] ^= 1;
        }
    }
}

void DVM17::randomiseLLR(float *llr, int nbBits)
{
    for (int i = 0; i < nbBits; i++)
    {
        if ((m_randomiser[(i / 8) % 46] >> (7 - (i % 8))) & 1) {
            llr[i] = -llr[i];
        }
    }
}

void DVM17::packLICH(const uint8_t *lsfBytes, int segment, uint8_t *bits)
{
    // Five bytes of the link setup frame and the segment number, in four Golay(24,12) codewords
    uint8_t content[6];
    std::memcpy(content, lsfBytes + 5 * segment, 5);
    content[5] = (uint8_t) ((segment & 7) << 5);
    uint8_t contentBits[48];
    DVBits::bytesToBitsMSB(content, 6, contentBits);

    for (int w = 0; w < 4; w++)
    {
        uint32_t data = 0;

        for (int b = 0; b < 12; b++) {
            data = (data << 1) | contentBits[12 * w + b];
        }

        uint32_t codeword = DVBlockCode::golay2412().encodeWord(data);

        for (int b = 0; b < 24; b++) {
            bits[24 * w + b] = (uint8_t) ((codeword >> (23 - b)) & 1);
        }
    }
}

int DVM17::unpackLICH(const float *llr, uint8_t *lsfChunk)
{
    uint8_t contentBits[48];

    for (int w = 0; w < 4; w++)
    {
        // The Golay codewords are decoded from the soft bits, as everything else here is
        if (DVBlockCode::golay2412().decodeSoft(llr + 24 * w, contentBits + 12 * w) < 0) {
            return -1;
        }
    }

    uint8_t content[6];
    DVBits::bitsToBytesMSB(contentBits, 48, content);
    std::memcpy(lsfChunk, content, 5);
    return (content[5] >> 5) & 7;
}

//! The 368 payload bits to 184 symbols after the sync word's 8
static void payloadLevels(const uint8_t *payload, int syncWord, int8_t *levels)
{
    DVM17::syncLevels(syncWord, levels);

    for (int i = 0; i < DVM17::PayloadBits / 2; i++)
    {
        int dibit = (payload[2 * i] << 1) | payload[2 * i + 1];
        levels[DVM17::SyncSymbols + i] = DVM17::dibitToLevel(dibit);
    }
}

//! Interleave, randomise and map a frame's payload bits
static void finishFrame(uint8_t *payload, int syncWord, int8_t *levels)
{
    uint8_t interleaved[DVM17::PayloadBits];
    DVM17::interleave(payload, interleaved);
    DVM17::randomise(interleaved, DVM17::PayloadBits);
    payloadLevels(interleaved, syncWord, levels);
}

void DVM17::buildLSFFrame(const LSF& lsf, int8_t *levels)
{
    uint8_t bytes[LSFBytes];
    packLSF(lsf, bytes);
    uint8_t bits[LSFBits];
    DVBits::bytesToBitsMSB(bytes, LSFBytes, bits);
    uint8_t payload[PayloadBits];
    encodePunctured(bits, LSFBits, m_puncture1, 61, payload);
    finishFrame(payload, SyncLSF, levels);
}

void DVM17::buildStreamFrame(const uint8_t *lsfBytes, int segment, uint16_t frameNumber, const uint8_t *payloadBytes, int8_t *levels)
{
    uint8_t payload[PayloadBits];
    packLICH(lsfBytes, segment, payload);
    uint8_t data[StreamDataBits];

    for (int i = 0; i < 16; i++) {
        data[i] = (uint8_t) ((frameNumber >> (15 - i)) & 1);
    }

    DVBits::bytesToBitsMSB(payloadBytes, 16, data + 16);
    encodePunctured(data, StreamDataBits, m_puncture2, 12, payload + LICHBits);
    finishFrame(payload, SyncStream, levels);
}

void DVM17::buildPacketFrame(const uint8_t *payloadBytes, int nbBytes, bool last, int counter, int8_t *levels)
{
    uint8_t bytes[PacketBytes] = { 0 };
    std::memcpy(bytes, payloadBytes, std::min(nbBytes, (int) PacketBytes));
    uint8_t data[PacketDataBits];
    DVBits::bytesToBitsMSB(bytes, PacketBytes, data);
    data[200] = last ? 1 : 0;

    for (int i = 0; i < 5; i++) {
        data[201 + i] = (uint8_t) ((counter >> (4 - i)) & 1);
    }

    uint8_t payload[PayloadBits];
    encodePunctured(data, PacketDataBits, m_puncture3, 8, payload);
    finishFrame(payload, SyncPacket, levels);
}

void DVM17::buildEOTFrame(int8_t *levels)
{
    // The end of transmission marker is the sync word repeated for a whole frame
    for (int i = 0; i < FrameSymbols; i += SyncSymbols) {
        syncLevels(SyncEOT, levels + i);
    }
}

void DVM17::framePayload(const float *llr, float *payload)
{
    float randomised[PayloadBits];
    std::copy(llr + SyncBits, llr + FrameBits, randomised);
    randomiseLLR(randomised, PayloadBits);
    deinterleave(randomised, payload);
}

bool DVM17::decodeLSFFrame(const float *payload, LSF& lsf, int *cost)
{
    uint8_t bits[LSFBits];

    if (!decodePunctured(payload, PayloadBits, LSFBits, m_puncture1, 61, bits, cost)) {
        return false;
    }

    uint8_t bytes[LSFBytes];
    DVBits::bitsToBytesMSB(bits, LSFBits, bytes);
    return unpackLSF(bytes, lsf);
}

bool DVM17::decodeStreamFrame(const float *payload, uint16_t& frameNumber, uint8_t *payloadBytes,
    int& segment, uint8_t *lsfChunk, int *cost)
{
    segment = unpackLICH(payload, lsfChunk);
    uint8_t data[StreamDataBits];

    if (!decodePunctured(payload + LICHBits, StreamCodedBits, StreamDataBits, m_puncture2, 12, data, cost)) {
        return false;
    }

    frameNumber = 0;

    for (int i = 0; i < 16; i++) {
        frameNumber = (uint16_t) ((frameNumber << 1) | data[i]);
    }

    DVBits::bitsToBytesMSB(data + 16, 128, payloadBytes);
    return true;
}

bool DVM17::decodePacketFrame(const float *payload, uint8_t *bytes, bool& last, int& counter, int *cost)
{
    uint8_t data[PacketDataBits];

    if (!decodePunctured(payload, PayloadBits, PacketDataBits, m_puncture3, 8, data, cost)) {
        return false;
    }

    DVBits::bitsToBytesMSB(data, 200, bytes);
    last = data[200] != 0;
    counter = 0;

    for (int i = 0; i < 5; i++) {
        counter = (counter << 1) | data[201 + i];
    }

    return true;
}

std::vector<uint8_t> DVM17::buildPacket(int protocol, const std::vector<uint8_t>& content)
{
    std::vector<uint8_t> packet;
    packet.push_back((uint8_t) protocol);
    packet.insert(packet.end(), content.begin(), content.end());

    if ((int) packet.size() > MaxPacketBytes - 2) {
        packet.resize(MaxPacketBytes - 2);
    }

    uint16_t crc = crc16(packet.data(), (int) packet.size());
    packet.push_back((uint8_t) (crc >> 8));
    packet.push_back((uint8_t) crc);
    return packet;
}

int DVM17::parsePacket(const std::vector<uint8_t>& packet, std::vector<uint8_t>& content)
{
    if ((packet.size() < 3) || (crc16(packet.data(), (int) packet.size()) != 0)) {
        return -1;
    }

    content.assign(packet.begin() + 1, packet.end() - 2);
    return packet[0];
}
