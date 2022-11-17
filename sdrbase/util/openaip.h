///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_OPENAIP_H
#define INCLUDE_OPENAIP_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDebug>
#include <QXmlStreamReader>
#include <QPointF>

#include <stdio.h>
#include <string.h>

#include "export.h"

#include "util/units.h"
#include "util/httpdownloadmanager.h"

// Server is moving, use new URL and .xml extension instead of .aip
//#define OPENAIP_AIRSPACE_URL    "https://www.openaip.net/customer_export_akfshb9237tgwiuvb4tgiwbf/%1_asp.aip"
//#define OPENAIP_NAVAIDS_URL     "https://www.openaip.net/customer_export_akfshb9237tgwiuvb4tgiwbf/%1_nav.aip"
#define OPENAIP_AIRSPACE_URL    "https://storage.googleapis.com/29f98e10-a489-4c82-ae5e-489dbcd4912f/%1_asp.xml"
#define OPENAIP_NAVAIDS_URL     "https://storage.googleapis.com/29f98e10-a489-4c82-ae5e-489dbcd4912f/%1_nav.xml"

struct SDRBASE_API Airspace {

    struct AltLimit {
        QString m_reference;    // STD, MSL
        int m_alt;              // Altitude
        QString m_altUnit;      // FL (Flight level) or F (Feet)
    };

    QString m_category;         // A-G, GLIDING, DANGER, PROHIBITED, TMZ
    QString m_country;          // GB
    QString m_name;             // BIGGIN HILL ATZ 129.405    - TODO: Extract frequency so we can tune to it
    AltLimit m_top;             // Top of airspace
    AltLimit m_bottom;          // Bottom of airspace
    QVector<QPointF> m_polygon;
    QPointF m_center;           // Center of polygon
    QPointF m_position;         // Position for text (not at the center, otherwise it will clash with airport)

