///////////////////////////////////////////////////////////////////////////////////
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

#ifndef PLUGINS_CHANNELTX_MODLORA_LORAMOD_H_
#define PLUGINS_CHANNELTX_MODLORA_LORAMOD_H_

#include <vector>
#include <iostream>
#include <fstream>

#include <QMutex>
#include <QNetworkRequest>

#include "dsp/basebandsamplesource.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "loramodsettings.h"
#include "loramodencoder.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class CWKeyer;
class LoRaModBaseband;

class LoRaMod : public BasebandSampleSource, public ChannelAPI {
    Q_OBJECT

public:
    class MsgConfigureLoRaMod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const LoRaModSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureLoRaMod* create(const LoRaModSettings& settings, bool force)
        {
            return new MsgConfigureLoRaMod(settings, force);
        }

    private:
        LoRaModSettings m_settings;
        bool m_force;

        MsgConfigureLoRaMod(const LoRaModSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgReportPayloadTime : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        float getPayloadTimeMs() const { return m_timeMs; }
        static MsgReportPayloadTime* create(float timeMs) {
            return new MsgReportPayloadTime(timeMs);
        }

    private:
        float m_timeMs; //!< time in milliseconds

        MsgReportPayloadTime(float timeMs) :
            Message(),
            m_timeMs(timeMs)
        {}
    };

    //=================================================================

    LoRaMod(DeviceAPI *deviceAPI);
    ~LoRaMod();
    virtual void destroy() { delete this; }

    virtual void start();
    virtual void stop();
    virtual void pull(SampleVector::iterator& begin, unsigned int nbSamples);
    virtual bool handleMessage(const Message& cmd);

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return m_settings.m_inputFrequencyOffset;
    }

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage);

    virtual int webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage);

    static void webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const LoRaModSettings& settings);

    static void webapiUpdateChannelSettings(
            LoRaModSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const;
    CWKeyer *getCWKeyer();
    void setLevelMeter(QObject *levelMeter);
    uint32_t getNumberOfDeviceStreams() const;
    bool getModulatorActive() const;

    static const QString m_channelIdURI;
    static const QString m_channelId;

private:
    DeviceAPI* m_deviceAPI;
    QThread *m_thread;
    LoRaModBaseband* m_basebandSource;
    LoRaModEncoder m_encoder; // TODO: check if it needs to be on its own thread
    LoRaModSettings m_settings;

    SampleVector m_sampleBuffer;
    QMutex m_settingsMutex;

    int m_sampleRate;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const LoRaModSettings& settings, bool force = false);
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LoRaModSettings& settings, bool force);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};


#endif /* PLUGINS_CHANNELTX_MODLORA_LORAMOD_H_ */
