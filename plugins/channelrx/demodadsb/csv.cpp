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

#include "csv.h"

#include <QString>
#include <QFile>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QDebug>

// Create a hash map from a CSV file with two columns
QHash<QString, QString> *csvHash(const QString& filename, int reserve)
{
    int cnt = 0;
    QHash<QString, QString> *map = nullptr;

    qDebug() << "csvHash: " << filename;

    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        // Read header
        if (!file.atEnd())
        {
            QByteArray row = file.readLine().trimmed();
            if (row.split(',').size() == 2)
            {
                map = new QHash<QString, QString>();
                if (reserve > 0)
                    map->reserve(reserve);
                // Read data
                while (!file.atEnd())
                {
                    row = file.readLine().trimmed();
                    QList<QByteArray> cols = row.split(',');
                    map->insert(QString(cols[0]), QString(cols[1]));
                    cnt++;
                }
            }
            else
                qDebug() << "csvHash: Unexpected header";
        }
        else
            qDebug() << "csvHash: Empty file";
        file.close();
    }
    else
        qDebug() << "csvHash: Failed to open " << filename;

    qDebug() << "csvHash: " << filename << ": read " << cnt << " entries";

    return map;
}
