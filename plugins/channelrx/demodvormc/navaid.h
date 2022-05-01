///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_NAVAID_H
#define INCLUDE_NAVAID_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDebug>
#include <QXmlStreamReader>

#include <stdio.h>
#include <string.h>

#include "util/units.h"
#include "util/csv.h"

#define OURAIRPORTS_NAVAIDS_URL "https://ourairports.com/data/navaids.csv"

#define OPENAIP_NAVAIDS_URL     "https://www.openaip.net/customer_export_akfshb9237tgwiuvb4tgiwbf/%1_nav.aip"

struct NavAid {

    int m_id;
    QString m_ident;            // 2 or 3 character ident
    QString m_type;             // VOR, VOR-DME or VORTAC
    QString m_name;
    float m_latitude;
    float m_longitude;
    float m_elevation;
    int m_frequencykHz;
    QString m_channel;
    int m_range;                // Nautical miles
    float m_magneticDeclination;
    bool m_alignedTrueNorth;    // Is the VOR aligned to true North, rather than magnetic (may be the case at high latitudes)

    static QString trimQuotes(const QString s)
    {
        if (s.startsWith('\"') && s.endsWith('\"'))
            return s.mid(1, s.size() - 2);
        else
            return s;
    }

    int getRangeMetres()
    {
        return Units::nauticalMilesToIntegerMetres((float)m_range);
    }

