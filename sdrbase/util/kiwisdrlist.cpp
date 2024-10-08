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

#include "kiwisdrlist.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QNetworkDiskCache>
#include <QRegularExpression>

KiwiSDRList::KiwiSDRList()
{
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(m_networkManager, &QNetworkAccessManager::finished, this, &KiwiSDRList::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("kiwisdr"))) {
        qDebug() << "Failed to create cache/kiwisdr";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("kiwisdr"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);

    connect(&m_timer, &QTimer::timeout, this, &KiwiSDRList::update);
}

KiwiSDRList::~KiwiSDRList()
{
    QObject::disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &KiwiSDRList::handleReply);
    delete m_networkManager;
}

void KiwiSDRList::getData()
{
    QUrl url(QString("http://kiwisdr.com/public/"));
    m_networkManager->get(QNetworkRequest(url));
}

void KiwiSDRList::getDataPeriodically(int periodInMins)
{
    m_timer.setInterval(periodInMins*60*1000);
    m_timer.start();
    update();
}

void KiwiSDRList::update()
{
    getData();
}

void KiwiSDRList::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QString url = reply->url().toEncoded().constData();
            QByteArray bytes = reply->readAll();

            handleHTML(url, bytes);
        }
        else
        {
            qDebug() << "KiwiSDRList::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "KiwiSDRList::handleReply: reply is null";
    }
}

void KiwiSDRList::handleHTML(const QString& url, const QByteArray& bytes)
{
    (void) url;

    QList<KiwiSDR> sdrs;
    QString html(bytes);

    // Strip nested divs, as the following div regexp can't handle them
    QRegularExpression divName("<div class='cl-name'>(.*?)<\\/div>", QRegularExpression::DotMatchesEverythingOption);
    html.replace(divName, "\\1");

    QRegularExpression div("<div class='cl-info'>(.*?)<\\/div>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator divItr = div.globalMatch(html);

    if (divItr.hasNext())
    {
        while (divItr.hasNext())
        {
            QRegularExpressionMatch divMatch = divItr.next();
            QString divText = divMatch.captured(1);
            QRegularExpression urlRe("a href='(.*?)'");
            QRegularExpressionMatch urlMatch = urlRe.match(divText);

            if (urlMatch.hasMatch())
            {
                KiwiSDR sdr;

                sdr.m_url = urlMatch.captured(1);

                QRegularExpression element("<!-- (\\w+)=(.*?) -->");
                QRegularExpressionMatchIterator elementItr = element.globalMatch(divText);
                while(elementItr.hasNext())
                {
                    QRegularExpressionMatch elementMatch = elementItr.next();
                    QString key = elementMatch.captured(1);
                    QString value = elementMatch.captured(2);

                    if (key == "name")
                    {
                        sdr.m_name = value;
                    }
                    else if (key == "sdr_hw")
                    {
                        sdr.m_sdrHW = value;
                    }
                    else if (key == "bands")
                    {
                        QRegularExpression freqRe("([\\d]+)-([\\d]+)");
                        QRegularExpressionMatch freqMatch = freqRe.match(value);

                        if (freqMatch.hasMatch())
                        {
                            sdr.m_lowFrequency = freqMatch.captured(1).toInt();
                            sdr.m_highFrequency = freqMatch.captured(2).toInt();
                        }
                    }
                    else if (key == "users")
                    {
                        sdr.m_users = value.toInt();
                    }
                    else if (key == "users_max")
                    {
                        sdr.m_usersMax = value.toInt();
                    }
                    else if (key == "gps")
                    {
                        QRegularExpression gpsRe("([\\d.+-]+), ([\\d.+-]+)");
                        QRegularExpressionMatch gpsMatch = gpsRe.match(value);

                        if (gpsMatch.hasMatch())
                        {
                            sdr.m_latitude = gpsMatch.captured(1).toFloat();
                            sdr.m_longitude = gpsMatch.captured(2).toFloat();
                        }
                    }
                    else if (key == "asl")
                    {
                        sdr.m_altitude = value.toInt();
                    }
                    else if (key == "loc")
                    {
                        sdr.m_location = value;
                    }
                    else if (key == "antenna")
                    {
                        sdr.m_antenna = value;
                    }
                    else if (key == "ant_connected")
                    {
                        sdr.m_antennaConnected = value == "1";
                    }
                    else if (key == "snr")
                    {
                        sdr.m_snr = value;
                    }
                }

                sdrs.append(sdr);
            }
            else
            {
                qDebug() << "KiwiSDRPublic::handleHTML: No URL found in:\n" << divText;
            }
        }
    }
    else
    {
        qDebug() << "KiwiSDRPublic::handleHTML: No cl-info found in:\n" << html;
    }

    emit dataUpdated(sdrs);
}
