///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "fileoutputsettings.h"

FileOutputSettings::FileOutputSettings()
{
    resetToDefaults();
}

void FileOutputSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_sampleRate = 48000;
    m_log2Interp = 0;
}

QByteArray FileOutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU64(1, m_sampleRate);
    s.writeU32(2, m_log2Interp);

    return s.final();
}

bool FileOutputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readU64(1, &m_sampleRate, 48000);
        d.readU32(2, &m_log2Interp, 0);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
