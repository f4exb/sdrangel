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

#include "rdsencoder.h"

namespace {
const quint16 offsetWords[4] = {252, 408, 360, 436}; // A, B, C, D
const double rdsBitRate = 1187.5;

// Block B flags common to groups 0A and 2A
const quint16 trafficProgramme = 0; // TP: this is not a traffic programme
const quint16 trafficAnnouncement = 0; // TA: no traffic announcement in progress
const quint16 musicSpeech = 1; // M/S: music

/** Biphase symbol shaping from IEC 62106. The data spectrum is shaped by
 * H(f) = cos(pi f td / 4) for |f| <= 2 / td and zero above, which confines RDS
 * to +/-2.4 kHz around 57 kHz. With time t in bit periods, the impulse response is
 * h(t) = 2a cos(4 pi t) / (a^2 - 4 pi^2 t^2) with a = pi / 4.
 */
class SymbolShape
{
public:
    static constexpr double m_delay = 2.75; //!< Delay in bits so that every symbol contributing to a sample is known

    SymbolShape() :
        m_table(2 * m_span * m_resolution + 1),
        m_scale(1.0)
    {
        for (int i = 0; i < (int) m_table.size(); i++) {
            m_table[i] = impulse((double) (i - m_span * m_resolution) / m_resolution);
        }

        // Scale so that the sum of all overlapping symbols can never exceed 1
        double worst = 0.0;

        for (int p = 0; p < m_resolution; p++)
        {
            double sum = 0.0;

            for (int j = 0; j < RDSEncoder::m_nbSymbols; j++) {
                sum += std::fabs(symbol((double) p / m_resolution + j - m_delay));
            }

            worst = std::max(worst, sum);
        }

        m_scale = 1.0 / worst;
    }

    /** Shaped biphase symbol: a positive impulse at 1/4 bit and a negative one at 3/4 bit.
     * tau is the time since the start of the symbol in bit periods.
     */
    Real symbol(double tau) const {
        return m_scale * (h(tau - 0.25) - h(tau - 0.75));
    }

private:
    static const int m_span = 3; //!< Impulse response is truncated to +/-3 bits
    static const int m_resolution = 256; //!< Table entries per bit
    std::vector<double> m_table;
    double m_scale;

    double h(double t) const
    {
        const double x = (t + m_span) * m_resolution;

        if ((x < 0.0) || (x >= m_table.size() - 1)) {
            return 0.0;
        }

        const int i = (int) x;
        const double frac = x - i;
        return m_table[i] + frac * (m_table[i + 1] - m_table[i]);
    }

    static double impulse(double t)
    {
        const double a = M_PI / 4.0;
        const double d = a * a - 4.0 * M_PI * M_PI * t * t;

        if (std::fabs(d) < 1e-9) {
            return 2.0; // limit at |t| = 1/8
        }

        return 2.0 * a * std::cos(4.0 * M_PI * t) / d;
    }
};

const SymbolShape& symbolShape()
{
    static const SymbolShape shape;
    return shape;
}

}

RDSEncoder::RDSEncoder() :
    m_sampleRate(456000),
    m_pi(0x1234),
    m_pty(0),
    m_stereo(true),
    m_bitIndex(0),
    m_bitPhase(0.0),
    m_differentialState(false),
    m_radioTextAB(false),
    m_radioTextSegments(16),
    m_radioTextSegment(0),
    m_groupIndex(0)
{
    setData(m_pi, m_pty, "SDRangel", "SDRangel Broadcast FM");
    reset();
}

QByteArray RDSEncoder::rdsText(const QString& text, int length)
{
    QByteArray result = text.toLatin1().left(length);
    result.replace('\n', ' ');
    result.replace('\r', ' ');
    result.append(QByteArray(length - result.size(), ' '));
    return result;
}

QByteArray RDSEncoder::rdsRadioText(const QString& text)
{
    QByteArray result = text.toLatin1().left(64);
    result.replace('\n', ' ');
    result.replace('\r', ' ');
    if (result.size() < 64) {
        result.append('\r');
    }
    result.append(QByteArray(64 - result.size(), ' '));
    return result;
}

void RDSEncoder::setSampleRate(int sampleRate)
{
    // The bit phase is in bit periods, so the stream continues at the new rate
    m_sampleRate = std::max(sampleRate, 1);
}

void RDSEncoder::setData(quint16 pi, quint8 pty, const QString& ps, const QString& radioText)
{
    // Groups are built from these when the previous group has been sent
    const QByteArray normalizedRadioText = rdsRadioText(radioText);
    if (normalizedRadioText != m_radioText)
    {
        if (!m_radioText.isEmpty()) {
            m_radioTextAB = !m_radioTextAB; // Tells receivers to clear the old text
        }
        m_radioText = normalizedRadioText;
        const int end = m_radioText.indexOf('\r');
        m_radioTextSegments = end < 0 ? 16U : (unsigned int) end / 4U + 1U;
        m_radioTextSegment = 0;
    }
    m_pi = pi;
    m_pty = std::min<quint8>(pty, 31);
    m_ps = rdsText(ps, 8);
}

