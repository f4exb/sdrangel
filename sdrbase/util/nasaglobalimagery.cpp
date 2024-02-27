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

#include "nasaglobalimagery.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QNetworkDiskCache>
#include <QJsonDocument>
#include <QJsonObject>

NASAGlobalImagery::NASAGlobalImagery()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &NASAGlobalImagery::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("nasaglobalimagery"))) {
        qDebug() << "Failed to create cache/nasaglobalimagery";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("nasaglobalimagery"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

NASAGlobalImagery::~NASAGlobalImagery()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &NASAGlobalImagery::handleReply);
    delete m_networkManager;
}

void NASAGlobalImagery::getData()
{
    QUrl url(QString("https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/1.0.0/WMTSCapabilities.xml"));
    m_networkManager->get(QNetworkRequest(url));
}

void NASAGlobalImagery::getMetaData()
{
    QUrl url(QString("https://worldview.earthdata.nasa.gov/config/wv.json"));
    m_networkManager->get(QNetworkRequest(url));
}

void NASAGlobalImagery::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QString url = reply->url().toEncoded().constData();
            QByteArray bytes = reply->readAll();

            if (url.endsWith(".xml")) {
                handleXML(bytes);
            } else if (url.endsWith(".svg")) {
                handleSVG(url, bytes);
            } else if (url.endsWith(".json")) {
                handleJSON(bytes);
            } else if (url.endsWith(".html")) {
                handleHTML(url, bytes);
            } else {
                qDebug() << "NASAGlobalImagery::handleReply: unexpected URL: " << url;
            }
        }
        else
        {
            qDebug() << "NASAGlobalImagery::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "NASAGlobalImagery::handleReply: reply is null";
    }
}

