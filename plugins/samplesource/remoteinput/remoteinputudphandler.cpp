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

#include <QUdpSocket>
#include <QDebug>
#include <QTimer>

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "device/deviceapi.h"

#include "remoteinputudphandler.h"
#include "remoteinput.h"

MESSAGE_CLASS_DEFINITION(RemoteInputUDPHandler::MsgReportMetaDataChange, Message)
MESSAGE_CLASS_DEFINITION(RemoteInputUDPHandler::MsgUDPAddressAndPort, Message)

RemoteInputUDPHandler::RemoteInputUDPHandler(SampleSinkFifo *sampleFifo, DeviceAPI *deviceAPI) :
    m_deviceAPI(deviceAPI),
    m_masterTimer(deviceAPI->getMasterTimer()),
    m_masterTimerConnected(false),
    m_running(false),
    m_rateDivider(1000/REMOTEINPUT_THROTTLE_MS),
	m_dataSocket(nullptr),
	m_dataAddress(QHostAddress::LocalHost),
	m_remoteAddress(QHostAddress::LocalHost),
	m_dataPort(9090),
    m_multicastAddress(QStringLiteral("224.0.0.1")),
    m_multicast(false),
	m_dataConnected(false),
	m_udpBuf(nullptr),
	m_udpReadBytes(0),
	m_sampleFifo(sampleFifo),
	m_samplerate(0),
	m_centerFrequency(0),
	m_tv_msec(0),
	m_messageQueueToGUI(0),
	m_tickCount(0),
	m_samplesCount(0),
	m_timer(0),
    m_throttlems(REMOTEINPUT_THROTTLE_MS),
    m_readLengthSamples(0),
    m_readLength(0),
    m_converterBuffer(nullptr),
    m_converterBufferNbSamples(0),
    m_throttleToggle(false),
	m_autoCorrBuffer(true)
{
    m_udpBuf = new char[RemoteUdpSize];

#ifdef USE_INTERNAL_TIMER
#warning "Uses internal timer"
    m_timer = new QTimer();
    m_timer->start(50);
    m_throttlems = m_timer->interval();
#else
    m_throttlems = m_masterTimer.interval();
#endif
    m_rateDivider = 1000 / m_throttlems;

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleMessages()));
}

RemoteInputUDPHandler::~RemoteInputUDPHandler()
{
	stop();
	delete[] m_udpBuf;
	if (m_converterBuffer) { delete[] m_converterBuffer; }
#ifdef USE_INTERNAL_TIMER
    if (m_timer) {
        delete m_timer;
    }
#endif
}

void RemoteInputUDPHandler::start()
{
	qDebug("RemoteInputUDPHandler::start");

	if (m_running) {
	    return;
	}

	if (!m_dataSocket)
    {
		m_dataSocket = new QUdpSocket(this);
        m_dataSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, getDataSocketBufferSize());
	}

    if (!m_dataConnected)
	{
        if (m_dataSocket->bind(m_multicast ? QHostAddress::AnyIPv4 : m_dataAddress, m_dataPort, QUdpSocket::ShareAddress))
		{
			qDebug("RemoteInputUDPHandler::start: bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);

            if (m_multicast)
            {
                if (m_dataSocket->joinMulticastGroup(m_multicastAddress)) {
                    qDebug("RemoteInputUDPHandler::start: joined multicast group %s", qPrintable(m_multicastAddress.toString()));
                } else {
                    qDebug("RemoteInputUDPHandler::start: failed joining multicast group %s", qPrintable(m_multicastAddress.toString()));
                }
            }

            connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead())); //, Qt::QueuedConnection);
			m_dataConnected = true;
		}
		else
		{
			qWarning("RemoteInputUDPHandler::start: cannot bind data port %d", m_dataPort);
			m_dataConnected = false;
		}
	}

    m_elapsedTimer.start();
    m_running = true;
}

void RemoteInputUDPHandler::stop()
{
	qDebug("RemoteInputUDPHandler::stop");

	if (!m_running) {
	    return;
	}

	disconnectTimer();

    if (m_dataConnected)
    {
		m_dataConnected = false;
	    disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
	}

	if (m_dataSocket)
	{
		delete m_dataSocket;
		m_dataSocket = nullptr;
	}

	m_centerFrequency = 0;
	m_samplerate = 0;
	m_running = false;
}

