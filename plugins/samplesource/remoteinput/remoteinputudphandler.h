///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
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

#ifndef PLUGINS_SAMPLESOURCE_REMOTEINPUT_REMOTEINPUTUDPHANDLER_H_
#define PLUGINS_SAMPLESOURCE_REMOTEINPUT_REMOTEINPUTUDPHANDLER_H_

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QMutex>
#include <QElapsedTimer>

#include "util/messagequeue.h"
#include "remoteinputbuffer.h"

#define REMOTEINPUT_THROTTLE_MS 50

class SampleSinkFifo;
class MessageQueue;
class QTimer;
class DeviceAPI;

class RemoteInputUDPHandler : public QObject
{
	Q_OBJECT
public:
	class MsgReportMetaDataChange : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const RemoteMetaDataFEC& getMetaData() const { return m_metaData; }

		static MsgReportMetaDataChange* create(const RemoteMetaDataFEC& metaData)
		{
			return new MsgReportMetaDataChange(metaData);
		}

	protected:
		RemoteMetaDataFEC m_metaData;

		MsgReportMetaDataChange(const RemoteMetaDataFEC& metaData) :
			Message(),
			m_metaData(metaData)
		{ }
	};

	RemoteInputUDPHandler(SampleSinkFifo* sampleFifo, DeviceAPI *deviceAPI);
	~RemoteInputUDPHandler();
	void setMessageQueueToInput(MessageQueue *queue) { m_messageQueueToInput = queue; }
	void setMessageQueueToGUI(MessageQueue *queue) { m_messageQueueToGUI = queue; }
    void start();
	void stop();
	void configureUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin);
	void getRemoteAddress(QString& s) const { s = m_remoteAddress.toString(); }
    int getNbOriginalBlocks() const { return RemoteNbOrginalBlocks; }
    bool isStreaming() const { return m_masterTimerConnected; }
    int getSampleRate() const { return m_samplerate; }
    int getCenterFrequency() const { return m_centerFrequency; }
    int getBufferGauge() const { return m_remoteInputBuffer.getBufferGauge(); }
    uint64_t getTVmSec() const { return m_tv_msec; }
    int getMinNbBlocks() { return m_remoteInputBuffer.getMinNbBlocks(); }
    int getMaxNbRecovery() { return m_remoteInputBuffer.getMaxNbRecovery(); }
	const RemoteMetaDataFEC& getCurrentMeta() const { return m_currentMeta; }

public slots:
	void dataReadyRead();

private:
    class MsgUDPAddressAndPort : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getAddress() const { return m_address; }
        quint16 getPort() const { return m_port; }
        const QString& getMulticastAddress() const { return m_multicastAddress; }
        bool getMulticastJoin() const { return m_multicastJoin; }

        static MsgUDPAddressAndPort* create(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin)
        {
            return new MsgUDPAddressAndPort(address, port, multicastAddress, multicastJoin);
        }

    private:
        QString m_address;
        quint16 m_port;
        QString m_multicastAddress;
        bool m_multicastJoin;

        MsgUDPAddressAndPort(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin) :
            Message(),
            m_address(address),
            m_port(port),
            m_multicastAddress(multicastAddress),
            m_multicastJoin(multicastJoin)
        { }
    };

	DeviceAPI *m_deviceAPI;
	const QTimer& m_masterTimer;
	bool m_masterTimerConnected;
	bool m_running;
    uint32_t m_rateDivider;
	RemoteInputBuffer m_remoteInputBuffer;
	RemoteMetaDataFEC m_currentMeta;
	QUdpSocket *m_dataSocket;
	QHostAddress m_dataAddress;
	QHostAddress m_remoteAddress;
	quint16 m_dataPort;
	QHostAddress m_multicastAddress;
	bool m_multicast;
	bool m_dataConnected;
	char *m_udpBuf;
	qint64 m_udpReadBytes;
	SampleSinkFifo *m_sampleFifo;
	uint32_t m_samplerate;
	uint64_t m_centerFrequency;
	uint64_t m_tv_msec;
    MessageQueue *m_messageQueueToInput;
	MessageQueue *m_messageQueueToGUI;
	uint32_t m_tickCount;
	std::size_t m_samplesCount;
    QTimer *m_timer;

	QElapsedTimer m_elapsedTimer;
	int m_throttlems;
    int32_t m_readLengthSamples;
    uint32_t m_readLength;
    int32_t *m_converterBuffer;
    uint32_t m_converterBufferNbSamples;
    bool m_throttleToggle;
    bool m_autoCorrBuffer;

    MessageQueue m_inputMessageQueue;

	void connectTimer();
    void disconnectTimer();
	void processData();
    void adjustNbDecoderSlots(const RemoteMetaDataFEC& metaData);
	int getDataSocketBufferSize();
	void applyUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool muticastJoin);
	bool handleMessage(const Message& message);

private slots:
	void tick();
    void handleMessages();
};



#endif /* PLUGINS_SAMPLESOURCE_REMOTEINPUT_REMOTEINPUTUDPHANDLER_H_ */
