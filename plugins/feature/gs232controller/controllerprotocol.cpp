///////////////////////////////////////////////////////////////////////////////////
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

#include <QRegularExpression>

#include "maincore.h"
#include "channel/channelwebapiutils.h"

#include "gs232controllerreport.h"
#include "controllerprotocol.h"
#include "gs232protocol.h"
#include "spidprotocol.h"
#include "rotctrldprotocol.h"
#include "dfmprotocol.h"

ControllerProtocol::ControllerProtocol() :
    m_device(nullptr),
    m_lastAzimuth(-1.0f),
    m_lastElevation(-1.0f),
    m_msgQueueToFeature(nullptr)
{
}

ControllerProtocol::~ControllerProtocol()
{
}

void ControllerProtocol::setAzimuth(float azimuth)
{
    setAzimuthElevation(azimuth, m_lastElevation);
    m_lastAzimuth = azimuth;
}

void ControllerProtocol::setAzimuthElevation(float azimuth, float elevation)
{
    m_lastAzimuth = azimuth;
    m_lastElevation = elevation;
}

void ControllerProtocol::applySettings(const GS232ControllerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void ControllerProtocol::sendMessage(Message *message)
{
    m_msgQueueToFeature->push(message);
}

void ControllerProtocol::reportAzEl(float az, float el)
{
    m_msgQueueToFeature->push(GS232ControllerReport::MsgReportAzAl::create(az, el));
}

void ControllerProtocol::reportError(const QString &message)
{
    m_msgQueueToFeature->push(GS232Controller::MsgReportWorker::create(message));
}

void ControllerProtocol::getPosition(float& latitude, float& longitude)
{
    if (!m_settings.m_track)
    {
        // When not tracking, use My Position from preferences
        // although this precludes having different antennas at different positions
        latitude = MainCore::instance()->getSettings().getLatitude();
        longitude = MainCore::instance()->getSettings().getLongitude();
    }
    else
    {
        // When tracking, get position from Star Tracker / Sat Tracker
        QRegularExpression re("([FTR])(\\d+):(\\d+)");
        QRegularExpressionMatch match = re.match(m_settings.m_source);
        if (match.hasMatch())
        {
            QString kind = match.captured(1);
            int setIndex = match.captured(2).toInt();
            int index = match.captured(3).toInt();
            if (kind == 'F')
            {
                double lat, lon;
                bool latOk = ChannelWebAPIUtils::getFeatureSetting(setIndex, index, "latitude", lat);
                bool lonOk = ChannelWebAPIUtils::getFeatureSetting(setIndex, index, "longitude", lon);
                if (latOk && lonOk)
                {
                    latitude = (float)lat;
                    longitude = (float)lon;
                }
                else
                {
                    qDebug() << "ControllerProtocol::getPosition - Failed to get position from source: " << m_settings.m_source;
                }
            }
            else
            {
                double lat, lon;
                bool latOk = ChannelWebAPIUtils::getChannelSetting(setIndex, index, "latitude", lat);
                bool lonOk = ChannelWebAPIUtils::getChannelSetting(setIndex, index, "longitude", lon);
                if (latOk && lonOk)
                {
                    latitude = (float)lat;
                    longitude = (float)lon;
                }
                else
                {
                    qDebug() << "ControllerProtocol::getPosition - Failed to get position from source: " << m_settings.m_source;
                }
            }
        }
        else
        {
            qDebug() << "ControllerProtocol::getPosition - Couldn't parse source: " << m_settings.m_source;
        }
    }
    //qDebug() << "ControllerProtocol::getPosition: " << latitude << longitude;
}

ControllerProtocol *ControllerProtocol::create(GS232ControllerSettings::Protocol protocol)
{
    switch (protocol)
    {
    case GS232ControllerSettings::GS232:
        return new GS232Protocol();
        break;
    case GS232ControllerSettings::SPID:
        return new SPIDProtocol();
        break;
    case GS232ControllerSettings::ROTCTLD:
        return new RotCtrlDProtocol();
        break;
    case GS232ControllerSettings::DFM:
        return new DFMProtocol();
        break;
    default:
        return nullptr;
    }
}


