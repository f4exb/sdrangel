///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef SDRBASE_WEBAPI_WEBAPIUTILS_H_
#define SDRBASE_WEBAPI_WEBAPIUTILS_H_

#include <QJsonObject>
#include <QString>

#include "export.h"

class SDRBASE_API WebAPIUtils
{
public:
    static bool getObjectInt(const QJsonObject &json, const QString &key, int &value);
    static bool getObjectString(const QJsonObject &json, const QString &key, QString &value);
    static bool getObjectObjects(const QJsonObject &json, const QString &key, QList<QJsonObject> &objects);
    static bool getSubObjectDouble(const QJsonObject &json, const QString &key, double &value);
    static bool setSubObjectDouble(QJsonObject &json, const QString &key, double value);
};

#endif