    // OpenAIP XML file
    static void readNavAidsXML(QHash<int, NavAid *> *navAidInfo, const QString &filename)
    {
        QFile file(filename);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QXmlStreamReader xmlReader(&file);

            while(!xmlReader.atEnd() && !xmlReader.hasError())
            {
                if (xmlReader.readNextStartElement())
                {
                    if (xmlReader.name() == "NAVAID")
                    {
                        QStringRef typeRef = xmlReader.attributes().value("TYPE");
                        if ((typeRef == QLatin1String("VOR"))
                            || (typeRef== QLatin1String("VOR-DME"))
                            || (typeRef == QLatin1String("VORTAC")))
                        {
                            QString type = typeRef.toString();
                            int identifier = 0;
                            QString name;
                            QString id;
                            float lat = 0.0f;
                            float lon = 0.0f;
                            float elevation = 0.0f;
                            int frequency = 0;
                            QString channel;
                            int range = 25;
                            float declination = 0.0f;
                            bool alignedTrueNorth = false;
                            while(xmlReader.readNextStartElement())
                            {
                                if (xmlReader.name() == QLatin1String("IDENTIFIER"))
                                    identifier = xmlReader.readElementText().toInt();
                                else if (xmlReader.name() == QLatin1String("NAME"))
                                    name = xmlReader.readElementText();
                                else if (xmlReader.name() == QLatin1String("ID"))
                                    id = xmlReader.readElementText();
                                else if (xmlReader.name() == QLatin1String("GEOLOCATION"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("LAT"))
                                            lat = xmlReader.readElementText().toFloat();
                                        else if (xmlReader.name() == QLatin1String("LON"))
                                            lon = xmlReader.readElementText().toFloat();
                                        else if (xmlReader.name() == QLatin1String("ELEV"))
                                            elevation = xmlReader.readElementText().toFloat();
                                        else
                                            xmlReader.skipCurrentElement();
                                    }
                                }
                                else if (xmlReader.name() == QLatin1String("RADIO"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("FREQUENCY"))
                                            frequency = (int)(xmlReader.readElementText().toFloat() * 1000);
                                        else if (xmlReader.name() == QLatin1String("CHANNEL"))
                                            channel = xmlReader.readElementText();
                                        else
                                            xmlReader.skipCurrentElement();
                                    }
                                }
                                else if (xmlReader.name() == QLatin1String("PARAMS"))
                                {
                                    while(xmlReader.readNextStartElement())
                                    {
                                        if (xmlReader.name() == QLatin1String("RANGE"))
                                            range = xmlReader.readElementText().toInt();
                                        else if (xmlReader.name() == QLatin1String("DECLINATION"))
                                            declination = xmlReader.readElementText().toFloat();
                                        else if (xmlReader.name() == QLatin1String("ALIGNEDTOTRUENORTH"))
                                            alignedTrueNorth = xmlReader.readElementText() == "TRUE";
                                        else
                                            xmlReader.skipCurrentElement();
                                    }
                                }
                                else
                                   xmlReader.skipCurrentElement();
                            }
                            NavAid *vor = new NavAid();
                            vor->m_id = identifier;
                            vor->m_ident = id;
                            // Check idents conform to our filtering rules
                            if (vor->m_ident.size() < 2)
                                qDebug() << "Warning: VOR Ident less than 2 characters: " << vor->m_ident;
                            else if (vor->m_ident.size() > 3)
                                qDebug() << "Warning: VOR Ident greater than 3 characters: " << vor->m_ident;
                            vor->m_type = type;
                            vor->m_name = name;
                            vor->m_frequencykHz = frequency;
                            vor->m_channel = channel;
                            vor->m_latitude = lat;
                            vor->m_longitude = lon;
                            vor->m_elevation = elevation;
                            vor->m_range = range;
                            vor->m_magneticDeclination = declination;
                            vor->m_alignedTrueNorth = alignedTrueNorth;
                            navAidInfo->insert(identifier, vor);
                        }
                    }
                }
            }

            file.close();
        }
        else
            qDebug() << "NavAid::readNavAidsXML: Could not open " << filename << " for reading.";
    }

    // Read OurAirport's NavAids CSV file
    // See comments for readOSNDB
    static QHash<int, NavAid *> *readNavAidsDB(const QString &filename)
    {
        int cnt = 0;
        QHash<int, NavAid *> *navAidInfo = nullptr;

        // Column numbers used for the data as of 2020/10/28
        int idCol = 0;
        int identCol = 2;
        int typeCol = 4;
        int nameCol = 3;
        int frequencyCol = 5;
        int latitudeCol = 6;
        int longitudeCol = 7;
        int elevationCol = 8;
        int powerCol = 18;

        qDebug() << "NavAid::readNavAidsDB: " << filename;

        FILE *file;
        QByteArray utfFilename = filename.toUtf8();
        QLocale cLocale(QLocale::C);
        if ((file = fopen(utfFilename.constData(), "r")) != NULL)
        {
            char row[2048];

            if (fgets(row, sizeof(row), file))
            {
                navAidInfo = new QHash<int, NavAid *>();
                navAidInfo->reserve(15000);

                // Read header
                int idx = 0;
                char *p = strtok(row, ",");
                while (p != NULL)
                {
                    if (!strcmp(p, "id"))
                        idCol = idx;
                    else if (!strcmp(p, "ident"))
                        identCol = idx;
                    else if (!strcmp(p, "type"))
                        typeCol = idx;
                    else if (!strcmp(p, "name"))
                        nameCol = idx;
                    else if (!strcmp(p, "frequency_khz"))
                        frequencyCol = idx;
                    else if (!strcmp(p, "latitude_deg"))
                        latitudeCol = idx;
                    else if (!strcmp(p, "longitude_deg"))
                        longitudeCol = idx;
                    else if (!strcmp(p, "elevation_ft"))
                        elevationCol = idx;
                    else if (!strcmp(p, "power"))
                        powerCol = idx;
                    p = strtok(NULL, ",");
                    idx++;
                }
                // Read data
                while (fgets(row, sizeof(row), file))
                {
                    int id = 0;
                    char *idString = NULL;
                    char *ident = NULL;
                    size_t identLen = 0;
                    char *type = NULL;
                    size_t typeLen = 0;
                    char *name = NULL;
                    size_t nameLen = 0;
                    char *frequencyString = NULL;
                    int frequency;
                    float latitude = 0.0f;
                    char *latitudeString = NULL;
                    size_t latitudeLen = 0;
                    float longitude = 0.0f;
                    char *longitudeString = NULL;
                    size_t longitudeLen = 0;
                    float elevation = 0.0f;
                    char *elevationString = NULL;
                    size_t elevationLen = 0;
                    char *power = NULL;
                    size_t powerLen = 0;

                    char *q = row;
                    idx = 0;
                    while ((p = csvNext(&q)) != nullptr)
                    {
                        // Read strings, stripping quotes
                        if (idx == idCol)
                        {
                            idString = p;
                            idString[strlen(idString)] = '\0';
                            id = strtol(idString, NULL, 10);
                        }
                        else if ((idx == identCol) && (p[0] == '\"'))
                        {
                            ident = p+1;
                            identLen = strlen(ident)-1;
                            ident[identLen] = '\0';
                        }
                        else if ((idx == typeCol) && (p[0] == '\"'))
                        {
                            type = p+1;
                            typeLen = strlen(type)-1;
                            type[typeLen] = '\0';
                        }
                        else if ((idx == nameCol) && (p[0] == '\"'))
                        {
                            name = p+1;
                            nameLen = strlen(name)-1;
                            name[nameLen] = '\0';
                        }
                        if (idx == frequencyCol)
                        {
                            frequencyString = p;
                            frequencyString[strlen(frequencyString)] = '\0';
                            frequency = strtol(frequencyString, NULL, 10);
                        }
                        else if (idx == latitudeCol)
                        {
                            latitudeString = p;
                            latitudeLen = strlen(latitudeString)-1;
                            latitudeString[latitudeLen] = '\0';
                            latitude = cLocale.toFloat(latitudeString);
                        }
                        else if (idx == longitudeCol)
                        {
                            longitudeString = p;
                            longitudeLen = strlen(longitudeString)-1;
                            longitudeString[longitudeLen] = '\0';
                            longitude = cLocale.toFloat(longitudeString);
                        }
                        else if (idx == elevationCol)
                        {
                            elevationString = p;
                            elevationLen = strlen(elevationString)-1;
                            elevationString[elevationLen] = '\0';
                            elevation = cLocale.toFloat(elevationString);
                        }
                        else if ((idx == powerCol) && (p[0] == '\"'))
                        {
                            power = p+1;
                            powerLen = strlen(power)-1;
                            power[powerLen] = '\0';
                        }
                        idx++;
                    }

                    // For now, we only want VORs
                    if (type && !strncmp(type, "VOR", 3))
                    {
                        NavAid *vor = new NavAid();
                        vor->m_id = id;
                        vor->m_ident = QString(ident);
                        // Check idents conform to our filtering rules
                        if (vor->m_ident.size() < 2)
                            qDebug() << "Warning: VOR Ident less than 2 characters: " << vor->m_ident;
                        else if (vor->m_ident.size() > 3)
                            qDebug() << "Warning: VOR Ident greater than 3 characters: " << vor->m_ident;
                        vor->m_type = QString(type);
                        vor->m_name = QString(name);
                        vor->m_frequencykHz = frequency;
                        vor->m_latitude = latitude;
                        vor->m_longitude = longitude;
                        vor->m_elevation = elevation;
                        if (power && !strcmp(power, "HIGH"))
                            vor->m_range = 100;
                        else if (power && !strcmp(power, "MEDIUM"))
                            vor->m_range = 40;
                        else
                            vor->m_range = 25;
                        vor->m_magneticDeclination = 0.0f;
                        vor->m_alignedTrueNorth = false;
                        navAidInfo->insert(id, vor);
                        cnt++;
                    }
                }
            }
            fclose(file);
        }
        else
            qDebug() << "NavAid::readNavAidsDB: Failed to open " << filename;

        qDebug() << "NavAid::readNavAidsDB: Read " << cnt << " VORs";

        return navAidInfo;
    }

};

#endif // INCLUDE_NAVAID_H
