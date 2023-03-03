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

#ifndef INCLUDE_CSV_H
#define INCLUDE_CSV_H

#include <QString>
#include <QHash>
#include <QTextStream>

#include "export.h"

// Extract string from CSV line, updating pp to next column (This doesn't handle , inside quotes)
static inline char *csvNext(char **pp, char delimiter=',')
{
    char *p = *pp;

    if (p[0] == '\0')
        return nullptr;

    char *start = p;

    while ((*p != delimiter) && (*p != '\n'))
        p++;
    *p++ = '\0';
    *pp = p;

    return start;
}

struct SDRBASE_API CSV {

    static QHash<QString, QString> *hash(const QString& filename, int reserve=0);

    static bool readRow(QTextStream &in, QStringList *row, char seperator=',');
    static QHash<QString, int> readHeader(QTextStream &in, QStringList requiredColumns, QString &error, char seperator=',');

};

#endif /* INCLUDE_CSV_H */
