///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#ifndef INCLUDE_DEVICE_H
#define INCLUDE_DEVICE_H

#include <QtCore>

#include "export.h"

class QNetworkAccessManager;
class QNetworkReply;

class SDRBASE_API DeviceDiscoverer : public QObject
{
    Q_OBJECT
public:
    enum Type {
        AUTO,
        BOOL,
        INT,
        FLOAT,
        STRING,
        LIST,
        BUTTON
    };
    enum WidgetType {
        SPIN_BOX,
        DIAL,
        SLIDER
    };

    struct SDRBASE_API ControlInfo {
        QString m_name;
        QString m_id;
        Type m_type;            // Data type
        float m_min;            // Min/max when m_type=INT/FLOAT
        float m_max;
        float m_scale;
        int m_precision;
        QStringList m_values;   // Allowed values when m_type==LIST or label for button when m_type==BUTTON
        WidgetType m_widgetType;// For m_type==FLOAT
        QString m_units;

        ControlInfo();
        virtual ~ControlInfo() {}
        operator QString() const;
        virtual ControlInfo *clone() const;
        virtual QByteArray serialize() const;
        virtual bool deserialize(const QByteArray& data);
    };

    struct SDRBASE_API SensorInfo {
        QString m_name;
        QString m_id;
        Type m_type;
        QString m_units;        // W/Watts etc

	virtual ~SensorInfo() {}
        operator QString() const;
        virtual SensorInfo *clone() const;
        virtual QByteArray serialize() const;
        virtual bool deserialize(const QByteArray& data);
    };

    struct SDRBASE_API DeviceInfo {
        QString m_name;     // User friendly name
        QString m_id;       // ID for the device used by the API
        QString m_model;    // Model name
        QList<ControlInfo *> m_controls;
        QList<SensorInfo *> m_sensors;

        DeviceInfo();
        DeviceInfo(const DeviceInfo &info);
        ~DeviceInfo();
        DeviceInfo& operator=(const DeviceInfo &info);
        operator QString() const;
        QByteArray serialize() const;
        bool deserialize(const QByteArray& data);
        ControlInfo *getControl(const QString &id) const;
        SensorInfo *getSensor(const QString &id) const;
        void deleteControl(const QString &id);
        void deleteSensor(const QString &id);
    };

protected:
    DeviceDiscoverer();

public:
    static DeviceDiscoverer *getDiscoverer(const QHash<QString, QVariant>& settings, const QString& protocol="TPLink");
    static const QStringList m_typeStrings;
    static const QStringList m_widgetTypeStrings;

    virtual void getDevices() = 0;

signals:
    void deviceList(const QList<DeviceInfo> &devices);
    void error(const QString &msg);
};

class SDRBASE_API Device : public QObject
{
    Q_OBJECT
protected:
    Device(DeviceDiscoverer::DeviceInfo *info=nullptr);

public:

    static Device* create(const QHash<QString, QVariant>& settings, const QString& protocol="TPLink", DeviceDiscoverer::DeviceInfo *info=nullptr);
    static bool checkSettings(const QHash<QString, QVariant>& settings, const QString& protocol);

    virtual void getState() = 0;
    virtual void setState(const QString &controlId, bool state);
    virtual void setState(const QString &controlId, int state);
    virtual void setState(const QString &controlId, float state);
    virtual void setState(const QString &controlId, const QString &state);
    virtual QString getProtocol() const = 0;
    virtual QString getDeviceId() const = 0;

signals:
    void deviceUpdated(QHash<QString, QVariant>);   // Called when new state available. Hash keys are control and sensor IDs
    void deviceUnavailable();                       // Called when device is unavailable. error() isn't signalled, as we expect devices to come and go
    void error(const QString &msg);                 // Called on terminal error, such as invalid authentication details

protected:
    DeviceDiscoverer::DeviceInfo m_info;

    QHash<void *, QDateTime> m_getRequests;         // These data and functions help prevent using stale data from slow getStates
    QHash<QString, QDateTime> m_setRequests;

    void recordGetRequest(void *ptr);
    void removeGetRequest(void *ptr);
    void recordSetRequest(const QString &id, int guardMS=0);
    bool getAfterSet(void *ptr, const QString &id);

};

#endif /* INCLUDE_DEVICE_H */
