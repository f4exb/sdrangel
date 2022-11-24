///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#ifndef INCLUDE_FEATURE_JOGDIALCONTROLLER_H_
#define INCLUDE_FEATURE_JOGDIALCONTROLLER_H_

#include <QList>
#include <QNetworkRequest>
#include <QTimer>

#include "feature/feature.h"
#include "util/message.h"

#include "jogdialcontrollersettings.h"

class WebAPIAdapterInterface;
class QNetworkAccessManager;
class QNetworkReply;

namespace SWGSDRangel {
    class SWGDeviceState;
}

class JogdialController : public Feature
{
	Q_OBJECT
public:
    class MsgConfigureJogdialController : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const JogdialControllerSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureJogdialController* create(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureJogdialController(settings, settingsKeys, force);
        }

    private:
        JogdialControllerSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureJogdialController(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

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

    class MsgRefreshChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgRefreshChannels* create() {
            return new MsgRefreshChannels();
        }

    protected:
        MsgRefreshChannels() :
            Message()
        { }
    };

    class MsgReportChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<JogdialControllerSettings::AvailableChannel>& getAvailableChannels() { return m_availableChannels; }

        static MsgReportChannels* create() {
            return new MsgReportChannels();
        }

    private:
        QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;

        MsgReportChannels() :
            Message()
        {}
    };

    class MsgSelectChannel : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getIndex() const { return m_index; }
        static MsgSelectChannel* create(int index) {
            return new MsgSelectChannel(index);
        }

    protected:
        int m_index;

        MsgSelectChannel(int index) :
            Message(),
            m_index(index)
        { }
    };

    class MsgReportControl : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getDeviceElseChannel() const { return m_deviceElseChannel; }

        static MsgReportControl* create(bool deviceElseChannel) {
            return new MsgReportControl(deviceElseChannel);
        }

    protected:
        bool m_deviceElseChannel;

        MsgReportControl(bool deviceElseChannel) :
            Message(),
            m_deviceElseChannel(deviceElseChannel)
        { }
    };

    JogdialController(WebAPIAdapterInterface *webAPIAdapterInterface);
    virtual ~JogdialController();
    virtual void destroy() { delete this; }
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) const { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual void getTitle(QString& title) const { title = m_settings.m_title; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int webapiRun(bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response,
            QString& errorMessage);

    void resetChannelFrequency();
    void stepFrequency(int step);

    static void webapiFormatFeatureSettings(
        SWGSDRangel::SWGFeatureSettings& response,
        const JogdialControllerSettings& settings);

    static void webapiUpdateFeatureSettings(
            JogdialControllerSettings& settings,
            const QStringList& featureSettingsKeys,
            SWGSDRangel::SWGFeatureSettings& response);

    static const char* const m_featureIdURI;
    static const char* const m_featureId;

public slots:
    void commandKeyPressed(Qt::Key key, Qt::KeyboardModifiers keyModifiers, bool release);

private:
    JogdialControllerSettings m_settings;
    QList<JogdialControllerSettings::AvailableChannel> m_availableChannels;
    DeviceAPI *m_selectedDevice;
    ChannelAPI *m_selectedChannel;
    int m_selectedIndex;
    bool m_deviceElseChannelControl;
    int m_multiplier;
    QTimer m_repeatTimer;
    static const int m_repeatms = 100;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void start();
    void stop();
    void applySettings(const JogdialControllerSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    void updateChannels();
    void channelUp();
    void channelDown();
    void webapiReverseSendSettings(const QList<QString>& featureSettingsKeys, const JogdialControllerSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleChannelMessageQueue(MessageQueue *messageQueues);
    void handleRepeat();
};

#endif // INCLUDE_FEATURE_DEMODANALYZER_H_
