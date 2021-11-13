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
QList<Airspace *> OpenAIP::readAirspaces()
{
    QList<Airspace *> airspaces;
    for (const auto& countryCode : m_countryCodes) {
        airspaces.append(readAirspaces(countryCode));
    }
    return airspaces;
}

// Read airspaces for a single country
QList<Airspace *> OpenAIP::readAirspaces(const QString& countryCode)
{
    return Airspace::readXML(getAirspaceFilename(countryCode));
}

// Read NavAids for all countries
QList<NavAid *> OpenAIP::readNavAids()
{
    QList<NavAid *> navAids;
    for (const auto& countryCode : m_countryCodes) {
        navAids.append(readNavAids(countryCode));
    }
    return navAids;
}

// Read NavAids for a single country
QList<NavAid *> OpenAIP::readNavAids(const QString& countryCode)
{
    return NavAid::readXML(getNavAidsFilename(countryCode));
}
