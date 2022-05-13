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

#ifndef INCLUDE_NOISEFIGURE_H
#define INCLUDE_NOISEFIGURE_H

#include <vector>

#include <QNetworkRequest>
#include <QUdpSocket>
#include <QThread>
#include <QDateTime>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"
#include "util/visa.h"

#include "noisefigurebaseband.h"
#include "noisefiguresettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;

class NoiseFigure : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureNoiseFigure : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const NoiseFigureSettings& getSettings() const { return m_settings; }
        bool getForce() const { return m_force; }

        static MsgConfigureNoiseFigure* create(const NoiseFigureSettings& settings, bool force)
        {
            return new MsgConfigureNoiseFigure(settings, force);
        }

    private:
        NoiseFigureSettings m_settings;
        bool m_force;

        MsgConfigureNoiseFigure(const NoiseFigureSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_force(force)
        { }
    };

    class MsgPowerMeasurement : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getPower() const { return m_power; }

        static MsgPowerMeasurement* create(double power)
        {
            return new MsgPowerMeasurement(power);
        }

    private:
        double m_power;

        MsgPowerMeasurement(double power) :
            Message(),
            m_power(power)
        {
        }
    };

    class MsgNFMeasurement : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        double getSweepValue() const { return m_sweepValue; }
        double getNF() const { return m_nf; }
        double getTemp() const { return m_temp; }
        double getY() const { return m_y; }
        double getENR() const { return m_enr; }
        double getFloor() const { return m_floor; }

        static MsgNFMeasurement* create(double sweepValue, double nf, double temp, double y, double enr, double floor)
        {
            return new MsgNFMeasurement(sweepValue, nf, temp, y, enr, floor);
        }

    private:
        double m_sweepValue;// In MHz for centerFrequency
        double m_nf;        // In dB
        double m_temp;      // In Kelvin
        double m_y;         // In dB
        double m_enr;       // In dB
        double m_floor;     // In dBm

        MsgNFMeasurement(double sweepValue, double nf, double temp, double y, double enr, double floor) :
            Message(),
            m_sweepValue(sweepValue),
            m_nf(nf),
            m_temp(temp),
            m_y(y),
            m_enr(enr),
            m_floor(floor)
        {
        }
    };

    // Sent from GUI to start of stop a measurement
    class MsgStartStop : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        static MsgStartStop* create()
        {
            return new MsgStartStop();
        }

    private:

        MsgStartStop() :
            Message()
        {
        }
    };

    // Sent to GUI to indicate measurements have finished
    class MsgFinished : public Message {
        MESSAGE_CLASS_DECLARATION

    public:

        QString getErrorMessage() const { return m_errorMessage; }

        static MsgFinished* create()
        {
            QString noError;
            return new MsgFinished(noError);
        }

        static MsgFinished* create(const QString& errorMessage)
        {
            return new MsgFinished(errorMessage);
        }

    private:
        QString m_errorMessage;

        MsgFinished(const QString& errorMessage) :
            Message(),
            m_errorMessage(errorMessage)
        {
        }
    };

    NoiseFigure(DeviceAPI *deviceAPI);
    virtual ~NoiseFigure();
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
            const NoiseFigureSettings& settings);

    static void webapiUpdateChannelSettings(
            NoiseFigureSettings& settings,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response);

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    bool openVISADevice();
    void closeVISADevice();
    void processVISA(QStringList commands);
    void powerOn();
    void powerOff();
    double calcENR(double frequency);

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    NoiseFigureBaseband* m_basebandSink;
    NoiseFigureSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const NoiseFigureSettings& settings, bool force = false);
    void webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NoiseFigureSettings& settings, bool force);
    void webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const NoiseFigureSettings& settings,
        bool force
    );

    enum State {
        IDLE,
        SET_SWEEP_VALUE,
        POWER_ON,
        MEASURE_ON,
        POWER_OFF,
        MEASURE_OFF,
        COMPLETE
    } m_state;
    double m_sweepValue;                // Current sweep value
    QList<double> m_values;             // Sweep values
    int m_step;                         // Current sweep step
    int m_steps;                        // Number of settings to sweep
    double m_onPower;
    double m_offPower;
    ViSession m_session;
    VISA m_visa;

private slots:
    void networkManagerFinished(QNetworkReply *reply);
    void nextState();
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_NOISEFIGURE_H
