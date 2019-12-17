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
class FileRecord;
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
		bool getForce() const { return m_force; }

		static MsgConfigureBladeRF2MIMO* create(const BladeRF2MIMOSettings& settings, bool force)
		{
			return new MsgConfigureBladeRF2MIMO(settings, force);
		}

	private:
		BladeRF2MIMOSettings m_settings;
		bool m_force;

		MsgConfigureBladeRF2MIMO(const BladeRF2MIMOSettings& settings, bool force) :
			Message(),
			m_settings(settings),
			m_force(force)
		{ }
	};

    class MsgFileRecord : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getStartStop() const { return m_startStop; }
        int getStreamIndex() const { return m_streamIndex; }

        static MsgFileRecord* create(bool startStop, int streamIndex) {
            return new MsgFileRecord(startStop, streamIndex);
        }

    protected:
        bool m_startStop;
        int m_streamIndex;

        MsgFileRecord(bool startStop, int streamIndex) :
            Message(),
            m_startStop(startStop),
            m_streamIndex(streamIndex)
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

    class MsgReportGainRange : public Message {
    MESSAGE_CLASS_DECLARATION

    public:
        int getMin() const { return m_min; }
        int getMax() const { return m_max; }
        int getStep() const { return m_step; }
        bool getRxElseTx() const { return m_rxElseTx; }

        static MsgReportGainRange* create(int min, int max, int step, bool rxElseTx) {
            return new MsgReportGainRange(min, max, step, rxElseTx);
        }

    protected:
        int m_min;
        int m_max;
        int m_step;
        bool m_rxElseTx;

        MsgReportGainRange(int min, int max, int step, bool rxElseTx) :
            Message(),
            m_min(min),
            m_max(max),
            m_step(step),
            m_rxElseTx(rxElseTx)
        {}
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
            const BladeRF2MIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            BladeRF2MIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    bool isRecording(unsigned int istream) const;

    void getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step);
    void getRxSampleRateRange(int& min, int& max, int& step);
    void getRxBandwidthRange(int& min, int& max, int& step);
    void getRxGlobalGainRange(int& min, int& max, int& step);
    const std::vector<GainMode>& getRxGainModes() { return m_rxGainModes; }

    void getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step);
    void getTxSampleRateRange(int& min, int& max, int& step);
    void getTxBandwidthRange(int& min, int& max, int& step);
    void getTxGlobalGainRange(int& min, int& max, int& step);

    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

private:
	DeviceAPI *m_deviceAPI;
    std::vector<FileRecord *> m_fileSinks; //!< File sinks to record device I/Q output
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

	bool applySettings(const BladeRF2MIMOSettings& settings, bool force);
    bool setRxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    bool setTxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    void webapiReverseSendSettings(QList<QString>& deviceSettingsKeys, const BladeRF2MIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_SAMPLEMIMO_BLADERF2MIMO_BLADERF2MIMO_H_
