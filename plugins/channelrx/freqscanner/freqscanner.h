///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB.                             //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_FREQSCANNER_H
#define INCLUDE_FREQSCANNER_H

#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "freqscannerbaseband.h"
#include "freqscannersettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;

class FreqScanner : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureFreqScanner : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const FreqScannerSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureFreqScanner* create(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force)
        {
            return new MsgConfigureFreqScanner(settings, settingsKeys, force);
        }

    private:
        FreqScannerSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureFreqScanner(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgReportChannels : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        QList<FreqScannerSettings::AvailableChannel>& getChannels() { return m_channels; }

        static MsgReportChannels* create() {
            return new MsgReportChannels();
        }

    private:
        QList<FreqScannerSettings::AvailableChannel> m_channels;

        MsgReportChannels() :
            Message()
        {}
    };

    class MsgStartScan : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgStartScan* create()
        {
            return new MsgStartScan();
        }

    private:

        MsgStartScan() :
            Message()
        {
        }
    };

    class MsgStopScan : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgStopScan* create()
        {
            return new MsgStopScan();
        }

    private:

        MsgStopScan() :
            Message()
        {
        }
    };

    class MsgScanComplete : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgScanComplete* create()
        {
            return new MsgScanComplete();
        }

    private:

        MsgScanComplete() :
            Message()
        {
        }
    };

    class MsgScanResult : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        struct ScanResult {
            qint64 m_frequency;
            float m_power;
        };

        const QDateTime& getFFTStartTime() { return m_fftStartTime; }
        QList<ScanResult>& getScanResults() { return m_scanResults; }

        static MsgScanResult* create(const QDateTime& fftStartTime) {
            return new MsgScanResult(fftStartTime);
        }

    private:
        QDateTime m_fftStartTime;
        QList<ScanResult> m_scanResults;

        MsgScanResult(const QDateTime& fftStartTime) :
            Message(),
            m_fftStartTime(fftStartTime)
        {}
    };

    class MsgStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        const QString& getText() const { return m_text; }

        static MsgStatus* create(const QString& text)
        {
            return new MsgStatus(text);
        }

    private:

        QString m_text;

        MsgStatus(const QString& text) :
            Message(),
            m_text(text)
        {
        }
    };

    class MsgReportActiveFrequency : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        qint64 getCenterFrequency() const { return m_centerFrequency; }

        static MsgReportActiveFrequency* create(qint64 centerFrequency)
        {
            return new MsgReportActiveFrequency(centerFrequency);
        }

    private:

        qint64 m_centerFrequency;

        MsgReportActiveFrequency(qint64 centerFrequency) :
            Message(),
            m_centerFrequency(centerFrequency)
        {
        }
    };

    class MsgReportActivePower : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        float getPower() const { return m_power; }

        static MsgReportActivePower* create(float power)
        {
            return new MsgReportActivePower(power);
        }

    private:

        Real m_power;

        MsgReportActivePower(float power) :
            Message(),
            m_power(power)
        {
        }
    };

    class MsgReportScanning : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgReportScanning* create()
        {
            return new MsgReportScanning();
        }

    private:

        MsgReportScanning() :
            Message()
        {
        }
    };

    class MsgReportScanRange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        qint64 getCenterFrequency() const { return m_centerFrequency; }
        int getTotalBandwidth() const { return m_totalBandwidth; }
        int getFftSize() const { return m_fftSize; }

        static MsgReportScanRange* create(qint64 centerFrequency, int totalBandwidth, int fftSize)
        {
            return new MsgReportScanRange(centerFrequency, totalBandwidth, fftSize);
        }

    private:

        qint64 m_centerFrequency;
        int m_totalBandwidth;
        int m_fftSize;

        MsgReportScanRange(qint64 centerFrequency, int totalBandwidth, int fftSize) :
            Message(),
            m_centerFrequency(centerFrequency),
            m_totalBandwidth(totalBandwidth),
            m_fftSize(fftSize)
        {
        }
    };

    FreqScanner(DeviceAPI *deviceAPI);
    virtual ~FreqScanner();
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
            const FreqScannerSettings& settings);

    static void webapiUpdateChannelSettings(
            FreqScannerSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    void setMessageQueueToGUI(MessageQueue* queue) override {
        ChannelAPI::setMessageQueueToGUI(queue);
        m_basebandSink->setMessageQueueToGUI(queue);
    }

    uint32_t getNumberOfDeviceStreams() const;

    void calcScannerSampleRate(int channelBW, int basebandSampleRate, int& scannerSampleRate, int& fftSize, int& binsPerChannel);

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread *m_thread;
    FreqScannerBaseband* m_basebandSink;
    QRecursiveMutex m_mutex;
    bool m_running;
    FreqScannerSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    QHash<ChannelAPI*, FreqScannerSettings::AvailableChannel> m_availableChannels;

    int m_scanDeviceSetIndex;
    int m_scanChannelIndex;
    qint64 m_activeFrequency;
    QDateTime m_minFFTStartTime;
    int m_scannerSampleRate;
    bool m_stepping;
    qint64 m_stepStartFrequency;
    qint64 m_stepStopFrequency;
    QList<MsgScanResult::ScanResult> m_scanResults;

    enum State {
        IDLE,
        START_SCAN,
        SCAN_FOR_MAX_POWER,
        WAIT_FOR_END_TX,
        WAIT_FOR_RETRANSMISSION
    } m_state;

    QTimer m_timeoutTimer;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force = false);
    void webapiReverseSendSettings(const QStringList& channelSettingsKeys, const FreqScannerSettings& settings, bool force);
    void webapiFormatChannelSettings(
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FreqScannerSettings& settings,
        bool force
    );
    void webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response);

    void scanAvailableChannels();
    void notifyUpdateChannels();
    void startScan();
    void stopScan();
    void initScan();
    void processScanResults(const QDateTime& fftStartTime, const QList<MsgScanResult::ScanResult>& results);
    void setDeviceCenterFrequency(qint64 frequency);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void handleIndexInDeviceSetChanged(int index);
    void handleChannelAdded(int deviceSetIndex, ChannelAPI* channel);
    void handleChannelRemoved(int deviceSetIndex, ChannelAPI* channel);
    void timeout();

};

#endif // INCLUDE_FREQSCANNER_H

