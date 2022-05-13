///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RADIOSONDEDEMOD_H
#define INCLUDE_RADIOSONDEDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QVector3D>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/radiosonde.h"

#include "radiosondedemodbaseband.h"
#include "radiosondedemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ScopeVis;

class RadiosondeDemod : public BasebandSampleSink, public ChannelAPI {

public:
    class MsgConfigureRadiosondeDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadiosondeDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadiosondeDemod* create(const RadiosondeDemodSettings& settings, bool force)
        {
            return new MsgConfigureRadiosondeDemod(settings, force);
        }

    private:
        RadiosondeDemodSettings m_settings;
        bool m_force;

        MsgConfigureRadiosondeDemod(const RadiosondeDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QByteArray getMessage() const { return m_message; }
        QDateTime getDateTime() const { return m_dateTime; }
        int getErrorsCorrected() const { return m_errorsCorrected; }
        int getThreshold() const { return m_threshold; }

        static MsgMessage* create(QByteArray message, int errorsCorrected, int threshold)
        {
            return new MsgMessage(message, QDateTime::currentDateTime(), errorsCorrected, threshold);
        }

    private:
        QByteArray m_message;
        QDateTime m_dateTime;
        int m_errorsCorrected;
        int m_threshold;

        MsgMessage(QByteArray message, QDateTime dateTime, int errorsCorrected, int threshold) :
            Message(),
            m_message(message),
            m_dateTime(dateTime),
            m_errorsCorrected(errorsCorrected),
            m_threshold(threshold)
        {
        }
    };

    RadiosondeDemod(DeviceAPI *deviceAPI);
    virtual ~RadiosondeDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    virtual int webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const RadiosondeDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            RadiosondeDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    ScopeVis *getScopeSink();
    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    RadiosondeDemodBaseband* m_basebandSink;
    RadiosondeDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    QHash<QString, RS41Subframe *> m_subframes; // Hash of serial to subframes

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const RadiosondeDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RadiosondeDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadiosondeDemodSettings& settings,
        bool force
    );

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_RADIOSONDEDEMOD_H
