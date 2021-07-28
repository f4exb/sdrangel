///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include <cmath>

#include <QDebug>
#include <QAbstractSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>
#include <QDateTime>

#include "SWGTargetAzimuthElevation.h"
#include "SWGMapItem.h"

#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

#include "util/units.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "feature/featurewebapiutils.h"
#include "maincore.h"

#include "satellitetracker.h"
#include "satellitetrackerworker.h"
#include "satellitetrackerreport.h"
#include "satellitetrackersgp4.h"

MESSAGE_CLASS_DEFINITION(SatelliteTrackerWorker::MsgConfigureSatelliteTrackerWorker, Message)
MESSAGE_CLASS_DEFINITION(SatelliteTrackerReport::MsgReportSat, Message)
MESSAGE_CLASS_DEFINITION(SatelliteTrackerReport::MsgReportAOS, Message)
MESSAGE_CLASS_DEFINITION(SatelliteTrackerReport::MsgReportLOS, Message)
MESSAGE_CLASS_DEFINITION(SatelliteTrackerReport::MsgReportTarget, Message)

SatelliteTrackerWorker::SatelliteTrackerWorker(SatelliteTracker* satelliteTracker, WebAPIAdapterInterface *webAPIAdapterInterface) :
    m_satelliteTracker(satelliteTracker),
    m_webAPIAdapterInterface(webAPIAdapterInterface),
    m_msgQueueToFeature(nullptr),
    m_msgQueueToGUI(nullptr),
    m_running(false),
    m_mutex(QMutex::Recursive),
    m_recalculatePasses(true),
    m_flipRotation(false),
    m_extendedAzRotation(false)
{
    connect(&m_pollTimer, SIGNAL(timeout()), this, SLOT(update()));
}

SatelliteTrackerWorker::~SatelliteTrackerWorker()
{
    m_inputMessageQueue.clear();
}

void SatelliteTrackerWorker::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
}

bool SatelliteTrackerWorker::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_pollTimer.start((int)round(m_settings.m_updatePeriod*1000.0));
    // Resume doppler timers
    QHashIterator<QString, SatWorkerState *> itr(m_workerState);
    while (itr.hasNext())
    {
        itr.next();
        SatWorkerState *satWorkerState = itr.value();
        if (satWorkerState->m_dopplerTimer.interval() > 0)
            satWorkerState->m_dopplerTimer.start();
    }
    m_recalculatePasses = true;
    m_running = true;
    return m_running;
}

void SatelliteTrackerWorker::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_pollTimer.stop();
    // Stop doppler timers
    QHashIterator<QString, SatWorkerState *> itr(m_workerState);
    while (itr.hasNext())
    {
        itr.next();
        itr.value()->m_dopplerTimer.stop();
    }
    m_running = false;
}

void SatelliteTrackerWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool SatelliteTrackerWorker::handleMessage(const Message& message)
{
    if (MsgConfigureSatelliteTrackerWorker::match(message))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSatelliteTrackerWorker& cfg = (MsgConfigureSatelliteTrackerWorker&) message;

        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (SatelliteTracker::MsgSatData::match(message))
    {
        SatelliteTracker::MsgSatData& satData = (SatelliteTracker::MsgSatData&) message;
        m_satellites = satData.getSatellites();
        m_recalculatePasses = true;
        return true;
    }
    else
    {
        return false;
    }
}

