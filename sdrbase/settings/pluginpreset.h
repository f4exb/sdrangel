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

#ifndef SDRBASE_SETTINGS_PLUGINPRESET_H_
#define SDRBASE_SETTINGS_PLUGINPRESET_H_

#include <QString>
#include <QByteArray>
#include <QMetaType>

#include "export.h"

class SDRBASE_API PluginPreset {
public:

    PluginPreset();
    PluginPreset(const PluginPreset& other);
    void resetToDefaults();

    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void setGroup(const QString& group) { m_group = group; }
    const QString& getGroup() const { return m_group; }
    void setDescription(const QString& description) { m_description = description; }
    const QString& getDescription() const { return m_description; }
    void setConfig(const QString& pluginIdURI, const QByteArray& config) { m_pluginIdURI = pluginIdURI; m_config = config; }
    const QByteArray& getConfig() const { return m_config; }
    const QString& getPluginIdURI() const { return m_pluginIdURI; }

    static bool presetCompare(const PluginPreset *p1, PluginPreset *p2)
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
    QString m_pluginIdURI; //!< Plugin type ID in URI form
    QByteArray m_config;

};

Q_DECLARE_METATYPE(const PluginPreset*)
Q_DECLARE_METATYPE(PluginPreset*)

#endif // SDRBASE_SETTINGS_PLUGINPRESET_H_
