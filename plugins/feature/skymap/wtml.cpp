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

#include "wtml.h"

#include <QUrl>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QDebug>
#include <QNetworkDiskCache>

WTML::WTML()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &WTML::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("wtml"))) {
        qDebug() << "Failed to create cache/wtml";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("wtml"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

WTML::~WTML()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &WTML::handleReply);
    delete m_networkManager;
}

void WTML::getData()
{
    // https://worldwidetelescope.org/wwtweb/catalog.aspx?X=ImageSets6
    // https://worldwidetelescope.org/wwtweb/catalog.aspx?W=exploreroot6
    QUrl url(QString("https://worldwidetelescope.org/wwtweb/catalog.aspx?X=ImageSets6"));
    m_networkManager->get(QNetworkRequest(url));
}

void WTML::handleReply(QNetworkReply* reply)
{
    if (!reply->error())
    {
        QXmlStreamReader xmlReader(reply->readAll());

        QList<ImageSet> dataSets;

        while (!xmlReader.atEnd() && !xmlReader.hasError())
        {
            while (xmlReader.readNextStartElement())
            {
                if (xmlReader.name() == QLatin1String("Folder"))
                {
                    while(xmlReader.readNextStartElement())
                    {
                        if (xmlReader.name() == QLatin1String("ImageSet"))
                        {
                            QString name = xmlReader.attributes().value("Name").toString();
                            QString dataSetType = xmlReader.attributes().value("DataSetType").toString();

                            if (!name.isEmpty() && !dataSetType.isEmpty())
                            {
                                ImageSet imageSet;
                                imageSet.m_name = name;
                                imageSet.m_dataSetType = dataSetType;
                                dataSets.append(imageSet);
                                //qDebug() << "Adding ImageSet " << name << dataSetType;
                            }

                            // Credits, Thumbnail etc
                            while(xmlReader.readNextStartElement())
                            {
                                xmlReader.skipCurrentElement();
                            }
                        }
                        else
                        {
                            xmlReader.skipCurrentElement();
                        }
                    }
                }
                else
                {
                    xmlReader.skipCurrentElement();
                }
            }
        }
        // Ignore "Premature end of document." here even if ok
        if (!xmlReader.atEnd() && xmlReader.hasError()) {
            qDebug() << "WTML::handleReply: Error parsing XML: " << xmlReader.errorString();
        }

        emit dataUpdated(dataSets);
    }
    else
    {
        qDebug() << "WTML::handleReply: error: " << reply->error();
    }
    reply->deleteLater();
}
