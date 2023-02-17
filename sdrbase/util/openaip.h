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
    static QList<Airspace *> readXML(const QString &filename);

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
    static QList<NavAid *> readXML(const QString &filename);
};

class SDRBASE_API OpenAIP : public QObject {
    Q_OBJECT

public:
    OpenAIP(QObject* parent = nullptr);
    ~OpenAIP();

    static const QStringList m_countryCodes;

    void downloadAirspaces();
    void downloadNavAids();

    static QSharedPointer<const QList<Airspace *>> getAirspaces();
    static QSharedPointer<const QList<NavAid *>> getNavAids();

private:
    HttpDownloadManager m_dlm;
    int m_countryIndex;

    static QSharedPointer<QList<Airspace *>> m_airspaces;
    static QSharedPointer<QList<NavAid *>> m_navAids;

    static QDateTime m_airspacesModifiedDateTime;
    static QDateTime m_navAidsModifiedDateTime;

    static QList<Airspace *> *readAirspaces();
    static QList<Airspace *> readAirspaces(const QString& countryCode);
    static QList<NavAid *> *readNavAids();
    static QList<NavAid *> readNavAids(const QString& countryCode);

    static QString getDataDir();
    static QString getAirspaceFilename(int i);
    static QString getAirspaceFilename(const QString& countryCode);
    static QString getAirspaceURL(int i);
    static QString getNavAidsFilename(int i);
    static QString getNavAidsFilename(const QString& countryCode);
    static QString getNavAidsURL(int i);
    static QDateTime getAirspacesModifiedDateTime();
    static QDateTime getNavAidsModifiedDateTime();

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

