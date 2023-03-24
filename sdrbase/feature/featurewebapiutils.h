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

#ifndef SDRBASE_FEATURE_FEATUREWEBAPIUTILS_H_
#define SDRBASE_FEATURE_FEATUREWEBAPIUTILS_H_

#include <QDateTime>

#include "export.h"

class Feature;

class SDRBASE_API FeatureWebAPIUtils
{
public:
    static bool mapFind(const QString& target, int featureSetIndex=-1, int featureIndex=-1);
    static bool mapSetDateTime(const QDateTime& dateTime, int featureSetIndex=-1, int featureIndex=-1);
    static Feature *getFeature(int& featureSetIndex, int& featureIndex, const QString& uri);
    static bool satelliteAOS(const QString name, const QDateTime aos, const QDateTime los);
    static bool satelliteLOS(const QString name);
};

#endif // SDRBASE_FEATURE_FEATUREWEBAPIUTILS_H_
