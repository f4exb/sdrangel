///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
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
QHash<QString, QString> *CSV::hash(const QString& filename, int reserve)
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

// Read a row from a CSV file (handling quotes)
// https://stackoverflow.com/questions/27318631/parsing-through-a-csv-file-in-qt
bool CSV::readRow(QTextStream &in, QStringList *row, char separator)
{
    static const int delta[][5] = {
        //  ,    "   \n    ?  eof
        {   1,   2,  -1,   0,  -1  }, // 0: parsing (store char)
        {   1,   2,  -1,   0,  -1  }, // 1: parsing (store column)
        {   3,   4,   3,   3,  -2  }, // 2: quote entered (no-op)
        {   3,   4,   3,   3,  -2  }, // 3: parsing inside quotes (store char)
        {   1,   3,  -1,   0,  -1  }, // 4: quote exited (no-op)
        // -1: end of row, store column, success
        // -2: eof inside quotes
    };

    row->clear();

    if (in.atEnd())
        return false;

    int state = 0, t;
    char ch;
    QString cell;

    while (state >= 0)
    {
        if (in.atEnd())
        {
            t = 4;
        }
        else
        {
            in >> ch;
            if (ch == separator) {
                t = 0;
            } else if (ch == '\"') {
                t = 1;
            } else if (ch == '\n') {
                t = 2;
            } else {
                t = 3;
            }
        }

        state = delta[state][t];

        switch (state) {
        case 0:
        case 3:
            cell += ch;
            break;
        case -1:
        case 1:
            row->append(cell);
            cell = "";
            break;
        }

    }

    if (state == -2) {
        return false;
    }

    return true;
}

// Read header row from CSV file and return a hash mapping names to column numbers
// Returns error if header row can't be read, or if all of requiredColumns aren't found
QHash<QString, int> CSV::readHeader(QTextStream &in, QStringList requiredColumns, QString &error, char separator)
{
    QHash<QString, int> colNumbers;
    QStringList row;

    // Read column names
    if (CSV::readRow(in, &row, separator))
    {
        // Create hash mapping column names to indices
        for (int i = 0; i < row.size(); i++) {
            colNumbers.insert(row[i], i);
        }
        // Check all required columns exist
        for (const auto& col : requiredColumns)
        {
            if (!colNumbers.contains(col)) {
                error = QString("Missing column %1").arg(col);
            }
        }
    }
    else
    {
        error = "Failed to read header row";
    }

    return colNumbers;
}

QString CSV::escape(const QString& string)
{
    QString s = string;
    s.replace('"', "\"\"");
    s = QString("\"%1\"").arg(s);
    return s;
}
