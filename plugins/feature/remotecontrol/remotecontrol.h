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

#ifndef INCLUDE_FEATURE_REMOTECONTROL_H_
#define INCLUDE_FEATURE_REMOTECONTROL_H_

#include "feature/feature.h"
#include "util/message.h"

#include "remotecontrolsettings.h"

class WebAPIAdapterInterface;
class RemoteControlWorker;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class RemoteControl : public Feature
{
	Q_OBJECT
public:
    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }

        static MsgStartStop* create(bool startStop) {
            return new MsgStartStop(startStop);
        }

    protected:
        bool m_startStop;

        MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    class MsgConfigureRemoteControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RemoteControlSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRemoteControl* create(const RemoteControlSettings& settings, bool force) {
            return new MsgConfigureRemoteControl(settings, force);
        }

    private:
        RemoteControlSettings m_settings;
        bool m_force;

        MsgConfigureRemoteControl(const RemoteControlSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgDeviceGetState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgDeviceGetState* create() {
            return new MsgDeviceGetState();
        }

    protected:

        MsgDeviceGetState() :
            Message()
        { }
    };

    class MsgDeviceSetState : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getProtocol() const { return m_protocol; }
        QString getDeviceId() const { return m_deviceId; }
        QString getId() const { return m_id; }
        QVariant getValue() const { return m_value; }

        static MsgDeviceSetState* create(const QString &protocol, const QString &deviceId, const QString &id, QVariant value) {
            return new MsgDeviceSetState(protocol, deviceId, id, value);
        }

    protected:
        QString m_protocol;
        QString m_deviceId;
        QString m_id;
        QVariant m_value;

        MsgDeviceSetState(const QString &protocol, const QString &deviceId, const QString &id, QVariant value) :
            Message(),
            m_protocol(protocol),
            m_deviceId(deviceId),
            m_id(id),
            m_value(value)
        { }
    };

    class MsgDeviceStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getProtocol() const { return m_protocol; }
        QString getDeviceId() const { return m_deviceId; }
        QHash<QString, QVariant> getStatus() const { return m_status; }

        static MsgDeviceStatus* create(const QString &protocol, const QString &deviceId, const QHash<QString, QVariant> status) {
            return new MsgDeviceStatus(protocol, deviceId, status);
        }

    protected:
        QString m_protocol;
        QString m_deviceId;
        QHash<QString, QVariant> m_status;

        MsgDeviceStatus(const QString &protocol, const QString &deviceId, const QHash<QString, QVariant> status) :
            Message(),
            m_protocol(protocol),
            m_deviceId(deviceId),
            m_status(status)
        { }
    };

    class MsgDeviceError : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getErrorMessage() const { return m_errorMessage; }

        static MsgDeviceError* create(const QString &errorMessage) {
            return new MsgDeviceError(errorMessage);
        }

    protected:
        QString m_errorMessage;

        MsgDeviceError(const QString &errorMessage) :
            Message(),
            m_errorMessage(errorMessage)
        { }
    };

    class MsgDeviceUnavailable : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getProtocol() const { return m_protocol; }
        QString getDeviceId() const { return m_deviceId; }

        static MsgDeviceUnavailable* create(const QString &protocol, const QString &deviceId) {
            return new MsgDeviceUnavailable(protocol, deviceId);
        }

    protected:
        QString m_protocol;
        QString m_deviceId;

        MsgDeviceUnavailable(const QString &protocol, const QString &deviceId) :
            Message(),
            m_protocol(protocol),
            m_deviceId(deviceId)
        { }
    };

    RemoteControl(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~RemoteControl();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

private:
    QThread *m_thread;
    RemoteControlWorker *m_worker;
    RemoteControlSettings m_settings;

    void start();
    void stop();
    void applySettings(const RemoteControlSettings& settings, bool force = false);

};

#endif // INCLUDE_FEATURE_REMOTECONTROL_H_
