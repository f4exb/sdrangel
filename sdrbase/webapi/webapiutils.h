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
#include <QMap>

#include "export.h"

class SDRBASE_API WebAPIUtils
{
public:
    static const QMap<QString, QString> m_channelURIToSettingsKey;
    static const QMap<QString, QString> m_deviceIdToSettingsKey;
    static const QMap<QString, QString> m_channelTypeToSettingsKey;
    static const QMap<QString, QString> m_sourceDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_sinkDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_mimoDeviceHwIdToSettingsKey;
    static const QMap<QString, QString> m_channelTypeToActionsKey;
    static const QMap<QString, QString> m_sourceDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_sinkDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_mimoDeviceHwIdToActionsKey;
    static const QMap<QString, QString> m_featureTypeToSettingsKey;
    static const QMap<QString, QString> m_featureTypeToActionsKey;
    static const QMap<QString, QString> m_featureURIToSettingsKey;

    static bool getObjectInt(const QJsonObject &json, const QString &key, int &value);
    static bool getObjectString(const QJsonObject &json, const QString &key, QString &value);
    static bool getObjectObjects(const QJsonObject &json, const QString &key, QList<QJsonObject> &objects);
    static bool getSubObjectDouble(const QJsonObject &json, const QString &key, double &value);
    static bool setSubObjectDouble(QJsonObject &json, const QString &key, double value);
    static bool getSubObjectInt(const QJsonObject &json, const QString &key, int &value);
    static bool setSubObjectInt(QJsonObject &json, const QString &key, int value);
    static bool getSubObjectString(const QJsonObject &json, const QString &key, QString &value);
    static bool setSubObjectString(QJsonObject &json, const QString &key, const QString &value);
    static bool getSubObjectIntList(const QJsonObject &json, const QString &key, const QString &subKey, QList<int> &values);
    static bool extractValue(const QJsonObject &json, const QString &key, QJsonValue &value);
    static bool extractArray(const QJsonObject &json, const QString &key, QJsonArray &value);
    static bool extractObject(const QJsonObject &json, const QString &key, QJsonObject &value);
    static bool setValue(const QJsonObject &json, const QString &key, const QJsonValue &value);
    static bool setArray(const QJsonObject &json, const QString &key, const QJsonArray &value);
    static bool setObject(const QJsonObject &json, const QString &key, const QJsonObject &value);
};

#endif
