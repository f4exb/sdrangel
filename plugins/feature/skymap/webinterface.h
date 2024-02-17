///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_FEATURE_WEBINTERFACE_H_
#define INCLUDE_FEATURE_WEBINTERFACE_H_

#include <QGeoCoordinate>

#include "websocketserver.h"

// Interface between C++ code and Web page via Web Socket using JSON
class WebInterface : public WebSocketServer
{
public:

    WebInterface(QObject *parent = nullptr);
    void setView(double ra, double dec, float zoom=1.0f);
    void setPosition(const QGeoCoordinate& position);
    void setDateTime(QDateTime dateTime);
    void track(const QString &name);
    void setBackground(const QString& name);
    void setProjection(const QString& name);
    void showConstellations(bool show);
    void showReticle(bool show);
    void showGrid(bool show);
    void showAntennaFoV(bool show);
    void showNames(bool show);
    void setAntennaFoV(float hpbw);
    void setWWTSettings(const QHash<QString, QVariant>& settings);

};

#endif // INCLUDE_FEATURE_WEBINTERFACE_H_
