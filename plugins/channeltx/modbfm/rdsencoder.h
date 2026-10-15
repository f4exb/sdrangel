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

#ifndef PLUGINS_CHANNELTX_MODBFM_RDSENCODER_H_
#define PLUGINS_CHANNELTX_MODBFM_RDSENCODER_H_

#include <array>
#include <vector>

#include <QString>

#include "dsp/dsptypes.h"

/** Generates the shaped baseband component used to DSB-SC modulate the RDS
 * 57 kHz subcarrier. Groups 0A (PS) and 2A (RadioText) are interleaved.
 * Output peak amplitude is at most 1.
 */
class RDSEncoder
{
public:
    RDSEncoder();

    void setSampleRate(int sampleRate);
    /** Changes take effect from the next group, without interrupting the bit stream,
     * so receivers keep block synchronisation.
     */
    void setData(quint16 pi, quint8 pty, const QString& ps, const QString& radioText);
    void setStereo(bool stereo) { m_stereo = stereo; }
    Real sample();
    void reset();

    static const int m_nbSymbols = 6; //!< Number of overlapping shaped symbols contributing to each sample

private:
    int m_sampleRate;
    quint16 m_pi;
    quint8 m_pty;
    bool m_stereo;
    QByteArray m_ps;
    QByteArray m_radioText;
    std::vector<bool> m_bits;
    unsigned int m_bitIndex;
    double m_bitPhase;
    bool m_differentialState;
    bool m_radioTextAB;
    unsigned int m_radioTextSegments; //!< Number of 4 character RadioText segments to send
    unsigned int m_radioTextSegment;  //!< Next RadioText segment to send
    unsigned int m_groupIndex;
    std::array<Real, m_nbSymbols> m_symbols; //!< Newest first: +1/-1 per biphase symbol, 0 before start

    static quint16 crc(quint16 data);
    static QByteArray rdsText(const QString& text, int length);
    static QByteArray rdsRadioText(const QString& text);
    void appendBlock(quint16 data, quint16 offsetWord);
    void buildNextGroup();
    bool nextDifferentialBit();
    void pushSymbol();
};

#endif // PLUGINS_CHANNELTX_MODBFM_RDSENCODER_H_
