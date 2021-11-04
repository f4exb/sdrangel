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

#ifndef INCLUDE_BEACON_H
#define INCLUDE_BEACON_H

#include <stdio.h>
#include <string.h>

#include <QString>
#include <QList>

#include "util/units.h"
#include "util/maidenhead.h"
#include "util/csv.h"

#define IARU_BEACONS_URL "https://iaru-r1-c5-beacons.org/wp-content/uploads/beacons.csv"

struct Beacon {

    QString m_callsign;
    quint64 m_frequency;         // In Hz
    QString m_locator;
    float m_latitude;
    float m_longitude;
    float m_altitude;           // In metres above sea-level
    QString m_power;            // In Watts - sometimes a string with extra infos
    QString m_polarization;     // H or V
    QString m_pattern;          // Omni or 30deg etc
    QString m_key;              // F1A, F1B
    QString m_mgm;              // Machine mode

    QString getText()
    {
        QStringList list;
        list.append("Beacon");
        list.append(QString("Callsign: %1").arg(m_callsign));
        list.append(QString("Frequency: %1").arg(getFrequencyText()));
        if (!m_power.isEmpty())
            list.append(QString("Power: %1 Watts ERP").arg(m_power));
        if (!m_polarization.isEmpty())
            list.append(QString("Polarization: %1").arg(m_polarization));
        if (!m_pattern.isEmpty())
            list.append(QString("Pattern: %1").arg(m_pattern));
        if (!m_key.isEmpty())
            list.append(QString("Key: %1").arg(m_key));
        if (!m_mgm.isEmpty())
            list.append(QString("MGM: %1").arg(m_mgm));
        list.append(QString("Locator: %1").arg(m_locator));
        return list.join("\n");
    }

    QString getFrequencyText()
    {
        if (m_frequency > 1000000000)
            return QString("%1 GHz").arg(m_frequency/1000000000.0, 0, ',', 6);
        else if (m_frequency > 1000000)
            return QString("%1 MHz").arg(m_frequency/1000000.0, 0, ',', 3);
        else
            return QString("%1 kHz").arg(m_frequency/1000.0, 0, ',', 3);
    }

    QString getFrequencyShortText()
    {
        if (m_frequency > 1000000000)
            return QString("%1G").arg(m_frequency/1000000000.0, 0, ',', 1);
        else if (m_frequency > 1000000)
            return QString("%1M").arg(std::floor(m_frequency/1000000.0), 0, ',', 0);
        else
            return QString("%1k").arg(std::floor(m_frequency/1000.0), 0, ',', 0);
    }

