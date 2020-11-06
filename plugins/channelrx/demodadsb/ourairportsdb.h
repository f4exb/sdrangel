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

#ifndef INCLUDE_OURAIRPORTSDB_H
#define INCLUDE_OURAIRPORTSDB_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDebug>

#include "adsbdemodsettings.h"

#define AIRPORTS_URL "https://ourairports.com/data/airports.csv"
#define AIRPORT_FREQUENCIES_URL "https://ourairports.com/data/airport-frequencies.csv"

struct AirportInformation {

    struct FrequencyInformation {
        QString m_type;
        QString m_description;
        float m_frequency;      // In MHz
    };

    int m_id;
    QString m_ident;
    ADSBDemodSettings::AirportType m_type;
    QString m_name;
    float m_latitude;
    float m_longitude;
    float m_elevation;
    QVector<FrequencyInformation *> m_frequencies;

    static QString trimQuotes(const QString s)
    {
        if (s.startsWith('\"') && s.endsWith('\"'))
            return s.mid(1, s.size() - 2);
        else
            return s;
    }

    // Read OurAirport's airport CSV file
    static QHash<int, AirportInformation *> *readAirportsDB(const QString &filename)
    {
        int cnt = 0;
        QHash<int, AirportInformation *> *airportInfo = new QHash<int, AirportInformation *>();
        airportInfo->reserve(70000);

        // Column numbers used for the data as of 2020/10/28
        int idCol = 0;
        int identCol = 1;
        int typeCol = 2;
        int nameCol = 3;
        int latitudeCol = 4;
        int longitudeCol = 5;
        int elevationCol = 6;

        qDebug() << "AirportInformation::readAirportsDB: " << filename;

        QFile file(filename);
        if (file.open(QIODevice::ReadOnly))
        {
            QList<QByteArray> colNames;
            int idx;

            // Read header
            if (!file.atEnd())
            {
                QByteArray row = file.readLine().trimmed();
                colNames = row.split(',');
                // Work out which columns the data is in, based on the headers
                idx = colNames.indexOf("id");
                if (idx >= 0)
                     idCol = idx;
                idx = colNames.indexOf("ident");
                if (idx >= 0)
                     identCol = idx;
                idx = colNames.indexOf("type");
                if (idx >= 0)
                     typeCol = idx;
                idx = colNames.indexOf("name");
                if (idx >= 0)
                     nameCol = idx;
                idx = colNames.indexOf("latitude_deg");
                if (idx >= 0)
                     latitudeCol = idx;
                idx = colNames.indexOf("longitude_deg");
                if (idx >= 0)
                     longitudeCol = idx;
                idx = colNames.indexOf("elevation_ft");
                if (idx >= 0)
                     elevationCol = idx;
            }
            // Read data
            while (!file.atEnd())
            {
                QByteArray row = file.readLine();
                QList<QByteArray> cols = row.split(',');

                bool ok = false;
                int id = trimQuotes(cols[idCol]).toInt(&ok, 10);
                if (ok)
                {
                    QString ident = trimQuotes(cols[identCol]);
                    QString type = trimQuotes(cols[typeCol]);
                    QString name = trimQuotes(cols[nameCol]);
                    float latitude = cols[latitudeCol].toFloat();
                    float longitude = cols[longitudeCol].toFloat();
                    float elevation = cols[elevationCol].toFloat();

                    if (type != "closed")
                    {
                        AirportInformation *airport = new AirportInformation();
                        airport->m_id = id;
                        airport->m_ident = ident;
                        if (type == "small_airport")
                            airport->m_type = ADSBDemodSettings::AirportType::Small;
                        else if (type == "medium_airport")
                            airport->m_type = ADSBDemodSettings::AirportType::Medium;
                        else if (type == "large_airport")
                            airport->m_type = ADSBDemodSettings::AirportType::Large;
                        else if (type == "heliport")
                            airport->m_type = ADSBDemodSettings::AirportType::Heliport;
                        airport->m_name = name;
                        airport->m_latitude = latitude;
                        airport->m_longitude = longitude;
                        airport->m_elevation = elevation;
                        airportInfo->insert(id, airport);
                        cnt++;
                    }
                }
            }
            file.close();
        }
        else
            qDebug() << "Failed to open " << filename << " " << file.errorString();

        qDebug() << "AirportInformation::readAirportsDB: - read " << cnt << " airports";

        return airportInfo;
    }

    // Read OurAirport's airport frequencies CSV file
    static bool readFrequenciesDB(const QString &filename, QHash<int, AirportInformation *> *airportInfo)
    {
        int cnt = 0;

        // Column numbers used for the data as of 2020/10/28
        int airportRefCol = 1;
        int typeCol = 3;
        int descriptionCol = 4;
        int frequencyCol = 5;

        qDebug() << "AirportInformation::readFrequenciesDB: " << filename;

        QFile file(filename);
        if (file.open(QIODevice::ReadOnly))
        {
            QList<QByteArray> colNames;
            int idx;

            // Read header
            if (!file.atEnd())
            {
                QByteArray row = file.readLine().trimmed();
                colNames = row.split(',');
                // Work out which columns the data is in, based on the headers
                idx = colNames.indexOf("airport_ref");
                if (idx >= 0)
                     airportRefCol = idx;
                idx = colNames.indexOf("type");
                if (idx >= 0)
                     typeCol = idx;
                idx = colNames.indexOf("descrption");
                if (idx >= 0)
                     descriptionCol = idx;
                idx = colNames.indexOf("frequency_mhz");
                if (idx >= 0)
                     frequencyCol = idx;
            }
            // Read data
            while (!file.atEnd())
            {
                QByteArray row = file.readLine();
                QList<QByteArray> cols = row.split(',');

                bool ok = false;
                int airportRef = cols[airportRefCol].toInt(&ok, 10);
                if (ok)
                {
                    if (airportInfo->contains(airportRef))
                    {
                        QString type = trimQuotes(cols[typeCol]);
                        QString description = trimQuotes(cols[descriptionCol]);
                        float frequency = cols[frequencyCol].toFloat();

                        FrequencyInformation *frequencyInfo = new FrequencyInformation();
                        frequencyInfo->m_type = type;
                        frequencyInfo->m_description = description;
                        frequencyInfo->m_frequency = frequency;
                        airportInfo->value(airportRef)->m_frequencies.append(frequencyInfo);
                        cnt++;
                    }
                }
            }
            file.close();
        }
        else
            qDebug() << "Failed to open " << filename << " " << file.errorString();

        qDebug() << "AirportInformation::readFrequenciesDB: - read " << cnt << " airports";

        return airportInfo;
    }

};

#endif
