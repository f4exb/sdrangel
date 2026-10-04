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

#include <QDebug>

#include <algorithm>
#include <bitset>
#include <cmath>

#include "pagerdemodbch.h"
#include "pagerdemodflex.h"

// 4-bit BCD. 0xC is fill
const char PagerDemodFlexDecoder::m_numericChars[17] = "0123456789 U -][";

bool PagerDemodFlexMessage::isNumeric() const
{
    return (m_type == Tone)
        || (m_type == StandardNumeric)
        || (m_type == SpecialNumeric)
        || (m_type == NumberedNumeric);
}

QString PagerDemodFlexMessage::typeName(Type type)
{
    switch (type)
    {
    case Secure:
        return "Secure";
    case ShortInstruction:
        return "Instruction";
    case Tone:
        return "Tone";
    case StandardNumeric:
        return "Numeric";
    case SpecialNumeric:
        return "Special Numeric";
    case Alphanumeric:
        return "Alpha";
    case Binary:
        return "Binary";
    case NumberedNumeric:
        return "Numbered Numeric";
    }
    return "";
}

bool PagerDemodFlexDecoder::checksumValid(quint32 data)
{
    quint32 sum = (data & 0xf)
        + ((data >> 4) & 0xf)
        + ((data >> 8) & 0xf)
        + ((data >> 12) & 0xf)
        + ((data >> 16) & 0xf)
        + ((data >> 20) & 0x1);
    return (sum & 0xf) == 0xf;
}

quint32 PagerDemodFlexDecoder::setChecksum(quint32 data)
{
    data &= 0x1ffff0;
    quint32 sum = ((data >> 4) & 0xf)
        + ((data >> 8) & 0xf)
        + ((data >> 12) & 0xf)
        + ((data >> 16) & 0xf)
        + ((data >> 20) & 0x1);
    return data | ((0xf - (sum & 0xf)) & 0xf);
}

bool PagerDemodFlexDecoder::correct(quint32 word, quint32& data, bool& parityError)
{
    // Received LSB first, so reverse to the bit order the BCH decoder uses
    quint32 cw = PagerDemodBCH::reverse(word);
    quint32 corrected;
    bool ok = PagerDemodBCH::decode(cw, corrected);

    data = PagerDemodBCH::reverse(corrected) & 0x1fffff;
    parityError = !PagerDemodBCH::evenParity(corrected, 1, 31, corrected & 1);
    return ok;
}

quint32 PagerDemodFlexDecoder::encode(quint32 data)
{
    quint32 cw = PagerDemodBCH::encode(PagerDemodBCH::reverse(data & 0x1fffff));
    cw |= PagerDemodBCH::xorBits(cw, 1, 31);
    return PagerDemodBCH::reverse(cw);
}

bool PagerDemodFlexDecoder::isLongAddress(quint32 addressWord)
{
    return (addressWord < 0x008001)
        || ((addressWord > 0x1e0000) && (addressWord < 0x1f0001))
        || (addressWord > 0x1f7ffe);
}

void PagerDemodFlexDecoder::reset()
{
    m_fragments.clear();

    for (auto& group : m_groupCalls) {
        group = Group();
    }
}

