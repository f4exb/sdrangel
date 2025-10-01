///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Edouard Griffiths, F4EXB <f4exb06@gmail.com>               //
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

#ifndef _SPECTRANMISO_SPECTRANMISO_H_
#define _SPECTRANMISO_SPECTRANMISO_H_

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>
#include <QThread>
#include <aaroniartsaapi.h>

#include "dsp/devicesamplemimo.h"
#include "spectranmisosettings.h"

class DeviceAPI;
class SpectranMISOStreamWorker;
class QThread;
class QNetworkAccessManager;
class QNetworkReply;

class SpectranMISO : public DeviceSampleMIMO {
    Q_OBJECT
public:
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

    class MsgChangeMode : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        SpectranMISOSettings getSettings() const { return m_settings; }

        static MsgChangeMode* create(const SpectranMISOSettings& settings) {
            return new MsgChangeMode(settings);
        }

    protected:
        SpectranMISOSettings m_settings;

        MsgChangeMode(const SpectranMISOSettings& settings) :
            Message(),
            m_settings(settings)
        { }
    };

    SpectranMISO(DeviceAPI *deviceAPI);
	virtual ~SpectranMISO();
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
    virtual void setSourceSampleRate(int sampleRate, int index);
	virtual quint64 getSourceCenterFrequency(int index) const;
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index);

	virtual int getSinkSampleRate(int index) const;
    virtual void setSinkSampleRate(int sampleRate, int index);
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
            const SpectranMISOSettings& settings);

    static void webapiUpdateDeviceSettings(
            SpectranMISOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    static int getSampleRate(const SpectranMISOSettings& settings);
    const SpectranModel& getSpectranModel() const { return m_spectranModel; }

private:
    struct DeviceSettingsKeys
    {
        QList<QString> m_commonSettingsKeys;
        QList<QList<QString>> m_streamsSettingsKeys;
    };

	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	SpectranMISOSettings m_settings;
    SpectranMISOMode m_restartMode;
	QString m_deviceDescription;
    SpectranModel m_spectranModel;
	bool m_running;
    const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    SpectranMISOStreamWorker *m_streamWorker;
    QThread *m_streamWorkerThread;
    AARTSAAPI_Device m_device;
    static const QMap<SpectranMISOMode, QString> m_modeNames;
    static const QMap<SpectranModel, QString> m_spectranModelNames;
    static const QMap<SpectranRxChannel, QString> m_rxChannelNames;
    static const QMap<SpectranMISOClockRate, QString> m_clockRateNames;
    static const QMap<unsigned int, QString> m_log2DecimationNames;

    void setSpectranModel(const QString& deviceDisplayName);
    bool start(const SpectranMISOMode& mode);
    void stop();
    void restart(const SpectranMISOMode& mode);
    static int getActualRawRate(const SpectranMISOClockRate& clockRate);
    void setSampleRate(int sampleRate);
	bool applySettings(const SpectranMISOSettings& settings, const QList<QString>& settingsKeys, bool force);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const SpectranMISOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);
    bool startMode(const SpectranMISOMode& mode);
    void stopMode();
    void applyCommonSettings(const SpectranMISOMode& mode);

private slots:
    void streamStopped();
    void streamStoppedForRestart();
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _SPECTRANMISO_SPECTRANMISO_H_