void SatelliteTrackerWorker::applySettings(const SatelliteTrackerSettings& settings, bool force)
{
    qDebug() << "SatelliteTrackerWorker::applySettings:"
            << " m_target: " << settings.m_target
            << " m_satellites: " << settings.m_satellites
            << " m_dateTime: " << settings.m_dateTime
            << " m_utc: " << settings.m_utc
            << " m_updatePeriod: " << settings.m_updatePeriod
            << " force: " << force;

    if ((m_settings.m_target != settings.m_target)
        || (m_settings.m_latitude != settings.m_latitude)
        || (m_settings.m_longitude != settings.m_longitude)
        || (m_settings.m_heightAboveSeaLevel != settings.m_heightAboveSeaLevel)
        || (m_settings.m_dateTime != settings.m_dateTime)
        || (m_settings.m_utc != settings.m_utc)
        || (m_settings.m_groundTrackPoints != settings.m_groundTrackPoints)
        || (m_settings.m_minAOSElevation != settings.m_minAOSElevation)
        || (m_settings.m_minPassElevation != settings.m_minPassElevation)
        || (m_settings.m_predictionPeriod != settings.m_predictionPeriod)
        || (m_settings.m_passStartTime != settings.m_passStartTime)
        || (m_settings.m_passFinishTime != settings.m_passFinishTime)
        || (!m_settings.m_drawOnMap && settings.m_drawOnMap)
        || force)
    {
        // Recalculate immediately
        m_recalculatePasses = true;
        QTimer::singleShot(1, this, &SatelliteTrackerWorker::update);
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }
    else if ((m_settings.m_updatePeriod != settings.m_updatePeriod) || force)
    {
        m_pollTimer.start((int)round(settings.m_updatePeriod*1000.0));
    }

    if (!settings.m_drawOnMap && m_settings.m_drawOnMap)
    {
        QHashIterator<QString, SatWorkerState *> itr(m_workerState);
        while (itr.hasNext())
        {
            itr.next();
            removeFromMap(itr.key());
        }
    }

    // Remove satellites no longer needed
    QMutableHashIterator<QString, SatWorkerState *> itr(m_workerState);
    while (itr.hasNext())
    {
        itr.next();
        if (settings.m_satellites.indexOf(itr.key()) == -1)
            itr.remove();
    }

    // Add new satellites
    for (int i = 0; i < settings.m_satellites.size(); i++)
    {
        if (!m_workerState.contains(settings.m_satellites[i]))
        {
            SatWorkerState *satWorkerState = new SatWorkerState(settings.m_satellites[i]);
            m_workerState.insert(settings.m_satellites[i], satWorkerState);
            connect(&satWorkerState->m_aosTimer, &QTimer::timeout, [this, satWorkerState]() {
                aos(satWorkerState);
            });
            connect(&satWorkerState->m_losTimer, &QTimer::timeout, [this, satWorkerState]() {
                los(satWorkerState);
            });
            m_recalculatePasses = true;
        }
    }

    m_settings = settings;
}

void SatelliteTrackerWorker::removeFromMap(QString id)
{
    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
    QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(m_satelliteTracker, "mapitems");
    if (mapMessageQueues)
        sendToMap(mapMessageQueues, id, "", "", 0.0, 0.0, 0.0, 0.0, nullptr, nullptr);
}

void SatelliteTrackerWorker::sendToMap(QList<MessageQueue*> *mapMessageQueues,
                                       QString name, QString image, QString text,
                                       double lat, double lon, double altitude, double rotation,
                                       QList<QGeoCoordinate *> *track, QList<QGeoCoordinate *> *predictedTrack)
{
    QList<MessageQueue*>::iterator it = mapMessageQueues->begin();

    for (; it != mapMessageQueues->end(); ++it)
    {
        SWGSDRangel::SWGMapItem *swgMapItem = new SWGSDRangel::SWGMapItem();
        swgMapItem->setName(new QString(name));
        swgMapItem->setLatitude(lat);
        swgMapItem->setLongitude(lon);
        swgMapItem->setAltitude(altitude);
        swgMapItem->setImage(new QString(image));
        swgMapItem->setImageRotation(rotation);
        swgMapItem->setText(new QString(text));
        swgMapItem->setImageMinZoom(0);
        if (track != nullptr)
        {
            QList<SWGSDRangel::SWGMapCoordinate *> *mapTrack = new QList<SWGSDRangel::SWGMapCoordinate *>();
            for (int i = 0; i < track->size(); i++)
            {
                SWGSDRangel::SWGMapCoordinate* p = new SWGSDRangel::SWGMapCoordinate();
                QGeoCoordinate *c = track->at(i);
                p->setLatitude(c->latitude());
                p->setLongitude(c->longitude());
                p->setAltitude(c->altitude());
                mapTrack->append(p);
            }
            swgMapItem->setTrack(mapTrack);
        }
        if (predictedTrack != nullptr)
        {
            QList<SWGSDRangel::SWGMapCoordinate *> *mapTrack = new QList<SWGSDRangel::SWGMapCoordinate *>();
            for (int i = 0; i < predictedTrack->size(); i++)
            {
                SWGSDRangel::SWGMapCoordinate* p = new SWGSDRangel::SWGMapCoordinate();
                QGeoCoordinate *c = predictedTrack->at(i);
                p->setLatitude(c->latitude());
                p->setLongitude(c->longitude());
                p->setAltitude(c->altitude());
                mapTrack->append(p);
            }
            swgMapItem->setPredictedTrack(mapTrack);
        }

        MainCore::MsgMapItem *msg = MainCore::MsgMapItem::create(m_satelliteTracker, swgMapItem);
        (*it)->push(msg);
    }
}

