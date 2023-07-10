///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AUDIOCATSISO_AUDIOCATSISO_H_
#define _AUDIOCATSISO_AUDIOCATSISO_H_

#include <QString>
#include <QByteArray>
#include <QTimer>
#include <QNetworkRequest>
#include <QThread>

#include "dsp/devicesamplemimo.h"
#include "audio/audiofifo.h"
#include "audiocatsisosettings.h"
#include "audiocatsisohamlib.h"

class DeviceAPI;
class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class AudioCATInputWorker;
class AudioCATOutputWorker;
class AudioCATSISOCATWorker;

class AudioCATSISO : public DeviceSampleMIMO {
    Q_OBJECT
public:
	class MsgConfigureAudioCATSISO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AudioCATSISOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureAudioCATSISO* create(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureAudioCATSISO(settings, settingsKeys, force);
		}

	private:
		AudioCATSISOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureAudioCATSISO(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    AudioCATSISO(DeviceAPI *deviceAPI);
	virtual ~AudioCATSISO();
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

    const QString& getInputDeviceName() { return m_settings.m_rxDeviceName; }
	virtual int getSourceSampleRate(int index) const;
    virtual void setSourceSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSourceCenterFrequency(int index) const;
    virtual void setSourceCenterFrequency(qint64 centerFrequency, int index);

    const QString& getOutputDeviceName() { return m_settings.m_txDeviceName; }
	virtual int getSinkSampleRate(int index) const;
    virtual void setSinkSampleRate(int sampleRate, int index) { (void) sampleRate; (void) index; }
	virtual quint64 getSinkCenterFrequency(int index) const;
    virtual void setSinkCenterFrequency(qint64 centerFrequency, int index);

    virtual quint64 getMIMOCenterFrequency() const { return getSourceCenterFrequency(0); }
    virtual unsigned int getMIMOSampleRate() const { return getSourceSampleRate(0); }

	virtual bool handleMessage(const Message& message);

    const QStringList& getComPorts() { return m_comPorts; }
    const QMap<uint32_t, QString>& getRigModels() const { return m_hamlibHandler.getRigModels(); }
    const QMap<QString, uint32_t>& getRigNames() const { return m_hamlibHandler.getRigNames(); }

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
            const AudioCATSISOSettings& settings);

    static void webapiUpdateDeviceSettings(
            AudioCATSISOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    struct DeviceSettingsKeys
    {
        QList<QString> m_commonSettingsKeys;
        QList<QList<QString>> m_streamsSettingsKeys;
    };

	DeviceAPI *m_deviceAPI;
    AudioFifo m_inputFifo;
    AudioFifo m_outputFifo;
	QMutex m_mutex;
	AudioCATSISOSettings m_settings;
    AudioCATInputWorker* m_inputWorker;
    AudioCATOutputWorker* m_outputWorker;
    AudioCATSISOCATWorker* m_catWorker;
    QThread *m_inputWorkerThread;
    QThread *m_outputWorkerThread;
    QThread *m_catWorkerThread;
	QString m_deviceDescription;
	bool m_rxRunning;
    int m_rxAudioDeviceIndex;
    int m_rxSampleRate;
	bool m_txRunning;
    int m_txAudioDeviceIndex;
    int m_txSampleRate;
    bool m_ptt;
    bool m_catRunning;
    const QTimer& m_masterTimer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;
    QStringList m_comPorts;
    AudioCATSISOHamlib m_hamlibHandler;

    void applySettings(const AudioCATSISOSettings& settings, const QList<QString>& settingsKeys, bool force);
    void listComPorts();
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AudioCATSISOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _AUDIOCATSISO_AUDIOCATSISO_H_