    void calculatePosition()
    {
        qreal minX, maxX;
        qreal minY, maxY;
        if (m_polygon.size() > 0)
        {
            minX = maxX = m_polygon[0].x();
            minY = maxY = m_polygon[0].y();
            for (int i = 1; i < m_polygon.size(); i++)
            {
                qreal x = m_polygon[i].x();
                qreal y = m_polygon[i].y();
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
            m_center.setX(minX + (maxX - minX) / 2.0);
            m_center.setY(minY + (maxY - minY) * 2.0);
            m_position.setX(minX + (maxX - minX) / 2.0);
            m_position.setY(minY + (maxY - minY) * 3.0 / 4.0);
        }
    }

    QString getAlt(const AltLimit *altlimit) const
    {
        // Format on UK charts
        if (altlimit->m_alt == 0) {
            return "SFC"; // Surface
        } if (altlimit->m_altUnit == "FL") {
            return QString("FL%1").arg(altlimit->m_alt);
        } else if (altlimit->m_altUnit == "F") {
            return QString("%1'").arg(altlimit->m_alt);
        } else {
            return QString("%1 %2").arg(altlimit->m_alt).arg(altlimit->m_altUnit);
        }
    }

    double heightInMetres(const AltLimit *altlimit) const
    {
        if (altlimit->m_altUnit == "FL") {
            return Units::feetToMetres(altlimit->m_alt * 100);
        } else if (altlimit->m_altUnit == "F") {
            return Units::feetToMetres(altlimit->m_alt);
        } else {
            return altlimit->m_alt;
        }
    }

    double topHeightInMetres() const
    {
        return heightInMetres(&m_top);
    }

    double bottomHeightInMetres() const
    {
        return heightInMetres(&m_bottom);
    }

    // Read OpenAIP XML file
    static QList<Airspace *> readXML(const QString &filename)
    {
        QList<Airspace *> airspaces;
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QXmlStreamReader xmlReader(&file);

            while(!xmlReader.atEnd() && !xmlReader.hasError())
            {
                if (xmlReader.readNextStartElement())
                {
                    if (xmlReader.name() == QLatin1String("ASP"))
                    {
                        Airspace *airspace = new Airspace();

                        airspace->m_category = xmlReader.attributes().value("CATEGORY").toString();

                        while(xmlReader.readNextStartElement())
                        {
                            if (xmlReader.name() == QLatin1String("COUNTRY"))
                            {
                                airspace->m_country = xmlReader.readElementText();
                            }
                            else if (xmlReader.name() == QLatin1String("NAME"))
                            {
                                airspace->m_name = xmlReader.readElementText();
                            }
                            else if (xmlReader.name() == QLatin1String("ALTLIMIT_TOP"))
                            {
                                while(xmlReader.readNextStartElement())
                                {
                                    airspace->m_top.m_reference = xmlReader.attributes().value("REFERENCE").toString();
                                    airspace->m_top.m_altUnit = xmlReader.attributes().value("UNIT").toString();
                                    if (xmlReader.name() == QLatin1String("ALT"))
                                    {
                                        airspace->m_top.m_alt = xmlReader.readElementText().toInt();
                                    }
                                    else
                                    {
                                        xmlReader.skipCurrentElement();
                                    }
                                }
                            }
                            else if (xmlReader.name() == QLatin1String("ALTLIMIT_BOTTOM"))
                            {
                                while(xmlReader.readNextStartElement())
                                {
                                    airspace->m_bottom.m_reference = xmlReader.attributes().value("REFERENCE").toString();
                                    airspace->m_bottom.m_altUnit = xmlReader.attributes().value("UNIT").toString();
                                    if (xmlReader.name() == QLatin1String("ALT"))
                                    {
                                        airspace->m_bottom.m_alt = xmlReader.readElementText().toInt();
                                    }
                                    else
                                    {
                                        xmlReader.skipCurrentElement();
                                    }
                                }
                            }
                            else if (xmlReader.name() == QLatin1String("GEOMETRY"))
                            {
                                while(xmlReader.readNextStartElement())
                                {
                                    if (xmlReader.name() == QLatin1String("POLYGON"))
                                    {
                                        QString pointsString = xmlReader.readElementText();
                                        QStringList points = pointsString.split(",");
                                        for (const auto& ps : points)
                                        {
                                            QStringList split = ps.trimmed().split(" ");
                                            if (split.size() == 2)
                                            {
                                                QPointF pf(split[0].toDouble(), split[1].toDouble());
                                                airspace->m_polygon.append(pf);
                                            }
                                            else
                                            {
                                                qDebug() << "Airspace::readXML - Unexpected polygon point format: " << ps;
                                            }
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

                        airspace->calculatePosition();
                        //qDebug() << "Adding airspace: " << airspace->m_name << " " << airspace->m_category;
                        airspaces.append(airspace);
                    }
                }
            }

            file.close();
        }
        else
        {
            // Don't warn, as many countries don't have files
            //qDebug() << "Airspace::readXML: Could not open " << filename << " for reading.";
        }
        return airspaces;
    }

};

struct SDRBASE_API NavAid {
    int m_id;                   // Unique ID needed by VOR feature - Don't use value from database as that's 96-bit
    QString m_ident;            // 2 or 3 character ident
    QString m_type;             // NDB, VOR, VOR-DME or VORTAC
    QString m_name;
    float m_latitude;
    float m_longitude;
    float m_elevation;
    float m_frequencykHz;
    QString m_channel;
    int m_range;                // Nautical miles
    float m_magneticDeclination;
    bool m_alignedTrueNorth;    // Is the VOR aligned to true North, rather than magnetic (may be the case at high latitudes)

    int getRangeMetres() const
    {
        return Units::nauticalMilesToIntegerMetres((float)m_range);
    }

    // OpenAIP XML file
    static QList<NavAid *> readXML(const QString &filename)
    {
        int uniqueId = 1;
        QList<NavAid *> navAidInfo;
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QXmlStreamReader xmlReader(&file);

            while(!xmlReader.atEnd() && !xmlReader.hasError())
            {
                if (xmlReader.readNextStartElement())
                {
                    if (xmlReader.name() == QLatin1String("NAVAID"))
                    {
                        QStringView typeRef = xmlReader.attributes().value("TYPE");
                        if ((typeRef == QLatin1String("NDB"))
                            || (typeRef == QLatin1String("DME"))
                            || (typeRef == QLatin1String("VOR"))
                            || (typeRef == QLatin1String("VOR-DME"))
                            || (typeRef == QLatin1String("VORTAC"))
                            || (typeRef == QLatin1String("DVOR"))
                            || (typeRef == QLatin1String("DVOR-DME"))
                            || (typeRef == QLatin1String("DVORTAC")))
                        {
                            QString type = typeRef.toString();
                            QString name;
                            QString id;
                            float lat = 0.0f;
                            float lon = 0.0f;
                            float elevation = 0.0f;
                            float frequency = 0.0f;
                            QString channel;
                            int range = 25;
                            float declination = 0.0f;
                            bool alignedTrueNorth = false;
                            while(xmlReader.readNextStartElement())
                            {
                                if (xmlReader.name() == QLatin1String("NAME"))
                                {
                                    name = xmlReader.readElementText();
                                }
                                else if (xmlReader.name() == QLatin1String("ID"))
                                {
                                    id = xmlReader.readElementText();
                                }
                                else if (xmlReader.name() == QLatin1String("GEOLOCATION"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("LAT")) {
                                            lat = xmlReader.readElementText().toFloat();
                                        } else if (xmlReader.name() == QLatin1String("LON")) {
                                            lon = xmlReader.readElementText().toFloat();
                                        } else if (xmlReader.name() == QLatin1String("ELEV")) {
                                            elevation = xmlReader.readElementText().toFloat();
                                        } else {
                                            xmlReader.skipCurrentElement();
                                        }
                                    }
                                }
                                else if (xmlReader.name() == QLatin1String("RADIO"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("FREQUENCY"))
                                        {
                                            if (type == "NDB") {
                                                frequency = xmlReader.readElementText().toFloat();
                                            } else {
                                                frequency = xmlReader.readElementText().toFloat() * 1000.0;
                                            }
                                        } else if (xmlReader.name() == QLatin1String("CHANNEL")) {
                                            channel = xmlReader.readElementText();
                                        } else {
                                            xmlReader.skipCurrentElement();
                                        }
                                    }
                                }
                                else if (xmlReader.name() == QLatin1String("PARAMS"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("RANGE")) {
                                            range = xmlReader.readElementText().toInt();
                                        } else if (xmlReader.name() == QLatin1String("DECLINATION")) {
                                            declination = xmlReader.readElementText().toFloat();
                                        } else if (xmlReader.name() == QLatin1String("ALIGNEDTOTRUENORTH")) {
                                            alignedTrueNorth = xmlReader.readElementText() == "TRUE";
                                        } else {
                                            xmlReader.skipCurrentElement();
                                        }
                                    }
                                }
                                else
                                {
                                   xmlReader.skipCurrentElement();
                                }
                            }
                            NavAid *navAid = new NavAid();
                            navAid->m_id = uniqueId++;
                            navAid->m_ident = id;
                            // Check idents conform to our filtering rules
                            if (navAid->m_ident.size() < 2) {
                                qDebug() << "NavAid::readXML: Ident less than 2 characters: " << navAid->m_ident;
                            } else if (navAid->m_ident.size() > 3) {
                                qDebug() << "NavAid::readXML: Ident greater than 3 characters: " << navAid->m_ident;
                            }
                            navAid->m_type = type;
                            navAid->m_name = name;
                            navAid->m_frequencykHz = frequency;
                            navAid->m_channel = channel;
                            navAid->m_latitude = lat;
                            navAid->m_longitude = lon;
                            navAid->m_elevation = elevation;
                            navAid->m_range = range;
                            navAid->m_magneticDeclination = declination;
                            navAid->m_alignedTrueNorth = alignedTrueNorth;
                            navAidInfo.append(navAid);
                        }
                    }
                }
            }

            file.close();
        }
        else
        {
            // Don't warn, as many countries don't have files
            //qDebug() << "NavAid::readNavAidsXML: Could not open " << filename << " for reading.";
        }
        return navAidInfo;
    }

};

class SDRBASE_API OpenAIP : public QObject {
    Q_OBJECT

public:
    OpenAIP(QObject* parent = nullptr);
    ~OpenAIP();

    static const QStringList m_countryCodes;

    static QList<Airspace *> readAirspaces();
    static QList<Airspace *> readAirspaces(const QString& countryCode);
    static QList<NavAid *> readNavAids();
    static QList<NavAid *> readNavAids(const QString& countryCode);

    void downloadAirspaces();
    void downloadNavAids();

private:
    HttpDownloadManager m_dlm;
    int m_countryIndex;

    static QString getDataDir();
    static QString getAirspaceFilename(int i);
    static QString getAirspaceFilename(const QString& countryCode);
    static QString getAirspaceURL(int i);
    static QString getNavAidsFilename(int i);
    static QString getNavAidsFilename(const QString& countryCode);
    static QString getNavAidsURL(int i);

    void downloadAirspace();
    void downloadNavAid();

public slots:
    void downloadFinished(const QString& filename, bool success);

signals:
    void downloadingURL(const QString& url);
    void downloadError(const QString& error);
    void downloadAirspaceFinished();
    void downloadNavAidsFinished();

};

#endif // INCLUDE_OPENAIP_H
