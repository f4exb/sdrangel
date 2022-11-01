///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021 Edouard Griffiths, F4EXB                                   //
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

#ifndef _PLUTOSDR_PLUTOSDRMIMO_H_
#define _PLUTOSDR_PLUTOSDRMIMO_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplemimo.h"
#include "plutosdr/deviceplutosdrbox.h"

#include "plutosdrmimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class PlutoSDRMIThread;
class PlutoSDRMOThread;
class DevicePlutoSDRParams;

class PlutoSDRMIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigurePlutoSDRMIMO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const PlutoSDRMIMOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigurePlutoSDRMIMO* create(const PlutoSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigurePlutoSDRMIMO(settings, settingsKeys, force);
		}

	private:
		PlutoSDRMIMOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigurePlutoSDRMIMO(const PlutoSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) :
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

    PlutoSDRMIMO(DeviceAPI *deviceAPI);
	virtual ~PlutoSDRMIMO();
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
            const PlutoSDRMIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            PlutoSDRMIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    int getNbRx() { return m_nbRx; }
    int getNbTx() { return m_nbTx; }
    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }
    uint32_t getADCSampleRate() const { return m_rxDeviceSampleRates.m_addaConnvRate; }
    uint32_t getDACSampleRate() const { return m_txDeviceSampleRates.m_addaConnvRate; }
    uint32_t getRxFIRSampleRate() const { return m_rxDeviceSampleRates.m_hb1Rate; }
    uint32_t getTxFIRSampleRate() const { return m_txDeviceSampleRates.m_hb1Rate; }
    void getRxRSSI(std::string& rssiStr, int chan);
    void getTxRSSI(std::string& rssiStr, int chan);
    void getRxGain(int& gainStr, int chan);
    void getLORange(qint64& minLimit, qint64& maxLimit);
    void getbbLPRange(quint32& minLimit, quint32& maxLimit);
    bool fetchTemperature();
    float getTemperature();

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	PlutoSDRMIMOSettings m_settings;
	PlutoSDRMIThread* m_sourceThread;
    PlutoSDRMOThread* m_sinkThread;
    DevicePlutoSDRBox::SampleRates m_rxDeviceSampleRates;
    DevicePlutoSDRBox::SampleRates m_txDeviceSampleRates;
	QString m_deviceDescription;
	bool m_runningRx;
	bool m_runningTx;
    struct iio_buffer *m_plutoRxBuffer;
    struct iio_buffer *m_plutoTxBuffer;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    DevicePlutoSDRParams *m_plutoParams;
    bool m_open;
    int m_nbRx;
    int m_nbTx;

    bool openDevice();
    void closeDevice();

	bool applySettings(const PlutoSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const PlutoSDRMIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // _PLUTOSDR_PLUTOSDRMIMO_H_
