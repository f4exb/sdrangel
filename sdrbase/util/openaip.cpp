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

#include "openaip.h"

QList<Airspace *> Airspace::readXML(const QString &filename)
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

QList<NavAid *> NavAid::readXML(const QString &filename)
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

const QStringList OpenAIP::m_countryCodes = {
    "ad",
    "ae",
    "af",
    "ag",
    "ai",
    "al",
    "am",
    "an",
    "ao",
    "aq",
    "ar",
    "as",
    "at",
    "au",
    "aw",
    "ax",
    "az",
    "ba",
    "bb",
    "bd",
    "be",
    "bf",
    "bg",
    "bh",
    "bi",
    "bj",
    "bl",
    "bm",
    "bn",
    "bo",
    "bq",
    "br",
    "bs",
    "bt",
    "bv",
    "bw",
    "by",
    "bz",
    "ca",
    "cc",
    "cd",
    "cf",
    "cg",
    "ch",
    "ci",
    "ck",
    "cl",
    "cm",
    "cn",
    "co",
    "cr",
    "cu",
    "cv",
    "cw",
    "cx",
    "cy",
    "cz",
    "de",
    "dj",
    "dk",
    "dm",
    "do",
    "dz",
    "ec",
    "ee",
    "eg",
    "eh",
    "er",
    "es",
    "et",
    "fi",
    "fj",
    "fk",
    "fm",
    "fo",
    "fr",
    "ga",
    "gb",
    "ge",
    "gf",
    "gg",
    "gh",
    "gi",
    "gl",
    "gm",
    "gn",
    "gp",
    "gq",
    "gr",
    "gs",
    "gt",
    "gu",
    "gw",
    "gy",
    "hk",
    "hm",
    "hn",
    "hr",
    "hu",
    "id",
    "ie",
    "il",
    "im",
    "in",
    "io",
    "iq",
    "ir",
    "is",
    "it",
    "je",
    "jm",
    "jo",
    "jp",
    "ke",
    "kg",
    "kh",
    "ki",
    "km",
    "kn",
    "kp",
    "kr",
    "kw",
    "ky",
    "kz",
    "la",
    "lb",
    "lc",
    "li",
    "lk",
    "lr",
    "ls",
    "lt",
    "lu",
    "lv",
    "ly",
    "ma",
    "mc",
    "md",
    "me",
    "mf",
    "mg",
    "mh",
    "mk",
    "ml",
    "mm",
    "mn",
    "mo",
    "mp",
    "mq",
    "mr",
    "ms",
    "mt",
    "mu",
    "mv",
    "mw",
    "mx",
    "my",
    "mz",
    "na",
    "nc",
    "ne",
    "nf",
    "ng",
    "ni",
    "nl",
    "no",
    "np",
    "nr",
    "nu",
    "nz",
    "om",
    "pa",
    "pe",
    "pf",
    "pg",
    "ph",
    "pk",
    "pl",
    "pm",
    "pn",
    "pr",
    "ps",
    "pt",
    "pw",
    "py",
    "qa",
    "re",
    "ro",
    "rs",
    "ru",
    "rw",
    "sa",
    "sb",
    "sc",
    "sd",
    "se",
    "sg",
    "sh",
    "si",
    "sj",
    "sk",
    "sl",
    "sm",
    "sn",
    "so",
    "sr",
    "ss",
    "st",
    "sv",
    "sx",
    "sy",
    "sz",
    "tc",
    "td",
    "tf",
    "tg",
    "th",
    "tj",
    "tk",
    "tl",
    "tm",
    "tn",
    "to",
    "tr",
    "tt",
    "tv",
    "tw",
    "tz",
    "ua",
    "ug",
    "um",
    "us",
    "uy",
    "uz",
    "va",
    "vc",
    "ve",
    "vg",
    "vi",
    "vn",
    "vu",
    "wf",
    "ws",
    "ye",
    "yt",
    "za",
    "zm",
    "zw"
};

QSharedPointer<QList<Airspace *>> OpenAIP::m_airspaces;
QSharedPointer<QList<NavAid *>> OpenAIP::m_navAids;

QDateTime OpenAIP::m_airspacesModifiedDateTime;
QDateTime OpenAIP::m_navAidsModifiedDateTime;

OpenAIP::OpenAIP(QObject *parent) :
    QObject(parent)
{
    connect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OpenAIP::downloadFinished);
}

OpenAIP::~OpenAIP()
{
    disconnect(&m_dlm, &HttpDownloadManager::downloadComplete, this, &OpenAIP::downloadFinished);
}

QString OpenAIP::getDataDir()
{
    // Get directory to store app data in
    QStringList locations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    // First dir is writable
    return locations[0];
}

QString OpenAIP::getAirspaceFilename(int i)
{
    return getAirspaceFilename(m_countryCodes[i]);
}

QString OpenAIP::getAirspaceFilename(const QString& countryCode)
{
    return getDataDir() + "/" + countryCode + "_asp.xml";
}

QString OpenAIP::getAirspaceURL(int i)
{
    if (i < m_countryCodes.size()) {
        return QString(OPENAIP_AIRSPACE_URL).arg(m_countryCodes[i]);
    } else {
        return QString();
    }
}

void OpenAIP::downloadAirspaces()
{
    m_countryIndex = 0;
    downloadAirspace();
}

