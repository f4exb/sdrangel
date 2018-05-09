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
    m_autoCorrOptions = AutoCorrNone;
    m_modulation = ModulationNone;
    m_modulationTone = 44; // 440 Hz
    m_amModulation = 50; // 50%
    m_fmDeviation = 50; // 5 kHz
    m_dcFactor = 0.0f;
    m_iFactor = 0.0f;
    m_qFactor = 0.0f;
    m_phaseImbalance = 0.0f;
    m_fileRecordName = "";
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
    s.writeS32(8, (int) m_autoCorrOptions);
    s.writeFloat(10, m_dcFactor);
    s.writeFloat(11, m_iFactor);
    s.writeFloat(12, m_qFactor);
    s.writeFloat(13, m_phaseImbalance);
    s.writeS32(14, (int) m_modulation);
    s.writeS32(15, m_modulationTone);
    s.writeS32(16, m_amModulation);
    s.writeS32(17, m_fmDeviation);

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
        d.readS32(8, &intval, 0);

        if (intval < 0 || intval > (int) AutoCorrLast) {
            m_autoCorrOptions = AutoCorrNone;
        } else {
            m_autoCorrOptions = (AutoCorrOptions) intval;
        }

        d.readFloat(10, &m_dcFactor, 0.0f);
        d.readFloat(11, &m_iFactor, 0.0f);
        d.readFloat(12, &m_qFactor, 0.0f);
        d.readFloat(13, &m_phaseImbalance, 0.0f);
        d.readS32(14, &intval, 0);

        if (intval < 0 || intval > (int) ModulationLast) {
            m_modulation = ModulationNone;
        } else {
            m_modulation = (Modulation) intval;
        }

        d.readS32(15, &m_modulationTone, 44);
        d.readS32(16, &m_amModulation, 50);
        d.readS32(17, &m_fmDeviation, 50);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}