void SatelliteTrackerWorker::update()
{
    // Get date and time to calculate position at
    QDateTime qdt;
    if (m_settings.m_dateTime == "")
        qdt = SatelliteTracker::currentDateTimeUtc();
    else if (m_settings.m_utc)
        qdt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs);
    else
        qdt = QDateTime::fromString(m_settings.m_dateTime, Qt::ISODateWithMs).toUTC();

    QHashIterator<QString, SatWorkerState *> itr(m_workerState);
    while (itr.hasNext())
    {
        itr.next();
        SatWorkerState *satWorkerState = itr.value();
        QString name = satWorkerState->m_name;
        if (m_satellites.contains(name))
        {
            SatNogsSatellite *sat = m_satellites.value(name);
            if (sat->m_tle != nullptr)
            {
                // Calculate position, AOS/LOS and other details for satellite
                int noOfPasses;
                bool recalcAsPastLOS = (satWorkerState->m_satState.m_passes.size() > 0) && (satWorkerState->m_satState.m_passes[0]->m_los < qdt);
                if (m_recalculatePasses || recalcAsPastLOS)
                    noOfPasses = (name == m_settings.m_target) ? 99 : 1;
                else
                    noOfPasses = 0;
                getSatelliteState(qdt, sat->m_tle->m_tle0, sat->m_tle->m_tle1, sat->m_tle->m_tle2,
                                    m_settings.m_latitude, m_settings.m_longitude, m_settings.m_heightAboveSeaLevel/1000.0,
                                    m_settings.m_predictionPeriod, m_settings.m_minAOSElevation, m_settings.m_minPassElevation,
                                    m_settings.m_passStartTime, m_settings.m_passFinishTime, m_settings.m_utc,
                                    noOfPasses, m_settings.m_groundTrackPoints, &satWorkerState->m_satState);

                // Update AOS/LOS (only set timers if using real time)
                if ((m_settings.m_dateTime == "") && (satWorkerState->m_satState.m_passes.size() > 0))
                {
                    // Do we have a new AOS?
                    if ((satWorkerState->m_aos != satWorkerState->m_satState.m_passes[0]->m_aos) || (satWorkerState->m_los != satWorkerState->m_satState.m_passes[0]->m_los))
                    {
                        qDebug() << "SatelliteTrackerWorker: New AOS: " << name << " new: " << satWorkerState->m_satState.m_passes[0]->m_aos << " old: " << satWorkerState->m_aos;
                        qDebug() << "SatelliteTrackerWorker: New LOS: " << name << " new: " << satWorkerState->m_satState.m_passes[0]->m_los << " old: " << satWorkerState->m_los;
                        satWorkerState->m_aos = satWorkerState->m_satState.m_passes[0]->m_aos;
                        satWorkerState->m_los = satWorkerState->m_satState.m_passes[0]->m_los;
                        if (satWorkerState->m_aos.isValid())
                        {
                            if (satWorkerState->m_aos > qdt)
                            {
                                satWorkerState->m_aosTimer.setInterval(satWorkerState->m_aos.toMSecsSinceEpoch() - qdt.toMSecsSinceEpoch());
                                satWorkerState->m_aosTimer.setSingleShot(true);
                                satWorkerState->m_aosTimer.start();
                            }
                            else if (qdt < satWorkerState->m_los)
                                aos(satWorkerState);

                            if (satWorkerState->m_los.isValid() && (m_settings.m_target == satWorkerState->m_name))
                                calculateRotation(satWorkerState);
                        }
                        if (satWorkerState->m_los.isValid() && (satWorkerState->m_los > qdt))
                        {
                            if (satWorkerState->m_losTimer.isActive()) {
                                qDebug() << "SatelliteTrackerWorker::update m_losTimer.remainingTime: " << satWorkerState->m_losTimer.remainingTime();
                            }
                            // We can detect a new AOS for a satellite, a little bit before the LOS has occured, presumably
                            // because the calculations aren't accurate to fractions of a second. Allow for 1s here
                            if (satWorkerState->m_losTimer.isActive() && (satWorkerState->m_losTimer.remainingTime() <= 1000))
                            {
                                satWorkerState->m_losTimer.stop();
                                // LOS hasn't been called yet - do so, before we reset timer
                                los(satWorkerState);
                            }
                            satWorkerState->m_losTimer.setInterval(satWorkerState->m_los.toMSecsSinceEpoch() - qdt.toMSecsSinceEpoch());
                            satWorkerState->m_losTimer.setSingleShot(true);
                            satWorkerState->m_losTimer.start();
                        }
                    }
                }
                else
                {
                    satWorkerState->m_aos = QDateTime();
                    satWorkerState->m_los = QDateTime();
                    satWorkerState->m_aosTimer.stop();
                    satWorkerState->m_losTimer.stop();
                }

                // Send Az/El of target to Rotator Controllers, if elevation above horizon
                if ((name == m_settings.m_target) && (satWorkerState->m_satState.m_elevation >= 0))
                {
                    double azimuth = satWorkerState->m_satState.m_azimuth;
                    double elevation = satWorkerState->m_satState.m_elevation;
                    if (m_extendedAzRotation)
                    {
                        if (azimuth < 180.0)
                            azimuth += 360.0;
                    }
                    else if (m_flipRotation)
                    {
                        azimuth = std::fmod(azimuth + 180.0, 360.0);
                        elevation = 180.0 - elevation;
                    }
                    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
                    QList<MessageQueue*> *rotatorMessageQueues = messagePipes.getMessageQueues(m_satelliteTracker, "target");
                    if (rotatorMessageQueues)
                    {
                        QList<MessageQueue*>::iterator it = rotatorMessageQueues->begin();

                        for (; it != rotatorMessageQueues->end(); ++it)
                        {
                            SWGSDRangel::SWGTargetAzimuthElevation *swgTarget = new SWGSDRangel::SWGTargetAzimuthElevation();
                            swgTarget->setName(new QString(m_settings.m_target));
                            swgTarget->setAzimuth(azimuth);
                            swgTarget->setElevation(elevation);
                            (*it)->push(MainCore::MsgTargetAzimuthElevation::create(m_satelliteTracker, swgTarget));
                        }
                    }
                }

                // Send to Map
                if (m_settings.m_drawOnMap)
                {
                    MessagePipes& messagePipes = MainCore::instance()->getMessagePipes();
                    QList<MessageQueue*> *mapMessageQueues = messagePipes.getMessageQueues(m_satelliteTracker, "mapitems");
                    if (mapMessageQueues)
                    {
                        QString image;
                        if (sat->m_name == "ISS")
                            image = "qrc:///satellitetracker/satellitetracker/iss-32.png";
                        else
                            image = "qrc:///satellitetracker/satellitetracker/satellite-32.png";

                        QString text = QString("Name: %1\nAltitude: %2 km\nRange: %3 km\nRange rate: %4 km/s\nSpeed: %5 km/h\nPeriod: %6 mins")
                                               .arg(sat->m_name)
                                               .arg((int)round(satWorkerState->m_satState.m_altitude))
                                               .arg((int)round(satWorkerState->m_satState.m_range))
                                               .arg(satWorkerState->m_satState.m_rangeRate, 0, 'f', 1)
                                               .arg(Units::kmpsToIntegerKPH(satWorkerState->m_satState.m_speed))
                                               .arg((int)round(satWorkerState->m_satState.m_period));
                        if (satWorkerState->m_satState.m_passes.size() > 0)
                        {
                            if ((qdt >= satWorkerState->m_satState.m_passes[0]->m_aos) && (qdt <= satWorkerState->m_satState.m_passes[0]->m_los))
                                text = text.append("\nSatellite is visible");
                            else
                                text = text.append("\nAOS in: %1 mins").arg((int)round((satWorkerState->m_satState.m_passes[0]->m_aos.toSecsSinceEpoch() - qdt.toSecsSinceEpoch())/60.0));
                            QString aosDateTime;
                            QString losDateTime;
                            if (m_settings.m_utc)
                            {
                                aosDateTime = satWorkerState->m_satState.m_passes[0]->m_aos.toString(m_settings.m_dateFormat + " hh:mm");
                                losDateTime = satWorkerState->m_satState.m_passes[0]->m_los.toString(m_settings.m_dateFormat + " hh:mm");
                            }
                            else
                            {
                                aosDateTime = satWorkerState->m_satState.m_passes[0]->m_aos.toLocalTime().toString(m_settings.m_dateFormat + " hh:mm");
                                losDateTime = satWorkerState->m_satState.m_passes[0]->m_los.toLocalTime().toString(m_settings.m_dateFormat + " hh:mm");
                            }
                            text = QString("%1\nAOS: %2\nLOS: %3\nMax El: %4%5")
                                            .arg(text)
                                            .arg(aosDateTime)
                                            .arg(losDateTime)
                                            .arg((int)round(satWorkerState->m_satState.m_passes[0]->m_maxElevation))
                                            .arg(QChar(0xb0));
                        }

                        sendToMap(mapMessageQueues, sat->m_name, image, text,
                                   satWorkerState->m_satState.m_latitude, satWorkerState->m_satState.m_longitude,
                                   satWorkerState->m_satState.m_altitude * 1000.0, 0,
                                   &satWorkerState->m_satState.m_groundTrack, &satWorkerState->m_satState.m_predictedGroundTrack);
                    }
                }

                // Send to GUI
                if (getMessageQueueToGUI())
                    getMessageQueueToGUI()->push(SatelliteTrackerReport::MsgReportSat::create(new SatelliteState(satWorkerState->m_satState)));
            }
            else
                qDebug() << "SatelliteTrackerWorker::update: No TLE for " << sat->m_name << ". Can't compute position.";
        }
    }
    m_recalculatePasses = false;
}

