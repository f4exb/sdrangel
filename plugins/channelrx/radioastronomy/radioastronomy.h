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

#ifndef INCLUDE_RADIOASTRONOMY_H
#define INCLUDE_RADIOASTRONOMY_H

#include <vector>

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>
#include <QHash>
#include <QTimer>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "radioastronomybaseband.h"
#include "radioastronomysettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class Feature;
class RadioAstronomyWorker;

class RadioAstronomy : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureRadioAstronomy : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const RadioAstronomySettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureRadioAstronomy* create(const RadioAstronomySettings& settings, bool force)
        {
            return new MsgConfigureRadioAstronomy(settings, force);
        }

    private:
        RadioAstronomySettings m_settings;
        bool m_force;

        MsgConfigureRadioAstronomy(const RadioAstronomySettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgSensorMeasurement : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getSensor() const { return m_sensor; }
        double getValue() const { return m_value; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgSensorMeasurement* create(int sensor, double value)
        {
            return new MsgSensorMeasurement(sensor, value, QDateTime::currentDateTime());
        }

    private:
        int m_sensor;
        double m_value;
        QDateTime m_dateTime;

        MsgSensorMeasurement(int sensor, double value, QDateTime dateTime) :
            Message(),
            m_sensor(sensor),
            m_value(value),
            m_dateTime(dateTime)
        {
        }
    };

    class MsgFFTMeasurement : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real *getFFT() const { return m_fft; }
        int getSize() const { return m_size; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgFFTMeasurement* create(const Real *fft, int size, QDateTime dateTime)
        {
            return new MsgFFTMeasurement(fft, size, dateTime);
        }

    private:
        Real *m_fft;
        int m_size;
        QDateTime m_dateTime;

        MsgFFTMeasurement(const Real *fft, int size, QDateTime dateTime) :
            Message(),
            m_size(size),
            m_dateTime(dateTime)
        {
            // Take a copy of the data
            m_fft = new Real[size];
            std::copy(fft, fft + size, m_fft);
        }

    };

    class MsgStartMeasurements : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStartMeasurements* create()
        {
            return new MsgStartMeasurements();
        }

    private:

        MsgStartMeasurements() :
            Message()
        {
        }
    };

    class MsgStopMeasurements : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgStopMeasurements* create()
        {
            return new MsgStopMeasurements();
        }

    private:

        MsgStopMeasurements() :
            Message()
        {
        }
    };

    class MsgMeasurementProgress : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getPercentComplete() const { return m_percentComplete; }

        static MsgMeasurementProgress* create(int percentComplete)
        {
            return new MsgMeasurementProgress(percentComplete);
        }

    private:
        int m_percentComplete;

        MsgMeasurementProgress(int percentComplete) :
            Message(),
            m_percentComplete(percentComplete)
        {
        }
    };

    class MsgStartCal : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getHot() const { return m_hot; }

        static MsgStartCal* create(bool hot)
        {
            return new MsgStartCal(hot);
        }

    private:
        bool m_hot;

        MsgStartCal(bool hot) :
            Message(),
            m_hot(hot)
        {
        }
    };

    class MsgCalComplete : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        Real *getCal() const { return m_cal; }
        int getSize() const { return m_size; }
        bool getHot() const { return m_hot; }
        QDateTime getDateTime() const { return m_dateTime; }

        static MsgCalComplete* create(const Real *cal, int size, QDateTime dateTime, bool hot)
        {
            return new MsgCalComplete(cal, size, dateTime, hot);
        }

    private:
        Real *m_cal;
        int m_size;
        QDateTime m_dateTime;
        bool m_hot;

        MsgCalComplete(const Real *cal, int size, QDateTime dateTime, bool hot) :
            Message(),
            m_size(size),
            m_dateTime(dateTime),
            m_hot(hot)
        {
            // Take a copy of the data
            m_cal = new Real[size];
            std::copy(cal, cal + size, m_cal);
        }
    };

    class MsgStartSweep : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgStartSweep* create()
        {
            return new MsgStartSweep();
        }

    private:

        MsgStartSweep() :
            Message()
        {
        }
    };

    class MsgStopSweep : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgStopSweep* create()
        {
            return new MsgStopSweep();
        }

    private:

        MsgStopSweep() :
            Message()
        {
        }
    };

    class MsgSweepComplete : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgSweepComplete* create()
        {
            return new MsgSweepComplete();
        }

    private:

        MsgSweepComplete() :
            Message()
        {
        }
    };

    class MsgSweepStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getStatus() const { return m_status; }

        static MsgSweepStatus* create(const QString& status)
        {
            return new MsgSweepStatus(status);
        }

    private:
        QString m_status;

        MsgSweepStatus(const QString& status) :
            Message(),
            m_status(status)
        {
        }
    };

    class MsgScanAvailableFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgScanAvailableFeatures* create()
        {
            return new MsgScanAvailableFeatures();
        }

    private:

        MsgScanAvailableFeatures() :
            Message()
        {
        }
    };

    class MsgReportAvailableFeatures : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<RadioAstronomySettings::AvailableFeature>& getFeatures() { return m_availableFeatures; }

        static MsgReportAvailableFeatures* create() {
            return new MsgReportAvailableFeatures();
        }

    private:
        QList<RadioAstronomySettings::AvailableFeature> m_availableFeatures;

        MsgReportAvailableFeatures() :
            Message()
        {}
    };

    class MsgReportAvailableRotators : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QList<RadioAstronomySettings::AvailableFeature>& getFeatures() { return m_availableFeatures; }

        static MsgReportAvailableRotators* create() {
            return new MsgReportAvailableRotators();
        }

    private:
        QList<RadioAstronomySettings::AvailableFeature> m_availableFeatures;

        MsgReportAvailableRotators() :
            Message()
        {}
    };

    RadioAstronomy(DeviceAPI *deviceAPI);
    virtual ~RadioAstronomy();
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
            QString& errorMessage) override;

    virtual int webapiWorkspaceGet(
            SWGSDRangel::SWGWorkspaceInfo& response,
            QString& errorMessage);

    virtual int webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage) override;

    virtual int webapiActionsPost(
            const QStringList& channelActionsKeys,
            SWGSDRangel::SWGChannelActions& query,
            QString& errorMessage) override;

    static void webapiFormatChannelSettings(
            SWGSDRangel::SWGChannelSettings& response,
            const RadioAstronomySettings& settings);

    static void webapiUpdateChannelSettings(
            RadioAstronomySettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

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
    QThread m_workerThread;
    RadioAstronomyBaseband* m_basebandSink;
    RadioAstronomyWorker *m_worker;
    RadioAstronomySettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QHash<Feature*, RadioAstronomySettings::AvailableFeature> m_availableFeatures;
    QHash<Feature*, RadioAstronomySettings::AvailableFeature> m_rotators;
    QObject *m_selectedPipe;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    int m_starTrackerFeatureSetIndex;
    int m_starTrackerFeatureIndex;
    int m_rotatorFeatureSetIndex;
    int m_rotatorFeatureIndex;

    float m_sweep1;     // Current sweep position
    float m_sweep2;
    float m_sweep1Stop;
    float m_sweep1Start;
    bool m_sweeping;
    bool m_sweepStop;
    QTimer m_sweepTimer;
    QMetaObject::Connection m_sweepTimerConnection;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const RadioAstronomySettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const RadioAstronomySettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const RadioAstronomySettings& settings,
        bool force
    );
    void callOnStartTime(void (RadioAstronomy::*f)());
    void sweepStart();
    void startCal(bool hot);
    void calComplete(MsgCalComplete* report);
    void scanAvailableFeatures();
    void notifyUpdateFeatures();
    void notifyUpdateRotators();

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void startMeasurement();
    void sweepStartMeasurement();
    void sweep1();
    void sweep2();
    void waitUntilOnTarget();
    void sweepNext();
    void sweepComplete();
    void handleIndexInDeviceSetChanged(int index);
    void handleFeatureAdded(int featureSetIndex, Feature *feature);
    void handleFeatureRemoved(int featureSetIndex, Feature *feature);
    void handleMessagePipeToBeDeleted(int reason, QObject* object);
    void handleFeatureMessageQueue(MessageQueue* messageQueue);
};

#endif // INCLUDE_RADIOASTRONOMY_H
