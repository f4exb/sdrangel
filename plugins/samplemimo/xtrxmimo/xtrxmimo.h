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

#ifndef PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMO_H_
#define PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMO_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "dsp/devicesamplemimo.h"
#include "xtrx/devicextrxshared.h"
#include "xtrxmimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class XTRXMIThread;
class XTRXMOThread;
struct DeviceLimeSDRParams;

#endif // PLUGINS_SAMPLEMIMO_XTRXMIMO_XTRXMIMO_H_

class XTRXMIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureXTRXMIMO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const XTRXMIMOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureXTRXMIMO* create(const XTRXMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureXTRXMIMO(settings, settingsKeys, force);
		}

	private:
		XTRXMIMOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureXTRXMIMO(const XTRXMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) :
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
        uint32_t getFifoFilledCountRx() const { return m_fifoFilledCountRx; }
        uint32_t getFifoFilledCountTx() const { return m_fifoFilledCountTx; }
        uint32_t getFifoSize() const { return m_fifoSize; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t fifoFilledCountRx,
                uint32_t fifoFilledCountTx,
                uint32_t fifoSize
                )
        {
            return new MsgReportStreamInfo(
                        success,
                        active,
                        fifoFilledCountRx,
                        fifoFilledCountTx,
                        fifoSize
                        );
        }

    private:
        bool     m_success;
        // everything from lms_stream_status_t
        bool     m_active; //!< Indicates whether the stream is currently active
        uint32_t m_fifoFilledCountRx; //!< Number of samples in FIFO buffer (Rx)
        uint32_t m_fifoFilledCountTx; //!< Number of samples in FIFO buffer (Tx)
        uint32_t m_fifoSize; //!< Size of FIFO buffer

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t fifoFilledCountRx,
                uint32_t fifoFilledCountTx,
                uint32_t fifoSize
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_fifoFilledCountRx(fifoFilledCountRx),
            m_fifoFilledCountTx(fifoFilledCountTx),
            m_fifoSize(fifoSize)
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

    XTRXMIMO(DeviceAPI *deviceAPI);
	virtual ~XTRXMIMO();
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

    uint32_t getRxDevSampleRate() const;
    uint32_t getTxDevSampleRate() const;
    uint32_t getLog2HardDecim() const;
    uint32_t getLog2HardInterp() const;
    double getClockGen() const;

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
            const XTRXMIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            XTRXMIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);

    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

    void getLORange(float& minF, float& maxF, float& stepF) const;
    void getSRRange(float& minF, float& maxF, float& stepF) const;
    void getLPRange(float& minF, float& maxF, float& stepF) const;

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	XTRXMIMOSettings m_settings;
	XTRXMIThread* m_sourceThread;
    XTRXMOThread* m_sinkThread;
	QString m_deviceDescription;
	bool m_runningRx;
	bool m_runningTx;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    bool m_open;
    DeviceXTRXShared m_deviceShared;

    bool openDevice();
    void closeDevice();

	bool applySettings(const XTRXMIMOSettings& settings, const QList<QString>& settingsKeys, bool force);
    void applyGainAuto(unsigned int channel, uint32_t gain);
    void applyGainLNA(unsigned int channel, double gain);
    void applyGainTIA(unsigned int channel, double gain);
    void applyGainPGA(unsigned int channel, double gain);
    void setRxDeviceCenterFrequency(xtrx_dev *dev, quint64 freq_hz);
    void setTxDeviceCenterFrequency(xtrx_dev *dev, quint64 freq_hz);

    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const XTRXMIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

    static xtrx_antenna_t toXTRXAntennaRx(XTRXMIMOSettings::RxAntenna antennaPath);
    static xtrx_antenna_t toXTRXAntennaTx(XTRXMIMOSettings::TxAntenna antennaPath);
    static double tiaToDB(unsigned idx);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};
