///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUT_H_
#define PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUT_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplesource.h"
#include "bladerf2/devicebladerf2shared.h"
#include "bladerf2inputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class BladeRF2InputThread;
struct bladerf_gain_modes;
struct bladerf;

class BladeRF2Input : public DeviceSampleSource {
    Q_OBJECT
public:
    class MsgConfigureBladeRF2 : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const BladeRF2InputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureBladeRF2* create(const BladeRF2InputSettings& settings, const QList<QString>& settingsKeys, bool force)
        {
            return new MsgConfigureBladeRF2(settings, settingsKeys, force);
        }

    private:
        BladeRF2InputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureBladeRF2(const BladeRF2InputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgReportGainRange : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int getMin() const { return m_min; }
        int getMax() const { return m_max; }
        int getStep() const { return m_step; }
        float getScale() const { return m_scale; }

        static MsgReportGainRange* create(int min, int max, int step, float scale) {
            return new MsgReportGainRange(min, max, step, scale);
        }

    protected:
        int m_min;
        int m_max;
        int m_step;
        float m_scale;

        MsgReportGainRange(int min, int max, int step, float scale) :
            Message(),
            m_min(min),
            m_max(max),
            m_step(step),
            m_scale(scale)
        {}
    };

    struct GainMode
    {
        QString m_name;
        int m_value;
    };

    BladeRF2Input(DeviceAPI *deviceAPI);
    virtual ~BladeRF2Input();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();
    BladeRF2InputThread *getThread() { return m_thread; }
    void setThread(BladeRF2InputThread *thread) { m_thread = thread; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

    void getFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale);
    void getSampleRateRange(int& min, int& max, int& step, float& scale);
    void getBandwidthRange(int& min, int& max, int& step, float& scale);
    void getGlobalGainRange(int& min, int& max, int& step, float& scale);
    const std::vector<GainMode>& getGainModes() { return m_gainModes; }

    virtual bool handleMessage(const Message& message);

    virtual int webapiSettingsGet(
                SWGSDRangel::SWGDeviceSettings& response,
                QString& errorMessage);

    virtual int webapiSettingsPutPatch(
                bool force,
                const QStringList& deviceSettingsKeys,
                SWGSDRangel::SWGDeviceSettings& response, // query + response
                QString& errorMessage);

    virtual int webapiReportGet(
            SWGSDRangel::SWGDeviceReport& response,
            QString& errorMessage);

    virtual int webapiRunGet(
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const BladeRF2InputSettings& settings);

    static void webapiUpdateDeviceSettings(
            BladeRF2InputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;
    BladeRF2InputSettings m_settings;
    QString m_deviceDescription;
    bool m_running;
    DeviceBladeRF2Shared m_deviceShared;
    BladeRF2InputThread *m_thread;
    std::vector<GainMode> m_gainModes;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
    BladeRF2InputThread *findThread();
    void moveThreadToBuddy();
    bool applySettings(const BladeRF2InputSettings& settings, const QList<QString>& settingsKeys, bool force = false);
    bool setDeviceCenterFrequency(struct bladerf *dev, int requestedChannel, quint64 freq_hz, int loPpmTenths);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF2InputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* PLUGINS_SAMPLESOURCE_BLADERF2INPUT_BLADERF2INPUT_H_ */
