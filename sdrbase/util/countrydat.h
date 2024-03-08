///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2024 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

// This source code file was last time modified by Arvo ES1JA on January 5th, 2019
// All changes are shown in the patch file coming together with the full JTDX source code.

/*
 * Reads cty.dat file
 * Establishes a map between prefixes and their country names
 * VK3ACF July 2013
 */


#ifndef INCLUDE_COUNTRYDAT_H
#define INCLUDE_COUNTRYDAT_H

#include <QCoreApplication>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QDate>

#include "export.h"

class SDRBASE_API CountryDat
{
public:
    struct CountryInfo {
        QString continent;
        QString masterPrefix;
        QString country;
        QString cqZone;
        QString ituZone;
    };

    void init();
    void load();
    const QHash<QString, CountryInfo>& getCountries() const { return _countries; }
    static const CountryInfo nullCountry;

private:
    QString _extractName(const QString line);
    QString _extractMasterPrefix(const QString line);
    QString _extractContinent(const QString line);
    QString _extractCQZ(const QString line);
    QString _extractITUZ(const QString line);
    QString _removeBrackets(QString &line, const QString a, const QString b);
    QStringList _extractPrefix(QString &line, bool &more);

    QString _filename;
    QHash<QString, QString> _name;
    QHash<QString, CountryInfo> _countries;
};

#endif