void NASAGlobalImagery::handleXML(const QByteArray& bytes)
{
    QXmlStreamReader xmlReader(bytes);
    QList<DataSet> dataSets;

    while (!xmlReader.atEnd() && !xmlReader.hasError())
    {
        while (xmlReader.readNextStartElement())
        {
            if (xmlReader.name() == QLatin1String("Capabilities"))
            {
                while(xmlReader.readNextStartElement())
                {
                    if (xmlReader.name() == QLatin1String("Contents"))
                    {
                        while(xmlReader.readNextStartElement())
                        {
                            if (xmlReader.name() == QLatin1String("Layer"))
                            {
                                QString identifier;
                                QString colorMap;
                                QList<Legend> legends;
                                QString tileMatrixSet;
                                QStringList urls;
                                QString format;
                                QString defaultDateTime;
                                QStringList dates;

                                while(xmlReader.readNextStartElement())
                                {
                                    if (xmlReader.name() == QLatin1String("Identifier"))
                                    {
                                        identifier = xmlReader.readElementText();
                                    }
                                    else if (xmlReader.name() == QLatin1String("TileMatrixSetLink"))
                                    {
                                        while(xmlReader.readNextStartElement())
                                        {
                                            if (xmlReader.name() == QLatin1String("TileMatrixSet"))
                                            {
                                                tileMatrixSet = xmlReader.readElementText();
                                            }
                                            else
                                            {
                                                xmlReader.skipCurrentElement();
                                            }
                                        }
                                    }
                                    else if (xmlReader.name() == QLatin1String("Style"))
                                    {
                                        while(xmlReader.readNextStartElement())
                                        {
                                            if (xmlReader.name() == QLatin1String("LegendURL"))
                                            {
                                                Legend legend;

                                                legend.m_url = xmlReader.attributes().value("xlink:href").toString();
                                                legend.m_width = (int)xmlReader.attributes().value("width").toFloat();
                                                legend.m_height = (int)xmlReader.attributes().value("height").toFloat();
                                                //qDebug() << legend.m_url << legend.m_width << legend.m_height;
                                                legends.append(legend);
                                                xmlReader.skipCurrentElement();
                                            }
                                            else
                                            {
                                                xmlReader.skipCurrentElement();
                                            }
                                        }
                                    }
                                    else if (xmlReader.name() == QLatin1String("Metadata"))
                                    {
                                        colorMap = xmlReader.attributes().value("xlink:href").toString();
                                        xmlReader.skipCurrentElement();
                                    }
                                    else if (xmlReader.name() == QLatin1String("Format"))
                                    {
                                        format = xmlReader.readElementText();
                                    }
                                    else if (xmlReader.name() == QLatin1String("ResourceURL"))
                                    {
                                        QString url = xmlReader.attributes().value("template").toString();
                                        if (!url.isEmpty()) {
                                            urls.append(url);
                                        }
                                        xmlReader.skipCurrentElement();
                                    }
                                    else if (xmlReader.name() == QLatin1String("Dimension"))
                                    {
                                        while(xmlReader.readNextStartElement())
                                        {
                                            if (xmlReader.name() == QLatin1String("Default"))
                                            {
                                                defaultDateTime = xmlReader.readElementText();
                                            }
                                            else if (xmlReader.name() == QLatin1String("Value"))
                                            {
                                                dates.append(xmlReader.readElementText());
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

                                // Some layers are <Format>application/vnd.mapbox-vector-tile</Format>
                                if (!identifier.isEmpty() && !tileMatrixSet.isEmpty() && ((format == "image/png") || (format == "image/jpeg")))
                                {
                                    DataSet dataSet;
                                    dataSet.m_identifier = identifier;
                                    dataSet.m_legends = legends;
                                    dataSet.m_tileMatrixSet = tileMatrixSet;
                                    dataSet.m_format = format;
                                    dataSet.m_defaultDateTime = defaultDateTime;
                                    dataSet.m_dates = dates;
                                    dataSets.append(dataSet);

                                    //qDebug() << "Adding layer" << identifier << colorMap << legends << tileMatrixSet;
                                }

                            }
                            else
                            {
                                xmlReader.skipCurrentElement();
                            }
                        }
                    }
                    else if (xmlReader.name() == QLatin1String("ServiceMetadataURL"))
                    {
                        // Empty?
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
    if (!xmlReader.atEnd() && xmlReader.hasError())
    {
        qDebug() << "NASAGlobalImagery::handleReply: Error parsing XML: " << xmlReader.errorString();
    }

    emit dataUpdated(dataSets);
}

void NASAGlobalImagery::downloadLegend(const Legend& legend)
{
    QUrl url(legend.m_url);
    m_networkManager->get(QNetworkRequest(url));
}

void NASAGlobalImagery::downloadHTML(const QString& urlString)
{
    QUrl url(urlString);
    m_networkManager->get(QNetworkRequest(url));
}

void NASAGlobalImagery::handleSVG(const QString& url, const QByteArray& bytes)
{
    emit legendAvailable(url, bytes);
}

void NASAGlobalImagery::handleJSON(const QByteArray& bytes)
{
    MetaData metaData;

    QJsonDocument document = QJsonDocument::fromJson(bytes);
    if (document.isObject())
    {
        QJsonObject obj = document.object();
        if (obj.contains(QStringLiteral("layers")))
        {
            for (const auto& oRef : obj.value(QStringLiteral("layers")).toObject())
            {
                Layer layer;
                QJsonObject o = oRef.toObject();

                if (o.contains(QStringLiteral("id"))) {
                    layer.m_identifier = o.value(QStringLiteral("id")).toString();
                }
                if (o.contains(QStringLiteral("title"))) {
                    layer.m_title = o.value(QStringLiteral("title")).toString();
                }
                if (o.contains(QStringLiteral("subtitle"))) {
                    layer.m_subtitle = o.value(QStringLiteral("subtitle")).toString();
                }
                if (o.contains(QStringLiteral("description"))) {
                    layer.m_descriptionURL = "https://worldview.earthdata.nasa.gov/config/metadata/layers/" + o.value(QStringLiteral("description")).toString() + ".html";
                }
                if (o.contains(QStringLiteral("startDate"))) {
                    layer.m_startDate = QDateTime::fromString(o.value(QStringLiteral("startDate")).toString(), Qt::ISODate);
                }
                if (o.contains(QStringLiteral("endDate"))) {
                    layer.m_endDate = QDateTime::fromString(o.value(QStringLiteral("endDate")).toString(), Qt::ISODate);
                }
                if (o.contains(QStringLiteral("ongoing"))) {
                    layer.m_ongoing = o.value(QStringLiteral("ongoing")).toBool();
                }
                if (o.contains(QStringLiteral("layerPeriod"))) {
                    layer.m_layerPeriod = o.value(QStringLiteral("layerPeriod")).toString();
                }
                if (o.contains(QStringLiteral("layergroup"))) {
                    layer.m_layerGroup = o.value(QStringLiteral("layergroup")).toString();
                }
                if (!layer.m_identifier.isEmpty()) {
                    metaData.m_layers.insert(layer.m_identifier, layer);
                }
            }
        }
    }

    emit metaDataUpdated(metaData);
}

void NASAGlobalImagery::handleHTML(const QString& url, const QByteArray& bytes)
{
    emit htmlAvailable(url, bytes);
}
