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

#include "rainviewer.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

RainViewer::RainViewer()
{
    connect(&m_timer, &QTimer::timeout, this, &RainViewer::update);
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &RainViewer::handleReply);
}

RainViewer::~RainViewer()
{
    m_timer.stop();
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &RainViewer::handleReply);
    delete m_networkManager;
}

void RainViewer::getPathPeriodically(int periodInMins)
{
    // Rain maps updated every 10mins
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void RainViewer::update()
{
    getPath();
}

void RainViewer::getPath()
{
    QUrl url(QString("https://api.rainviewer.com/public/weather-maps.json"));
    m_networkManager->get(QNetworkRequest(url));
}

void RainViewer::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
            if (document.isObject())
            {
                QJsonObject obj = document.object();
                QString radarPath = "";
                QString satellitePath = "";

                if (obj.contains(QStringLiteral("radar")))
                {
                    QJsonValue val = obj.value(QStringLiteral("radar"));
                    QJsonObject mainObj = val.toObject();
                    if (mainObj.contains(QStringLiteral("past")))
                    {
                        QJsonArray past = mainObj.value(QStringLiteral("past")).toArray();
                        if (past.size() > 0)
                        {
                            QJsonObject mostRecent = past.last().toObject();
                            if (mostRecent.contains(QStringLiteral("path"))) {
                                radarPath = mostRecent.value(QStringLiteral("path")).toString();
                            }
                        }
                    }
                }
                else
                {
                    qDebug() << "RainViewer::handleReply: Object doesn't contain a radar: " << obj;
                }
                if (obj.contains(QStringLiteral("satellite")))
                {
                    QJsonValue val = obj.value(QStringLiteral("satellite"));
                    QJsonObject mainObj = val.toObject();
                    if (mainObj.contains(QStringLiteral("infrared")))
                    {
                        QJsonArray ir = mainObj.value(QStringLiteral("infrared")).toArray();
                        if (ir.size() > 0)
                        {
                            QJsonObject mostRecent = ir.last().toObject();
                            if (mostRecent.contains(QStringLiteral("path"))) {
                                satellitePath = mostRecent.value(QStringLiteral("path")).toString();
                            }
                        }
                    }
                }
                else
                {
                    qDebug() << "RainViewer::handleReply: Object doesn't contain a satellite: " << obj;
                }
                emit pathUpdated(radarPath, satellitePath);
            }
            else
            {
                qDebug() << "RainViewer::handleReply: Document is not an object: " << document;
            }
        }
        else
        {
            qDebug() << "RainViewer::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "RainViewer::handleReply: reply is null";
    }
}
