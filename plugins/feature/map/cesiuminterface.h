///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FEATURE_CESIUMINTERFACE_H_
#define INCLUDE_FEATURE_CESIUMINTERFACE_H_

#include "mapwebsocketserver.h"
#include "czml.h"
#include "SWGMapAnimation.h"

class ObjectMapItem;
class PolygonMapItem;
class PolylineMapItem;

class CesiumInterface : public MapWebSocketServer
{
public:

    struct Animation {
        Animation(SWGSDRangel::SWGMapAnimation *swgAnimation)
        {
            m_name = *swgAnimation->getName();
            m_startDateTime = *swgAnimation->getStartDateTime();
            m_reverse = swgAnimation->getReverse();
            m_loop =  swgAnimation->getLoop();
            m_stop =  swgAnimation->getStop();
            m_startOffset =  swgAnimation->getStartOffset();
            m_duration =  swgAnimation->getDuration();
            m_multiplier =  swgAnimation->getMultiplier();
        }

        QString m_name;
        QString m_startDateTime; // No need to convert to QDateTime, as we don't use it in c++
        bool m_reverse;
        bool m_loop;
        bool m_stop;            // Stop looped animation
        float m_delay;          // Delay in seconds before animation starts
        float m_startOffset;    // [0..1] What point to start playing animation
        float m_duration;       // How long to play animation for
        float m_multiplier;     // Speed to play animation at
    };

    CesiumInterface(const MapSettings *settings, QObject *parent = nullptr);
    void setHomeView(float latitude, float longitude, float angle=1.0f);
    void setView(float latitude, float longitude, float altitude=60000);
    void playAnimation(const QString &name, Animation *animation);
    void setDateTime(QDateTime dateTime);
    void getDateTime();
    void track(const QString &name);
    void setTerrain(const QString &terrain, const QString &maptilerAPIKey);
    void setBuildings(const QString &buildings);
    void setCameraReferenceFrame(bool eci);
    void setSunLight(bool useSunLight);
    void setAntiAliasing(const QString &antiAliasing);
    void showMUF(bool show);
    void showfoF2(bool show);
    void updateImage(const QString &name, float east, float west, float north, float south, float altitude, const QString &data);
    void removeImage(const QString &name);
    void removeAllImages();
    void removeAllCZMLEntities();
    void initCZML();
    void czml(QJsonObject &obj);
    void update(ObjectMapItem *mapItem, bool isTarget, bool isSelected);
    void update(PolygonMapItem *mapItem);
    void update(PolylineMapItem *mapItem);
    void setPosition(const QGeoCoordinate& position);

protected:

    CZML m_czml;
};

#endif // INCLUDE_FEATURE_CESIUMINTERFACE_H_
