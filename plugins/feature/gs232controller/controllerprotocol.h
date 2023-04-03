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

#ifndef INCLUDE_FEATURE_CONTROLLERPROTOCOL_H_
#define INCLUDE_FEATURE_CONTROLLERPROTOCOL_H_

#include <QIODevice>

#include "util/messagequeue.h"
#include "gs232controllersettings.h"
#include "gs232controller.h"

class ControllerProtocol
{
public:
    ControllerProtocol();
    virtual ~ControllerProtocol();
    virtual void setAzimuth(float azimuth);
    virtual void setAzimuthElevation(float azimuth, float elevation) = 0;
    virtual void readData() = 0;
    virtual void update() = 0;
    void setDevice(QIODevice *device) { m_device = device; }
    virtual void applySettings(const GS232ControllerSettings& settings, const QList<QString>& settingsKeys, bool force);
    void setMessageQueue(MessageQueue *messageQueue) { m_msgQueueToFeature = messageQueue; }
    void sendMessage(Message *message);
    void reportAzEl(float az, float el);
    void reportError(const QString &message);
    void getPosition(float& latitude, float& longitude);

    static ControllerProtocol *create(GS232ControllerSettings::Protocol protocol);

protected:
    QIODevice *m_device;
    GS232ControllerSettings m_settings;
    float m_lastAzimuth;
    float m_lastElevation;

private:
    MessageQueue *m_msgQueueToFeature;
};

#endif // INCLUDE_FEATURE_CONTROLLERPROTOCOL_H_

