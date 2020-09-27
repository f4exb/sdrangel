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

#include <QJsonArray>
#include <QList>

#include "webapiutils.h"

// Get integer value from within JSON object
bool WebAPIUtils::getObjectInt(const QJsonObject &json, const QString &key, int &value)
{
    if (json.contains(key))
    {
        value = json[key].toInt();
        return true;
    }

    return false;
}

// Get string value from within JSON object
bool WebAPIUtils::getObjectString(const QJsonObject &json, const QString &key, QString &value)
{
    if (json.contains(key))
    {
        value = json[key].toString();
        return true;
    }

    return false;
}

// Get a list of JSON objects from within JSON object
bool WebAPIUtils::getObjectObjects(const QJsonObject &json, const QString &key, QList<QJsonObject> &objects)
{
    bool processed = false;

    if (json.contains(key))
    {
        if (json[key].isArray())
        {
            QJsonArray a = json[key].toArray();

            for (QJsonArray::const_iterator  it = a.begin(); it != a.end(); it++)
            {
                if (it->isObject())
                {
                    objects.push_back(it->toObject());
                    processed = true;
                }
            }
        }
    }

    return processed;
}

// Get double value from within nested JSON object
bool WebAPIUtils::getSubObjectDouble(const QJsonObject &json, const QString &key, double &value)
{
    for (QJsonObject::const_iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                value = subObject[key].toDouble();
                return true;
            }
        }
    }

    return false;
}

// Set double value withing nested JSON object
bool WebAPIUtils::setSubObjectDouble(QJsonObject &json, const QString &key, double value)
{
    for (QJsonObject::iterator  it = json.begin(); it != json.end(); it++)
    {
        QJsonValue jsonValue = it.value();

        if (jsonValue.isObject())
        {
            QJsonObject subObject = jsonValue.toObject();

            if (subObject.contains(key))
            {
                subObject[key] = value;
                it.value() = subObject;
                return true;
            }
        }
    }

    return false;
}