void SatelliteTrackerWorker::aos(SatWorkerState *satWorkerState)
{
    qDebug() << "SatelliteTrackerWorker::aos " << satWorkerState->m_name;

    // Indicate AOS to GUI
    if (getMessageQueueToGUI())
    {
        int durationMins = (int)round((satWorkerState->m_los.toSecsSinceEpoch() - satWorkerState->m_aos.toSecsSinceEpoch())/60.0);
        int maxElevation = 0;
        if (satWorkerState->m_satState.m_passes.size() > 0)
            maxElevation = satWorkerState->m_satState.m_passes[0]->m_maxElevation;
        getMessageQueueToGUI()->push(SatelliteTrackerReport::MsgReportAOS::create(satWorkerState->m_name, durationMins, maxElevation));
    }

    // Update target
    if (m_settings.m_autoTarget && (satWorkerState->m_name != m_settings.m_target))
    {
        // Only switch if higher priority (earlier in list) or other target not in AOS
        SatWorkerState *targetSatWorkerState = m_workerState.value(m_settings.m_target);
        int currentTargetIdx = m_settings.m_satellites.indexOf(m_settings.m_target);
        int newTargetIdx = m_settings.m_satellites.indexOf(satWorkerState->m_name);
        if ((newTargetIdx < currentTargetIdx) || !targetSatWorkerState->hasAOS())
        {
            // Stop doppler correction for current target
            if (m_workerState.contains(m_settings.m_target))
                 m_workerState.value(m_settings.m_target)->m_dopplerTimer.stop();

            qDebug() << "SatelliteTrackerWorker::aos - autoTarget setting " << satWorkerState->m_name;
            m_settings.m_target = satWorkerState->m_name;
            // Update GUI with new target
            if (getMessageQueueToGUI())
                getMessageQueueToGUI()->push(SatelliteTrackerReport::MsgReportTarget::create(satWorkerState->m_name));
        }
    }

    // TODO: Detect if different device sets are used and support multiple sats simultaneously
    if (m_settings.m_target == satWorkerState->m_name)
        applyDeviceAOSSettings(satWorkerState->m_name);
}

