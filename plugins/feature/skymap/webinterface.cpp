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

#include <QJsonObject>
#include <QHashIterator>

#include "webinterface.h"

WebInterface::WebInterface(QObject *parent) :
    WebSocketServer(parent)
{
}

// Set the current camera view to a given coordinates
void WebInterface::setView(double ra, double dec, float zoom)
{
    QJsonObject obj {
        {"command", "setView"},
        {"ra", ra},
        {"dec", dec},
        {"zoom", zoom}
    };
    send(obj);
}

// Set observation position
void WebInterface::setPosition(const QGeoCoordinate& position)
{
    QJsonObject obj {
        {"command", "setPosition"},
        {"latitude", position.latitude()},
        {"longitude", position.longitude()},
        {"altitude", position.altitude()}
    };
    send(obj);
}

// Set date and time
void WebInterface::setDateTime(QDateTime dateTime)
{
    QJsonObject obj {
        {"command", "setDateTime"},
        {"dateTime", dateTime.toUTC().toString(Qt::ISODateWithMs)}
    };
    send(obj);
}

// Set the camera to track the item with the specified name
void WebInterface::track(const QString& name)
{
    QJsonObject obj {
        {"command", "track"},
        {"name", name}
    };
    send(obj);
}

void WebInterface::setBackground(const QString &background)
{
    QJsonObject obj {
        {"command", "setBackground"},
        {"background", background},
    };
    send(obj);
}

void WebInterface::setProjection(const QString &projection)
{
    QJsonObject obj {
        {"command", "setProjection"},
        {"projection", projection},
    };
    send(obj);
}

void WebInterface::showConstellations(bool show)
{
    QJsonObject obj {
        {"command", "showConstellations"},
        {"show", show}
    };
    send(obj);
}

void WebInterface::showReticle(bool show)
{
    QJsonObject obj {
        {"command", "showReticle"},
        {"show", show}
    };
    send(obj);
}

void WebInterface::showGrid(bool show)
{
    QJsonObject obj {
        {"command", "showGrid"},
        {"show", show}
    };
    send(obj);
}

void WebInterface::showNames(bool show)
{
    QJsonObject obj {
        {"command", "showNames"},
        {"show", show}
    };
    send(obj);
}

void WebInterface::showAntennaFoV(bool show)
{
    QJsonObject obj {
        {"command", "showAntennaFoV"},
        {"show", show}
    };
    send(obj);
}

void WebInterface::setAntennaFoV(float hpbw)
{
    QJsonObject obj {
        {"command", "setAntennaFoV"},
        {"hpbw", hpbw}
    };
    send(obj);
}

void WebInterface::setWWTSettings(const QHash<QString, QVariant>& settings)
{
    QJsonObject obj {
        {"command", "setWWTSettings"},
    };

    QHashIterator<QString, QVariant> itr(settings);
    while (itr.hasNext())
    {
        itr.next();
        QString key = itr.key();
        QVariant value = itr.value();
        obj.insert(key, value.toString());
    }

    send(obj);
}
