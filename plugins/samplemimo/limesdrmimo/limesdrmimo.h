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

#ifndef PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMO_H_
#define PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMO_H_

#include <stdint.h>

#include <QString>
#include <QByteArray>
#include <QNetworkRequest>

#include "lime/LimeSuite.h"

#include "dsp/devicesamplemimo.h"
#include "limesdrmimosettings.h"

class QNetworkAccessManager;
class QNetworkReply;
class DeviceAPI;
class LimeSDRMIThread;
class LimeSDRMOThread;
struct DeviceLimeSDRParams;

class LimeSDRMIMO : public DeviceSampleMIMO {
    Q_OBJECT

public:
	class MsgConfigureLimeSDRMIMO : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const LimeSDRMIMOSettings& getSettings() const { return m_settings; }
        const QList<QString>& getSettingsKeys() const { return m_settingsKeys; }
		bool getForce() const { return m_force; }

		static MsgConfigureLimeSDRMIMO* create(const LimeSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) {
			return new MsgConfigureLimeSDRMIMO(settings, settingsKeys, force);
		}

	private:
		LimeSDRMIMOSettings m_settings;
        QList<QString> m_settingsKeys;
		bool m_force;

		MsgConfigureLimeSDRMIMO(const LimeSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force) :
			Message(),
			m_settings(settings),
            m_settingsKeys(settingsKeys),
			m_force(force)
		{ }
	};

    class MsgGetStreamInfo : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getRxElseTx() const { return m_rxElseTx; }
        uint32_t getChannel() const { return m_channel; }

        static MsgGetStreamInfo* create(bool rxElseTx, uint32_t channel)
        {
            return new MsgGetStreamInfo(rxElseTx, channel);
        }

    private:
        bool     m_rxElseTx;
        uint32_t m_channel;

        MsgGetStreamInfo(bool rxElseTx, uint32_t channel) :
            Message(),
            m_rxElseTx(rxElseTx),
            m_channel(channel)
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
        uint32_t getFifoFilledCount() const { return m_fifoFilledCount; }
        uint32_t getFifoSize() const { return m_fifoSize; }
        uint32_t getUnderrun() const { return m_underrun; }
        uint32_t getOverrun() const { return m_overrun; }
        uint32_t getDroppedPackets() const { return m_droppedPackets; }
        float    getLinkRate() const { return m_linkRate; }
        uint64_t getTimestamp() const { return m_timestamp; }

        static MsgReportStreamInfo* create(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    linkRate,
                uint64_t timestamp
                )
        {
            return new MsgReportStreamInfo(
                    success,
                    active,
                    fifoFilledCount,
                    fifoSize,
                    underrun,
                    overrun,
                    droppedPackets,
                    linkRate,
                    timestamp
                    );
        }

    private:
        bool     m_success;
        // everything from lms_stream_status_t
        bool     m_active; //!< Indicates whether the stream is currently active
        uint32_t m_fifoFilledCount; //!< Number of samples in FIFO buffer
        uint32_t m_fifoSize; //!< Size of FIFO buffer
        uint32_t m_underrun; //!< FIFO underrun count
        uint32_t m_overrun; //!< FIFO overrun count
        uint32_t m_droppedPackets; //!< Number of dropped packets by HW
        float    m_linkRate; //!< Combined data rate of all stream of the same direction (TX or RX)
        uint64_t m_timestamp; //!< Current HW timestamp

        MsgReportStreamInfo(
                bool     success,
                bool     active,
                uint32_t fifoFilledCount,
                uint32_t fifoSize,
                uint32_t underrun,
                uint32_t overrun,
                uint32_t droppedPackets,
                float    linkRate,
                uint64_t timestamp
                ) :
            Message(),
            m_success(success),
            m_active(active),
            m_fifoFilledCount(fifoFilledCount),
            m_fifoSize(fifoSize),
            m_underrun(underrun),
            m_overrun(overrun),
            m_droppedPackets(droppedPackets),
            m_linkRate(linkRate),
            m_timestamp(timestamp)
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

    LimeSDRMIMO(DeviceAPI *deviceAPI);
	virtual ~LimeSDRMIMO();
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
            const LimeSDRMIMOSettings& settings);

    static void webapiUpdateDeviceSettings(
            LimeSDRMIMOSettings& settings,
            const QStringList& deviceSettingsKeys,
            SWGSDRangel::SWGDeviceSettings& response);

    bool isRecording(unsigned int istream) const { (void) istream; return false; }

    void getRxFrequencyRange(uint64_t& min, uint64_t& max, int& step);
    void getRxSampleRateRange(int& min, int& max, int& step);
    void getRxLPFRange(int& min, int& max, int& step);

    void getTxFrequencyRange(uint64_t& min, uint64_t& max, int& step);
    void getTxSampleRateRange(int& min, int& max, int& step);
    void getTxLPFRange(int& min, int& max, int& step);

    bool getRxRunning() const { return m_runningRx; }
    bool getTxRunning() const { return m_runningTx; }