// Determine if we need to flip rotator or use extended azimuth to avoid 360/0 discontinuity
void SatelliteTrackerWorker::calculateRotation(SatWorkerState *satWorkerState)
{
    m_flipRotation = false;
    m_extendedAzRotation = false;
    if (satWorkerState->m_satState.m_passes.size() > 0)
    {
        SatNogsSatellite *sat = m_satellites.value(satWorkerState->m_name);
        bool passes0 = getPassesThrough0Deg(sat->m_tle->m_tle0, sat->m_tle->m_tle1, sat->m_tle->m_tle2,
                                            m_settings.m_latitude, m_settings.m_longitude, m_settings.m_heightAboveSeaLevel/1000.0,
                                            satWorkerState->m_satState.m_passes[0]->m_aos, satWorkerState->m_satState.m_passes[0]->m_los);
        if (passes0)
        {
            double aosAz = satWorkerState->m_satState.m_passes[0]->m_aosAzimuth;
            double losAz = satWorkerState->m_satState.m_passes[0]->m_losAzimuth;
            double minAz = std::min(aosAz, losAz);
            if ((m_settings.m_rotatorMaxAzimuth - 360.0) > minAz)
                m_extendedAzRotation = true;
            else if (m_settings.m_rotatorMaxElevation == 180.0)
                m_flipRotation = true;
        }
    }
}

