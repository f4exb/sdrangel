///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017, 2018 Edouard Griffiths, F4EXB                             //
// Copyright (C) 2017 Sergey Kostanbaev, Fairwaves Inc.                          //
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

#ifndef PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_
#define PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_
#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplesource.h"
#include "xtrx/devicextrxshared.h"
#include "xtrxinputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class XTRXInputThread;
struct DeviceXTRXParams;

class XTRXInput : public DeviceSampleSource
{
    Q_OBJECT
public:
    class MsgConfigureXTRX : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const XTRXInputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureXTRX* create(const XTRXInputSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureXTRX(settings, settingsKeys, force);
        }

    private:
        XTRXInputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureXTRX(const XTRXInputSettings& settings, const QList<QString>& settingsKeys, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgGetStreamInfo* create()
        {
            return new MsgGetStreamInfo();
        }

    private:
        MsgGetStreamInfo() :
            Message()
        { }
    };

    class MsgGetDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgGetDeviceInfo* create()
        {
            return new MsgGetDeviceInfo();
        }

    private:
        MsgGetDeviceInfo() :
            Message()
        { }
    };

    class MsgReportClockGenChange : public Message {
        MESSAGE_CLASS_DECLARATION

        public:
            static MsgReportClockGenChange* create()
        {
            return new MsgReportClockGenChange();
        }

    private:
        MsgReportClockGenChange() :
            Message()
        { }
    };

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool     getSuccess() const { return m_success; }
        bool     getActive() const { return m_active; }
        uint32_t getFifoFilledCount() const { return m_fifoFilledCount; }
        uint32_t getFifoSize() const { return m_fifoSize; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize
                )
        {
            return new MsgReportStreamInfo(
                        success,
                        active,
                        fifoFilledCount,
                        fifoSize
                        );
        }

    private:
        bool     m_success;
        // everything from lms_stream_status_t
        bool     m_active; //!< Indicates whether the stream is currently active
        uint32_t m_fifoFilledCount; //!< Number of samples in FIFO buffer
        uint32_t m_fifoSize; //!< Size of FIFO buffer

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_fifoFilledCount(fifoFilledCount),
            m_fifoSize(fifoSize)
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

    XTRXInput(DeviceAPI *deviceAPI);
    virtual ~XTRXInput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();
    XTRXInputThread *getThread() { return m_XTRXInputThread; }
    void setThread(XTRXInputThread *thread) { m_XTRXInputThread = thread; }

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
    uint32_t getDevSampleRate() const;
    uint32_t getLog2HardDecim() const;
    double getClockGen() const;
    virtual quint64 getCenterFrequency() const;
    virtual void setCenterFrequency(qint64 centerFrequency);

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
            const XTRXInputSettings& settings);

    static void webapiUpdateDeviceSettings(
            XTRXInputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    std::size_t getChannelIndex();
    void getLORange(float& minF, float& maxF, float& stepF) const;
    void getSRRange(float& minF, float& maxF, float& stepF) const;
    void getLPRange(float& minF, float& maxF, float& stepF) const;

    void apply_gain_auto(uint32_t gain);
    void apply_gain_lna(double gain);
    void apply_gain_tia(double gain);
    void apply_gain_pga(double gain);

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;
    XTRXInputSettings m_settings;
    XTRXInputThread* m_XTRXInputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceXTRXShared m_deviceShared;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
    XTRXInputThread *findThread();
    void moveThreadToBuddy();

    void suspendTxThread();
    void resumeTxThread();
    bool applySettings(const XTRXInputSettings& settings, const QList<QString>& settingsKeys, bool force = false, bool forceNCOFrequency = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const XTRXInputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* PLUGINS_SAMPLESOURCE_XTRXINPUT_XTRXINPUT_H_ */