private:
	DeviceAPI *m_deviceAPI;
	QMutex m_mutex;
	LimeSDRMIMOSettings m_settings;
	LimeSDRMIThread* m_sourceThread;
    LimeSDRMOThread* m_sinkThread;
	QString m_deviceDescription;
	bool m_runningRx;
	bool m_runningTx;
    QNetworkAccessManager *m_networkManager;
    QNetworkRequest m_networkRequest;

    DeviceLimeSDRParams *m_deviceParams;
    bool m_rxChannelEnabled[2];
    bool m_txChannelEnabled[2];
    lms_stream_t m_rxStreams[2];
    bool m_rxStreamStarted[2];
    lms_stream_t m_txStreams[2];
    bool m_txStreamStarted[2];
    bool m_open;

    bool openDevice();
    void closeDevice();
    bool setupRxStream(unsigned int channel);
    void destroyRxStream(unsigned int channel);
    bool setupTxStream(unsigned int channel);
    void destroyTxStream(unsigned int channel);

	bool applySettings(const LimeSDRMIMOSettings& settings, const QList<QString>& settingsKeys, bool force);
    void applyRxGainMode(
        unsigned int channel,
        bool& doCalibration,
        LimeSDRMIMOSettings::RxGainMode gainMode,
        uint32_t gain,
        uint32_t lnaGain,
        uint32_t tiaGain,
        uint32_t pgaGain
    );
    void applyRxGain(unsigned int channel, bool& doCalibration, uint32_t gain);
    void applyRxLNAGain(unsigned int channel, bool& doCalibration, uint32_t lnaGain);
    void applyRxTIAGain(unsigned int channel, bool& doCalibration, uint32_t tiaGain);
    void applyRxPGAGain(unsigned int channel, bool& doCalibration, uint32_t pgaGain);
    void applyRxLPFIRBW(unsigned int channel, bool lpfFIREnable, float lpfFIRBW);
    void applyRxNCOFrequency(unsigned int channel, bool ncoEnable, int ncoFrequency);
    void applyRxAntennaPath(unsigned int channel, bool& doCalibration, LimeSDRMIMOSettings::PathRxRFE path);
    void applyRxCalibration(unsigned int channel, qint32 devSampleRate);
    void applyRxLPCalibration(unsigned int channel, float lpfBW);
    void applyTxGain(unsigned int channel, bool& doCalibration, uint32_t gain);
    void applyTxLPFIRBW(unsigned int channel, bool lpfFIREnable, float lpfFIRBW);
    void applyTxNCOFrequency(unsigned int channel, bool ncoEnable, int ncoFrequency);
    void applyTxAntennaPath(unsigned int channel, bool& doCalibration, LimeSDRMIMOSettings::PathTxRFE path);
    bool setRxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    bool setTxDeviceCenterFrequency(struct bladerf *dev, quint64 freq_hz, int loPpmTenths);
    void applyTxCalibration(unsigned int channel, qint32 devSampleRate);
    void applyTxLPCalibration(unsigned int channel, float lpfBW);
    void webapiFormatDeviceReport(SWGSDRangel::SWGDeviceReport& response);
    void webapiReverseSendSettings(const QList<QString>& deviceSettingsKeys, const LimeSDRMIMOSettings& settings, bool force);
    void webapiReverseSendStartStop(bool start);

private slots:
    void networkManagerFinished(QNetworkReply *reply);
};

#endif // PLUGINS_SAMPLEMIMO_LIMESDRMIMO_LIMESDRMIMO_H_
