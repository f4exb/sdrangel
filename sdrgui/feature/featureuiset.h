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

#ifndef SDRGUI_FEATURE_FEATUREUISET_H_
#define SDRGUI_FEATURE_FEATUREUISET_H_

#include <QString>
#include <QList>

#include "export.h"

class QWidget;
class FeatureWindow;
class PluginInstanceGUI;

class SDRGUI_API FeatureUISet
{
public:
    FeatureUISet(int tabIndex);
    ~FeatureUISet();

    void addRollupWidget(QWidget *widget); //!< Add feature rollup widget to feature window
    int getNumberOfFeatures() const { return m_featureInstanceRegistrations.size(); }
    void registerFeatureInstance(const QString& featureName, PluginInstanceGUI* pluginGUI);
    void removeFeatureInstance(PluginInstanceGUI* pluginGUI);
    void freeFeatures();
    void deleteFeature(int featureIndex);

    FeatureWindow *m_featureWindow;

private:
    struct FeatureInstanceRegistration
    {
        QString m_featureName;
        PluginInstanceGUI* m_gui;

        FeatureInstanceRegistration() :
            m_featureName(),
            m_gui(nullptr)
        { }

        FeatureInstanceRegistration(const QString& featureName, PluginInstanceGUI* pluginGUI) :
            m_featureName(featureName),
            m_gui(pluginGUI)
        { }

        bool operator<(const FeatureInstanceRegistration& other) const;
    };

    typedef QList<FeatureInstanceRegistration> FeatureInstanceRegistrations;

    FeatureInstanceRegistrations m_featureInstanceRegistrations;
    int m_featureTabIndex;

    void renameFeatureInstances();
};

#endif // SDRGUI_FEATURE_FEATUREUISET_H_
