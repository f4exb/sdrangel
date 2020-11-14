///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#include "featuresetpreset.h"

FeatureSetPreset::FeatureSetPreset()
{
	resetToDefaults();
}

FeatureSetPreset::FeatureSetPreset(const FeatureSetPreset& other) :
	m_group(other.m_group),
	m_description(other.m_description),
	m_featureConfigs(other.m_featureConfigs)
{}

void FeatureSetPreset::resetToDefaults()
{
	m_group = "default";
	m_description = "no name";
	m_featureConfigs.clear();
}

QByteArray FeatureSetPreset::serialize() const
{
	SimpleSerializer s(1);

	s.writeString(1, m_group);
	s.writeString(2, m_description);
	s.writeS32(100, m_featureConfigs.size());

	for(int i = 0; i < m_featureConfigs.size(); i++)
	{
		s.writeString(101 + i * 2, m_featureConfigs[i].m_featureIdURI);
		s.writeBlob(102 + i * 2, m_featureConfigs[i].m_config);
	}

	return s.final();
}

bool FeatureSetPreset::deserialize(const QByteArray& data)
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

        qint32 featureCount;
        d.readS32(100, &featureCount, 0);

		m_featureConfigs.clear();

		for (int i = 0; i < featureCount; i++)
		{
			QString feature;
			QByteArray config;

			d.readString(101 + i * 2, &feature, "unknown-feature");
			d.readBlob(102 + i * 2, &config);
			m_featureConfigs.append(FeatureConfig(feature, config));
		}

		return true;
	}
	else
	{
		resetToDefaults();
		return false;
	}
}
