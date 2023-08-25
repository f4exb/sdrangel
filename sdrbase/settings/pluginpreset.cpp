///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include "pluginpreset.h"

PluginPreset::PluginPreset()
{
    resetToDefaults();
}

PluginPreset::PluginPreset(const PluginPreset& other) :
    m_group(other.m_group),
    m_description(other.m_description),
    m_pluginIdURI(other.m_pluginIdURI),
    m_config(other.m_config)
{}

void PluginPreset::resetToDefaults()
{
    m_group = "default";
    m_description = "no name";
    m_pluginIdURI = "";
    m_config = QByteArray();
}

QByteArray PluginPreset::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(1, m_group);
    s.writeString(2, m_description);
    s.writeString(3, m_pluginIdURI);
    s.writeBlob(4, m_config);

    return s.final();
}

bool PluginPreset::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        d.readString(1, &m_group, "default");
        d.readString(2, &m_description, "no name");
        d.readString(3, &m_pluginIdURI, "");
        d.readBlob(4, &m_config);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