void RemoteInputUDPHandler::configureUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin)
{
    Message* msg = MsgUDPAddressAndPort::create(address, port, multicastAddress, multicastJoin);
    m_inputMessageQueue.push(msg);
}

void RemoteInputUDPHandler::applyUDPLink(const QString& address, quint16 port, const QString& multicastAddress, bool multicastJoin)
{
    qDebug() << "RemoteInputUDPHandler::applyUDPLink: "
        << " address: " << address
        << " port: " << port
        << " multicastAddress: " << multicastAddress
        << " multicastJoin: " << multicastJoin;

	bool addressOK = m_dataAddress.setAddress(address);

	if (!addressOK)
	{
		qWarning("RemoteInputUDPHandler::applyUDPLink: invalid address %s. Set to localhost.", address.toStdString().c_str());
		m_dataAddress = QHostAddress::LocalHost;
	}

    m_multicast = multicastJoin;
    addressOK = m_multicastAddress.setAddress(multicastAddress);

    if (!addressOK)
    {
        qWarning("RemoteInputUDPHandler::applyUDPLink: invalid multicast address %s. disabling multicast.", address.toStdString().c_str());
        m_multicast = false;
    }

	m_dataPort = port;
	stop();
	start();
}

void RemoteInputUDPHandler::dataReadyRead()
{
    m_udpReadBytes = 0;

	while (m_dataSocket->hasPendingDatagrams() && m_dataConnected)
	{
		qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
		m_udpReadBytes += m_dataSocket->readDatagram(&m_udpBuf[m_udpReadBytes], pendingDataSize, &m_remoteAddress, 0);

		if (m_udpReadBytes == RemoteUdpSize) {
		    processData();
		    m_udpReadBytes = 0;
		}
	}
}

void RemoteInputUDPHandler::processData()
{
    m_remoteInputBuffer.writeData(m_udpBuf);
    const RemoteMetaDataFEC& metaData =  m_remoteInputBuffer.getCurrentMeta();

    if (!(m_currentMeta == metaData))
    {
        m_currentMeta = metaData;

        if (m_messageQueueToInput)
        {
            MsgReportMetaDataChange *msg = MsgReportMetaDataChange::create(m_currentMeta);
            m_messageQueueToInput->push(msg);
        }
    }

    bool change = false;

    m_tv_msec = m_remoteInputBuffer.getTVOutMSec();

    if (m_centerFrequency != metaData.m_centerFrequency)
    {
        m_centerFrequency = metaData.m_centerFrequency;
        change = true;
    }

    if (m_samplerate != metaData.m_sampleRate)
    {
        disconnectTimer();
        adjustNbDecoderSlots(metaData);
        m_samplerate = metaData.m_sampleRate;
        change = true;
    }

    if (change && (m_samplerate != 0))
    {
        qDebug("RemoteInputUDPHandler::processData: m_samplerate: %u S/s m_centerFrequency: %lu Hz", m_samplerate, m_centerFrequency);

        DSPSignalNotification *notif = new DSPSignalNotification(m_samplerate, m_centerFrequency); // Frequency in Hz for the DSP engine
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);

        if (m_messageQueueToGUI)
        {
            RemoteInput::MsgReportRemoteInputStreamData *report = RemoteInput::MsgReportRemoteInputStreamData::create(
                m_samplerate,
                m_centerFrequency, // Frequency in Hz for the GUI
                m_tv_msec);

            m_messageQueueToGUI->push(report);
        }

        m_dataSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, getDataSocketBufferSize());
        m_elapsedTimer.restart();
        m_throttlems = 0;
        connectTimer();
    }
}

void RemoteInputUDPHandler::adjustNbDecoderSlots(const RemoteMetaDataFEC& metaData)
{
    int sampleRate = metaData.m_sampleRate;
    int sampleBytes = metaData.m_sampleBytes;
    int bufferFrameSize = RemoteInputBuffer::getBufferFrameSize();
    float fNbDecoderSlots = (float) (4 * sampleBytes * sampleRate) / (float) bufferFrameSize;
    int rawNbDecoderSlots = ((((int) ceil(fNbDecoderSlots)) / 2) * 2) + 2; // next multiple of 2
    qDebug("RemoteInputUDPHandler::adjustNbDecoderSlots: rawNbDecoderSlots: %d", rawNbDecoderSlots);
    m_remoteInputBuffer.setNbDecoderSlots(rawNbDecoderSlots < 4 ? 4 : rawNbDecoderSlots);
    m_remoteInputBuffer.setBufferLenSec(metaData);
}

