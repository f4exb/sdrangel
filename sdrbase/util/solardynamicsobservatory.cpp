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

#include "solardynamicsobservatory.h"

#include <QDebug>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkDiskCache>

SolarDynamicsObservatory::SolarDynamicsObservatory() :
    m_size(512),
    m_todayCache(nullptr)
{
    connect(&m_dataTimer, &QTimer::timeout, this, qOverload<>(&SolarDynamicsObservatory::getImage));
    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &SolarDynamicsObservatory::handleReply);

    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir writeableDir(locations[0]);
    if (!writeableDir.mkpath(QStringLiteral("cache") + QDir::separator() + QStringLiteral("solardynamicsobservatory"))) {
        qDebug() << "SolarDynamicsObservatory::SolarDynamicsObservatory: Failed to create cache/solardynamicsobservatory";
    }

    m_cache = new QNetworkDiskCache();
    m_cache->setCacheDirectory(locations[0] + QDir::separator() + QStringLiteral("cache") + QDir::separator() + QStringLiteral("solardynamicsobservatory"));
    m_cache->setMaximumCacheSize(100000000);
    m_networkManager->setCache(m_cache);
}

SolarDynamicsObservatory::~SolarDynamicsObservatory()
{
    disconnect(&m_dataTimer, &QTimer::timeout, this, qOverload<>(&SolarDynamicsObservatory::getImage));
    disconnect(m_networkManager, &QNetworkAccessManager::finished, this, &SolarDynamicsObservatory::handleReply);
    delete m_networkManager;
    delete m_todayCache;
}

SolarDynamicsObservatory* SolarDynamicsObservatory::create()
{
    return new SolarDynamicsObservatory();
}

QList<int> SolarDynamicsObservatory::getImageSizes()
{
    return {512, 1024, 2048, 4096};
}

QList<int> SolarDynamicsObservatory::getVideoSizes()
{
    return {512, 1024};
}

const QStringList SolarDynamicsObservatory::getImageNames()
{
    QChar angstronm(0x212B);
    QStringList names;

    // SDO
    names.append(QString("AIA 094 %1").arg(angstronm));
    names.append(QString("AIA 131 %1").arg(angstronm));
    names.append(QString("AIA 171 %1").arg(angstronm));
    names.append(QString("AIA 193 %1").arg(angstronm));
    names.append(QString("AIA 211 %1").arg(angstronm));
    names.append(QString("AIA 304 %1").arg(angstronm));
    names.append(QString("AIA 335 %1").arg(angstronm));
    names.append(QString("AIA 1600 %1").arg(angstronm));
    names.append(QString("AIA 1700 %1").arg(angstronm));
    names.append(QString("AIA 211 %1, 193 %1, 171 %1").arg(angstronm));
    names.append(QString("AIA 304 %1, 211 %1, 171 %1").arg(angstronm));
    names.append(QString("AIA 094 %1, 335 %1, 193 %1").arg(angstronm));
    names.append(QString("AIA 171 %1, HMIB").arg(angstronm));
    names.append("HMI Magneotgram");
    names.append("HMI Colorized Magneotgram");
    names.append("HMI Intensitygram - Colored");
    names.append("HMI Intensitygram - Flattened");
    names.append("HMI Intensitygram");
    names.append("HMI Dopplergram");

    // SOHO
    names.append("LASCO C2");
    names.append("LASCO C3");

    return names;
}

const QStringList SolarDynamicsObservatory::getChannelNames()
{
    QStringList channelNames = {
        "0094",
        "0131",
        "0171",
        "0193",
        "0211",
        "0304",
        "0335",
        "1600",
        "1700",
        "211193171",
        "304211171",
        "094335193",
        "HMImag",
        "HMIB",
        "HMIBC",
        "HMIIC",
        "HMIIF",
        "HMII",
        "HMID",
        "c2",
        "c3"
    };

    return channelNames;
}

