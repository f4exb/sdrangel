///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>    //
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

#ifndef INCLUDE_BLADERFINPUT_H
#define INCLUDE_BLADERFINPUT_H

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include <libbladeRF.h>
#include <dsp/devicesamplesource.h>

#include "bladerf1/devicebladerf1.h"
#include "bladerf1/devicebladerf1param.h"
#include "bladerf1inputsettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class Bladerf1InputThread;

class Bladerf1Input : public DeviceSampleSource {
    Q_OBJECT
public:
	class MsgConfigureBladerf1 : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const BladeRF1InputSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureBladerf1* create(const BladeRF1InputSettings& settings, QList<QString> settingsKeys, bool force)
		{
			return new MsgConfigureBladerf1(settings, settingsKeys, force);
		}

	private:
		BladeRF1InputSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureBladerf1(const BladeRF1InputSettings& settings, QList<QString> settingsKeys, bool force) :
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

    private:
        bool m_startStop;

        explicit MsgStartStop(bool startStop) :
            Message(),
            m_startStop(startStop)
        { }
    };

    explicit Bladerf1Input(DeviceAPI *deviceAPI);
	~Bladerf1Input() final;
	void destroy() final;

    void init() final;
	bool start() final;
	void stop() final;

    QByteArray serialize() const final;
    bool deserialize(const QByteArray& data) final;

    void setMessageQueueToGUI(MessageQueue *queue) final { m_guiMessageQueue = queue; }
	const QString& getDeviceDescription() const final;
	int getSampleRate() const final;
    void setSampleRate(int sampleRate) final { (void) sampleRate; }
	quint64 getCenterFrequency() const final;
    void setCenterFrequency(qint64 centerFrequency) final;

	bool handleMessage(const Message& message) final;

    int webapiSettingsGet(
        SWGSDRangel::SWGDeviceSettings& response,
        QString& errorMessage) final;

    int webapiSettingsPutPatch(
        bool force,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response, // query + response
        QString& errorMessage) final;

    int webapiRunGet(
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage) final;

    int webapiRun(
        bool run,
        SWGSDRangel::SWGDeviceState& response,
        QString& errorMessage) final;

    static void webapiFormatDeviceSettings(
        SWGSDRangel::SWGDeviceSettings& response,
        const BladeRF1InputSettings& settings);

    static void webapiUpdateDeviceSettings(
        BladeRF1InputSettings& settings,
        const QStringList& deviceSettingsKeys,
        SWGSDRangel::SWGDeviceSettings& response);

private:
	DeviceAPI *m_deviceAPI;
	QRecursiveMutex m_mutex;
	BladeRF1InputSettings m_settings;
	struct bladerf* m_dev;
	Bladerf1InputThread* m_bladerfThread;
	QString m_deviceDescription;
	DeviceBladeRF1Params m_sharedParams;
	bool m_running;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool openDevice();
    void closeDevice();
	bool applySettings(const BladeRF1InputSettings& settings, const QList<QString>& settingsKeys, bool force);
	bladerf_lna_gain getLnaGain(int lnaGain) const;
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF1InputSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply) const;
};

#endif // INCLUDE_BLADERFINPUT_H