void RemoteInputUDPHandler::connectTimer()
{
    if (!m_masterTimerConnected)
    {
        qDebug() << "RemoteInputUDPHandler::connectTimer";
#ifdef USE_INTERNAL_TIMER
#warning "Uses internal timer"
        connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
#else
        connect(&m_masterTimer, SIGNAL(timeout()), this, SLOT(tick()));
#endif
        m_masterTimerConnected = true;
    }
}

void RemoteInputUDPHandler::disconnectTimer()
{
    if (m_masterTimerConnected)
    {
        qDebug() << "RemoteInputUDPHandler::disconnectTimer";
#ifdef USE_INTERNAL_TIMER
#warning "Uses internal timer"
        disconnect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
#else
        disconnect(&m_masterTimer, SIGNAL(timeout()), this, SLOT(tick()));
#endif
        m_masterTimerConnected = false;
    }
}

void RemoteInputUDPHandler::tick()
{
    // auto throttling
    int throttlems = m_elapsedTimer.restart();

    if (throttlems != m_throttlems)
    {
        m_throttlems = throttlems;
        m_readLengthSamples = (m_currentMeta.m_sampleRate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
        m_throttleToggle = !m_throttleToggle;
    }

    if (m_autoCorrBuffer)
    {
        m_readLengthSamples += m_remoteInputBuffer.getRWBalanceCorrection();
        // Eliminate negative or excessively high values
        m_readLengthSamples = m_readLengthSamples < 0 ?
            0 : m_readLengthSamples > (int) m_currentMeta.m_sampleRate/5 ?
                m_remoteInputBuffer.getCurrentMeta().m_sampleRate/5 : m_readLengthSamples;
    }

    m_readLength = m_readLengthSamples * (m_currentMeta.m_sampleBytes & 0xF) * 2;

    if (m_currentMeta.m_sampleBits == SDR_RX_SAMP_SZ) // no conversion
    {
        // read samples directly feeding the SampleFifo (no callback)
        m_sampleFifo->write(reinterpret_cast<quint8*>(m_remoteInputBuffer.readData(m_readLength)), m_readLength);
        m_samplesCount += m_readLengthSamples;
    }
    else if ((m_currentMeta.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 16)) // 8 -> 16
    {
        if (m_readLengthSamples > (int) m_converterBufferNbSamples)
        {
            if (m_converterBuffer) { delete[] m_converterBuffer; }
            m_converterBuffer = new int32_t[m_readLengthSamples];
        }

        int8_t *buf = (int8_t*) m_remoteInputBuffer.readData(m_readLength);

        for (int is = 0; is < m_readLengthSamples; is++)
        {
            m_converterBuffer[is] = buf[2*is+1] * (1<<8); // Q -> MSB
            m_converterBuffer[is] <<= 16;
            m_converterBuffer[is] += buf[2*is] * (1<<8);  // I -> LSB
        }
    }
    else if ((m_currentMeta.m_sampleBits == 8) && (SDR_RX_SAMP_SZ == 24)) // 8 -> 24
    {
        if (m_readLengthSamples > (int) m_converterBufferNbSamples)
        {
            if (m_converterBuffer) { delete[] m_converterBuffer; }
            m_converterBuffer = new int32_t[m_readLengthSamples*2];
        }

        int8_t *buf = (int8_t*) m_remoteInputBuffer.readData(m_readLength);

        for (int is = 0; is < m_readLengthSamples; is++)
        {
            m_converterBuffer[2*is] = buf[2*is] * (1<<16);     // I
            m_converterBuffer[2*is+1] = buf[2*is+1] * (1<<16); // Q
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(m_converterBuffer), m_readLengthSamples*sizeof(Sample));
    }
    else if (m_currentMeta.m_sampleBits == 16) // 16 -> 24
    {
        if (m_readLengthSamples > (int) m_converterBufferNbSamples)
        {
            if (m_converterBuffer) { delete[] m_converterBuffer; }
            m_converterBuffer = new int32_t[m_readLengthSamples*2];
        }

        int16_t *buf = (int16_t*) m_remoteInputBuffer.readData(m_readLength);

        for (int is = 0; is < m_readLengthSamples; is++)
        {
            m_converterBuffer[2*is] = buf[2*is] * (1<<8);     // I
            m_converterBuffer[2*is+1] = buf[2*is+1] * (1<<8); // Q
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(m_converterBuffer), m_readLengthSamples*sizeof(Sample));
    }
    else if (m_currentMeta.m_sampleBits == 24) // 24 -> 16
    {
        if (m_readLengthSamples > (int) m_converterBufferNbSamples)
        {
            if (m_converterBuffer) { delete[] m_converterBuffer; }
            m_converterBuffer = new int32_t[m_readLengthSamples];
        }

        int32_t *buf = (int32_t*) m_remoteInputBuffer.readData(m_readLength);

        for (int is = 0; is < m_readLengthSamples; is++)
        {
            m_converterBuffer[is] =  buf[2*is+1] / (1<<8); // Q -> MSB
            m_converterBuffer[is] <<= 16;
            m_converterBuffer[is] += buf[2*is] / (1<<8);   // I -> LSB
        }

        m_sampleFifo->write(reinterpret_cast<quint8*>(m_converterBuffer), m_readLengthSamples*sizeof(Sample));
    }
    else // invalid size
    {
        qWarning("RemoteInputUDPHandler::tick: unexpected sample size in stream: %d bits", (int) m_currentMeta.m_sampleBits);
    }

	if (m_tickCount < m_rateDivider)
	{
		m_tickCount++;
	}
	else
	{
		m_tickCount = 0;

		if (m_messageQueueToGUI)
		{
	        int framesDecodingStatus;
	        int minNbBlocks = m_remoteInputBuffer.getMinNbBlocks();
	        int minNbOriginalBlocks = m_remoteInputBuffer.getMinOriginalBlocks();
	        int nbOriginalBlocks = m_remoteInputBuffer.getCurrentMeta().m_nbOriginalBlocks;
	        int nbFECblocks = m_remoteInputBuffer.getCurrentMeta().m_nbFECBlocks;
	        int sampleBits = m_remoteInputBuffer.getCurrentMeta().m_sampleBits;
	        int sampleBytes = m_remoteInputBuffer.getCurrentMeta().m_sampleBytes;

	        //framesDecodingStatus = (minNbOriginalBlocks == nbOriginalBlocks ? 2 : (minNbOriginalBlocks < nbOriginalBlocks - nbFECblocks ? 0 : 1));
	        if (minNbBlocks < nbOriginalBlocks) {
	            framesDecodingStatus = 0;
	        } else if (minNbBlocks < nbOriginalBlocks + nbFECblocks) {
	            framesDecodingStatus = 1;
	        } else {
	            framesDecodingStatus = 2;
	        }

	        RemoteInput::MsgReportRemoteInputStreamTiming *report = RemoteInput::MsgReportRemoteInputStreamTiming::create(
	            m_tv_msec,
	            m_remoteInputBuffer.getBufferLengthInSecs(),
	            m_remoteInputBuffer.getBufferGauge(),
	            framesDecodingStatus,
	            minNbBlocks == nbOriginalBlocks + nbFECblocks,
	            minNbBlocks,
	            minNbOriginalBlocks,
	            m_remoteInputBuffer.getMaxNbRecovery(),
	            m_remoteInputBuffer.getAvgNbBlocks(),
	            m_remoteInputBuffer.getAvgOriginalBlocks(),
	            m_remoteInputBuffer.getAvgNbRecovery(),
	            nbOriginalBlocks,
	            nbFECblocks,
	            sampleBits,
	            sampleBytes);

	            m_messageQueueToGUI->push(report);
		}
	}
}

void RemoteInputUDPHandler::handleMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool RemoteInputUDPHandler::handleMessage(const Message& cmd)
{
    if (MsgUDPAddressAndPort::match(cmd))
    {
        MsgUDPAddressAndPort& notif = (MsgUDPAddressAndPort&) cmd;
        applyUDPLink(notif.getAddress(), notif.getPort(), notif.getMulticastAddress(), notif.getMulticastJoin());
        return true;
    }
    else
    {
        return false;
    }
}

int RemoteInputUDPHandler::getDataSocketBufferSize()
{
    // set a floor value at 96 kS/s
    uint32_t samplerate = m_samplerate < 96000 ? 96000 : m_samplerate;
    // 250 ms (1/4s) at current sample rate
    int bufferSize = (samplerate * 2 * (SDR_RX_SAMP_SZ == 16 ? 2 : 4)) / 4;
    qDebug("RemoteInputUDPHandler::getDataSocketBufferSize: %d bytes", bufferSize);
    return bufferSize;
}
