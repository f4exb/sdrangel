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

#ifndef SDRBASE_SETTINGS_FEATURESETSETTINGS_H_
#define SDRBASE_SETTINGS_FEATURESETSETTINGS_H_

#include <QString>
#include <QByteArray>
#include <QList>
#include <QMetaType>

#include "export.h"

class SDRBASE_API FeatureSetPreset {
public:
	struct FeatureConfig {
		QString m_featureIdURI; //!< Channel type ID in URI form
		QByteArray m_config;

		FeatureConfig(const QString& featureIdURI, const QByteArray& config) :
			m_featureIdURI(featureIdURI),
			m_config(config)
		{ }
	};
	typedef QList<FeatureConfig> FeatureConfigs;

	FeatureSetPreset();
	FeatureSetPreset(const FeatureSetPreset& other);
	void resetToDefaults();

	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setGroup(const QString& group) { m_group = group; }
	const QString& getGroup() const { return m_group; }
	void setDescription(const QString& description) { m_description = description; }
	const QString& getDescription() const { return m_description; }

	void clearFeatures() { m_featureConfigs.clear(); }
	void addFeature(const QString& feature, const QByteArray& config) { m_featureConfigs.append(FeatureConfig(feature, config)); }
	int getFeatureCount() const { return m_featureConfigs.count(); }
	const FeatureConfig& getFeatureConfig(int index) const { return m_featureConfigs.at(index); }

	static bool presetCompare(const FeatureSetPreset *p1, FeatureSetPreset *p2)
	{
	    if (p1->m_group != p2->m_group)
	    {
	        return p1->m_group < p2->m_group;
	    }
	    else
	    {
            return p1->m_description < p2->m_description;
	    }
	}

private:
    // group and preset description
	QString m_group;
	QString m_description;
	// features and configurations
	FeatureConfigs m_featureConfigs;
};

Q_DECLARE_METATYPE(const FeatureSetPreset*)
Q_DECLARE_METATYPE(FeatureSetPreset*)

#endif // SDRBASE_SETTINGS_FEATURESETSETTINGS_H_
