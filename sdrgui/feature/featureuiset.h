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

#include <QObject>
#include <QString>
#include <QList>

#include "export.h"

class QWidget;
class FeatureGUI;
class PluginAPI;
class FeatureSet;
class Feature;
class FeatureSetPreset;
class WebAPIAdapterInterface;
class Workspace;

class SDRGUI_API FeatureUISet : public QObject
{
    Q_OBJECT
public:
    FeatureUISet(int tabIndex, FeatureSet *featureSet);
    ~FeatureUISet();

    int getNumberOfFeatures() const { return m_featureInstanceRegistrations.size(); }
    void registerFeatureInstance(FeatureGUI* featureGUI, Feature *feature);
    void deleteFeature(int featureIndex);
    const Feature *getFeatureAt(int featureIndex) const;
    Feature *getFeatureAt(int featureIndex);
    const FeatureGUI *getFeatureGuiAt(int featureIndex) const;
    FeatureGUI *getFeatureGuiAt(int featureIndex);
    void loadFeatureSetSettings(
        const FeatureSetPreset* preset,
        PluginAPI *pluginAPI,
        WebAPIAdapterInterface *apiAdapter,
        QList<Workspace*> *workspaces,
        Workspace *currentWorkspace
    );
    void saveFeatureSetSettings(FeatureSetPreset* preset);
    void freeFeatures();

private:
    struct FeatureInstanceRegistration
    {
        FeatureGUI* m_gui;
        Feature* m_feature;

        FeatureInstanceRegistration() :
            m_gui(nullptr),
            m_feature(nullptr)
        { }

        FeatureInstanceRegistration(FeatureGUI* pluginGUI, Feature *feature) :
            m_gui(pluginGUI),
            m_feature(feature)
        { }

        bool operator<(const FeatureInstanceRegistration& other) const;
    };

    typedef QList<FeatureInstanceRegistration> FeatureInstanceRegistrations;

    FeatureInstanceRegistrations m_featureInstanceRegistrations;
    int m_featureTabIndex;
    FeatureSet *m_featureSet;


private slots:
    void handleClosingFeatureGUI(FeatureGUI *featureGUI);
    void handleDeleteFeature(Feature *feature);

};

#endif // SDRGUI_FEATURE_FEATUREUISET_H_
