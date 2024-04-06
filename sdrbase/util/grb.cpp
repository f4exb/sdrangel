///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Jon Beniston, M7RCE                                        //
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

#include "grb.h"
#include "util/csv.h"

#include <QDebug>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QTextStream>

GRB::GRB()
{
    connect(&m_dataTimer, &QTimer::timeout, this,&GRB::getData);
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &GRB::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("grb"))) {
        qDebug() << "Failed to create cache/grb";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("grb"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

GRB::~GRB()
{
    disconnect(&m_dataTimer, &QTimer::timeout, this, &GRB::getData);
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &GRB::handleReply);
    delete m_networkManager;
}

GRB* GRB::create()
{
    return new GRB();
}

void GRB::getDataPeriodically(int periodInMins)
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

void GRB::getData()
{
    QUrl url("https://user-web.icecube.wisc.edu/~grbweb_public/Summary_table.txt");

    m_networkManager->get(QNetworkRequest(url));
}

void GRB::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            if (reply->url().fileName().endsWith(".txt"))
            {
                QByteArray bytes = reply->readAll();
                handleText(bytes);
            }
            else
            {
                qDebug() << "GRB::handleReply: Unexpected file" << reply->url().fileName();
            }
        }
        else
        {
            qDebug() << "GRB::handleReply: Error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "GRB::handleReply: Reply is null";
    }
}

void GRB::handleText(QByteArray& bytes)
{
    // Convert to CSV
    QString s(bytes);
    QStringList l = s.split("\n");
    for (int i = 0; i < l.size(); i++) {
        l[i] = l[i].simplified().replace(" ", ",");
    }
    s = l.join("\n");

    QTextStream in(&s);

    // Skip header
    for (int i = 0; i < 4; i++) {
        in.readLine();
    }

    QList<Data> grbs;
    QStringList cols;
    while(CSV::readRow(in, &cols))
    {
        Data grb;

        if (cols.length() >= 10)
        {
            grb.m_name = cols[0];
            grb.m_fermiName = cols[1];
            int year = grb.m_name.mid(3, 2).toInt();
            if (year >= 90) {
                year += 1900;
            } else {
                year += 2000;
            }
            QDate date(year, grb.m_name.mid(5, 2).toInt(), grb.m_name.mid(7, 2).toInt());
            QTime time = QTime::fromString(cols[2]);
            grb.m_dateTime = QDateTime(date, time);
            grb.m_ra = cols[3].toFloat();
            grb.m_dec = cols[4].toFloat();
            grb.m_fluence = cols[9].toFloat();

            //qDebug() << grb.m_name <<  grb.m_dateTime.toString() << grb.m_ra << grb.m_dec << grb.m_fluence ;

            if (grb.m_dateTime.isValid()) {
                grbs.append(grb);
            }
        }
    }

    emit dataUpdated(grbs);
}

 QString GRB::Data::getFermiURL() const
{
    if (m_fermiName.isEmpty() || (m_fermiName == "None")) {
        return "";
    }
    QString base = "https://heasarc.gsfc.nasa.gov/FTP/fermi/data/gbm/bursts/";
    QString yearDir = "20" + m_fermiName.mid(3, 2);
    QString dataDir = m_fermiName;
    dataDir.replace("GRB", "bn");
    return base + yearDir + "/" + dataDir + "/current/";
}

QString GRB::Data::getFermiPlotURL() const
{
    QString base = getFermiURL();
    if (base.isEmpty()) {
        return "";
    }

    QString name = m_fermiName;
    name.replace("GRB", "bn");
    return getFermiURL() + "glg_lc_all_" + name + "_v00.gif"; // Could be v01.gif? How to know without fetching index?
}

QString GRB::Data::getFermiSkyMapURL() const
{
    QString base = getFermiURL();
    if (base.isEmpty()) {
        return "";
    }

    QString name = m_fermiName;
    name.replace("GRB", "bn");
    return getFermiURL() + "glg_skymap_all_" + name + "_v00.png";
}

QString GRB::Data::getSwiftURL() const
{
    QString name = m_name;
    name.replace("GRB", "");
    return "https://swift.gsfc.nasa.gov/archive/grb_table/" + name;
}
