///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include <unistd.h>

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "sdrdaemonfecinput.h"
#include "sdrdaemonfecudphandler.h"

SDRdaemonFECUDPHandler::SDRdaemonFECUDPHandler(SampleFifo *sampleFifo, MessageQueue *outputMessageQueueToGUI, DeviceAPI *devieAPI) :
    m_deviceAPI(devieAPI),
	m_sdrDaemonBuffer(m_rateDivider),
	m_dataSocket(0),
	m_dataAddress(QHostAddress::LocalHost),
	m_remoteAddress(QHostAddress::LocalHost),
	m_dataPort(9090),
	m_dataConnected(false),
	m_udpBuf(0),
	m_udpReadBytes(0),
	m_sampleFifo(sampleFifo),
	m_samplerate(0),
	m_centerFrequency(0),
	m_tv_sec(0),
	m_tv_usec(0),
	m_outputMessageQueueToGUI(outputMessageQueueToGUI),
	m_tickCount(0),
	m_samplesCount(0),
	m_timer(0),
    m_throttlems(SDRDAEMONFEC_THROTTLE_MS),
    m_readLengthSamples(0),
    m_readLength(0),
    m_throttleToggle(false),
    m_rateDivider(1000/SDRDAEMONFEC_THROTTLE_MS),
	m_autoCorrBuffer(true)
{
    m_udpBuf = new char[SDRdaemonFECBuffer::m_udpPayloadSize];
}

SDRdaemonFECUDPHandler::~SDRdaemonFECUDPHandler()
{
	stop();
	delete[] m_udpBuf;
#ifdef USE_INTERNAL_TIMER
    if (m_timer) {
        delete m_timer;
    }
#endif
}

void SDRdaemonFECUDPHandler::start()
{
	qDebug("SDRdaemonFECUDPHandler::start");

	if (!m_dataSocket)
	{
		m_dataSocket = new QUdpSocket(this);
	}

	if (!m_dataConnected)
	{
		if (m_dataSocket->bind(m_dataAddress, m_dataPort))
		{
			qDebug("SDRdaemonFECUDPHandler::start: bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);
			connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()), Qt::QueuedConnection); // , Qt::QueuedConnection
			m_dataConnected = true;
		}
		else
		{
			qWarning("SDRdaemonFECUDPHandler::start: cannot bind data port %d", m_dataPort);
			m_dataConnected = false;
		}
	}

	// Need to notify the DSP engine to actually start
	DSPSignalNotification *notif = new DSPSignalNotification(m_samplerate, m_centerFrequency * 1000); // Frequency in Hz for the DSP engine
	m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
    m_elapsedTimer.start();
}

void SDRdaemonFECUDPHandler::stop()
{
	qDebug("SDRdaemonFECUDPHandler::stop");

	if (m_dataConnected) {
		disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
		m_dataConnected = false;
	}

	if (m_dataSocket)
	{
		delete m_dataSocket;
		m_dataSocket = 0;
	}
}

void SDRdaemonFECUDPHandler::configureUDPLink(const QString& address, quint16 port)
{
	qDebug("SDRdaemonFECUDPHandler::configureUDPLink: %s:%d", address.toStdString().c_str(), port);
	bool addressOK = m_dataAddress.setAddress(address);

	if (!addressOK)
	{
		qWarning("SDRdaemonFECUDPHandler::configureUDPLink: invalid address %s. Set to localhost.", address.toStdString().c_str());
		m_dataAddress = QHostAddress::LocalHost;
	}

	stop();
	m_dataPort = port;
	start();
}

void SDRdaemonFECUDPHandler::dataReadyRead()
{
    m_udpReadBytes = 0;

	while (m_dataSocket->hasPendingDatagrams())
	{
		qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
		m_udpReadBytes += m_dataSocket->readDatagram(&m_udpBuf[m_udpReadBytes], pendingDataSize, &m_remoteAddress, 0);

		if (m_udpReadBytes == SDRdaemonFECBuffer::m_udpPayloadSize) {
		    processData();
		    m_udpReadBytes = 0;
		}
	}
}

void SDRdaemonFECUDPHandler::processData()
{
    m_sdrDaemonBuffer.writeData(m_udpBuf, m_udpReadBytes);
    const SDRdaemonFECBuffer::MetaDataFEC& metaData =  m_sdrDaemonBuffer.getCurrentMeta();

    bool change = false;
    m_tv_sec = metaData.m_tv_sec;
    m_tv_usec = metaData.m_tv_usec;

    if (m_centerFrequency != metaData.m_centerFrequency)
    {
        m_centerFrequency = metaData.m_centerFrequency;
        change = true;
    }

    if (m_samplerate != metaData.m_sampleRate)
    {
        m_samplerate = metaData.m_sampleRate;
        change = true;
    }

    if (change)
    {
        DSPSignalNotification *notif = new DSPSignalNotification(m_samplerate, m_centerFrequency * 1000); // Frequency in Hz for the DSP engine
        m_deviceAPI->getDeviceInputMessageQueue()->push(notif);
        SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData *report = SDRdaemonFECInput::MsgReportSDRdaemonFECStreamData::create(
            m_samplerate,
            m_centerFrequency * 1000, // Frequency in Hz for the GUI
            m_tv_sec,
            m_tv_usec);
        m_outputMessageQueueToGUI->push(report);
    }
}

void SDRdaemonFECUDPHandler::connectTimer(const QTimer* timer)
{
	qDebug() << "SDRdaemonFECUDPHandler::connectTimer";
#ifdef USE_INTERNAL_TIMER
#warning "Uses internal timer"
    m_timer = new QTimer();
    m_timer->start(50);
    m_throttlems = m_timer->interval();
    connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
#else
    m_throttlems = timer->interval();
    connect(timer, SIGNAL(timeout()), this, SLOT(tick()));
#endif
    m_rateDivider = 1000 / m_throttlems;
}

void SDRdaemonFECUDPHandler::tick()
{
    // auto throttling
    int throttlems = m_elapsedTimer.restart();

    if (throttlems != m_throttlems)
    {
        m_throttlems = throttlems;
        m_readLengthSamples = (m_sdrDaemonBuffer.getCurrentMeta().m_sampleRate * (m_throttlems+(m_throttleToggle ? 1 : 0))) / 1000;
        m_throttleToggle = !m_throttleToggle;
    }

    if (m_autoCorrBuffer) {
        m_readLengthSamples += m_sdrDaemonBuffer.getRWBalanceCorrection();
    }

    m_readLength = m_readLengthSamples * SDRdaemonFECBuffer::m_iqSampleSize;

    // read samples directly feeding the SampleFifo (no callback)
    m_sampleFifo->write(reinterpret_cast<quint8*>(m_sdrDaemonBuffer.readData(m_readLength)), m_readLength);
    m_samplesCount += m_readLengthSamples;

	if (m_tickCount < m_rateDivider)
	{
		m_tickCount++;
	}
	else
	{
		m_tickCount = 0;
		SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming *report = SDRdaemonFECInput::MsgReportSDRdaemonFECStreamTiming::create(
			m_tv_sec,
			m_tv_usec,
			m_sdrDaemonBuffer.getBufferLengthInSecs(),
            m_sdrDaemonBuffer.getBufferGauge(),
            SDRdaemonFECBuffer::m_nbOriginalBlocks,
            m_sdrDaemonBuffer.getCurNbBlocks(),
            m_sdrDaemonBuffer.getCurNbRecovery(),
            m_sdrDaemonBuffer.getAvgNbBlocks(),
            m_sdrDaemonBuffer.getAvgNbRecovery());
            m_outputMessageQueueToGUI->push(report);
	}
}