const QStringList SolarDynamicsObservatory::getImageFileNames()
{
    // Ordering needs to match getImageNames()
    // %1 replaced with size
    QStringList filenames = {
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0094.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0131.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0171.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0193.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0211.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0304.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_0335.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_1600.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_1700.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_211193171.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/f_304_211_171_%1.jpg",
        //"https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_304211171.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/f_094_335_193_%1.jpg",
        //"https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_094335193.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/f_HMImag_171_%1.jpg",
        //"https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMImag.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMIB.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMIBC.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMIIC.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMIIF.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMII.jpg",
        "https://sdo.gsfc.nasa.gov/assets/img/latest/latest_%1_HMID.jpg",
        "https://soho.nascom.nasa.gov/data/realtime/c2/512/latest.jpg",
        "https://soho.nascom.nasa.gov/data/realtime/c3/512/latest.jpg"
    };

    return filenames;
}

const QStringList SolarDynamicsObservatory::getVideoNames()
{
    QChar angstronm(0x212B);
    QStringList names;

    // SDO
    names.append(QString("AIA 094 %1").arg(angstronm));
    names.append(QString("AIA 131 %1").arg(angstronm));
    names.append(QString("AIA 171 %1").arg(angstronm));
    names.append(QString("AIA 193 %1").arg(angstronm));
    names.append(QString("AIA 211 %1").arg(angstronm));
    names.append(QString("AIA 304 %1").arg(angstronm));
    names.append(QString("AIA 335 %1").arg(angstronm));
    names.append(QString("AIA 1600 %1").arg(angstronm));
    names.append(QString("AIA 1700 %1").arg(angstronm));

    // SOHO
    names.append("LASCO C2");
    names.append("LASCO C3");

    return names;
}

const QStringList SolarDynamicsObservatory::getVideoFileNames()
{
    const QStringList filenames = {
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0094.mp4",   // Videos sometimes fail to load on Windows if https used
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0131.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0171.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0193.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0211.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0304.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_0335.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_1600.mp4",
        "http://sdo.gsfc.nasa.gov/assets/img/latest/mpeg/latest_%1_1700.mp4",
        "http://soho.nascom.nasa.gov/data/LATEST/current_c2.mp4",
        "http://soho.nascom.nasa.gov/data/LATEST/current_c3.mp4",
    };

    return filenames;
}

QString SolarDynamicsObservatory::getImageURL(const QString& image, int size)
{
    const QStringList names = SolarDynamicsObservatory::getImageNames();
    const QStringList filenames = SolarDynamicsObservatory::getImageFileNames();
    int idx = names.indexOf(image);

    if (idx != -1) {
        return QString(filenames[idx]).arg(size);
    } else {
        return "";
    }
}

QString SolarDynamicsObservatory::getVideoURL(const QString& video, int size)
{
    const QStringList names = SolarDynamicsObservatory::getVideoNames();
    const QStringList filenames = SolarDynamicsObservatory::getVideoFileNames();
    int idx = names.indexOf(video);

    if (idx != -1) {
        return QString(filenames[idx]).arg(size);
    } else {
        return "";
    }
}

void SolarDynamicsObservatory::getImagePeriodically(const QString& image, int size, int periodInMins)
{
    m_image = image;
    m_size = size;
    if (periodInMins > 0)
    {
        m_dataTimer.setInterval(periodInMins*60*1000);
        m_dataTimer.start();
        getImage();
    }
    else
    {
        m_dataTimer.stop();
    }
}

void SolarDynamicsObservatory::getImage()
{
    getImage(m_image, m_size);
}

void SolarDynamicsObservatory::getImage(const QString& imageName, int size)
{
    QString urlString = getImageURL(imageName, size);
    if (!urlString.isEmpty())
    {
        QUrl url(urlString);

        m_networkManager->get(QNetworkRequest(url));
    }
}

void SolarDynamicsObservatory::getImage(const QString& imageName, QDateTime dateTime, int size)
{
    // Stop periodic updates, if not after latest data
    m_dataTimer.stop();

    // Save details of image we are after
    Request request;
    request.m_dateTime = dateTime;
    request.m_size = size;
    request.m_image = imageName;

    // Get file index, as we don't know what time will be used in the file
    QDate date = dateTime.date();
    if (m_indexCache.contains(date))
    {
        handleIndex(m_indexCache.take(date), request);
    }
    else if ((m_todayCache != nullptr) && (date == m_todayCacheDateTime.date()) && (dateTime < m_todayCacheDateTime.addSecs(-60 * 60)))
    {
        handleIndex(m_todayCache, request);
    }
    else
    {
        QString urlString = QString("https://sdo.gsfc.nasa.gov/assets/img/browse/%1/%2/%3/")
            .arg(date.year())
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0'));
        QUrl url(urlString);

        request.m_url = urlString;
        m_requests.append(request);

        m_networkManager->get(QNetworkRequest(url));
    }
}

