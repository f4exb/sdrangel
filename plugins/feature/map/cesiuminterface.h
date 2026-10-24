///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <QJsonArray>

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
            if (swgAnimation->getSound()) {
                m_sound = *swgAnimation->getSound();
            }
        }

        QString m_name;
        QString m_startDateTime; // No need to convert to QDateTime, as we don't use it in c++
        bool m_reverse;
        bool m_loop;
        bool m_stop;            // Stop looped animation
        float m_startOffset;    // [0..1] What point to start playing animation
        float m_duration;       // How long to play animation for
        float m_multiplier;     // Speed to play animation at
        QString m_sound;        // Audio file to play when the animation starts. Empty for none
    };

    CesiumInterface(const MapSettings *settings, QObject *parent = nullptr);
    void setHomeView(float latitude, float longitude, float angle=1.0f);
    void setView(float latitude, float longitude, float altitude=60000);
    void setCameraMode(int mode);
    void playAnimation(const QString &name, Animation *animation);
    void setDateTime(QDateTime dateTime);
    void getDateTime();
    void track(const QString &name);
    void setDefaultImagery(const QString &imagery);
    void setTerrain(const QString &terrain, const QString &maptilerAPIKey, bool lighting, bool water);
    void setBuildings(const QString &buildings);
    void setGooglePhotorealistic3DTiles(bool enabled);
    void setCameraReferenceFrame(bool eci);
    void setLighting(bool useSunLight, float cameraLightIntensity, float distantLightIntensity);
    void setLowDetailModels(bool enabled);
    void setAntiAliasing(bool fxaa, int msaa);
    void setHDR(bool enabled);
    void setFog(bool enabled);
    void showFPS(bool show);
    void setFrameRate(int frameRate);
    void showPFD(bool show);
    void showND(bool show);
    void showEFIS(bool show);
    void showRMP(bool show);
    void showDCDU(bool show);
    void setRMPState(const QJsonObject& state);
    void setPanelGeometry(const QJsonObject& panels);
    void setEFISState(const QJsonObject& state);
    void setNDAirports(const QJsonArray& airports);
    void setNDNavAids(const QJsonArray& navAids);
    void setAudioVolume(int mapItemVolume, int cockpitVolume);
    void showMUF(bool show);
    void showfoF2(bool show);
    void showMagDec(bool show);
    void showMaidenheadGrid(bool show);
    void showLayer(const QString& layer, bool show);
    void setLayerSettings(const QString& layer, const QStringList& settings, const QList<QVariant>& values);
    void updateImage(const QString &name, float east, float west, float north, float south, float altitude, const QString &data);
    void removeImage(const QString &name);
    void removeAllImages();
    void removeAllCZMLEntities();
    void initCZML();
    void czml(QJsonObject &obj);

    // Collect CZML packets instead of sending each one on its own. A flush of a busy sky
    // is one message, one JSON.parse and one czmlStream.process() in the page rather than
    // one of each per aircraft - which is where the 3D map's cost actually is, since the
    // C++ side of a packet is only a few microseconds.

    void beginBatch();
    void endBatch();

private:
    void flushPendingCommands();

public:

    bool m_batching = false;
    QJsonArray m_batch;
    // Commands issued while a batch is open. They must not overtake the CZML packets they
    // refer to: ObjectMapModel::update3D() updates an entity and then plays its
    // animations, and with the update queued but the animation sent immediately, the
    // animation reached Cesium first - referring to an entity that had not been updated,
    // or on the first sighting did not yet exist.
    QList<QJsonObject> m_pendingCommands;
    void update(ObjectMapItem *mapItem, bool isTarget, bool isSelected);
    void update(PolygonMapItem *mapItem);
    void update(PolylineMapItem *mapItem);
    void setPosition(const QGeoCoordinate& position);
    void save(const QString& filename, const QString& dataDir);

protected:

    CZML m_czml;
};

#endif // INCLUDE_FEATURE_CESIUMINTERFACE_H_
