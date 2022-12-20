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

#ifndef INCLUDE_CONFIGURATION_H
#define INCLUDE_CONFIGURATION_H

#include <QList>
#include <QByteArray>
#include <QString>
#include <QMetaType>

#include "featuresetpreset.h"
#include "preset.h"
#include "export.h"

class SDRBASE_API WorkspaceConfiguration {
public:
};

class SDRBASE_API Configuration {
public:
    Configuration();
    ~Configuration();
    void resetToDefaults();

	QByteArray serialize() const;
	bool deserialize(const QByteArray& data);

	void setGroup(const QString& group) { m_group = group; }
	const QString& getGroup() const { return m_group; }
	void setDescription(const QString& description) { m_description = description; }
	const QString& getDescription() const { return m_description; }

    int getNumberOfWorkspaceGeometries() const { return m_workspaceGeometries.size(); }
    QList<QByteArray>& getWorkspaceGeometries() { return m_workspaceGeometries; }
    const QList<QByteArray>& getWorkspaceGeometries() const { return m_workspaceGeometries; }
    QList<bool>& getWorkspaceAutoStackOptions() { return m_workspaceAutoStackOptions; }
    const QList<bool>& getWorkspaceAutoStackOptions() const { return m_workspaceAutoStackOptions; }
    QList<bool>& getWorkspaceTabSubWindowsOptions() { return m_workspaceTabSubWindowsOptions; }
    const QList<bool>& getWorkspaceTabSubWindowsOptions() const { return m_workspaceTabSubWindowsOptions; }
    FeatureSetPreset& getFeatureSetPreset() { return m_featureSetPreset; }
    const FeatureSetPreset& getFeatureSetPreset() const { return m_featureSetPreset; }
    QList<Preset>& getDeviceSetPresets() { return m_deviceSetPresets; }
    const QList<Preset>& getDeviceSetPresets() const { return m_deviceSetPresets; }
    int getNumberOfDeviceSetPresets() const { return m_deviceSetPresets.size(); }
    void clearData();

	static bool configCompare(const Configuration *p1, Configuration *p2)
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
	QString m_group;
	QString m_description;
    QList<QByteArray> m_workspaceGeometries;
    QList<bool> m_workspaceAutoStackOptions;
    QList<bool> m_workspaceTabSubWindowsOptions;
    FeatureSetPreset m_featureSetPreset;
    QList<Preset> m_deviceSetPresets;
};

Q_DECLARE_METATYPE(const Configuration*)
Q_DECLARE_METATYPE(Configuration*)

#endif // INCLUDE_CONFIGURATION_H
