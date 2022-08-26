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

#ifndef INCLUDE_OSNDB_H
#define INCLUDE_OSNDB_H

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDebug>
#include <QIcon>
#include <QMutex>
#include <QStandardPaths>
#include <QResource>

#include <stdio.h>
#include <string.h>

#include "util/csv.h"
#include "export.h"

#define OSNDB_URL "https://opensky-network.org/datasets/metadata/aircraftDatabase.zip"

struct SDRBASE_API AircraftInformation {

    int m_icao;
    QString m_registration;
    QString m_manufacturerName;
    QString m_model;
    QString m_owner;
    QString m_operator;
    QString m_operatorICAO;
    QString m_registered;

    static QHash<QString, QIcon *> m_airlineIcons; // Hashed on airline ICAO
    static QHash<QString, bool> m_airlineMissingIcons; // Hash containing which ICAOs we don't have icons for
    static QHash<QString, QIcon *> m_flagIcons;    // Hashed on country
    static QHash<QString, QString> *m_prefixMap;   // Registration to country (flag name)
    static QHash<QString, QString> *m_militaryMap;   // Operator airforce to military (flag name)
    static QMutex m_mutex;

    static void init()
    {
        QMutexLocker locker(&m_mutex);

        // Read registration prefix to country map
        m_prefixMap = CSV::hash(":/flags/regprefixmap.csv");
        // Read operator air force to military map
        m_militaryMap = CSV::hash(":/flags/militarymap.csv");
    }