    // Uses ; rather than ,
    static QList<Beacon *> *readIARUCSV(const QString &filename)
    {
        int cnt = 0;
        QList<Beacon *> *beacons = nullptr;

        // Column numbers used for the data as of 2021/1/20
        int callsignCol = 0;
        int qrgCol = 1;
        int locatorCol = 2;
        int heightCol = 5;
        int patternCol = 7;
        int polarizationCol = 9;
        int powerCol = 10;
        int keyCol = 11;
        int mgmCol = 12;

        FILE *file;
        QByteArray utfFilename = filename.toUtf8();
        if ((file = fopen(utfFilename.constData(), "r")) != nullptr)
        {
            char row[2048];

            if (fgets(row, sizeof(row), file))
            {
                beacons = new QList<Beacon *>();

                // Read header
                int idx = 0;
                char *p = strtok(row, ";");
                while (p != nullptr)
                {
                    if (!strcmp(p, "Callsign"))
                        callsignCol = idx;
                    else if (!strcmp(p, "QRG"))
                        qrgCol = idx;
                    else if (!strcmp(p, "Locator"))
                        locatorCol = idx;
                    else if (!strcmp(p, "Hight ASL") || !strcmp(p, "Height ASL"))
                        heightCol = idx;
                    else if (!strcmp(p, "Pattern"))
                        patternCol = idx;
                    else if (!strcmp(p, "H/V"))
                        polarizationCol = idx;
                    else if (!strcmp(p, "Power"))
                        powerCol = idx;
                    else if (!strcmp(p, "Keying"))
                        keyCol = idx;
                    else if (!strcmp(p, "MGM"))
                        mgmCol = idx;
                    p = strtok(nullptr, ",");
                    idx++;
                }
                // Read data
                while (fgets(row, sizeof(row), file))
                {
                    char *callsign = nullptr;
                    size_t callsignLen = 0;
                    char *frequencyString = nullptr;
                    quint64 frequency;
                    char *locator = nullptr;
                    int height = 0;
                    char *heightString = nullptr;
                    char *pattern = nullptr;
                    char *polarization = nullptr;
                    char *power = nullptr;
                    char *key = nullptr;
                    char *mgm = nullptr;

                    char *q = row;
                    idx = 0;
                    while ((p = csvNext(&q, ';')) != nullptr)
                    {
                        // Read strings, stripping quotes
                        if ((idx == callsignCol) && (p[0] == '\"'))
                        {
                            callsign = p+1;
                            callsignLen = strlen(callsign)-1;
                            callsign[callsignLen] = '\0';
                        }
                        else if ((idx == qrgCol) && (p[0] == '\"'))
                        {
                            frequencyString = p+1;
                            frequencyString[strlen(frequencyString)-1] = '\0';
                            frequency = QString(frequencyString).toLongLong();
                        }
                        else if ((idx == locatorCol) && (p[0] == '\"'))
                        {
                            locator = p+1;
                            locator[strlen(locator)-1] = '\0';
                        }
                        else if ((idx == heightCol) && (p[0] == '\"'))
                        {
                            heightString = p+1;
                            heightString[strlen(heightString)-1] = '\0';
                            height = atoi(heightString);
                        }
                        else if ((idx == patternCol) && (p[0] == '\"'))
                        {
                            pattern = p+1;
                            pattern[strlen(pattern)-1] = '\0';
                        }
                        else if ((idx == polarizationCol) && (p[0] == '\"'))
                        {
                            polarization = p+1;
                            polarization[strlen(polarization)-1] = '\0';
                        }
                        else if ((idx == powerCol) && (p[0] == '\"'))
                        {
                            power = p+1;
                            power[strlen(power)-1] = '\0';
                        }
                        else if ((idx == keyCol) && (p[0] == '\"'))
                        {
                            key = p+1;
                            key[strlen(key)-1] = '\0';
                        }
                        else if ((idx == mgmCol) && (p[0] == '\"'))
                        {
                            mgm = p+1;
                            mgm[strlen(mgm)-1] = '\0';
                        }
                        idx++;
                    }
                    float latitude, longitude;
                    if (callsign && frequencyString && locator && Maidenhead::fromMaidenhead(locator, latitude, longitude))
                    {
                        Beacon *beacon = new Beacon();
                        beacon->m_callsign = callsign;
                        beacon->m_frequency = frequency * 1000;   // kHz to Hz
                        beacon->m_locator = locator;
                        beacon->m_latitude = latitude;
                        beacon->m_longitude = longitude;
                        beacon->m_altitude = height;
                        if (!QString("omni").compare(pattern, Qt::CaseInsensitive))
                            beacon->m_pattern = "Omni"; // Eliminate usage of mixed case
                        else
                            beacon->m_pattern = pattern;
                        beacon->m_polarization = polarization;
                        beacon->m_power = power;
                        beacon->m_key = key;
                        beacon->m_mgm = mgm;

                        beacons->append(beacon);
                        cnt++;
                    }
                }
            }
            fclose(file);
        }
        else
            qDebug() << "Beacon::readIARUCSV: Failed to open " << filename;

        qDebug() << "Beacon::readIARUCSV: Read " << cnt << " beacons";

        return beacons;
    }

};

#endif // INCLUDE_BEACON_H
