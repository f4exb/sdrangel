///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "aurora.h"

#include <QDebug>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkDiskCache>

// Green -> Yellow -> Red
const unsigned char Aurora::m_colorMap[] = {
    0, 255, 0,
    1, 255, 0,
    3, 255, 0,
    5, 255, 0,
    7, 255, 0,
    9, 255, 0,
    11, 255, 0,
    13, 255, 0,
    15, 255, 0,
    17, 255, 0,
    19, 255, 0,
    21, 255, 0,
    23, 255, 0,
    25, 255, 0,
    27, 255, 0,
    29, 255, 0,
    31, 255, 0,
    33, 255, 0,
    35, 255, 0,
    37, 255, 0,
    39, 255, 0,
    41, 255, 0,
    43, 255, 0,
    45, 255, 0,
    47, 255, 0,
    49, 255, 0,
    51, 255, 0,
    53, 255, 0,
    55, 255, 0,
    57, 255, 0,
    59, 255, 0,
    61, 255, 0,
    63, 255, 0,
    65, 255, 0,
    67, 255, 0,
    69, 255, 0,
    71, 255, 0,
    73, 255, 0,
    75, 255, 0,
    77, 255, 0,
    79, 255, 0,
    81, 255, 0,
    83, 255, 0,
    85, 255, 0,
    87, 255, 0,
    89, 255, 0,
    91, 255, 0,
    93, 255, 0,
    95, 255, 0,
    97, 255, 0,
    99, 255, 0,
    101, 255, 0,
    103, 255, 0,
    105, 255, 0,
    107, 255, 0,
    109, 255, 0,
    111, 255, 0,
    113, 255, 0,
    115, 255, 0,
    117, 255, 0,
    119, 255, 0,
    121, 255, 0,
    123, 255, 0,
    125, 255, 0,
    127, 255, 0,
    129, 255, 0,
    131, 255, 0,
    133, 255, 0,
    135, 255, 0,
    137, 255, 0,
    139, 255, 0,
    141, 255, 0,
    143, 255, 0,
    145, 255, 0,
    147, 255, 0,
    149, 255, 0,
    151, 255, 0,
    153, 255, 0,
    155, 255, 0,
    157, 255, 0,
    159, 255, 0,
    161, 255, 0,
    163, 255, 0,
    165, 255, 0,
    167, 255, 0,
    169, 255, 0,
    171, 255, 0,
    173, 255, 0,
    175, 255, 0,
    177, 255, 0,
    179, 255, 0,
    181, 255, 0,
    183, 255, 0,
    185, 255, 0,
    187, 255, 0,
    189, 255, 0,
    191, 255, 0,
    193, 255, 0,
    195, 255, 0,
    197, 255, 0,
    199, 255, 0,
    201, 255, 0,
    203, 255, 0,
    205, 255, 0,
    207, 255, 0,
    209, 255, 0,
    211, 255, 0,
    213, 255, 0,
    215, 255, 0,
    217, 255, 0,
    219, 255, 0,
    221, 255, 0,
    223, 255, 0,
    225, 255, 0,
    227, 255, 0,
    229, 255, 0,
    231, 255, 0,
    233, 255, 0,
    235, 255, 0,
    237, 255, 0,
    239, 255, 0,
    241, 255, 0,
    243, 255, 0,
    245, 255, 0,
    247, 255, 0,
    249, 255, 0,
    251, 255, 0,
    253, 255, 0,
    255, 255, 0,
    255, 253, 0,
    255, 251, 0,
    255, 249, 0,
    255, 247, 0,
    255, 245, 0,
    255, 243, 0,
    255, 241, 0,
    255, 239, 0,
    255, 237, 0,
    255, 235, 0,
    255, 233, 0,
    255, 231, 0,
    255, 229, 0,
    255, 227, 0,
    255, 225, 0,
    255, 223, 0,
    255, 221, 0,
    255, 219, 0,
    255, 217, 0,
    255, 215, 0,
    255, 213, 0,
    255, 211, 0,
    255, 209, 0,
    255, 207, 0,
    255, 205, 0,
    255, 203, 0,
    255, 201, 0,
    255, 199, 0,
    255, 197, 0,
    255, 195, 0,
    255, 193, 0,
    255, 191, 0,
    255, 189, 0,
    255, 187, 0,
    255, 185, 0,
    255, 183, 0,
    255, 181, 0,
    255, 179, 0,
    255, 177, 0,
    255, 175, 0,
    255, 173, 0,
    255, 171, 0,
    255, 169, 0,
    255, 167, 0,
    255, 165, 0,
    255, 163, 0,
    255, 161, 0,
    255, 159, 0,
    255, 157, 0,
    255, 155, 0,
    255, 153, 0,
    255, 151, 0,
    255, 149, 0,
    255, 147, 0,
    255, 145, 0,
    255, 143, 0,
    255, 141, 0,
    255, 139, 0,
    255, 137, 0,
    255, 135, 0,
    255, 133, 0,
    255, 131, 0,
    255, 129, 0,
    255, 127, 0,
    255, 125, 0,
    255, 123, 0,
    255, 121, 0,
    255, 119, 0,
    255, 117, 0,
    255, 115, 0,
    255, 113, 0,
    255, 111, 0,
    255, 109, 0,
    255, 107, 0,
    255, 105, 0,
    255, 103, 0,
    255, 101, 0,
    255, 99, 0,
    255, 97, 0,
    255, 95, 0,
    255, 93, 0,
    255, 91, 0,
    255, 89, 0,
    255, 87, 0,
    255, 85, 0,
    255, 83, 0,
    255, 81, 0,
    255, 79, 0,
    255, 77, 0,
    255, 75, 0,
    255, 73, 0,
    255, 71, 0,
    255, 69, 0,
    255, 67, 0,
    255, 65, 0,
    255, 63, 0,
    255, 61, 0,
    255, 59, 0,
    255, 57, 0,
    255, 55, 0,
    255, 53, 0,
    255, 51, 0,
    255, 49, 0,
    255, 47, 0,
    255, 45, 0,
    255, 43, 0,
    255, 41, 0,
    255, 39, 0,
    255, 37, 0,
    255, 35, 0,
    255, 33, 0,
    255, 31, 0,
    255, 29, 0,
    255, 27, 0,
    255, 25, 0,
    255, 23, 0,
    255, 21, 0,
    255, 19, 0,
    255, 17, 0,
    255, 15, 0,
    255, 13, 0,
    255, 11, 0,
    255, 9, 0,
    255, 7, 0,
    255, 5, 0,
    255, 3, 0,
    255, 1, 0,
};