void SatelliteTrackerWorker::applyDeviceAOSSettings(const QString& name)
{
    // Execute global program/script
    if (!m_settings.m_aosCommand.isEmpty())
    {
        qDebug() << "SatelliteTrackerWorker::aos: executing command: " << m_settings.m_aosCommand;
        QProcess::startDetached(m_settings.m_aosCommand);
    }

    // Update device set
    if (m_settings.m_deviceSettings.contains(name))
    {
        QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *m_deviceSettingsList = m_settings.m_deviceSettings.value(name);

        MainCore *mainCore = MainCore::instance();

        // Load presets
        for (int i = 0; i < m_deviceSettingsList->size(); i++)
        {
            SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
            if (!devSettings->m_presetGroup.isEmpty() && !devSettings->m_deviceSet.isEmpty())
            {
                const MainSettings& mainSettings = mainCore->getSettings();

                QString presetType = QString(devSettings->m_deviceSet[0]);
                const Preset* preset = mainSettings.getPreset(devSettings->m_presetGroup, devSettings->m_presetFrequency, devSettings->m_presetDescription, presetType);
                if (preset != nullptr)
                {
                    qDebug() << "SatelliteTrackerWorker::aos: Loading preset " << preset->getDescription() << " to " << devSettings->m_deviceSet[0];
                    unsigned int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                    std::vector<DeviceSet*>& deviceSets = mainCore->getDeviceSets();
                    if (deviceSetIndex < deviceSets.size())
                    {
                        MainCore::MsgLoadPreset *msg = MainCore::MsgLoadPreset::create(preset, deviceSetIndex);
                        mainCore->getMainMessageQueue()->push(msg);
                    }
                    else
                        qWarning() << "SatelliteTrackerWorker::aos: device set " << devSettings->m_deviceSet << " does not exist";
                }
                else
                    qWarning() << "SatelliteTrackerWorker::aos: Unable to get preset: " << devSettings->m_presetGroup << " " << devSettings->m_presetFrequency << " " << devSettings->m_presetDescription;
            }
        }

        // Wait a little bit for presets to load before performing other steps
        QTimer::singleShot(1000, [this, mainCore, name, m_deviceSettingsList]()
        {

            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);

                // Override frequency
                if (devSettings->m_frequency != 0)
                {
                    qDebug() << "SatelliteTrackerWorker::aos: setting frequency to: " << devSettings->m_frequency;
                    int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                    ChannelWebAPIUtils::setCenterFrequency(deviceSetIndex, devSettings->m_frequency);
                }

                // Execute per satellite program/script
                if (!devSettings->m_aosCommand.isEmpty())
                {
                    qDebug() << "SatelliteTrackerWorker::aos: executing command: " << devSettings->m_aosCommand;
                    QProcess::startDetached(devSettings->m_aosCommand);
                }

            }

            // Start acquisition - Need to use WebAPI, in order for GUI to correctly reflect being started
            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
                if (devSettings->m_startOnAOS)
                {
                    qDebug() << "SatelliteTrackerWorker::aos: starting acqusition";
                    int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                    ChannelWebAPIUtils::run(deviceSetIndex);
                }
            }

            // Send AOS message to channels/features
            SatWorkerState *satWorkerState = m_workerState.value(name);
            ChannelWebAPIUtils::satelliteAOS(name, satWorkerState->m_satState.m_passes[0]->m_northToSouth);
            FeatureWebAPIUtils::satelliteAOS(name, satWorkerState->m_aos, satWorkerState->m_los);

            // Start Doppler correction, if needed
            satWorkerState->m_initFrequencyOffset.clear();
            bool requiresDoppler = false;
            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
                if (devSettings->m_doppler.size() > 0)
                {
                    requiresDoppler = true;
                    for (int j = 0; j < devSettings->m_doppler.size(); j++)
                    {
                        int offset;
                        int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                        if (ChannelWebAPIUtils::getFrequencyOffset(deviceSetIndex, devSettings->m_doppler[j], offset))
                        {
                            satWorkerState->m_initFrequencyOffset.append(offset);
                            qDebug() << "SatelliteTrackerWorker::applyDeviceAOSSettings: Initial frequency offset: " << offset;
                        }
                        else
                        {
                            qDebug() << "SatelliteTrackerWorker::applyDeviceAOSSettings: Failed to get initial frequency offset";
                            satWorkerState->m_initFrequencyOffset.append(0);
                        }
                    }
                }
            }
            if (requiresDoppler)
            {
                satWorkerState->m_dopplerTimer.setInterval(m_settings.m_dopplerPeriod * 1000);
                satWorkerState->m_dopplerTimer.start();
                connect(&satWorkerState->m_dopplerTimer, &QTimer::timeout, [this, satWorkerState]() {
                    doppler(satWorkerState);
                });
            }

            // Start file sinks (need a little delay to ensure sample rate message has been handled in filerecord)
            QTimer::singleShot(1000, [this, m_deviceSettingsList]()
            {
                for (int i = 0; i < m_deviceSettingsList->size(); i++)
                {
                    SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);

                    if (devSettings->m_startStopFileSink)
                    {
                        qDebug() << "SatelliteTrackerWorker::aos: starting file sinks";
                        int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                        ChannelWebAPIUtils::startStopFileSinks(deviceSetIndex, true);
                    }
                }
            });

        });
    }
    else
    {
        // Send AOS message to channels/features
        SatWorkerState *satWorkerState = m_workerState.value(name);
        ChannelWebAPIUtils::satelliteAOS(name, satWorkerState->m_satState.m_passes[0]->m_northToSouth);
        FeatureWebAPIUtils::satelliteAOS(name, satWorkerState->m_aos, satWorkerState->m_los);
    }

}