QList<PagerDemodFlexMessage> PagerDemodFlexDecoder::decodeFrame(const Frame& frame)
{
    QList<PagerDemodFlexMessage> timedOut;
    const int framesPerHour = m_cycles * m_framesPerCycle;
    int frameIndex = frame.m_cycle * m_framesPerCycle + frame.m_frame;

    // Output fragmented messages whose next part is overdue
    for (auto it = m_fragments.begin(); it != m_fragments.end(); )
    {
        int age = (frameIndex - it->m_frameIndex + framesPerHour) % framesPerHour;

        if (age > m_fragmentTimeoutFrames)
        {
            it->m_message.m_incomplete = true;
            timedOut.append(it->m_message);
            it = m_fragments.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Expand group calls before their groups can expire
    QList<PagerDemodFlexMessage> messages = expandGroups(timedOut);
    expireGroups(frameIndex);

    m_codewords = 0;
    m_uncorrectable = 0;
    QList<PagerDemodFlexMessage> decoded;

    for (const auto& phase : frame.m_phases) {
        decodePhase(frame, phase, decoded);
    }

    messages.append(expandGroups(decoded));
    return messages;
}

QList<PagerDemodFlexMessage> PagerDemodFlexDecoder::flush()
{
    QList<PagerDemodFlexMessage> messages;

    for (auto& fragment : m_fragments)
    {
        fragment.m_message.m_incomplete = true;
        messages.append(fragment.m_message);
    }

    m_fragments.clear();
    return expandGroups(messages);
}

// Short instruction vector: instruction (bits 7-9). Instruction 0 assigns the address to
// a temporary address (bits 17-20) for a frame (bits 10-16) in this cycle or the next
void PagerDemodFlexDecoder::assignGroup(const Frame& frame, qint64 capcode, quint32 viw)
{
    if (((viw >> 7) & 0x7) != 0) {
        return;
    }

    int assignedFrame = (viw >> 10) & 0x7f;
    int group = (viw >> 17) & 0xf;
    int cycle = frame.m_cycle;

    if (assignedFrame <= frame.m_frame) {
        cycle = (cycle + 1) % m_cycles;
    }

    int frameIndex = cycle * m_framesPerCycle + assignedFrame;
    Group& groupCall = m_groupCalls[group];

    // A temporary address is only used for one call at a time, so an assignment
    // for a different frame starts a new group
    if (groupCall.m_frameIndex != frameIndex)
    {
        groupCall.m_capcodes.clear();
        groupCall.m_frameIndex = frameIndex;
    }

    if (!groupCall.m_capcodes.contains(capcode) && (groupCall.m_capcodes.size() < m_maxGroupMembers)) {
        groupCall.m_capcodes.append(capcode);
    }
}

// Forget groups whose frame has passed without a call, unless the call is a fragmented
// message that is still being received
void PagerDemodFlexDecoder::expireGroups(int frameIndex)
{
    const int framesPerHour = m_cycles * m_framesPerCycle;

    for (int g = 0; g < m_groups; g++)
    {
        Group& groupCall = m_groupCalls[g];

        if ((groupCall.m_frameIndex >= 0) && !m_fragments.contains(m_groupCapcodeFirst + g))
        {
            int age = (frameIndex - groupCall.m_frameIndex + framesPerHour) % framesPerHour;

            if ((age > 0) && (age < framesPerHour / 2))
            {
                qDebug() << "PagerDemodFlexDecoder::expireGroups: Group call missed. Temporary address" << g
                    << "for" << groupCall.m_capcodes.size() << "capcodes";
                groupCall = Group();
            }
        }
    }
}

// Replace messages to temporary addresses, that start in the frame the group was assigned
// for, with a copy for each capcode in the group
QList<PagerDemodFlexMessage> PagerDemodFlexDecoder::expandGroups(const QList<PagerDemodFlexMessage>& messages)
{
    QList<PagerDemodFlexMessage> expanded;

    for (const auto& message : messages)
    {
        if ((message.m_capcode < m_groupCapcodeFirst) || (message.m_capcode >= m_groupCapcodeFirst + m_groups))
        {
            expanded.append(message);
            continue;
        }

        int group = (int) (message.m_capcode - m_groupCapcodeFirst);
        Group& groupCall = m_groupCalls[group];
        PagerDemodFlexMessage groupMessage = message;
        groupMessage.m_group = group;
        int frameIndex = message.m_cycle * m_framesPerCycle + message.m_frame;

        if (groupCall.m_capcodes.isEmpty() || (groupCall.m_frameIndex != frameIndex))
        {
            // Assignments not received, or for a later call, which they're kept for
            expanded.append(groupMessage);
        }
        else
        {
            for (qint64 capcode : groupCall.m_capcodes)
            {
                groupMessage.m_capcode = capcode;
                expanded.append(groupMessage);
            }

            groupCall = Group();
        }
    }

    return expanded;
}

void PagerDemodFlexDecoder::decodePhase(const Frame& frame, const Phase& phase, QList<PagerDemodFlexMessage>& messages)
{
    quint32 data[m_wordsPerPhase];
    bool valid[m_wordsPerPhase];
    bool parity[m_wordsPerPhase];

    for (int i = 0; i < m_wordsPerPhase; i++)
    {
        bool parityError;
        bool ok = correct(phase.m_words[i], data[i], parityError);

        m_codewords++;
        if (!ok && !phase.m_erased[i]) {
            m_uncorrectable++;
        }

        // About half of random words pass BCH correction, so words received after the
        // carrier was lost are never trusted. Uncorrectable words keep their received
        // bits, so message text stays aligned
        valid[i] = ok && !phase.m_erased[i];
        parity[i] = valid[i] && parityError;
    }

    // Block information word
    quint32 biw = data[0];

    if (!valid[0] || !checksumValid(biw)) {
        return;
    }

    int aoffset = ((biw >> 8) & 0x3) + 1;   // Address field start, after any extra BIWs
    int voffset = (biw >> 10) & 0x3f;       // Vector field start
    int addressWords = voffset - aoffset;

    // Each address word has a vector word at the same offset into the vector field
    if ((addressWords < 0) || (voffset + addressWords > m_wordsPerPhase)) {
        return;
    }

    int frameIndex = frame.m_cycle * m_framesPerCycle + frame.m_frame;
    // Message field follows the vector field. Vectors may only point in to it, apart from
    // long addresses, which have the first word of their message in the second vector word
    int messageField = voffset + addressWords;

    for (int i = aoffset; i < voffset; i++)
    {
        int j = voffset + i - aoffset;
        quint32 aw = data[i];

        if (!valid[i] || (aw == 0) || (aw == 0x1fffff)) {
            continue; // Idle or unknown address
        }

        PagerDemodFlexMessage message;
        message.m_longAddress = isLongAddress(aw);
        message.m_bitRate = frame.m_bitRate;
        message.m_cycle = frame.m_cycle;
        message.m_frame = frame.m_frame;
        message.m_phase = phase.m_name;

        if (message.m_longAddress)
        {
            // Long addresses use two address words and two vector words
            i++;

            if ((i >= voffset) || !valid[i]) {
                continue;
            }

            message.m_capcode = (qint64) aw + ((qint64) (data[i] ^ 0x1fffff) << 15) + 0x1f9000;
        }
        else
        {
            message.m_capcode = (qint64) aw - 0x8000;
        }

        // Vector word. Its checksum guards against phantom pages from damaged address words
        if (!valid[j] || !checksumValid(data[j])) {
            continue;
        }

        quint32 viw = data[j];
        message.m_type = (PagerDemodFlexMessage::Type) ((viw >> 4) & 0x7);

        auto countErrors = [&](int w) {
            if (!valid[w]) {
                message.m_bchErrors++;
            } else if (parity[w]) {
                message.m_parityErrors++;
            }
        };

        switch (message.m_type)
        {
        case PagerDemodFlexMessage::Alphanumeric:
        case PagerDemodFlexMessage::Secure:
        case PagerDemodFlexMessage::Binary:
        {
            int mw1 = (viw >> 7) & 0x7f;    // First word of message field
            int len = (viw >> 14) & 0x7f;   // Words, including the header
            int hdr;

            // The header is in the second vector word for long addresses,
            // otherwise it's the first word of the message field
            if (message.m_longAddress)
            {
                hdr = j + 1;
            }
            else
            {
                hdr = mw1;
                mw1++;
            }
            if (len >= 1) {
                len--;
            }

            if ((!message.m_longAddress && (hdr < messageField))
                || (hdr >= m_wordsPerPhase)
                || ((len > 0) && (mw1 < messageField))
                || (mw1 + len > m_wordsPerPhase)) {
                continue;
            }

            // Header: fragment check (bits 0-9), continued (10), fragment number (11-12),
            // message number (13-18). An unreadable header is taken as a whole message
            quint32 header = data[hdr];
            int fragment = valid[hdr] ? (header >> 11) & 0x3 : 3;
            bool continued = valid[hdr] && ((header >> 10) & 0x1);
            int messageNumber = (header >> 13) & 0x3f;
            countErrors(hdr);

            for (int w = mw1; w < mw1 + len; w++)
            {
                countErrors(w);
                quint32 dw = data[w];

                if (message.m_type == PagerDemodFlexMessage::Binary)
                {
                    if (!message.m_text.isEmpty()) {
                        message.m_text.append(' ');
                    }
                    message.m_text.append(QString("%1").arg(dw, 6, 16, QChar('0')).toUpper());
                }
                else
                {
                    for (int c = 0; c < 3; c++)
                    {
                        // The first character of a message's first fragment is its signature
                        if ((c == 0) && (w == mw1) && (fragment == 3)) {
                            continue;
                        }

                        char ch = (dw >> (7 * c)) & 0x7f;

                        // NUL and ETX are used as fill
                        if ((ch != 0x00) && (ch != 0x03)) {
                            message.m_text.append(QChar(ch));
                        }
                    }
                }
            }

            if (message.m_type == PagerDemodFlexMessage::Binary) {
                messages.append(message);
            } else {
                addFragment(message, fragment, continued, messageNumber, frameIndex, messages);
            }
            break;
        }

        case PagerDemodFlexMessage::StandardNumeric:
        case PagerDemodFlexMessage::SpecialNumeric:
        case PagerDemodFlexMessage::NumberedNumeric:
        {
            int w1 = (viw >> 7) & 0x7f;
            int w2 = ((viw >> 14) & 0x7) + w1;
            std::vector<int> words;

            // For long addresses, the first digits are in the second vector word
            if (message.m_longAddress)
            {
                words.push_back(j + 1);
                for (int w = w1; w < w2; w++) {
                    words.push_back(w);
                }
            }
            else
            {
                for (int w = w1; w <= w2; w++) {
                    words.push_back(w);
                }
            }

            bool inMessageField = std::all_of(words.begin(), words.end(), [&](int w) {
                return (w >= messageField) || (message.m_longAddress && (w == j + 1));
            });

            if (!inMessageField || (words.back() >= m_wordsPerPhase)) {
                continue;
            }

            // Digits are 4 bits, LSB first, after a 2-bit header (10 bits for numbered numeric)
            int count = 4 + ((message.m_type == PagerDemodFlexMessage::NumberedNumeric) ? 10 : 2);
            unsigned int digit = 0;

            for (int w : words)
            {
                countErrors(w);
                quint32 dw = data[w];

                for (int k = 0; k < 21; k++)
                {
                    digit = (digit >> 1) & 0x7;
                    if (dw & 1) {
                        digit |= 0x8;
                    }
                    dw >>= 1;

                    if (--count == 0)
                    {
                        if (digit != 0xc) {
                            message.m_text.append(m_numericChars[digit]);
                        }
                        count = 4;
                    }
                }
            }

            messages.append(message);
            break;
        }

        case PagerDemodFlexMessage::Tone:
        {
            // Short message type 0 carries 3 digits in the vector word (8 for long addresses).
            // Other types are tone only
            if (((viw >> 7) & 0x3) == 0)
            {
                for (int b = 9; b <= 17; b += 4)
                {
                    unsigned int digit = (viw >> b) & 0xf;
                    if (digit != 0xc) {
                        message.m_text.append(m_numericChars[digit]);
                    }
                }

                if (message.m_longAddress)
                {
                    countErrors(j + 1);
                    for (int b = 0; b <= 16; b += 4)
                    {
                        unsigned int digit = (data[j + 1] >> b) & 0xf;
                        if (digit != 0xc) {
                            message.m_text.append(m_numericChars[digit]);
                        }
                    }
                }
            }

            messages.append(message);
            break;
        }

        case PagerDemodFlexMessage::ShortInstruction:
            assignGroup(frame, message.m_capcode, viw);
            break;
        }
    }
}

// Alphanumeric messages can be split over several frames. The first fragment is numbered 3,
// subsequent fragments 0, 1, 2, 0..., and all but the last have the continued flag set.
// All fragments of a message have the same message number
void PagerDemodFlexDecoder::addFragment(PagerDemodFlexMessage& message, int fragment, bool continued, int messageNumber, int frameIndex, QList<PagerDemodFlexMessage>& messages)
{
    auto it = m_fragments.find(message.m_capcode);

    // A continuation of a different message means the rest of the earlier one was lost
    if ((fragment != 3) && (it != m_fragments.end()) && (it->m_messageNumber != messageNumber))
    {
        it->m_message.m_incomplete = true;
        messages.append(it->m_message);
        m_fragments.erase(it);
        it = m_fragments.end();
    }

    if (fragment == 3)
    {
        // A new message, so any earlier one for this capcode won't be completed
        if (it != m_fragments.end())
        {
            it->m_message.m_incomplete = true;
            messages.append(it->m_message);
            m_fragments.erase(it);
        }

        if (continued) {
            m_fragments.insert(message.m_capcode, Fragment{message, messageNumber, 0, frameIndex});
        } else {
            messages.append(message);
        }
    }
    else if (it == m_fragments.end())
    {
        // The start of the message was missed
        message.m_incomplete = true;

        if (continued) {
            m_fragments.insert(message.m_capcode, Fragment{message, messageNumber, (fragment + 1) % 3, frameIndex});
        } else {
            messages.append(message);
        }
    }
    else
    {
        Fragment& part = *it;

        if (fragment != part.m_nextFragment) {
            part.m_message.m_incomplete = true;
        }

        part.m_message.m_text.append(message.m_text);
        part.m_message.m_bchErrors += message.m_bchErrors;
        part.m_message.m_parityErrors += message.m_parityErrors;
        part.m_nextFragment = (fragment + 1) % 3;
        part.m_frameIndex = frameIndex;

        if (!continued)
        {
            messages.append(part.m_message);
            m_fragments.erase(it);
        }
    }
}

PagerDemodFlex::PagerDemodFlex(int sampleRate) :
    m_sampleRate(sampleRate),
    m_sps1600(sampleRate / 1600),
    m_candidateFlywheel(false),
    m_candidateFirst(0),
    m_candidateEnd(0),
    m_flywheelSyncEnd(0.0),
    m_flywheelPeriods(0),
    m_flywheelCode(0),
    m_flywheelInverted(false),
    m_flywheelCycle(0),
    m_flywheelFrame(0),
    m_syncEnd(0.0),
    m_polarity(0.0f),
    m_dc(0.0f),
    m_deviation(0.0f),
    m_syncPower(0.0f),
    m_bitIndex(0),
    m_nextBitSample(0.0),
    m_deviationSum(0.0),
    m_fiw(0),
    m_sps(0),
    m_timing(0.0),
    m_gotMid(false),
    m_mid(0.0f),
    m_prevSymbol(0.0f),
    m_havePrevSymbol(false),
    m_prevMf(0.0f),
    m_symbolIndex(0),
    m_sync2Symbols(0),
    m_dataSymbols(0),
    m_blockPower{},
    m_blockSamples{}
{
    int histSize = 1;
    while (histSize <= m_syncBits * m_sps1600) {
        histSize <<= 1;
    }

    m_fm.resize(histSize);
    m_magsq.resize(histSize);
    m_mfHist.resize(histSize);
    // Large enough for the flywheel's widest search window
    int candidateSize = 2 * m_sps1600 * m_flywheelFrames + 2;
    m_candidateMetric.resize(candidateSize);
    m_candidateCode.resize(candidateSize);
    m_candidateInverted.resize(candidateSize);
    m_candidateDC.resize(candidateSize);
    m_candidatePower.resize(candidateSize);
    m_histMask = histSize - 1;
    m_syncReg.resize(m_sps1600);
    reset();
}

void PagerDemodFlex::reset()
{
    std::fill(m_fm.begin(), m_fm.end(), 0.0f);
    std::fill(m_magsq.begin(), m_magsq.end(), 0.0f);
    std::fill(m_mfHist.begin(), m_mfHist.end(), 0.0f);
    std::fill(m_syncReg.begin(), m_syncReg.end(), 0);
    m_sum1600 = 0.0;
    m_sum3200 = 0.0;
    m_sumSync = 0.0;
    m_sumMagsqSync = 0.0;
    m_mf1600 = 0.0f;
    m_mf3200 = 0.0f;
    m_sampleCount = 0;
    m_state = Hunting;
    m_candidate = false;
    m_flywheel = false;
    m_decoder.reset();
    m_messages.clear();
    m_frameCount = 0;
    m_lastFrameSample = 0;
    m_lastFrame = FrameInfo();
}

int PagerDemodFlex::popcount64(quint64 x)
{
    return (int) std::bitset<64>(x).count();
}

// 64-bit sync code: 16-bit mode code, 0xA6C6AAAA, then the inverted mode code
bool PagerDemodFlex::syncMatch(quint64 reg, quint16& code)
{
    quint32 marker = (reg >> 16) & 0xffffffff;
    quint16 high = (reg >> 48) & 0xffff;
    quint16 low = ~reg & 0xffff;

    if ((popcount64(marker ^ 0xa6c6aaaa) < 4) && (popcount64(high ^ low) < 4))
    {
        code = high;
        return true;
    }

    return false;
}

bool PagerDemodFlex::mode(quint16& code, int& baud, int& levels)
{
    static const struct {
        quint16 m_code;
        int m_baud;
        int m_levels;
    } modes[] = {
        {0x870c, 1600, 2},
        {0xb068, 1600, 4},
        {0x7b18, 3200, 2},
        {0xdea0, 3200, 4}
    };

    for (const auto& m : modes)
    {
        if (popcount64(code ^ m.m_code) <= 3)
        {
            code = m.m_code;
            baud = m.m_baud;
            levels = m.m_levels;
            return true;
        }
    }

    return false;
}

void PagerDemodFlex::process(float fm, float magsq)
{
    qint64 n = m_sampleCount++;
    const int syncSamples = m_syncBits * m_sps1600;
    int idx = n & m_histMask;

    // Running sums for the matched filters (a boxcar over one symbol), and the mean
    // frequency and power over the sync code, which has as many 1s as 0s
    float old1600 = m_fm[(n - m_sps1600) & m_histMask];
    float old3200 = m_fm[(n - m_sps1600 / 2) & m_histMask];
    float oldSync = m_fm[(n - syncSamples) & m_histMask];
    float oldMagsq = m_magsq[(n - syncSamples) & m_histMask];
    m_fm[idx] = fm;
    m_magsq[idx] = magsq;
    m_sum1600 += fm - old1600;
    m_sum3200 += fm - old3200;
    m_sumSync += fm - oldSync;
    m_sumMagsqSync += magsq - oldMagsq;

    // Recalculate the sums occasionally, so rounding errors can't accumulate
    if ((n & 0xfffff) == 0)
    {
        m_sum1600 = m_sum3200 = m_sumSync = m_sumMagsqSync = 0.0;
        for (int i = 0; i < syncSamples; i++)
        {
            int j = (n - i) & m_histMask;
            if (i < m_sps1600) {
                m_sum1600 += m_fm[j];
            }
            if (i < m_sps1600 / 2) {
                m_sum3200 += m_fm[j];
            }
            m_sumSync += m_fm[j];
            m_sumMagsqSync += m_magsq[j];
        }
    }

    m_mf1600 = m_sum1600 / m_sps1600;
    m_mf3200 = m_sum3200 / (m_sps1600 / 2);
    m_mfHist[idx] = m_mf1600;

    // Sync 1 is searched for on every sample phase of the 1600 bit/s stream.
    // Bits are 1 for low frequency, which is multimon-ng's normal polarity
    float dc = m_sumSync / syncSamples;
    quint64& reg = m_syncReg[n % m_sps1600];
    reg = (reg << 1) | ((m_mf1600 < dc) ? 1 : 0);

    switch (m_state)
    {
    case Hunting:
        hunt(n, reg, dc);
        break;

    case FIW:
        if (n >= std::llround(m_nextBitSample)) {
            fiwBit(m_mf1600);
        }
        break;

    case Data:
        dataSample((m_info.m_baud == 3200) ? m_mf3200 : m_mf1600, magsq);
        break;
    }
}

void PagerDemodFlex::hunt(qint64 n, quint64 reg, float dc)
{
    const int syncSamples = m_syncBits * m_sps1600;
    quint16 code;
    bool inverted = false;
    bool match = syncMatch(reg, code);

    if (!match)
    {
        match = syncMatch(~reg, code);
        inverted = match;
    }

    int baud, levels;
    if (match && !mode(code, baud, levels)) {
        match = false; // Unsupported mode, e.g. ReFLEX
    }

    if (match)
    {
        // A sync match takes precedence over the flywheel's prediction
        if (!m_candidate || m_candidateFlywheel) {
            openCandidate(n, n + m_sps1600, false);
        }

        // Matches occur on a run of consecutive sample phases. With a good signal, that's
        // nearly all of them, so the best timing is where the matched filter output over
        // the sync code is largest. Phases at the edges of the run, where bit decisions
        // are marginal, can read the mode code with errors, so the mode is taken from
        // the best phase too
        int i = (int) (n - m_candidateFirst);
        float metric = 0.0f;

        for (int k = 0; k < m_syncBits; k++) {
            metric += std::fabs(m_mfHist[(n - k * m_sps1600) & m_histMask] - dc);
        }

        m_candidateMetric[i] = metric;
        m_candidateCode[i] = code;
        m_candidateInverted[i] = inverted;
        m_candidateDC[i] = dc;
        m_candidatePower[i] = m_sumMagsqSync / syncSamples;
    }
    else if (m_flywheel && (!m_candidate || m_candidateFlywheel))
    {
        flywheelSample(n, reg, dc);
    }

    if (m_candidate && (n + 1 >= m_candidateEnd))
    {
        m_candidate = false;

        if (*std::max_element(m_candidateMetric.begin(), m_candidateMetric.end()) > 0.0f) {
            startFrame();
        }
    }

    // The decoder times fragments out in frames, which it only sees while frames are
    // received, so output any still waiting once that long has passed without a frame
    const qint64 fragmentTimeout = (qint64) PagerDemodFlexDecoder::m_fragmentTimeoutFrames * m_sampleRate * 15 / 8;

    if (((n & 0xffff) == 0) && m_decoder.hasFragments() && (n - m_lastFrameSample > fragmentTimeout)) {
        m_messages.append(m_decoder.flush());
    }
}

void PagerDemodFlex::openCandidate(qint64 first, qint64 end, bool flywheel)
{
    m_candidate = true;
    m_candidateFlywheel = flywheel;
    m_candidateFirst = first;
    m_candidateEnd = end;
    std::fill(m_candidateMetric.begin(), m_candidateMetric.end(), 0.0f);
}

// Search for the predicted sync code, with many more bit errors allowed than for a sync match
void PagerDemodFlex::flywheelSample(qint64 n, quint64 reg, float dc)
{
    const int frameSamples = m_sampleRate * 15 / 8;
    // The window widens with time since the last frame, to allow for clock error
    double halfWidth = m_sps1600 * m_flywheelPeriods;
    qint64 first = (qint64) std::floor(m_flywheelSyncEnd - halfWidth);
    qint64 last = (qint64) std::ceil(m_flywheelSyncEnd + halfWidth);

    if (n > last)
    {
        // Nothing found, so predict the frame after
        if (m_flywheelPeriods < m_flywheelFrames)
        {
            m_flywheelPeriods++;
            m_flywheelSyncEnd += frameSamples;
            if (++m_flywheelFrame == PagerDemodFlexDecoder::m_framesPerCycle)
            {
                m_flywheelFrame = 0;
                m_flywheelCycle = (m_flywheelCycle + 1) % PagerDemodFlexDecoder::m_cycles;
            }
        }
        else
        {
            m_flywheel = false;
        }
        return;
    }

    if (n < first) {
        return;
    }

    if (!m_candidate) {
        openCandidate(n, last + 1, true);
    }

    quint64 expected = ((quint64) m_flywheelCode << 48) | (0xa6c6aaaaULL << 16) | (quint16) ~m_flywheelCode;
    quint64 received = m_flywheelInverted ? ~reg : reg;

    if (popcount64(received ^ expected) > m_flywheelSyncErrors) {
        return;
    }

    // Correlate with the expected code, as with bit errors, the matched filter energy alone
    // doesn't indicate the right timing. Sync bits are 1 for low frequency
    float metric = 0.0f;

    for (int k = 0; k < m_syncBits; k++)
    {
        bool low = (((expected >> k) & 1) != 0) != m_flywheelInverted;
        float v = m_mfHist[(n - k * m_sps1600) & m_histMask] - dc;
        metric += low ? -v : v;
    }

    if (metric > 0.0f)
    {
        int i = (int) (n - m_candidateFirst);
        m_candidateMetric[i] = metric;
        m_candidateCode[i] = m_flywheelCode;
        m_candidateInverted[i] = m_flywheelInverted;
        m_candidateDC[i] = dc;
        m_candidatePower[i] = m_sumMagsqSync / (m_syncBits * m_sps1600);
    }
}

void PagerDemodFlex::startFrame()
{
    // Peak of the sync code energy, refined by fitting a parabola. The matched filter output
    // at m_syncEnd is the mean over the last bit of the sync code
    int length = (int) (m_candidateEnd - m_candidateFirst);
    int best = (int) (std::max_element(m_candidateMetric.begin(), m_candidateMetric.begin() + length) - m_candidateMetric.begin());
    double fraction = 0.0;

    m_info = FrameInfo();
    m_info.m_code = m_candidateCode[best];
    m_info.m_inverted = m_candidateInverted[best];
    m_info.m_flywheel = m_candidateFlywheel;
    mode(m_info.m_code, m_info.m_baud, m_info.m_levels);

    if ((best > 0) && (best < length - 1) && (m_candidateMetric[best - 1] > 0.0f) && (m_candidateMetric[best + 1] > 0.0f))
    {
        float left = m_candidateMetric[best - 1];
        float centre = m_candidateMetric[best];
        float right = m_candidateMetric[best + 1];
        float denom = left - 2.0f * centre + right;

        if (denom < 0.0f) {
            fraction = std::max(-0.5f, std::min(0.5f, 0.5f * (left - right) / denom));
        }
    }

    m_syncEnd = m_candidateFirst + best + fraction;
    m_info.m_syncSample = std::llround(m_syncEnd);
    m_dc = m_candidateDC[best];
    m_syncPower = m_candidatePower[best];

    // Data bits are 1 for high frequency, the opposite of the sync code
    m_polarity = m_info.m_inverted ? -1.0f : 1.0f;

    m_bitIndex = 0;
    m_nextBitSample = m_syncEnd + m_sps1600;
    m_deviationSum = 0.0;
    m_fiw = 0;
    m_state = FIW;
}

void PagerDemodFlex::fiwBit(float mf)
{
    float r = (mf - m_dc) * m_polarity;
    m_deviationSum += std::fabs(r);

    if ((m_bitIndex >= m_syncTailBits) && (r > 0.0f)) {
        m_fiw |= 1u << (m_bitIndex - m_syncTailBits);
    }

    m_bitIndex++;
    m_nextBitSample += m_sps1600;

    if (m_bitIndex < m_syncTailBits + m_fiwBits) {
        return;
    }

    // Frame information word: checksum (bits 0-3), cycle (4-7), frame (8-14)
    quint32 fiw;
    bool parityError;

    if (!PagerDemodFlexDecoder::correct(m_fiw, fiw, parityError) || !PagerDemodFlexDecoder::checksumValid(fiw))
    {
        m_state = Hunting;
        return;
    }

    m_info.m_cycle = (fiw >> 4) & 0xf;
    m_info.m_frame = (fiw >> 8) & 0x7f;

    if (m_info.m_cycle >= PagerDemodFlexDecoder::m_cycles)
    {
        m_state = Hunting;
        return;
    }

    // A frame found by the flywheel must be the one predicted
    if (m_info.m_flywheel && ((m_info.m_cycle != m_flywheelCycle) || (m_info.m_frame != m_flywheelFrame)))
    {
        m_state = Hunting;
        return;
    }

    m_deviation = m_deviationSum / m_bitIndex;
    m_info.m_deviation = m_deviation;
    m_info.m_offset = m_dc;

    if (m_deviation <= 0.0f)
    {
        m_state = Hunting;
        return;
    }

    // Sync 2 starts at the end of the FIW, at the data symbol rate
    double fiwEnd = m_nextBitSample - m_sps1600;
    m_sps = m_sampleRate / m_info.m_baud;
    m_timing = fiwEnd + m_sps - (m_sampleCount - 1);
    m_gotMid = false;
    m_havePrevSymbol = false;
    m_prevMf = (m_info.m_baud == 3200) ? m_mf3200 : m_mf1600;
    m_symbolIndex = 0;
    m_sync2Symbols = m_info.m_baud * 25 / 1000;
    m_dataSymbols = m_info.m_baud * 1760 / 1000;

    for (int b = 0; b < m_blocks; b++)
    {
        m_blockPower[b] = 0.0;
        m_blockSamples[b] = 0;
    }

    // Phases in transmission order. At 3200 baud, symbols alternate between A/B and C/D.
    // With 4 levels, the second bit of each symbol carries B (or D)
    static const char phaseNames[2][2][4] = {
        {{'A'}, {'A', 'B'}},
        {{'A', 'C'}, {'A', 'B', 'C', 'D'}}
    };
    int fast = (m_info.m_baud == 3200) ? 1 : 0;
    int fourLevel = (m_info.m_levels == 4) ? 1 : 0;
    int phases = (1 + fast) * (1 + fourLevel);

    m_frame = PagerDemodFlexDecoder::Frame();
    m_frame.m_cycle = m_info.m_cycle;
    m_frame.m_frame = m_info.m_frame;
    m_frame.m_bitRate = m_info.m_baud * (m_info.m_levels == 4 ? 2 : 1);
    m_frame.m_phases.resize(phases);

    for (int p = 0; p < phases; p++) {
        m_frame.m_phases[p].m_name = phaseNames[fast][fourLevel][p];
    }

    m_state = Data;
}

void PagerDemodFlex::dataSample(float mf, float magsq)
{
    // Measure power per block, to detect loss of carrier
    int dataIndex = m_symbolIndex - m_sync2Symbols;

    if (dataIndex >= 0)
    {
        int bitIndex = (m_info.m_baud == 3200) ? (dataIndex >> 1) : dataIndex;
        int block = bitIndex >> 8;

        if (block < m_blocks)
        {
            m_blockPower[block] += magsq;
            m_blockSamples[block]++;
        }
    }

    // Matched filter output at the symbol instants and midway between them,
    // interpolated between samples
    m_timing -= 1.0;

    if (!m_gotMid && (m_timing <= m_sps / 2.0))
    {
        float f = m_sps / 2.0 - m_timing;
        m_mid = mf * (1.0f - f) + m_prevMf * f;
        m_gotMid = true;
    }

    if (m_timing <= 0.0)
    {
        float f = -m_timing;
        float y = mf * (1.0f - f) + m_prevMf * f;
        m_timing += m_sps;
        m_gotMid = false;
        symbol(y);
    }

    m_prevMf = mf;
}

void PagerDemodFlex::symbol(float value)
{
    // Normalise so the outer levels are +/-1, with the highest frequency positive
    float r = (value - m_dc) * m_polarity / m_deviation;
    float mid = (m_mid - m_dc) * m_polarity / m_deviation;
    int sym;
    float level;

    if (m_info.m_levels == 4)
    {
        if (r > 2.0f / 3.0f)
        {
            sym = 3;
            level = 1.0f;
        }
        else if (r > 0.0f)
        {
            sym = 2;
            level = 1.0f / 3.0f;
        }
        else if (r > -2.0f / 3.0f)
        {
            sym = 1;
            level = -1.0f / 3.0f;
        }
        else
        {
            sym = 0;
            level = -1.0f;
        }
    }
    else
    {
        sym = (r > 0.0f) ? 3 : 0;
        level = (r > 0.0f) ? 1.0f : -1.0f;
    }

    // Timing: midway between two symbols, the matched filter output is their average when
    // on time, and moves towards the later symbol by the fraction of a symbol we are late.
    // Only transitions of at least two levels are used, as they have the best SNR
    if (m_havePrevSymbol)
    {
        float diff = r - m_prevSymbol;

        if (std::fabs(diff) >= 1.0f)
        {
            float e = (mid - (r + m_prevSymbol) / 2.0f) / diff;
            e = std::max(-0.5f, std::min(0.5f, e));
            m_timing -= m_timingGain * e * m_sps;
        }
    }

    m_prevSymbol = r;
    m_havePrevSymbol = true;

    // Track the slicer levels: the offset from every decision, the deviation from outer levels
    float err = r - level;
    m_dc += m_levelGain * err * m_deviation * m_polarity;

    if (std::fabs(level) == 1.0f) {
        m_deviation += m_levelGain * (std::fabs(r) - 1.0f) * m_deviation;
    }

    int dataIndex = m_symbolIndex - m_sync2Symbols;
    m_symbolIndex++;

    if (dataIndex < 0) {
        return; // Sync 2
    }

    // Each phase's 88 words are sent in 11 blocks of 8, interleaved a bit at a time
    int half = 0;
    int bitIndex = dataIndex;

    if (m_info.m_baud == 3200)
    {
        half = dataIndex & 1;
        bitIndex = dataIndex >> 1;
    }

    int word = ((bitIndex >> 8) * PagerDemodFlexDecoder::m_wordsPerBlock) + (bitIndex & 7);
    int bit = (bitIndex >> 3) & 31;
    quint32 a = (sym > 1) ? 1 : 0;
    quint32 b = ((sym == 1) || (sym == 2)) ? 1 : 0;
    int p = half * ((m_info.m_levels == 4) ? 2 : 1);

    m_frame.m_phases[p].m_words[word] |= a << bit;
    if (m_info.m_levels == 4) {
        m_frame.m_phases[p + 1].m_words[word] |= b << bit;
    }

    if (dataIndex + 1 == m_dataSymbols) {
        endFrame();
    }
}

void PagerDemodFlex::endFrame()
{
    // Erase blocks where the carrier was lost
    float threshold = m_syncPower * std::pow(10.0f, -m_carrierLossDB / 10.0f);
    int erasedBlocks = 0;

    for (int b = 0; b < m_blocks; b++)
    {
        if ((m_blockSamples[b] > 0) && (m_blockPower[b] / m_blockSamples[b] < threshold))
        {
            erasedBlocks++;

            for (auto& phase : m_frame.m_phases)
            {
                for (int w = 0; w < PagerDemodFlexDecoder::m_wordsPerBlock; w++) {
                    phase.m_erased[b * PagerDemodFlexDecoder::m_wordsPerBlock + w] = true;
                }
            }
        }
    }

    m_messages.append(m_decoder.decodeFrame(m_frame));

    m_info.m_codewords = m_decoder.getCodewords();
    m_info.m_uncorrectable = m_decoder.getUncorrectable();
    m_info.m_erasedBlocks = erasedBlocks;
    m_lastFrame = m_info;
    m_lastFrameSample = m_sampleCount;
    m_frameCount++;

    // Predict the next frame
    m_flywheel = true;
    m_flywheelSyncEnd = m_syncEnd + m_sampleRate * 15.0 / 8.0;
    m_flywheelPeriods = 1;
    m_flywheelCode = m_info.m_code;
    m_flywheelInverted = m_info.m_inverted;
    m_flywheelFrame = m_info.m_frame + 1;
    m_flywheelCycle = m_info.m_cycle;
    if (m_flywheelFrame == PagerDemodFlexDecoder::m_framesPerCycle)
    {
        m_flywheelFrame = 0;
        m_flywheelCycle = (m_flywheelCycle + 1) % PagerDemodFlexDecoder::m_cycles;
    }

    m_state = Hunting;
}
