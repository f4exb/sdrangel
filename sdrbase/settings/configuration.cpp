///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
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
#include "configuration.h"

Configuration::Configuration()
{
    resetToDefaults();
}

Configuration::~Configuration()
{ }

void Configuration::resetToDefaults()
{
	m_group = "default";
	m_description = "no name";
    m_workspaceGeometries.clear();
}

QByteArray Configuration::serialize() const
{
	SimpleSerializer s(1);

	s.writeString(1, m_group);
	s.writeString(2, m_description);
    QByteArray b = m_featureSetPreset.serialize();
    s.writeBlob(3, b);

    s.writeS32(100, m_workspaceGeometries.size());

	for(int i = 0; i < m_workspaceGeometries.size(); i++) {
		s.writeBlob(101 + i, m_workspaceGeometries[i]);
	}

	return s.final();
}

bool Configuration::deserialize(const QByteArray& data)
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

        QByteArray b;
        d.readBlob(3, &b);
        m_featureSetPreset.deserialize(b);

        int nbWorkspaces;
        d.readS32(100, &nbWorkspaces, 0);

        for(int i = 0; i < nbWorkspaces; i++)
        {
            m_workspaceGeometries.push_back(QByteArray());
            d.readBlob(101 + i, &m_workspaceGeometries.back());
        }

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}

int Configuration::getNumberOfWorkspaces() const
{
    return m_workspaceGeometries.size();
}

void Configuration::clearData()
{
    m_featureSetPreset.clearFeatures();
    m_workspaceGeometries.clear();
}
