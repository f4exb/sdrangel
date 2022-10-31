///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMO_H_
#define PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMO_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplemimo.h"
#include "bladerf2/devicebladerf2shared.h"
#include "bladerf2mimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class BladeRF2MIThread;
class BladeRF2MOThread;
class DeviceBladeRF2;
struct bladerf_gain_modes;
struct bladerf;

class BladeRF2MIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureBladeRF2MIMO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const BladeRF2MIMOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureBladeRF2MIMO* create(const BladeRF2MIMOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureBladeRF2MIMO(settings, settingsKeys, force);
		}

	private:
		BladeRF2MIMOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureBladeRF2MIMO(const BladeRF2MIMOSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    struct GainMode
    {
        QString m_name;
        int m_value;
    };

    BladeRF2MIMO(DeviceAPI *deviceAPI);
	virtual ~BladeRF2MIMO();
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
            const BladeRF2MIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            BladeRF2MIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    bool isRecording(unsigned int istream) const { (void) istream; return false; }

    void getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale);
    void getRxSampleRateRange(int& min, int& max, int& step, float& scale);
    void getRxBandwidthRange(int& min, int& max, int& step, float& scale);
    void getRxGlobalGainRange(int& min, int& max, int& step, float& scale);
    const std::vector<GainMode>& getRxGainModes() { return m_rxGainModes; }

    void getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step, float& scale);
    void getTxSampleRateRange(int& min, int& max, int& step, float& scale);
    void getTxBandwidthRange(int& min, int& max, int& step, float& scale);
    void getTxGlobalGainRange(int& min, int& max, int& step, float& scale);

    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	BladeRF2MIMOSettings m_settings;
	BladeRF2MIThread* m_sourceThread;
    BladeRF2MOThread* m_sinkThread;
	QString m_deviceDescription;
	bool m_runningRx;
	bool m_runningTx;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    DeviceBladeRF2 *m_dev;
    bool m_open;
    std::vector<GainMode> m_rxGainModes;

    bool openDevice();
    void closeDevice();

	bool applySettings(const BladeRF2MIMOSettings& settings, const QList<QString>& settingsKeys, bool force);
    bool setRxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    bool setTxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const BladeRF2MIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMO_H_