    // Read OpenSky Network CSV file
    // This is large and contains lots of data we don't want, so we convert to
    // a smaller version to speed up loading time
    // Note that we use C file functions rather than QT, as these are ~30% faster
    // and the QT version seemed to occasionally crash
    static QHash<int, AircraftInformation *> *readOSNDB(const QString &filename)
    {
        int cnt = 0;
        QHash<int, AircraftInformation *> *aircraftInfo = nullptr;

        // Column numbers used for the data as of 2020/10/28
        int icaoCol = 0;
        int registrationCol = 1;
        int manufacturerNameCol = 3;
        int modelCol = 4;
        int ownerCol = 13;
        int operatorCol = 9;
        int operatorICAOCol = 11;
        int registeredCol = 15;

        qDebug() << "AircraftInformation::readOSNDB: " << filename;

        FILE *file;
        QByteArray utfFilename = filename.toUtf8();
        if ((file = fopen(utfFilename.constData(), "r")) != NULL)
        {
            char row[2048];

            if (fgets(row, sizeof(row), file))
            {
                aircraftInfo = new QHash<int, AircraftInformation *>();
                aircraftInfo->reserve(500000);
                // Read header
                int idx = 0;
                char *p = strtok(row, ",");
                while (p != NULL)
                {
                    if (!strcmp(p, "icao24"))
                        icaoCol = idx;
                    else if (!strcmp(p, "registration"))
                        registrationCol = idx;
                    else if (!strcmp(p, "manufacturername"))
                        manufacturerNameCol = idx;
                    else if (!strcmp(p, "model"))
                        modelCol = idx;
                    else if (!strcmp(p, "owner"))
                        ownerCol = idx;
                    else if (!strcmp(p, "operator"))
                        operatorCol = idx;
                    else if (!strcmp(p, "operatoricao"))
                        operatorICAOCol = idx;
                    else if (!strcmp(p, "registered"))
                        registeredCol = idx;
                    p = strtok(NULL, ",");
                    idx++;
                }
                // Read data
                while (fgets(row, sizeof(row), file))
                {
                    int icao = 0;
                    char *icaoString = NULL;
                    char *registration = NULL;
                    size_t registrationLen = 0;
                    char *manufacturerName = NULL;
                    size_t manufacturerNameLen = 0;
                    char *model = NULL;
                    size_t modelLen = 0;
                    char *owner = NULL;
                    size_t ownerLen = 0;
                    char *operatorName = NULL;
                    size_t operatorNameLen = 0;
                    char *operatorICAO = NULL;
                    size_t operatorICAOLen = 0;
                    char *registered = NULL;
                    size_t registeredLen = 0;

                    p = strtok(row, ",");
                    idx = 0;
                    while (p != NULL)
                    {
                        // Read strings, stripping quotes
                        if (idx == icaoCol)
                        {
                            icaoString = p+1;
                            icaoString[strlen(icaoString)-1] = '\0';
                            icao = strtol(icaoString, NULL, 16);
                        }
                        else if (idx == registrationCol)
                        {
                            registration = p+1;
                            registrationLen = strlen(registration)-1;
                            registration[registrationLen] = '\0';
                        }
                        else if (idx == manufacturerNameCol)
                        {
                            manufacturerName = p+1;
                            manufacturerNameLen = strlen(manufacturerName)-1;
                            manufacturerName[manufacturerNameLen] = '\0';
                        }
                        else if (idx == modelCol)
                        {
                            model = p+1;
                            modelLen = strlen(model)-1;
                            model[modelLen] = '\0';
                        }
                        else if (idx == ownerCol)
                        {
                            owner = p+1;
                            ownerLen = strlen(owner)-1;
                            owner[ownerLen] = '\0';
                        }
                        else if (idx == operatorCol)
                        {
                            operatorName = p+1;
                            operatorNameLen = strlen(operatorName)-1;
                            operatorName[operatorNameLen] = '\0';
                        }
                        else if (idx == operatorICAOCol)
                        {
                            operatorICAO = p+1;
                            operatorICAOLen = strlen(operatorICAO)-1;
                            operatorICAO[operatorICAOLen] = '\0';
                        }
                        else if (idx == registeredCol)
                        {
                            registered = p+1;
                            registeredLen = strlen(registered)-1;
                            registered[registeredLen] = '\0';
                        }
                        p = strtok(NULL, ",");
                        idx++;
                    }

                    // Only create the entry if we have some interesting data
                    if ((icao != 0) && (registrationLen > 0 || modelLen > 0 || ownerLen > 0 || operatorNameLen > 0 || operatorICAOLen > 0))
                    {
                        QString modelQ = QString(model);
                        // Tidy up the model names
                        if (modelQ.endsWith(" (Boeing)"))
                            modelQ = modelQ.left(modelQ.size() - 9);
                        else if (modelQ.startsWith("BOEING "))
                            modelQ = modelQ.right(modelQ.size() - 7);
                        else if (modelQ.startsWith("Boeing "))
                            modelQ = modelQ.right(modelQ.size() - 7);
                        else if (modelQ.startsWith("AIRBUS "))
                            modelQ = modelQ.right(modelQ.size() - 7);
                        else if (modelQ.startsWith("Airbus "))
                            modelQ = modelQ.right(modelQ.size() - 7);
                        else if (modelQ.endsWith(" (Cessna)"))
                            modelQ = modelQ.left(modelQ.size() - 9);

                        AircraftInformation *aircraft = new AircraftInformation();
                        aircraft->m_icao = icao;
                        aircraft->m_registration = QString(registration);
                        aircraft->m_manufacturerName = QString(manufacturerName);
                        aircraft->m_model = modelQ;
                        aircraft->m_owner = QString(owner);
                        aircraft->m_operator = QString(operatorName);
                        aircraft->m_operatorICAO = QString(operatorICAO);
                        aircraft->m_registered = QString(registered);
                        aircraftInfo->insert(icao, aircraft);
                        cnt++;
                    }

                }
            }
            fclose(file);
        }
        else
            qDebug() << "AircraftInformation::readOSNDB: Failed to open " << filename;

        qDebug() << "AircraftInformation::readOSNDB: Read " << cnt << " aircraft";

        return aircraftInfo;
    }

