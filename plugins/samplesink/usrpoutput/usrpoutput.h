///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUT_H_
#define PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUT_H_

#include <stdint.h>

#include <QString>
#include <QNetworkRequest>

#include <uhd/usrp/multi_usrp.hpp>

#include "dsp/devicesamplesink.h"
#include "usrp/deviceusrpshared.h"
#include "usrpoutputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class USRPOutputThread;
struct DeviceUSRPParams;

class USRPOutput : public DeviceSampleSink
{
    Q_OBJECT
public:
    class MsgConfigureUSRP : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const USRPOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureUSRP* create(const USRPOutputSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureUSRP(settings, settingsKeys, force);
        }

    private:
        USRPOutputSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureUSRP(const USRPOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool     getSuccess() const { return m_success; }
        bool     getActive() const { return m_active; }
        uint32_t getUnderrun() const { return m_underrun; }
        uint32_t getDroppedPackets() const { return m_droppedPackets; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t underrun,
                uint32_t droppedPackets
                )
        {
            return new MsgReportStreamInfo(
                    success,
                    active,
                    underrun,
                    droppedPackets
                    );
        }

    private:
        bool     m_success;
        bool     m_active;              //!< Indicates whether the stream is currently active
        uint32_t m_underrun;            //!< FIFO underrun count
        uint32_t m_droppedPackets;      //!< Number of dropped packets by HW

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t underrun,
                uint32_t droppedPackets
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_underrun(underrun),
            m_droppedPackets(droppedPackets)
        { }
    };

    USRPOutput(DeviceAPI *deviceAPI);
    virtual ~USRPOutput();
    virtual void destroy();

    virtual void init();
    virtual bool start();
    virtual void stop();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;
    virtual int getSampleRate() const;
    virtual void setSampleRate(int sampleRate) { (void) sampleRate; }
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
            const USRPOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
            USRPOutputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    int getChannelIndex();
    void getLORange(float& minF, float& maxF) const;
    void getSRRange(float& minF, float& maxF) const;
    void getLPRange(float& minF, float& maxF) const;
    void getGainRange(float& minF, float& maxF) const;
    QStringList getTxAntennas() const;
    QStringList getClockSources() const;

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;
    USRPOutputSettings m_settings;
    USRPOutputThread* m_usrpOutputThread;
    QString m_deviceDescription;
    bool m_running;
    DeviceUSRPShared m_deviceShared;
    bool m_channelAcquired;
    uhd::tx_streamer::sptr m_streamId;
    size_t m_bufSamples;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
    bool acquireChannel();
    void releaseChannel();
    void suspendRxBuddies();
    void resumeRxBuddies();
    void suspendTxBuddies();
    void resumeTxBuddies();
    bool applySettings(const USRPOutputSettings& settings, const QList<QString>& settingsKeys, bool preGetStream, bool force = false);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const USRPOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif /* PLUGINS_SAMPLESOURCE_USRPOUTPUT_USRPOUTPUT_H_ */