void OpenAIP::downloadAirspace()
{
    QString filename = getAirspaceFilename(m_countryIndex);
    QString urlString = getAirspaceURL(m_countryIndex);
    QUrl dbURL(urlString);
    qDebug() << "OpenAIP::downloadAirspace: Downloading " << urlString;
    emit downloadingURL(urlString);
    m_dlm.download(dbURL, filename);
}

QString OpenAIP::getNavAidsFilename(int i)
{
    return getNavAidsFilename(m_countryCodes[i]);
}

QString OpenAIP::getNavAidsFilename(const QString& countryCode)
{
    return getDataDir() + "/" + countryCode + "_nav.xml";
}

QString OpenAIP::getNavAidsURL(int i)
{
    if (i < m_countryCodes.size()) {
        return QString(OPENAIP_NAVAIDS_URL).arg(m_countryCodes[i]);
    } else {
        return QString();
    }
}

void OpenAIP::downloadNavAids()
{
    m_countryIndex = 0;
    downloadNavAid();
}

void OpenAIP::downloadNavAid()
{
    QString filename = getNavAidsFilename(m_countryIndex);
    QString urlString = getNavAidsURL(m_countryIndex);
    QUrl dbURL(urlString);
    qDebug() << "OpenAIP::downloadNavAid: Downloading " << urlString;
    emit downloadingURL(urlString);
    m_dlm.download(dbURL, filename);
}

void OpenAIP::downloadFinished(const QString& filename, bool success)
{
    // Not all countries have corresponding files, so we should expect some errors
    if (!success) {
        qDebug() << "OpenAIP::downloadFinished: Failed: " << filename;
    }

    if (filename == getNavAidsFilename(m_countryIndex))
    {
        m_countryIndex++;
        if (m_countryIndex < m_countryCodes.size()) {
            downloadNavAid();
        } else {
            emit downloadNavAidsFinished();
        }
    }
    else if (filename == getAirspaceFilename(m_countryIndex))
    {
        m_countryIndex++;
        if (m_countryIndex < m_countryCodes.size()) {
            downloadAirspace();
        } else {
            emit downloadAirspaceFinished();
        }
    }
    else
    {
        qDebug() << "OpenAIP::downloadFinished: Unexpected filename: " << filename;
        emit downloadError(QString("Unexpected filename: %1").arg(filename));
    }
}

// Read airspaces for all countries
QList<Airspace *> *OpenAIP::readAirspaces()
{
    QList<Airspace *> *airspaces = new QList<Airspace *>();
    for (const auto& countryCode : m_countryCodes) {
        airspaces->append(readAirspaces(countryCode));
    }
    return airspaces;
}

// Read airspaces for a single country
QList<Airspace *> OpenAIP::readAirspaces(const QString& countryCode)
{
    return Airspace::readXML(getAirspaceFilename(countryCode));
}

// Read NavAids for all countries
QList<NavAid *> *OpenAIP::readNavAids()
{
    QList<NavAid *> *navAids = new QList<NavAid *>();
    for (const auto& countryCode : m_countryCodes) {
        navAids->append(readNavAids(countryCode));
    }
    return navAids;
}

// Read NavAids for a single country
QList<NavAid *> OpenAIP::readNavAids(const QString& countryCode)
{
    return NavAid::readXML(getNavAidsFilename(countryCode));
}

QSharedPointer<const QList<Airspace *>> OpenAIP::getAirspaces()
{
    QDateTime filesDateTime = getAirspacesModifiedDateTime();

    if (!m_airspaces || (filesDateTime > m_airspacesModifiedDateTime))
    {
        // Using shared pointer, so old object, if it exists, will be deleted, when no longer user
        m_airspaces = QSharedPointer<QList<Airspace *>>(readAirspaces());
        m_airspacesModifiedDateTime = filesDateTime;
    }
    return m_airspaces;
}

QSharedPointer<const QList<NavAid *>> OpenAIP::getNavAids()
{
    QDateTime filesDateTime = getNavAidsModifiedDateTime();

    if (!m_navAids || (filesDateTime > m_navAidsModifiedDateTime))
    {
        // Using shared pointer, so old object, if it exists, will be deleted, when no longer user
        m_navAids = QSharedPointer<QList<NavAid *>>(readNavAids());
        m_navAidsModifiedDateTime = filesDateTime;
    }
    return m_navAids;
}

// Gets the date and time the airspaces files were most recently modified
QDateTime OpenAIP::getAirspacesModifiedDateTime()
{
    QDateTime dateTime;
    for (const auto& countryCode : m_countryCodes)
    {
        QFileInfo fileInfo(getAirspaceFilename(countryCode));
        QDateTime fileModifiedDateTime = fileInfo.lastModified();
        if (fileModifiedDateTime > dateTime) {
            dateTime = fileModifiedDateTime;
        }
    }
    return dateTime;
}

// Gets the date and time the navaid files were most recently modified
QDateTime OpenAIP::getNavAidsModifiedDateTime()
{
    QDateTime dateTime;
    for (const auto& countryCode : m_countryCodes)
    {
        QFileInfo fileInfo(getNavAidsFilename(countryCode));
        QDateTime fileModifiedDateTime = fileInfo.lastModified();
        if (fileModifiedDateTime > dateTime) {
            dateTime = fileModifiedDateTime;
        }
    }
    return dateTime;
}

