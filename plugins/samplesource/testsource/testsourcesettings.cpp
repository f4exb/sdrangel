///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#include <QtGlobal>
#include "util/simpleserializer.h"
#include "testsourcesettings.h"

TestSourceSettings::TestSourceSettings()
{
    resetToDefaults();
}

void TestSourceSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_frequencyShift = 0;
    m_sampleRate = 768*1000;
    m_log2Decim = 4;
    m_fcPos = FC_POS_CENTER;
    m_sampleSizeIndex = 0;
    m_amplitudeBits = 127;
    m_dcBlock = false;
    m_iqImbalance = false;
    m_dcFactor = 0.0f;
    m_iFactor = 0.0f;
    m_qFactor = 0.0f;
}

QByteArray TestSourceSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(2, m_frequencyShift);
    s.writeU32(3, m_sampleRate);
    s.writeU32(4, m_log2Decim);
    s.writeS32(5, (int) m_fcPos);
    s.writeU32(6, m_sampleSizeIndex);
    s.writeS32(7, m_amplitudeBits);
    s.writeBool(8, m_dcBlock);
    s.writeBool(9, m_iqImbalance);
    s.writeFloat(10, m_dcFactor);
    s.writeFloat(11, m_iFactor);
    s.writeFloat(12, m_qFactor);

    return s.final();
}

bool TestSourceSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int intval;

        d.readS32(2, &m_frequencyShift, 0);
        d.readU32(3, &m_sampleRate, 768*1000);
        d.readU32(4, &m_log2Decim, 4);
        d.readS32(5, &intval, 0);
        m_fcPos = (fcPos_t) intval;
        d.readU32(6, &m_sampleSizeIndex, 0);
        d.readS32(7, &m_amplitudeBits, 128);
        d.readBool(8, &m_dcBlock, false);
        d.readBool(9, &m_iqImbalance, false);
        d.readFloat(10, &m_dcFactor, 0.0f);
        d.readFloat(11, &m_iFactor, 0.0f);
        d.readFloat(12, &m_qFactor, 0.0f);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}






