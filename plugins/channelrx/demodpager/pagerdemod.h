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

#ifndef INCLUDE_PAGERDEMOD_H
#define INCLUDE_PAGERDEMOD_H

#include <vector>

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "pagerdemodbaseband.h"
#include "pagerdemodsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class ScopeVis;

class PagerDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigurePagerDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const PagerDemodSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigurePagerDemod* create(const PagerDemodSettings& settings, bool force) {
            return new MsgConfigurePagerDemod(settings, force);
        }

    private:
        PagerDemodSettings m_settings;
        bool m_force;

        MsgConfigurePagerDemod(const PagerDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgPagerMessage : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getAddress() const { return m_address; }
        int getFunctionBits() const { return m_functionBits; }
        QString getAlphaMessage() const { return m_alphaMessage; }
        QString getNumericMessage() const { return m_numericMessage; }
        int getEvenParityErrors() const { return m_evenParityErrors; }
        int getBCHParityErrors() const { return m_bchParityErrors; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgPagerMessage* create(
            int address,
            int functionBits,
            const QString& alphaMessage,
            const QString& numericMessage,
            int evenParityErrors,
            int bchParityErrors
        )
        {
            return new MsgPagerMessage(
                address,
                functionBits,
                alphaMessage,
                numericMessage,
                evenParityErrors,
                bchParityErrors,
                QDateTime::currentDateTime()
            );
        }

    private:
        int m_address;
        int m_functionBits;
        QString m_alphaMessage;
        QString m_numericMessage;
        int m_evenParityErrors;
        int m_bchParityErrors;
        QDateTime m_dateTime;

        MsgPagerMessage(int address, int functionBits, const QString& alphaMessage, const QString& numericMessage, int evenParityErrors, int bchParityErrors, QDateTime dateTime) :
            Message(),
            m_address(address),
            m_functionBits(functionBits),
            m_alphaMessage(alphaMessage),
            m_numericMessage(numericMessage),
            m_evenParityErrors(evenParityErrors),
            m_bchParityErrors(bchParityErrors),
            m_dateTime(dateTime)
        {
        }
    };

    PagerDemod(DeviceAPI *deviceAPI);
    virtual ~PagerDemod();
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

    virtual int webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage);

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const PagerDemodSettings& settings);

    static void webapiUpdateChannelSettings(
            PagerDemodSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    ScopeVis *getScopeSink();
    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }
/*    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }*/

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    PagerDemodBaseband* m_basebandSink;
    PagerDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;
    QUdpSocket m_udpSocket;
    QFile m_logFile;
    QTextStream m_logStream;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const PagerDemodSettings& settings, bool force = false);
    void sendSampleRateToDemodAnalyzer();
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const PagerDemodSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const PagerDemodSettings& settings,
        bool force
    );
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_PAGERDEMOD_H
