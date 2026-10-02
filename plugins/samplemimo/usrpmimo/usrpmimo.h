///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMO_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMO_H_

#include <stdint.h>

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QNetworkRequest>
#include <QMutex>
#include <QRecursiveMutex>

#include <uhd/usrp/multi_usrp.hpp>

#include "dsp/devicesamplemimo.h"
#include "usrpmimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class DeviceAPI;
class USRPMIThread;
class USRPMOThread;
class USRPMIMOWorker;
struct DeviceUSRPParams;

class USRPMIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
    class MsgConfigureUSRPMIMO : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const USRPMIMOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureUSRPMIMO* create(const USRPMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) {
            return new MsgConfigureUSRPMIMO(settings, settingsKeys, force);
        }

    private:
        USRPMIMOSettings m_settings;
        QList<QString> m_settingsKeys;
        bool m_force;

        MsgConfigureUSRPMIMO(const USRPMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) :
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
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgStartStop* create(bool startStop, bool rxElseTx) {
            return new MsgStartStop(startStop, rxElseTx);
        }

    protected:
        bool m_startStop;
        bool m_rxElseTx;

        MsgStartStop(bool startStop, bool rxElseTx) :
            Message(),
            m_startStop(startStop),
            m_rxElseTx(rxElseTx)
        { }
    };

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetStreamInfo* create() {
            return new MsgGetStreamInfo();
        }

    private:
        MsgGetStreamInfo() :
            Message()
        { }
    };

    class MsgReportStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getRxElseTx() const { return m_rxElseTx; }
        bool getSuccess() const { return m_success; }
        bool getActive() const { return m_active; }
        quint32 getOverUnderRuns() const { return m_overUnderRuns; }
        quint32 getTimeoutsDropped() const { return m_timeoutsDropped; }

        static MsgReportStreamInfo* create(bool rxElseTx, bool success, bool active, quint32 overUnderRuns, quint32 timeoutsDropped) {
            return new MsgReportStreamInfo(rxElseTx, success, active, overUnderRuns, timeoutsDropped);
        }

    private:
        bool m_rxElseTx;
        bool m_success;
        bool m_active;
        quint32 m_overUnderRuns;    //!< Rx overruns or Tx underruns
        quint32 m_timeoutsDropped;  //!< Rx timeouts or Tx dropped packets

        MsgReportStreamInfo(bool rxElseTx, bool success, bool active, quint32 overUnderRuns, quint32 timeoutsDropped) :
            Message(),
            m_rxElseTx(rxElseTx),
            m_success(success),
            m_active(active),
            m_overUnderRuns(overUnderRuns),
            m_timeoutsDropped(timeoutsDropped)
        { }
    };

    class MsgGetDeviceInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        static MsgGetDeviceInfo* create() {
            return new MsgGetDeviceInfo();
        }

    private:
        MsgGetDeviceInfo() :
            Message()
        { }
    };

    USRPMIMO(DeviceAPI *deviceAPI);
    virtual ~USRPMIMO();
    virtual void destroy();

    virtual void init();
    virtual bool startRx();
    virtual void stopRx();
    virtual bool startTx();
    virtual void stopTx();

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual void setMessageQueueToGUI(MessageQueue *queue) { m_guiMessageQueue = queue; }
    virtual const QString& getDeviceDescription() const;

    virtual int getSourceSampleRate(int index) const;
    virtual void setSourceSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
    virtual quint64 getSourceCenterFrequency(int index) const;
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index);

    virtual int getSinkSampleRate(int index) const;
    virtual void setSinkSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
    virtual quint64 getSinkCenterFrequency(int index) const;
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index);

    virtual quint64 getMIMOCenterFrequency() const { return getSourceCenterFrequency(0); }
    virtual unsigned int getMIMOSampleRate() const { return getSourceSampleRate(0); }

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
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            int subsystemIndex,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const USRPMIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            USRPMIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    int getNbRxChannels() const { return m_nbRx; }
    int getNbTxChannels() const { return m_nbTx; }
    void getRxLORange(float& minF, float& maxF) const;
    void getTxLORange(float& minF, float& maxF) const;
    void getSRRange(float& minF, float& maxF) const;
    void getRxLPRange(float& minF, float& maxF) const;
    void getTxLPRange(float& minF, float& maxF) const;
    void getRxGainRange(float& minF, float& maxF) const;
    void getTxGainRange(float& minF, float& maxF) const;
    QStringList getRxAntennas() const;
    QStringList getTxAntennas() const;
    QStringList getClockSources() const;

    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

private:
    DeviceAPI *m_deviceAPI;
    QMutex m_mutex;                 //!< Serialises start and stop
    QMutex m_threadMutex;           //!< Protects thread pointers, for access from other threads
    QRecursiveMutex m_settingsMutex; //!< Protects m_settings, which is written in GUI thread and read in device engine thread in start
    USRPMIMOSettings m_settings;
    USRPMIThread* m_sourceThread;
    USRPMOThread* m_sinkThread;
    QString m_deviceDescription;
    bool m_runningRx;
    bool m_runningTx;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    DeviceUSRPParams *m_deviceParams;
    int m_nbRx;                     //!< Number of Rx channels used
    int m_nbTx;                     //!< Number of Tx channels used
    uhd::rx_streamer::sptr m_rxStream;
    uhd::tx_streamer::sptr m_txStream;
    QThread *m_workerThread;
    USRPMIMOWorker *m_worker;

    bool openDevice();
    void closeDevice();

    bool applySettings(const USRPMIMOSettings& settings, const QList<QString>& settingsKeys, bool force);
    static unsigned int getSampleMOFifoSize(const USRPMIMOSettings& settings);
    void resizeSampleMOFifo();
    void forwardChangeRxDSP();
    void forwardChangeTxDSP();
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const USRPMIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool rxElseTx, bool start);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMO_H_
