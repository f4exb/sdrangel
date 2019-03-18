///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 F4EXB                                                      //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QColor>
#include <QDebug>

#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "datvdemodsettings.h"

DATVDemodSettings::DATVDemodSettings() :
    m_channelMarker(0)
{
    resetToDefaults();
}

void DATVDemodSettings::resetToDefaults()
{
    m_rgbColor = QColor(Qt::magenta).rgb();
    m_title = "DATV Demodulator";
    m_rfBandwidth = 512000;
    m_centerFrequency = 0;
    m_standard = DVB_S;
    m_modulation = BPSK;
    m_fec = leansdr::FEC12;
    m_symbolRate = 250000;
    m_notchFilters = 1;
    m_allowDrift = false;
    m_fastLock = false;
    m_filter = SAMP_LINEAR;
    m_hardMetric = false;
    m_rollOff = 0.35;
    m_viterbi = false;
    m_excursion = 10;
}

QByteArray DATVDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(2, m_rfBandwidth);
    s.writeS32(3, m_centerFrequency);
    s.writeS32(4, (int) m_standard);
    s.writeS32(5, (int) m_modulation);

    if (m_channelMarker) {
        s.writeBlob(6, m_channelMarker->serialize());
    }

    s.writeU32(7, m_rgbColor);
    s.writeString(8, m_title);
    s.writeS32(9, (int) m_fec);
    s.writeS32(11, m_symbolRate);
    s.writeS32(12, m_notchFilters);
    s.writeBool(13, m_allowDrift);
    s.writeBool(14, m_fastLock);
    s.writeS32(15, (int) m_filter);
    s.writeBool(16, m_hardMetric);
    s.writeFloat(17, m_rollOff);
    s.writeBool(18, m_viterbi);
    s.writeS32(19, m_excursion);

    return s.final();
}

bool DATVDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        qint32 tmp;
        QString strtmp;

        d.readS32(2, &m_rfBandwidth, 512000);
        d.readS32(3, &m_centerFrequency, 0);

        d.readS32(4, &tmp, (int) DVB_S);
        tmp = tmp < 0 ? 0 : tmp > (int) DVB_S2 ? (int) DVB_S2 : tmp;
        m_standard = (dvb_version) tmp;

        d.readS32(5, &tmp, (int) BPSK);
        tmp = tmp < 0 ? 0 : tmp > (int) QAM256 ? (int) QAM256 : tmp;
        m_modulation = (DATVModulation) tmp;

        d.readBlob(6, &bytetmp);

        if (m_channelMarker) {
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(7, &m_rgbColor, QColor(Qt::magenta).rgb());
        d.readString(8, &m_title, "DATV Demodulator");

        d.readS32(9, &tmp, (int) leansdr::code_rate::FEC12);
        tmp = tmp < 0 ? 0 : tmp >= (int) leansdr::code_rate::FEC_COUNT ? (int) leansdr::code_rate::FEC_COUNT - 1 : tmp;
        m_fec = (leansdr::code_rate) tmp;

        d.readS32(11, &m_symbolRate, 250000);
        d.readS32(12, &m_notchFilters, 1);
        d.readBool(13, &m_allowDrift, false);
        d.readBool(14, &m_fastLock, false);

        d.readS32(15, &tmp, (int) SAMP_LINEAR);
        tmp = tmp < 0 ? 0 : tmp > (int) SAMP_RRC ? (int) SAMP_RRC : tmp;
        m_filter = (dvb_sampler) tmp;

        d.readBool(16, &m_hardMetric, false);
        d.readFloat(17, &m_rollOff, 0.35);
        d.readBool(18, &m_viterbi, false);
        d.readS32(19, &m_excursion, 10);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DATVDemodSettings::debug(const QString& msg) const
{
    qDebug() << msg
        << " m_allowDrift: " << m_allowDrift
        << " m_rfBandwidth: " << m_rfBandwidth
        << " m_centerFrequency: " << m_centerFrequency
        << " m_fastLock: " << m_fastLock
        << " m_hardMetric: " << m_hardMetric
        << " m_filter: " << m_filter
        << " m_rollOff: " << m_rollOff
        << " m_viterbi: " << m_viterbi
        << " m_fec: " << m_fec
        << " m_modulation: " << m_modulation
        << " m_standard: " << m_standard
        << " m_notchFilters: " << m_notchFilters
        << " m_symbolRate: " << m_symbolRate
        << " m_excursion: " << m_excursion;
}

bool DATVDemodSettings::isDifferent(const DATVDemodSettings& other)
{
    return ((m_allowDrift != other.m_allowDrift)
        || (m_rfBandwidth != other.m_rfBandwidth)
        || (m_centerFrequency != other.m_centerFrequency)
        || (m_fastLock != other.m_fastLock)
        || (m_hardMetric != other.m_hardMetric)
        || (m_filter != other.m_filter)
        || (m_rollOff != other.m_rollOff)
        || (m_viterbi != other.m_viterbi)
        || (m_fec != other.m_fec)
        || (m_modulation != other.m_modulation)
        || (m_standard != other.m_standard)
        || (m_notchFilters != other.m_notchFilters)
        || (m_symbolRate != other.m_symbolRate)
        || (m_excursion != other.m_excursion));
}