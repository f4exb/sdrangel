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

#include <QFileInfo>
#include <QResource>

#if (QT_VERSION < QT_VERSION_CHECK(6, 6, 0))
#include <QtGui/private/qzipreader_p.h>
#else
#include <QtCore/private/qzipreader_p.h>
#endif

#include "util/osndb.h"

QHash<QString, QIcon *> AircraftInformation::m_airlineIcons;
QHash<QString, bool> AircraftInformation::m_airlineMissingIcons;
QHash<QString, QIcon *> AircraftInformation::m_flagIcons;
QHash<QString, QString> *AircraftInformation::m_prefixMap;
QHash<QString, QString> *AircraftInformation::m_militaryMap;
QMutex AircraftInformation::m_mutex;

QSharedPointer<const QHash<int, AircraftInformation *>> OsnDB::m_aircraftInformation;
QSharedPointer<const QHash<QString, AircraftInformation *>> OsnDB::m_aircraftInformationByReg;
QDateTime OsnDB::m_modifiedDateTime;


OsnDB::OsnDB(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OsnDB::downloadFinished);
}

OsnDB::~OsnDB()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OsnDB::downloadFinished);
}

void OsnDB::downloadAircraftInformation()
{
    QString filename = OsnDB::getOSNDBZipFilename();
    QString urlString = OSNDB_URL;
    QUrl dbURL(urlString);
    qDebug() << "OsnDB::downloadAircraftInformation: Downloading " << urlString;
    emit downloadingURL(urlString);
    QNetworkReply *reply = m_dlm.download(dbURL, filename);
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 bytesRead, qint64 totalBytes) {
        emit downloadProgress(bytesRead, totalBytes);
    });
}

void OsnDB::downloadFinished(const QString& filename, bool success)
{
    if (!success)
    {
        qWarning() << "OsnDB::downloadFinished: Failed to download: " << filename;
        emit downloadError(QString("Failed to download: %1").arg(filename));
    }
    else if (filename == OsnDB::getOSNDBZipFilename())
    {
        // Extract .csv file from .zip file
        QZipReader reader(filename);
        QByteArray database = reader.fileData("media/data/samples/metadata/aircraftDatabase.csv");
        if (database.size() > 0)
        {
            QFile file(OsnDB::getOSNDBFilename());
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(database);
                file.close();
                emit downloadAircraftInformationFinished();
            }
            else
            {
                qWarning() << "OsnDB::downloadFinished - Failed to open " << file.fileName() << " for writing";
                emit downloadError(QString("Failed to open %1 for writing").arg(file.fileName()));
            }
        }
        else
        {
            qWarning() << "OsnDB::downloadFinished - aircraftDatabase.csv not in expected dir. Extracting all.";
            if (reader.extractAll(getDataDir()))
            {
                emit downloadAircraftInformationFinished();
            }
            else
            {
                qWarning() << "OsnDB::downloadFinished - Failed to extract files from " << filename;
                emit downloadError(QString("Failed to extract files from ").arg(filename));
            }
        }
    }
    else
    {
        qDebug() << "OsnDB::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

QSharedPointer<const QHash<int, AircraftInformation *>> OsnDB::getAircraftInformation()
{
    QFileInfo fastFileInfo(getFastDBFilename());
    QFileInfo fullFileInfo(getOSNDBFilename());
    QDateTime fastModifiedDateTime = fastFileInfo.lastModified();
    QDateTime fullModifiedDateTime = fullFileInfo.lastModified();

    // Update fast database, if full database is newer
    if (fullModifiedDateTime > fastModifiedDateTime)
    {
        qDebug() << "AircraftInformation::getAircraftInformation: Creating fast database";
        m_aircraftInformation = QSharedPointer<const QHash<int, AircraftInformation *>>(OsnDB::readOSNDB(getOSNDBFilename()));
        if (m_aircraftInformation)
        {
            OsnDB::writeFastDB(OsnDB::getFastDBFilename(), m_aircraftInformation.get());
            fastModifiedDateTime = fastFileInfo.lastModified();
            m_modifiedDateTime = fastModifiedDateTime;
            m_aircraftInformationByReg = QSharedPointer<const QHash<QString, AircraftInformation *>> (OsnDB::registrationHash(m_aircraftInformation.get()));
        }
    }

    if (!m_aircraftInformation || (fastModifiedDateTime > m_modifiedDateTime))
    {
        m_aircraftInformation = QSharedPointer<const QHash<int, AircraftInformation *>>(OsnDB::readFastDB(getFastDBFilename()));
        if (m_aircraftInformation)
        {
            m_modifiedDateTime = fastModifiedDateTime;
            m_aircraftInformationByReg = QSharedPointer<const QHash<QString, AircraftInformation *>> (OsnDB::registrationHash(m_aircraftInformation.get()));
        }
    }

    return m_aircraftInformation;
}

QSharedPointer<const QHash<QString, AircraftInformation *>> OsnDB::getAircraftInformationByReg()
{
    getAircraftInformation();
    return m_aircraftInformationByReg;
}

QHash<int, AircraftInformation *> *OsnDB::readOSNDB(const QString &filename)
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
                        // Ignore entries with uppercase - might be better to try and merge?
                        // See: https://opensky-network.org/forum/bug-reports/652-are-the-aircraft-database-dumps-working
                        for (int i = 0; icaoString[i] != '\0'; i++)
                        {
                            char c = icaoString[i];
                            if ((c >= 'A') && (c <= 'F'))
                            {
                                icao = 0;
                                break;
                            }
                        }
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

QHash<QString, AircraftInformation *> *OsnDB::registrationHash(const QHash<int, AircraftInformation *> *in)
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

bool OsnDB::writeFastDB(const QString &filename, const QHash<int, AircraftInformation *> *aircraftInfo)
{
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write("icao24,registration,manufacturername,model,owner,operator,operatoricao,registered\n");
        QHash<int, AircraftInformation *>::const_iterator i = aircraftInfo->begin();
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

QHash<int, AircraftInformation *> *OsnDB::readFastDB(const QString &filename)
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

QString AircraftInformation::getFlag() const
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

QString AircraftInformation::getAirlineIconPath(const QString &operatorICAO)
{
    QString endPath = QString("/airlinelogos/%1.bmp").arg(operatorICAO);
    // Try in user directory first, so they can customise
    QString userIconPath = OsnDB::getDataDir() + endPath;
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

QIcon *AircraftInformation::getAirlineIcon(const QString &operatorICAO)
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

QString AircraftInformation::getFlagIconPath(const QString &country)
{
    QString endPath = QString("/flags/%1.bmp").arg(country);
    // Try in user directory first, so they can customise
    QString userIconPath = OsnDB::getDataDir() + endPath;
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

QString AircraftInformation::getFlagIconURL(const QString &country)
{
    QString path = getFlagIconPath(country);
    if (path.startsWith(':')) {
        path = "qrc://" + path.mid(1);
    }
    return path;
}

QIcon *AircraftInformation::getFlagIcon(const QString &country)
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