void RDSEncoder::reset()
{
    m_bits.clear();
    m_bitIndex = 0;
    m_bitPhase = 0.0;
    m_differentialState = false;
    m_groupIndex = 0;
    m_radioTextSegment = 0;
    m_symbols.fill(0.0f);
    buildNextGroup();
    pushSymbol();
}

quint16 RDSEncoder::crc(quint16 data)
{
    quint32 reg = 0;
    const quint32 polynomial = 0x5B9;

    for (int i = 15; i >= 0; --i)
    {
        reg = (reg << 1) | ((data >> i) & 1U);
        if (reg & 0x400U) {
            reg ^= polynomial;
        }
    }

    for (int i = 0; i < 10; ++i)
    {
        reg <<= 1;
        if (reg & 0x400U) {
            reg ^= polynomial;
        }
    }

    return reg & 0x3ffU;
}

void RDSEncoder::appendBlock(quint16 data, quint16 offsetWord)
{
    quint32 block = (quint32(data) << 10) | (crc(data) ^ offsetWord);
    for (int bit = 25; bit >= 0; --bit) {
        m_bits.push_back(((block >> bit) & 1U) != 0);
    }
}

void RDSEncoder::buildNextGroup()
{
    m_bits.clear();
    m_bitIndex = 0;

    // Four PS groups followed by one RadioText group gives rapid station-name
    // acquisition while continuously refreshing the text.
    const bool psGroup = (m_groupIndex % 5U) != 4U;
    quint16 blocks[4] = {};
    blocks[0] = m_pi;

    if (psGroup)
    {
        const unsigned int segment = (m_groupIndex % 5U);
        // Decoder identification bits are sent one per segment, d3 first. Only
        // d0 (stereo) is set: no dynamic PTY, compression or artificial head.
        const quint16 di = ((segment == 3U) && m_stereo) ? 1U : 0U;
        blocks[1] = quint16((trafficProgramme << 10) | (quint16(m_pty) << 5)
                  | (trafficAnnouncement << 4) | (musicSpeech << 3) | (di << 2) | (segment & 3U));
        blocks[2] = 0xE0CD; // AF codes: 224 = no alternative frequencies, 205 = filler
        blocks[3] = (quint16(quint8(m_ps[segment * 2])) << 8)
                  | quint16(quint8(m_ps[segment * 2 + 1]));
    }
    else
    {
        // Only the segments up to the one containing the end-of-text carriage return are sent
        const unsigned int segment = m_radioTextSegment;
        m_radioTextSegment = (m_radioTextSegment + 1) % m_radioTextSegments;
        blocks[1] = quint16((2U << 12) | (trafficProgramme << 10) | (quint16(m_pty) << 5)
                  | (m_radioTextAB ? (1U << 4) : 0U) | (segment & 0xfU));
        blocks[2] = (quint16(quint8(m_radioText[segment * 4])) << 8)
                  | quint16(quint8(m_radioText[segment * 4 + 1]));
        blocks[3] = (quint16(quint8(m_radioText[segment * 4 + 2])) << 8)
                  | quint16(quint8(m_radioText[segment * 4 + 3]));
    }

    for (int i = 0; i < 4; ++i) {
        appendBlock(blocks[i], offsetWords[i]);
    }

    ++m_groupIndex;
}

bool RDSEncoder::nextDifferentialBit()
{
    if (m_bitIndex >= m_bits.size()) {
        buildNextGroup();
    }

    // RDS differential coding: a data one changes state at the bit boundary.
    if (m_bits[m_bitIndex++]) {
        m_differentialState = !m_differentialState;
    }
    return m_differentialState;
}

void RDSEncoder::pushSymbol()
{
    std::copy_backward(m_symbols.begin(), m_symbols.end() - 1, m_symbols.end());
    m_symbols[0] = nextDifferentialBit() ? 1.0f : -1.0f;
}

Real RDSEncoder::sample()
{
    const SymbolShape& shape = symbolShape();
    Real out = 0.0f;

    // Symbol j started (m_bitPhase + j) bits ago. Sum the shaped symbols that overlap.
    for (int j = 0; j < m_nbSymbols; j++)
    {
        if (m_symbols[j] != 0.0f) {
            out += m_symbols[j] * shape.symbol(m_bitPhase + j - SymbolShape::m_delay);
        }
    }

    m_bitPhase += rdsBitRate / m_sampleRate;

    if (m_bitPhase >= 1.0)
    {
        m_bitPhase -= 1.0;
        pushSymbol();
    }

    return out;
}