void SatelliteTrackerWorker::doppler(SatWorkerState *satWorkerState)
{
    qDebug() << "SatelliteTrackerWorker::doppler " << satWorkerState->m_name;

    QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *m_deviceSettingsList = m_settings.m_deviceSettings.value(satWorkerState->m_name);
    if (m_deviceSettingsList != nullptr)
    {
        for (int i = 0; i < m_deviceSettingsList->size(); i++)
        {
            SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
            if (devSettings->m_doppler.size() > 0)
            {
                // Get center frequency for this device
                int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                double centerFrequency;
                if (ChannelWebAPIUtils::getCenterFrequency(deviceSetIndex, centerFrequency))
                {
                    // Calculate frequency delta due to Doppler
                    double c = 299792458.0;
                    double deltaF = centerFrequency * satWorkerState->m_satState.m_rangeRate * 1000.0 / c;

                    for (int j = 0; j < devSettings->m_doppler.size(); j++)
                    {
                        // For receive, we subtract, transmit we add
                        int offset;
                        if (devSettings->m_deviceSet[0] == "R")
                            offset = satWorkerState->m_initFrequencyOffset[i] - (int)round(deltaF);
                        else
                            offset = satWorkerState->m_initFrequencyOffset[i] + (int)round(deltaF);

                        if (!ChannelWebAPIUtils::setFrequencyOffset(deviceSetIndex, devSettings->m_doppler[j], offset))
                            qDebug() << "SatelliteTrackerWorker::doppler: Failed to set frequency offset";
                    }
                }
                else
                    qDebug() << "SatelliteTrackerWorker::doppler: couldn't get centre frequency for " << devSettings->m_deviceSet;
            }
        }
    }
}

