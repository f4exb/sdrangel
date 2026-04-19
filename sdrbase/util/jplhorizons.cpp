///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE                                        //
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

#include "jplhorizons.h"

#include <QDebug>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkReply>
#include <QNetworkDiskCache>
#include <QJsonDocument>
#include <QRegularExpression>

#include "util/units.h"

const QString JPLHorizons::m_majorBodiesURL = QStringLiteral("https://ssd.jpl.nasa.gov/api/horizons.api?COMMAND=MB&format=text");
QMutex JPLHorizons::m_mutex;
QHash<QString, JPLHorizons::BodyID> JPLHorizons::m_bodies;

JPLHorizons::JPLHorizons()
{
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &JPLHorizons::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("jplhorizons"))) {
        qDebug() << "Failed to create cache/jplhorizons";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("jplhorizons"));
    m_cache->setMaximumCacheSize(10000000);
    m_networkManager->setCache(m_cache);
}

JPLHorizons::~JPLHorizons()
{
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &JPLHorizons::handleReply);
    delete m_networkManager;
}

JPLHorizons* JPLHorizons::create()
{
    return new JPLHorizons();
}

void JPLHorizons::getData(const QString& target, QDateTime startDateTime, QDateTime stopDateTime, double latitude, double longitude, double altitudeKM)
{
    QMutexLocker locker(&m_mutex);

    QString id;
    if (m_bodies.contains(target)) {
        id = QString::number(m_bodies.value(target).m_id);
    } else {
        id = target;
    }

    QString urlString = QString("https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='%1'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='OBSERVER'&START_TIME='%2'&STOP_TIME='%3'&STEP_SIZE='%4'&EMAIL_ADDR=sdrangel@sdrangel.org&CSV_FORMAT=NO&CENTER='COORD'&SITE_COORD='%5,%6,%7'")
        .arg(id)
        .arg(startDateTime.toString("yyyy-MM-dd hh:mm"))
        .arg(stopDateTime.toString("yyyy-MM-dd hh:mm"))
        .arg("10 min")
        .arg(longitude)
        .arg(latitude)
        .arg(altitudeKM)
        ;
    qDebug() << urlString;
    QUrl url(urlString);
    m_networkManager->get(QNetworkRequest(url));
}

void JPLHorizons::getMajorBodiesList()
{
    QMutexLocker locker(&m_mutex);

    if (m_bodies.isEmpty())
    {
        QUrl url(m_majorBodiesURL);
        m_networkManager->get(QNetworkRequest(url));
    }
    else
    {
        emit majorBodiesUpdated(m_bodies);
    }
}

bool JPLHorizons::getBodyId(const QString& body, int &id)
{
    QMutexLocker locked(&m_mutex);

    if (m_bodies.contains(body))
    {
        id = m_bodies.value(body).m_id;
        return true;
    }
    else
    {
        return false;
    }
}

void JPLHorizons::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            QByteArray bytes = reply->readAll();

            if (reply->url() == m_majorBodiesURL) {
                handleMajorBodiesList(bytes);
            } else {
                handleEphemerides(bytes);
            }
        }
        else
        {
            qDebug() << "JPLHorizons::handleReply: error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "JPLHorizons::handleReply: reply is null";
    }
}

void JPLHorizons::handleEphemerides(const QByteArray& bytes)
{
    QString file = QString::fromLatin1(bytes);

    QRegularExpression targetRE(R"(Target body name: ([A-Za-z\d' \(\)]+)\(-?\d+\))");
    QRegularExpressionMatch match = targetRE.match(file);
    int soe = file.indexOf("$$SOE");
    int eoe = file.indexOf("$$EOE");

    if (match.hasMatch() && (soe >= 0) && (eoe >= soe))
    {
        QString target = match.captured(1).trimmed();

        QList<Ephemeris> ephemerides;
        soe += 6;
        QString text = file.mid(soe, eoe - soe - 1);
        QStringList lines = text.split("\n");

        for (int i = 0; i < lines.size(); i++)
        {
            QString& line = lines[i];

            QDate date = QDate::fromString(line.mid(1, 11), "yyyy-MMM-dd");
            QTime time = QTime::fromString(line.mid(13, 5), "hh:mm");

            float ra;
            float dec;
            bool ok = Units::stringToRADec(line.mid(23, 23), ra, dec);

            if (date.isValid() && time.isValid() && ok)
            {
                Ephemeris ephemeris;
                ephemeris.m_dateTime = QDateTime(date, time);
                ephemeris.m_ra = ra;
                ephemeris.m_dec = dec;
                ephemerides.append(ephemeris);
            }
            else
            {
                qDebug() << "JPLHorizons::handleEphemerides: Failed to parse line:" << line << date.toString() << time.toString();
            }
        }

        emit ephemeridesUpdated(target, ephemerides);
    }
    else
    {
        qDebug() << "JPLHorizons::handleEphemerides: Failed to parse file" << file;
    }
}

void JPLHorizons::handleMajorBodiesList(const QByteArray& bytes)
{
    QHash<QString, BodyID> bodies;

    QString file = QString::fromLatin1(bytes);
    QString separator = "-------  ---------------------------------- -----------  -------------------";
    file = file.mid(file.indexOf(separator) + separator.size() + 1);

    QStringList lines = file.split("\n");

    for (int i = 0; i < lines.size(); i++)
    {
        QString& line = lines[i];
        if (line.size() == 80)
        {
            bool ok;
            int id = line.mid(0, 9).trimmed().toInt(&ok);
            QString name = line.mid(11, 34).trimmed();
            QString designation = line.mid(46, 11).trimmed();
            if (ok && !name.isEmpty())
            {
                BodyID body;
                body.m_id = id;
                body.m_name = name;
                body.m_designation = designation;
                bodies.insert(body.m_name, body);
            }
        }
    }
    if (bodies.isEmpty()) {
        qDebug() << "JPLHorizons::handleMajorBodiesList: Failed to parse" << file;
    }

    QMutexLocker locker(&m_mutex);
    m_bodies = bodies;
    emit majorBodiesUpdated(bodies);
}
