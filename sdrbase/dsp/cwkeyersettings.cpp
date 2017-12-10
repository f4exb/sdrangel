///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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
#include "cwkeyersettings.h"

CWKeyerSettings::CWKeyerSettings()
{
    resetToDefaults();
}

void CWKeyerSettings::resetToDefaults()
{
    m_loop = false;
    m_mode = CWNone;
    m_sampleRate = 48000;
    m_text = "";
    m_wpm = 13;
}

QByteArray CWKeyerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeBool(2, m_loop);
    s.writeS32(3, (int) m_mode);
    s.writeS32(4, m_sampleRate);
    s.writeString(5, m_text);
    s.writeS32(6, m_wpm);

    return s.final();
}

bool CWKeyerSettings::deserialize(const QByteArray& data)
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

        d.readBool(2, &m_loop, false);
        d.readS32(3, &intval, 0);
        m_mode = (CWMode) intval;
        d.readS32(4, &m_sampleRate, 48000);
        d.readString(5, &m_text, "");
        d.readS32(6, &m_wpm, 13);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
