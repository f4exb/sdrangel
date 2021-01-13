///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "SWGFeatureActions.h"
#include "SWGMapActions.h"

#include "maincore.h"
#include "feature/featureset.h"
#include "feature/feature.h"
#include "featurewebapiutils.h"

// Find the specified target on the map
bool FeatureWebAPIUtils::mapFind(const QString& target, int featureSetIndex, int featureIndex)
{
    Feature *feature = FeatureWebAPIUtils::getFeature(featureSetIndex, featureIndex, "sdrangel.feature.map");
    if (feature != nullptr)
    {
        QString errorMessage;
        QStringList featureActionKeys = {"find"};
        SWGSDRangel::SWGFeatureActions query;
        SWGSDRangel::SWGMapActions *mapActions = new SWGSDRangel::SWGMapActions();

        mapActions->setFind(new QString(target));
        query.setMapActions(mapActions);

        int httpRC = feature->webapiActionsPost(featureActionKeys, query, errorMessage);
        if (httpRC/100 != 2)
        {
            qWarning("FeatureWebAPIUtils::mapFind: error %d: %s", httpRC, errorMessage);
            return false;
        }

        return true;
    }
    else
    {
        qWarning("FeatureWebAPIUtils::mapFind: no Map feature");
        return false;
    }
}

// Get first feature with the given URI
Feature* FeatureWebAPIUtils::getFeature(int featureSetIndex, int featureIndex, const QString& uri)
{
    FeatureSet *featureSet;
    Feature *feature;
    std::vector<FeatureSet*>& featureSets = MainCore::instance()->getFeatureeSets();

    if (featureSetIndex != -1)
    {
        // Find feature with specific index
        if (featureSetIndex < featureSets.size())
        {
            featureSet = featureSets[featureSetIndex];
            if (featureIndex < featureSet->getNumberOfFeatures())
            {
                 feature = featureSet->getFeatureAt(featureIndex);
                 if (uri.isEmpty() || feature->getURI() == uri)
                     return feature;
                 else
                     return nullptr;
            }
            else
                return nullptr;
        }
        else
            return nullptr;
    }
    else
    {
        // Find first feature matching URI
        for (std::vector<FeatureSet*>::const_iterator it = featureSets.begin(); it != featureSets.end(); ++it, featureIndex++)
        {
            for (int fi = 0; fi < (*it)->getNumberOfFeatures(); fi++)
            {
                feature = (*it)->getFeatureAt(fi);
                if (feature->getURI() == uri)
                {
                    return feature;
                }
            }
        }
        return nullptr;
    }
}