void SatelliteTrackerWorker::los(SatWorkerState *satWorkerState)
{
    qDebug() << "SatelliteTrackerWorker::los " << satWorkerState->m_name;

    // Indicate LOS to GUI
    if (getMessageQueueToGUI())
        getMessageQueueToGUI()->push(SatelliteTrackerReport::MsgReportLOS::create(satWorkerState->m_name));

    // Stop Doppler timer, and set interval to 0, so we don't restart it in start()
    satWorkerState->m_dopplerTimer.stop();
    satWorkerState->m_dopplerTimer.setInterval(0);

    if (m_settings.m_target == satWorkerState->m_name)
    {
        // Execute program/script
        if (!m_settings.m_losCommand.isEmpty())
        {
            qDebug() << "SatelliteTrackerWorker::los: executing command: " << m_settings.m_losCommand;
            QProcess::startDetached(m_settings.m_losCommand);
        }

        if (m_settings.m_deviceSettings.contains(satWorkerState->m_name))
        {
            QList<SatelliteTrackerSettings::SatelliteDeviceSettings *> *m_deviceSettingsList = m_settings.m_deviceSettings.value(satWorkerState->m_name);

            // Stop file sinks
            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
                if (devSettings->m_startStopFileSink)
                {
                    qDebug() << "SatelliteTrackerWorker::los: stopping file sinks";
                    int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                    ChannelWebAPIUtils::startStopFileSinks(deviceSetIndex, false);
                }
            }

            // Send LOS message to channels/features
            ChannelWebAPIUtils::satelliteLOS(satWorkerState->m_name);
            FeatureWebAPIUtils::satelliteLOS(satWorkerState->m_name);

            // Stop acquisition
            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
                if (devSettings->m_stopOnLOS)
                {
                    int deviceSetIndex = devSettings->m_deviceSet.mid(1).toInt();
                    ChannelWebAPIUtils::stop(deviceSetIndex);
                }
            }

            // Execute per satellite program/script
            // Do after stopping acquisition, so files are closed by file sink
            for (int i = 0; i < m_deviceSettingsList->size(); i++)
            {
                SatelliteTrackerSettings::SatelliteDeviceSettings *devSettings = m_deviceSettingsList->at(i);
                if (!devSettings->m_losCommand.isEmpty())
                {
                    qDebug() << "SatelliteTrackerWorker::los: executing command: " << devSettings->m_losCommand;
                    QProcess::startDetached(devSettings->m_losCommand);
                }
            }
        }
    }

    // Is another lower-priority satellite with AOS available to switch to?
    if (m_settings.m_autoTarget)
    {
        for (int i = m_settings.m_satellites.indexOf(m_settings.m_target) + 1; i < m_settings.m_satellites.size(); i++)
        {
            if (m_workerState.contains(m_settings.m_satellites[i]))
            {
                SatWorkerState *newSatWorkerState = m_workerState.value(m_settings.m_satellites[i]);
                if (newSatWorkerState->hasAOS())
                {
                    qDebug() << "SatelliteTrackerWorker::los - autoTarget setting " << m_settings.m_satellites[i];
                    m_settings.m_target = m_settings.m_satellites[i];
                    // Update GUI with new target
                    if (getMessageQueueToGUI())
                        getMessageQueueToGUI()->push(SatelliteTrackerReport::MsgReportTarget::create(m_settings.m_target));
                    // Apply device settings
                    applyDeviceAOSSettings(m_settings.m_target);
                    break;
                }
            }
        }
    }

    m_recalculatePasses = true;
}

bool SatWorkerState::hasAOS()
{
    QDateTime currentTime = SatelliteTracker::currentDateTimeUtc();
    return (m_aos <= currentTime) && (m_los > currentTime);
}