    // Create hash table using registration as key
    static QHash<QString, AircraftInformation *> *registrationHash(const QHash<int, AircraftInformation *> *in)
    {
        QHash<QString, AircraftInformation *> *out = new QHash<QString, AircraftInformation *>();
        QHashIterator<int, AircraftInformation *> i(*in);
        while (i.hasNext())
        {
            i.next();
            AircraftInformation *info = i.value();
            out->insert(info->m_registration, info);
        }
        return out;
    }

    // Write a reduced size and validated version of the DB, so it loads quicker
    static bool writeFastDB(const QString &filename, QHash<int, AircraftInformation *> *aircraftInfo)
    {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly))
        {
            file.write("icao24,registration,manufacturername,model,owner,operator,operatoricao,registered\n");
            QHash<int, AircraftInformation *>::iterator i = aircraftInfo->begin();
            while (i != aircraftInfo->end())
            {
                AircraftInformation *info = i.value();
                file.write(QString("%1").arg(info->m_icao, 1, 16).toUtf8());
                file.write(",");
                file.write(info->m_registration.toUtf8());
                file.write(",");
                file.write(info->m_manufacturerName.toUtf8());
                file.write(",");
                file.write(info->m_model.toUtf8());
                file.write(",");
                file.write(info->m_owner.toUtf8());
                file.write(",");
                file.write(info->m_operator.toUtf8());
                file.write(",");
                file.write(info->m_operatorICAO.toUtf8());
                file.write(",");
                file.write(info->m_registered.toUtf8());
                file.write("\n");
                ++i;
            }
            file.close();
            return true;
        }
        else
        {
            qCritical() << "AircraftInformation::writeFastDB failed to open " << filename << " for writing: " << file.errorString();
            return false;
        }
    }

    // Read smaller CSV file with no validation. Takes about 0.5s instead of 2s.
    static QHash<int, AircraftInformation *> *readFastDB(const QString &filename)
    {
        int cnt = 0;
        QHash<int, AircraftInformation *> *aircraftInfo = nullptr;

        qDebug() << "AircraftInformation::readFastDB: " << filename;

        FILE *file;
        QByteArray utfFilename = filename.toUtf8();
        if ((file = fopen(utfFilename.constData(), "r")) != NULL)
        {
            char row[2048];

            if (fgets(row, sizeof(row), file))
            {
                // Check header
                if (!strcmp(row, "icao24,registration,manufacturername,model,owner,operator,operatoricao,registered\n"))
                {
                    aircraftInfo = new QHash<int, AircraftInformation *>();
                    aircraftInfo->reserve(500000);
                    // Read data
                    while (fgets(row, sizeof(row), file))
                    {
                        char *p = row;
                        AircraftInformation *aircraft = new AircraftInformation();
                        char *icaoString = csvNext(&p);
                        int icao = strtol(icaoString, NULL, 16);
                        aircraft->m_icao = icao;
                        aircraft->m_registration = QString(csvNext(&p));
                        aircraft->m_manufacturerName = QString(csvNext(&p));
                        aircraft->m_model = QString(csvNext(&p));
                        aircraft->m_owner = QString(csvNext(&p));
                        aircraft->m_operator = QString(csvNext(&p));
                        aircraft->m_operatorICAO = QString(csvNext(&p));
                        aircraft->m_registered = QString(csvNext(&p));
                        aircraftInfo->insert(icao, aircraft);
                        cnt++;
                    }
                }
                else
                    qDebug() << "AircraftInformation::readFastDB: Unexpected header";
            }
            else
                qDebug() << "AircraftInformation::readFastDB: Empty file";
            fclose(file);
        }
        else
            qDebug() << "AircraftInformation::readFastDB: Failed to open " << filename;

        qDebug() << "AircraftInformation::readFastDB - read " << cnt << " aircraft";

        return aircraftInfo;
    }

    // Get flag based on registration
    QString getFlag() const
    {
        QString flag;
        if (m_prefixMap)
        {
            int idx = m_registration.indexOf('-');
            if (idx >= 0)
            {
                QString prefix;

                // Some countries use AA-A - try these first as first letters are common
                prefix = m_registration.left(idx + 2);
                if (m_prefixMap->contains(prefix))
                {
                    flag = m_prefixMap->value(prefix);
                }
                else
                {
                    // Try letters before '-'
                    prefix = m_registration.left(idx);
                    if (m_prefixMap->contains(prefix)) {
                        flag = m_prefixMap->value(prefix);
                    }
                }
            }
            else
            {
                // No '-' Could be one of a few countries or military.
                // See: https://en.wikipedia.org/wiki/List_of_aircraft_registration_prefixes
                if (m_registration.startsWith("N")) {
                    flag = m_prefixMap->value("N"); // US
                } else if (m_registration.startsWith("JA")) {
                    flag = m_prefixMap->value("JA"); // Japan
                } else if (m_registration.startsWith("HL")) {
                    flag = m_prefixMap->value("HL"); // Korea
                } else if (m_registration.startsWith("YV")) {
                    flag = m_prefixMap->value("YV"); // Venezuela
                } else if ((m_militaryMap != nullptr) && (m_militaryMap->contains(m_operator))) {
                    flag = m_militaryMap->value(m_operator);
                }
            }
        }
        return flag;
    }

    static QString getOSNDBZipFilename()
    {
        return getDataDir() + "/aircraftDatabase.zip";
    }

    static QString getOSNDBFilename()
    {
        return getDataDir() + "/aircraftDatabase.csv";
    }

    static QString getFastDBFilename()
    {
        return getDataDir() + "/aircraftDatabaseFast.csv";
    }

    static QString getDataDir()
    {
        // Get directory to store app data in (aircraft & airport databases and user-definable icons)
        QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
        // First dir is writable
        return locations[0];
    }

    static QString getAirlineIconPath(const QString &operatorICAO)
    {
        QString endPath = QString("/airlinelogos/%1.bmp").arg(operatorICAO);
        // Try in user directory first, so they can customise
        QString userIconPath = getDataDir() + endPath;
        QFile file(userIconPath);
        if (file.exists())
        {
            return userIconPath;
        }
        else
        {
            // Try in resources
            QString resourceIconPath = ":" + endPath;
            QResource resource(resourceIconPath);
            if (resource.isValid())
            {
                return resourceIconPath;
            }
        }
        return QString();
    }

    // Try to find an airline logo based on ICAO
    static QIcon *getAirlineIcon(const QString &operatorICAO)
    {
        if (m_airlineIcons.contains(operatorICAO))
        {
            return m_airlineIcons.value(operatorICAO);
        }
        else
        {
            QIcon *icon = nullptr;
            QString path = getAirlineIconPath(operatorICAO);
            if (!path.isEmpty())
            {
                icon = new QIcon(path);
                m_airlineIcons.insert(operatorICAO, icon);
            }
            else
            {
                if (!m_airlineMissingIcons.contains(operatorICAO))
                {
                    qDebug() << "ADSBDemodGUI: No airline logo for " << operatorICAO;
                    m_airlineMissingIcons.insert(operatorICAO, true);
                }
            }
            return icon;
        }
    }

    static QString getFlagIconPath(const QString &country)
    {
        QString endPath = QString("/flags/%1.bmp").arg(country);
        // Try in user directory first, so they can customise
        QString userIconPath = getDataDir() + endPath;
        QFile file(userIconPath);
        if (file.exists())
        {
            return userIconPath;
        }
        else
        {
            // Try in resources
            QString resourceIconPath = ":" + endPath;
            QResource resource(resourceIconPath);
            if (resource.isValid())
            {
                return resourceIconPath;
            }
        }
        return QString();
    }

    // Try to find an flag logo based on a country
    static QIcon *getFlagIcon(const QString &country)
    {
        if (m_flagIcons.contains(country))
        {
            return m_flagIcons.value(country);
        }
        else
        {
            QIcon *icon = nullptr;
            QString path = getFlagIconPath(country);
            if (!path.isEmpty())
            {
                icon = new QIcon(path);
                m_flagIcons.insert(country, icon);
            }
            return icon;
        }
    }

};

#endif
