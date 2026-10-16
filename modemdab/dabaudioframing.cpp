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

#include "dabprofile.h"
#include "util/reedsolomon.h"

#include "dabaudioframing.h"
#include "dabtables.h"

namespace modemdab
{

namespace
{

// RS(120, 110, t = 5), shortened from RS(255, 245): generator roots alpha^0..alpha^9,
// field polynomial x^8 + x^4 + x^3 + x^2 + 1
const ReedSolomon::RS<255, 245>& reedSolomon()
{
    static const ReedSolomon::RS<255, 245> rs;
    return rs;
}

// Bits read MSB first
class BitReader
{
public:
    BitReader(const uint8_t *data, int size, int bit) : m_data(data), m_bits(size * 8), m_bit(bit) {}

    bool read(int count, int& value)
    {
        if (m_bit + count > m_bits) {
            return false;
        }
        value = 0;
        for (int i = 0; i < count; i++, m_bit++) {
            value = (value << 1) | ((m_data[m_bit >> 3] >> (7 - (m_bit & 7))) & 1);
        }
        return true;
    }
    int position() const { return m_bit; }

private:
    const uint8_t *m_data;
    int m_bits;
    int m_bit;
};

} // anonymous namespace

bool DABPlusParameters::operator==(const DABPlusParameters& o) const
{
    return (dac48 == o.dac48) && (sbr == o.sbr) && (stereo == o.stereo) && (ps == o.ps)
        && (mpegSurround == o.mpegSurround) && (numAUs == o.numAUs);
}

DABPlusFramer::DABPlusFramer()
{
    reset();
}

void DABPlusFramer::reset()
{
    m_frames.clear();
    m_framesLost.clear();
    m_synchronized = false;
    m_phase = 0;
    m_failures = 0;
    m_parameters = DABPlusParameters();
    m_superframes = 0;
    m_fireCodeErrors = 0;
    m_rsCorrected = 0;
    m_rsUncorrectable = 0;
    m_auCount = 0;
    m_auErrors = 0;
}

bool DABPlusFramer::decode(std::vector<uint8_t>& superframe, int& corrected, int& uncorrectable) const
{
    // The virtual interleaver: row i is bytes i, i + s, i + 2s... of the superframe
    const int s = static_cast<int>(superframe.size()) / 120;
    uint8_t codeword[120];

    corrected = 0;
    uncorrectable = 0;
    for (int i = 0; i < s; i++)
    {
        for (int j = 0; j < 120; j++) {
            codeword[j] = superframe[i + j * s];
        }
        const int result = reedSolomon().decode(codeword, 110, codeword + 110);
        if (result < 0)
        {
            uncorrectable++;
        }
        else if (result > 0)
        {
            corrected += result;
            for (int j = 0; j < 110; j++) {
                superframe[i + j * s] = codeword[j];
            }
        }
    }
    return uncorrectable == 0;
}

bool DABPlusFramer::parse(const std::vector<uint8_t>& superframe, DABPlusSuperframe& out)
{
    const int dataSize = static_cast<int>(superframe.size()) / 120 * 110;
    DABPlusParameters p;

    p.dac48 = (superframe[2] & 0x40) != 0;
    p.sbr = (superframe[2] & 0x20) != 0;
    p.stereo = (superframe[2] & 0x10) != 0;
    p.ps = (superframe[2] & 0x08) != 0;
    p.mpegSurround = superframe[2] & 0x07;

    int auStart[7];
    if (p.sbr) {
        p.numAUs = p.dac48 ? 3 : 2;
    } else {
        p.numAUs = p.dac48 ? 6 : 4;
    }
    switch (p.numAUs)
    {
    case 2: auStart[0] = 5; break;
    case 3: auStart[0] = 6; break;
    case 4: auStart[0] = 8; break;
    default: auStart[0] = 11; break;
    }
    for (int n = 1; n < p.numAUs; n++)
    {
        const int byte = 3 + ((n - 1) * 3) / 2;
        if ((n & 1) == 1) {
            auStart[n] = (superframe[byte] << 4) | (superframe[byte + 1] >> 4);
        } else {
            auStart[n] = ((superframe[byte] & 0x0f) << 8) | superframe[byte + 1];
        }
    }
    auStart[p.numAUs] = dataSize;

    out.parameters = p;
    out.accessUnits.clear();
    for (int n = 0; n < p.numAUs; n++)
    {
        DABAccessUnit au;
        const int start = auStart[n];
        const int end = auStart[n + 1];

        if ((start >= 0) && (end <= dataSize) && (end - start > 2))
        {
            au.valid = DABTables::crc16Check(&superframe[start], end - start);
            au.data.assign(superframe.begin() + start, superframe.begin() + end - 2);
        }
        m_auCount++;
        if (!au.valid) {
            m_auErrors++;
        }
        out.accessUnits.push_back(std::move(au));
    }
    m_parameters = p;
    return true;
}

bool DABPlusFramer::push(const uint8_t *data, int size, DABPlusSuperframe& superframe)
{
    DABProfileScope profile(DABProfile::AudioFraming);
    if ((size <= 0) || (size % 3)) {
        return false;
    }
    if (data) {
        m_frames.emplace_back(data, data + size);
    } else {
        m_frames.emplace_back(size, 0);
    }
    m_framesLost.push_back(data == nullptr);

    if (m_frames.size() < 5) {
        return false;
    }
    while (m_frames.size() > 5)
    {
        m_frames.pop_front();
        m_framesLost.pop_front();
    }

    // Superframe of 5 logical frames: 120 bytes for each 8 kbit/s
    std::vector<uint8_t> sf;
    bool lost = false;
    for (std::size_t i = 0; i < 5; i++)
    {
        sf.insert(sf.end(), m_frames[i].begin(), m_frames[i].end());
        lost = lost || m_framesLost[i];
    }
    if (sf.size() % 120) {
        return false;
    }

    int corrected = 0;
    int uncorrectable = 0;

    if (!m_synchronized)
    {
        // Find the superframe boundary: a valid Fire code, before or after correction
        bool found = !lost && DABTables::fireCodeCheck(sf.data());
        if (!found && !lost)
        {
            std::vector<uint8_t> copy = sf;
            decode(copy, corrected, uncorrectable);
            found = DABTables::fireCodeCheck(copy.data());
        }
        if (!found)
        {
            m_frames.pop_front();
            m_framesLost.pop_front();
            return false;
        }
        m_synchronized = true;
        m_failures = 0;
    }

    m_frames.clear();
    m_framesLost.clear();

    decode(sf, corrected, uncorrectable);
    m_rsCorrected += corrected;
    m_rsUncorrectable += uncorrectable;
    m_superframes++;

    superframe.rsCorrectedBytes = corrected;
    superframe.rsUncorrectableRows = uncorrectable;

    if (!DABTables::fireCodeCheck(sf.data()))
    {
        // The header cannot be trusted: every AU of the superframe is lost
        m_fireCodeErrors++;
        if (!lost) {
            m_failures++;
        }
        if (m_failures >= 3) {
            m_synchronized = false;
        }
        superframe.parameters = m_parameters;
        superframe.accessUnits.assign(std::max(m_parameters.numAUs, 0), DABAccessUnit());
        m_auCount += m_parameters.numAUs;
        m_auErrors += m_parameters.numAUs;
        return true;
    }

    m_failures = 0;
    return parse(sf, superframe);
}

bool DABPlusFramer::extractPAD(const std::vector<uint8_t>& au, const uint8_t *&pad, int& padLength)
{
    // data_stream_element(): id_syn_ele (3 bits, 4), element_instance_tag (4),
    // data_byte_align_flag (1), count (8), [esc_count (8)]
    if ((au.size() < 2) || ((au[0] >> 5) != 4)) {
        return false;
    }
    int count = au[1];
    std::size_t position = 2;
    if (count == 255)
    {
        if (au.size() < 3) {
            return false;
        }
        count += au[2];
        position = 3;
    }
    if ((count < 2) || (position + count > au.size())) {
        return false;
    }
    pad = &au[position];
    padLength = count;
    return true;
}

DABMP2Framer::DABMP2Framer()
{
    reset();
}

void DABMP2Framer::reset()
{
    m_pending.clear();
    m_pendingLost = false;
    m_frames = 0;
    m_crcErrors = 0;
}

bool DABMP2Framer::check(const uint8_t *data, int size, DABMP2Frame& frame)
{
    static const int bitratesMPEG1[16] = {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0};
    static const int bitratesMPEG2[16] = {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0};
    static const int ratesMPEG1[4] = {44100, 48000, 32000, 0};
    static const int ratesMPEG2[4] = {22050, 24000, 16000, 0};
    // Bits of the bit allocation of each sub-band (ISO/IEC 11172-3 tables B.2a and B.2c,
    // ISO/IEC 13818-3 table B.1)
    static const int tableA[27] = {4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2};
    static const int tableC[8] = {4, 4, 3, 3, 3, 3, 3, 3};
    static const int tableLSF[30] = {4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

    frame.valid = false;
    if (size < 8) {
        return false;
    }
    if ((data[0] != 0xff) || ((data[1] & 0xf0) != 0xf0)) {
        return false;
    }
    const bool mpeg1 = (data[1] & 0x08) != 0;
    const int layer = (data[1] >> 1) & 0x03;
    const bool protection = (data[1] & 0x01) == 0;
    const int bitrateIndex = data[2] >> 4;
    const int rateIndex = (data[2] >> 2) & 0x03;
    const int mode = data[3] >> 6;
    const int modeExtension = (data[3] >> 4) & 0x03;

    if ((layer != 2) || !protection) {
        return false;
    }

    frame.sampleRate = mpeg1 ? ratesMPEG1[rateIndex] : ratesMPEG2[rateIndex];
    frame.bitrate = mpeg1 ? bitratesMPEG1[bitrateIndex] : bitratesMPEG2[bitrateIndex];
    frame.channels = mode == 3 ? 1 : 2;
    if ((frame.sampleRate == 0) || (frame.bitrate == 0)) {
        return false;
    }

    const int *nbal;
    int sblimit;
    const int perChannel = frame.bitrate / frame.channels;

    if (!mpeg1)
    {
        nbal = tableLSF;
        sblimit = 30;
        frame.scfCRCLength = 4;
    }
    else if (perChannel <= 48)
    {
        nbal = tableC;
        sblimit = 8;
        frame.scfCRCLength = 2;
    }
    else
    {
        nbal = tableA;
        sblimit = 27;
        frame.scfCRCLength = 4;
    }

    int bound = sblimit;
    if (mode == 1) {
        bound = std::min(sblimit, 4 * (modeExtension + 1));
    }

    // The CRC covers the header from the bit rate index and the bit allocation and ScFSI
    BitReader reader(data, size, 48);
    int allocation[2][32] = {};
    int value;

    for (int sb = 0; sb < sblimit; sb++)
    {
        if (sb < bound)
        {
            for (int ch = 0; ch < frame.channels; ch++)
            {
                if (!reader.read(nbal[sb], allocation[ch][sb])) {
                    return false;
                }
            }
        }
        else
        {
            if (!reader.read(nbal[sb], value)) {
                return false;
            }
            allocation[0][sb] = value;
            allocation[1][sb] = value;
        }
    }
    for (int sb = 0; sb < sblimit; sb++)
    {
        for (int ch = 0; ch < frame.channels; ch++)
        {
            if (allocation[ch][sb] && !reader.read(2, value)) {
                return false;
            }
        }
    }

    uint16_t crc = DABTables::crc16MPEG(data, 16, 16);
    crc = DABTables::crc16MPEG(data, 48, reader.position() - 48, crc);
    frame.valid = crc == static_cast<uint16_t>((data[4] << 8) | data[5]);
    return frame.valid;
}

bool DABMP2Framer::push(const uint8_t *data, int size, int bitrate, DABMP2Frame& frame)
{
    DABProfileScope profile(DABProfile::AudioFraming);
    (void) bitrate;

    if (!data)
    {
        // The frame this was part of is lost
        if (!m_pending.empty()) {
            m_pending.clear();
        }
        m_pendingLost = true;
        return false;
    }

    if (!m_pending.empty())
    {
        // Second half of a 24 kHz (MPEG-2 LSF) frame
        frame.data = m_pending;
        frame.data.insert(frame.data.end(), data, data + size);
        m_pending.clear();
        check(frame.data.data(), static_cast<int>(frame.data.size()), frame);
        m_frames++;
        if (!frame.valid) {
            m_crcErrors++;
        }
        return true;
    }

    const bool sync = (size >= 4) && (data[0] == 0xff) && ((data[1] & 0xf0) == 0xf0);
    if (sync && ((data[1] & 0x08) == 0))
    {
        // 24 kHz: the frame continues in the next logical frame
        m_pending.assign(data, data + size);
        m_pendingLost = false;
        return false;
    }
    if (!sync && m_pendingLost)
    {
        // The second half of a frame whose first half was lost
        m_pendingLost = false;
        return false;
    }

    frame.data.assign(data, data + size);
    check(frame.data.data(), size, frame);
    m_frames++;
    if (!frame.valid) {
        m_crcErrors++;
    }
    return true;
}

} // namespace modemdab
