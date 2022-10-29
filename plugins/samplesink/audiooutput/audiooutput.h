///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB                                   //
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

#ifndef _AUDIOOUTPUT_AUDIOOUTPUT_H_
#define _AUDIOOUTPUT_AUDIOOUTPUT_H_

#include <QNetworkRequest>

#include "dsp/devicesamplesink.h"
#include "audio/audiooutputdevice.h"
#include "audio/audiofifo.h"

#include "audiooutputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class QThread;
class AudioOutputWorker;
class DeviceAPI;

class AudioOutput : public DeviceSampleSink {
public:
	class MsgConfigureAudioOutput : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const AudioOutputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureAudioOutput* create(const AudioOutputSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureAudioOutput(settings, settingsKeys, force);
		}

	private:
		AudioOutputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureAudioOutput(const AudioOutputSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

	AudioOutput(DeviceAPI *deviceAPI);
	virtual ~AudioOutput();
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
    virtual void setCenterFrequency(qint64 centerFrequency) { (void) centerFrequency; }
    const QString& getDeviceName() const { return m_settings.m_deviceName; }
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
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    virtual int webapiRun(
            bool run,
            SWGSDRangel::SWGDeviceState& response,
            QString& errorMessage);

    static void webapiFormatDeviceSettings(
            SWGSDRangel::SWGDeviceSettings& response,
            const AudioOutputSettings& settings);

    static void webapiUpdateDeviceSettings(
            AudioOutputSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

private:
    DeviceAPI *m_deviceAPI;
    AudioOutputDevice m_audioOutputDevice;
    AudioFifo m_audioFifo;
    QMutex m_mutex;
    AudioOutputSettings m_settings;
    int m_audioDeviceIndex;
    int m_sampleRate;
    qint64 m_centerFrequency;
    AudioOutputWorker *m_worker;
    QThread *m_workerThread;
    QString m_deviceDescription;
    bool m_running;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    void applySettings(const AudioOutputSettings& settings, const QList<QString>& settingsKeys, bool force);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const AudioOutputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};


#endif
