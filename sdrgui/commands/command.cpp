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

#include "command.h"
#include "util/simpleserializer.h"

Command::Command()
{
    resetToDefaults();
}

Command::~Command()
{}

void Command::resetToDefaults()
{
    m_group = "default";
    m_description = "no name";
    m_command = "";
    m_argString = "";
    m_pressKey = static_cast<Qt::Key>(0);
    m_releaseKey = static_cast<Qt::Key>(0);
}

QByteArray Command::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_group);
    s.writeString(2, m_description);
    s.writeString(3, m_command);
    s.writeString(4, m_argString);
    s.writeS32(5, (int) m_pressKey);
    s.writeS32(6, (int) m_releaseKey);

    return s.final();
}


bool Command::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        int tmpInt;

        d.readString(1, &m_group, "default");
        d.readString(2, &m_description, "no name");
        d.readString(3, &m_command, "");
        d.readString(4, &m_argString, "");
        d.readS32(5, &tmpInt, 0);
        m_pressKey = static_cast<Qt::Key>(tmpInt);
        d.readS32(6, &tmpInt, 0);
        m_releaseKey = static_cast<Qt::Key>(tmpInt);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
