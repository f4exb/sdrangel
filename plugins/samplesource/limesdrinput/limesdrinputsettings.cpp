///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "limesdrinputsettings.h"

LimeSDRInputSettings::LimeSDRInputSettings()
{
    resetToDefaults();
}

void LimeSDRInputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 6000000;
    m_log2HardDecim = 2;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_log2SoftDecim = 0;
    m_fcPos = FC_POS_CENTER;
    m_lpfBW = 1.5e6f;
    m_lpfFIREnable = false;
    m_lpfFIRBW = 1.5e6f;
    m_gain = 0;
}

QByteArray LimeSDRInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeU32(2, m_log2HardDecim);
    s.writeBool(3, m_dcBlock);
    s.writeBool(4, m_iqCorrection);
    s.writeU32(5, m_log2SoftDecim);
    s.writeS32(6, (int) m_fcPos);
    s.writeFloat(7, m_lpfBW);
    s.writeBool(8, m_lpfFIREnable);
    s.writeFloat(9, m_lpfFIRBW);
    s.writeU32(10, m_gain);

    return s.final();
}

bool LimeSDRInputSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_devSampleRate, 3072000);
        d.readU32(2, &m_log2HardDecim, 2);
        d.readBool(3, &m_dcBlock, false);
        d.readBool(4, &m_iqCorrection, false);
        d.readU32(5, &m_log2SoftDecim, 0);
        d.readS32(6, &intval, 0);
        m_fcPos = (fcPos_t) intval;
        d.readFloat(7, &m_lpfBW, 1.5e6);
        d.readBool(8, &m_lpfFIREnable, false);
        d.readFloat(9, &m_lpfFIRBW, 1.5e6);
        d.readU32(10, &m_gain, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }

}


