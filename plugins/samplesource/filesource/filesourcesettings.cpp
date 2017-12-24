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

#include "filesourcesettings.h"

FileSourceSettings::FileSourceSettings()
{
    resetToDefaults();
}

void FileSourceSettings::resetToDefaults()
{
    m_centerFrequency = 435000000;
    m_sampleRate = 48000;
    m_fileName = "./test.sdriq";
}

QByteArray FileSourceSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeString(1, m_fileName);
    return s.final();
}

bool FileSourceSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1) {
        d.readString(1, &m_fileName, "./test.sdriq");
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}




