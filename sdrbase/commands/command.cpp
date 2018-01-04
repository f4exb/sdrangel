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

#include <QKeySequence>

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
    m_key = static_cast<Qt::Key>(0);
    m_associateKey = false;
    m_release = false;
}

QByteArray Command::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_group);
    s.writeString(2, m_description);
    s.writeString(3, m_command);
    s.writeString(4, m_argString);
    s.writeS32(5, (int) m_key);
    s.writeS32(6, (int) m_keyModifiers);
    s.writeBool(7, m_associateKey);
    s.writeBool(8, m_release);

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
        m_key = static_cast<Qt::Key>(tmpInt);
        d.readS32(6, &tmpInt, 0);
        m_keyModifiers = static_cast<Qt::KeyboardModifiers>(tmpInt);
        d.readBool(7, &m_associateKey, false);
        d.readBool(8, &m_release, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

QString Command::getKeyLabel() const
{
    if (m_key == 0)
    {
       return "";
    }
    else if (m_keyModifiers != Qt::NoModifier)
    {
        QString altGrStr = m_keyModifiers & Qt::GroupSwitchModifier ? "Gr " : "";
        int maskedModifiers = (m_keyModifiers & 0x3FFFFFFF) + ((m_keyModifiers & 0x40000000)>>3);
        return altGrStr + QKeySequence(maskedModifiers, m_key).toString();
    }
    else
    {
        return QKeySequence(m_key).toString();
    }
}