Aurora::Aurora()
{
    connect(&m_dataTimer, &QTimer::timeout, this, &Aurora::getData);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &Aurora::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("aurora"))) {
        qDebug() << "Failed to create cache/aurora";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("giro"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

Aurora::~Aurora()
{
    disconnect(&m_dataTimer, &QTimer::timeout, this, &Aurora::getData);
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &Aurora::handleReply);
    delete m_networkManager;
}

Aurora* Aurora::create(const QString& service)
{
    if (service == "noaa.gov")
    {
        return new Aurora();
    }
    else
    {
        qDebug() << "Aurora::create: Unsupported service: " << service;
        return nullptr;
    }
}

void Aurora::getDataPeriodically(int periodInMins)
{
    if (periodInMins > 0)
    {
        m_dataTimer.setInterval(periodInMins*60*1000);
        m_dataTimer.start();
        getData();
    }
    else
    {
        m_dataTimer.stop();
    }
}

void Aurora::getData()
{
    QUrl url(QString("https://services.swpc.noaa.gov/json/ovation_aurora_latest.json"));
    m_networkManager->get(QNetworkRequest(url));
}

void Aurora::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QJsonDocument document = QJsonDocument::fromJson(reply->readAll());

            QString fileName = reply->url().fileName();
            if (fileName == "ovation_aurora_latest.json")
            {
                handleJSON(document);
            }
            else
            {
                qDebug() << "Aurora::handleReply: unexpected filename: " << fileName;
            }
        }
        else
        {
            qDebug() << "Aurora::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "Aurora::handleReply: reply is null";
    }
}

void Aurora::handleJSON(QJsonDocument& document)
{
    if (document.isObject())
    {
        QJsonObject obj = document.object();

        if (obj.contains(QStringLiteral("coordinates")))
        {
            QJsonArray array = obj.value(QStringLiteral("coordinates")).toArray();

            // Longitude: [0, 359]
            // Latitude: [-90, 90] including 0
            QImage image(360, 181, QImage::Format_ARGB32);
            image.fill(qRgba(0, 0, 0, 0));

            for (auto valRef : array)
            {
                if (valRef.isArray())
                {
                    QJsonArray coords = valRef.toArray();

                    if (coords.size() == 3)
                    {
                        int longitude = coords[0].toInt();
                        int latitude = coords[1].toInt();
                        int probabilty = coords[2].toInt();
                        const int min = 5; // Don't display anything for <5% probabilty
                        const int alphaMax = 180; // Always slightly transparent

                        if ((probabilty > min) && (std::abs(latitude) > 5)) // Ignore data around equator, as can incorrectly predict aurora
                        {
                            // Scale from 5%-100% to 256 entry colormap
                            int index = (int) ((probabilty - min) / (100.0f - min) * 255.0f);
                            // Make lowest probababilties a bit more transparent
                            int alpha = index < 25 ? (int)((index / 25.0f) * alphaMax) : alphaMax;
                            //qDebug() << probabilty << index << alpha;
                            image.setPixel((longitude + 180) % 360, 180 - (latitude + 90), qRgba(m_colorMap[index*3], m_colorMap[index*3+1], m_colorMap[index*3+2], alpha));
                        }
                    }
                    else
                    {
                        qDebug() << "Aurora::handleJSON: Expected coordinates array to be of length 3";
                    }
                }
            }

            emit dataUpdated(image);
        }
        else
        {
            qDebug() << "Aurora::handleJSON: No coordinates";
        }
    }
    else
    {
        qDebug() << "Aurora::handleJSON: Expected an object";
    }
}