void SolarDynamicsObservatory::handleReply(QNetworkReply* reply)
{
    if (reply)
    {
        if (!reply->error())
        {
            if (reply->url().fileName().endsWith(".jpg"))
            {
                handleJpeg(reply->readAll());
            }
            else
            {
                // Find corresponding request
                QString urlString = reply->url().toString();

                for (int i = 0; i < m_requests.size(); i++)
                {
                    if (m_requests[i].m_url == urlString)
                    {
                        QByteArray *bytes = new QByteArray(reply->readAll());

                        handleIndex(bytes, m_requests[i]);
                        m_requests.removeAt(i);
                        break;
                    }
                }
            }
        }
        else
        {
            qDebug() << "SolarDynamicsObservatory::handleReply: Error: " << reply->error();
        }
        reply->deleteLater();
    }
    else
    {
        qDebug() << "SolarDynamicsObservatory::handleReply: Reply is null";
    }
}

void SolarDynamicsObservatory::handleJpeg(const QByteArray& bytes)
{
    QImage image;

    if (image.loadFromData(bytes)) {
        emit imageUpdated(image);
    } else {
        qWarning() << "SolarDynamicsObservatory::handleJpeg: Failed to load image";
    }
}

void SolarDynamicsObservatory::handleIndex(QByteArray* bytes, const Request& request)
{
    const QStringList names = SolarDynamicsObservatory::getImageNames();
    const QStringList channelNames = SolarDynamicsObservatory::getChannelNames();
    int idx = names.indexOf(request.m_image);
    if (idx < 0) {
        return;
    }
    QString channel = channelNames[idx];

    QString file(*bytes);
    QStringList lines = file.split("\n");

    QString date = request.m_dateTime.date().toString("yyyyMMdd");
    QString pattern = QString("\"%1_([0-9]{6})_%2_%3.jpg\"").arg(date).arg(request.m_size).arg(channel);
    QRegularExpression re(pattern);

    // Get all times the image is available
    QList<QTime> times;
    for (const auto& line : lines)
    {
        QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
        {
            QString t = match.capturedTexts()[1];
            int h = t.left(2).toInt();
            int m = t.mid(2, 2).toInt();
            int s = t.right(2).toInt();
            times.append(QTime(h, m, s));
        }
    }

    if (times.length() > 0)
    {
        QTime target = request.m_dateTime.time();
        QTime current = times[0];
        for (int i = 1; i < times.size(); i++)
        {
            if (target < times[i]) {
                break;
            }
            current = times[i];
        }

        // Get image
        QDate date = request.m_dateTime.date();
        QString urlString = QString("https://sdo.gsfc.nasa.gov/assets/img/browse/%1/%2/%3/%1%2%3_%4%5%6_%7_%8.jpg")
            .arg(date.year())
            .arg(date.month(), 2, 10, QLatin1Char('0'))
            .arg(date.day(), 2, 10, QLatin1Char('0'))
            .arg(current.hour(), 2, 10, QLatin1Char('0'))
            .arg(current.minute(), 2, 10, QLatin1Char('0'))
            .arg(current.second(), 2, 10, QLatin1Char('0'))
            .arg(request.m_size)
            .arg(channel);

        QUrl url(urlString);

        m_networkManager->get(QNetworkRequest(url));
    }
    else
    {
        qDebug() << "SolarDynamicsObservatory: No image available for " << request.m_dateTime;
    }

    // Save index in cache
    if (request.m_dateTime.date() == QDate::currentDate())
    {
        if (m_todayCache != bytes)
        {
            m_todayCache = bytes;
            m_todayCacheDateTime = QDateTime::currentDateTime();
        }
    }
    else if (request.m_dateTime.date() < QDate::currentDate())
    {
        m_indexCache.insert(request.m_dateTime.date(), bytes);
    }
    else
    {
        delete bytes;
    }
}
