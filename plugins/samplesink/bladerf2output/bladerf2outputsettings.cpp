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

#include "bladerf2outputsettings.h"

#include <QtGlobal>
#include "util/simpleserializer.h"


BladeRF2OutputSettings::BladeRF2OutputSettings()
{
    resetToDefaults();
}

void BladeRF2OutputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_devSampleRate = 3072000;
    m_bandwidth = 1500000;
    m_gainMode = 0;
    m_globalGain = 0;
    m_biasTee = false;
    m_log2Interp = 0;
}

QByteArray BladeRF2OutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_devSampleRate);
    s.writeS32(2, m_bandwidth);
    s.writeS32(3, m_gainMode);
    s.writeS32(4, m_globalGain);
    s.writeBool(5, m_biasTee);
    s.writeU32(6, m_log2Interp);

    return s.final();
}

bool BladeRF2OutputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readS32(1, &m_devSampleRate);
        d.readS32(2, &m_bandwidth);
        d.readS32(3, &m_gainMode);
        d.readS32(4, &m_globalGain);
        d.readBool(5, &m_biasTee);
        d.readU32(6, &m_log2Interp);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}




