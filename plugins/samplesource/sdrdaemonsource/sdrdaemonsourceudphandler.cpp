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

#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include <device/devicesourceapi.h>

#include "sdrdaemonsourceinput.h"
#include "sdrdaemonsourceudphandler.h"

SDRdaemonSourceUDPHandler::SDRdaemonSourceUDPHandler(SampleSinkFifo *sampleFifo, MessageQueue *outputMessageQueueToGUI, DeviceSourceAPI *devieAPI) :
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
    m_throttlems(SDRDAEMONSOURCE_THROTTLE_MS),
    m_readLengthSamples(0),
    m_readLength(0),
    m_throttleToggle(false),
    m_rateDivider(1000/SDRDAEMONSOURCE_THROTTLE_MS),
	m_autoCorrBuffer(true)
{
    m_udpBuf = new char[SDRdaemonSourceBuffer::m_udpPayloadSize];
}

SDRdaemonSourceUDPHandler::~SDRdaemonSourceUDPHandler()
{
	stop();
	delete[] m_udpBuf;
#ifdef USE_INTERNAL_TIMER
    if (m_timer) {
        delete m_timer;
    }
#endif
}

void SDRdaemonSourceUDPHandler::start()
{
	qDebug("SDRdaemonSourceUDPHandler::start");

	if (!m_dataSocket)
	{
		m_dataSocket = new QUdpSocket(this);
	}

    if (!m_dataConnected)
	{
        connect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()), Qt::QueuedConnection); // , Qt::QueuedConnection

        if (m_dataSocket->bind(m_dataAddress, m_dataPort))
		{
			qDebug("SDRdaemonSourceUDPHandler::start: bind data socket to %s:%d", m_dataAddress.toString().toStdString().c_str(),  m_dataPort);
			m_dataConnected = true;
		}
		else
		{
			qWarning("SDRdaemonSourceUDPHandler::start: cannot bind data port %d", m_dataPort);
	        disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
			m_dataConnected = false;
		}
	}

	// Need to notify the DSP engine to actually start
	DSPSignalNotification *notif = new DSPSignalNotification(m_samplerate, m_centerFrequency * 1000); // Frequency in Hz for the DSP engine
	m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
    m_elapsedTimer.start();
}

void SDRdaemonSourceUDPHandler::stop()
{
	qDebug("SDRdaemonSourceUDPHandler::stop");

    if (m_dataConnected)
    {
		m_dataConnected = false;
	    disconnect(m_dataSocket, SIGNAL(readyRead()), this, SLOT(dataReadyRead()));
	}

	if (m_dataSocket)
	{
		delete m_dataSocket;
		m_dataSocket = 0;
	}
}

void SDRdaemonSourceUDPHandler::configureUDPLink(const QString& address, quint16 port)
{
	qDebug("SDRdaemonSourceUDPHandler::configureUDPLink: %s:%d", address.toStdString().c_str(), port);
	bool addressOK = m_dataAddress.setAddress(address);

	if (!addressOK)
	{
		qWarning("SDRdaemonSourceUDPHandler::configureUDPLink: invalid address %s. Set to localhost.", address.toStdString().c_str());
		m_dataAddress = QHostAddress::LocalHost;
	}

	stop();
	m_dataPort = port;
	start();
}

void SDRdaemonSourceUDPHandler::dataReadyRead()
{
    m_udpReadBytes = 0;

	while (m_dataSocket->hasPendingDatagrams() && m_dataConnected)
	{
		qint64 pendingDataSize = m_dataSocket->pendingDatagramSize();
		m_udpReadBytes += m_dataSocket->readDatagram(&m_udpBuf[m_udpReadBytes], pendingDataSize, &m_remoteAddress, 0);

		if (m_udpReadBytes == SDRdaemonSourceBuffer::m_udpPayloadSize) {
		    processData();
		    m_udpReadBytes = 0;
		}
	}
}

void SDRdaemonSourceUDPHandler::processData()
{
    m_sdrDaemonBuffer.writeData(m_udpBuf);
    const SDRdaemonSourceBuffer::MetaDataFEC& metaData =  m_sdrDaemonBuffer.getCurrentMeta();

    bool change = false;
//    m_tv_sec = metaData.m_tv_sec;
//    m_tv_usec = metaData.m_tv_usec;
    m_tv_sec = m_sdrDaemonBuffer.getTVOutSec();
    m_tv_usec = m_sdrDaemonBuffer.getTVOutUsec();

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
        m_deviceAPI->getDeviceEngineInputMessageQueue()->push(notif);
        SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData *report = SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamData::create(
            m_samplerate,
            m_centerFrequency * 1000, // Frequency in Hz for the GUI
            m_tv_sec,
            m_tv_usec);

        m_outputMessageQueueToGUI->push(report);
    }
}

void SDRdaemonSourceUDPHandler::connectTimer(const QTimer* timer)
{
	qDebug() << "SDRdaemonSourceUDPHandler::connectTimer";
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

void SDRdaemonSourceUDPHandler::tick()
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

    m_readLength = m_readLengthSamples * SDRdaemonSourceBuffer::m_iqSampleSize;

    // read samples directly feeding the SampleFifo (no callback)
    m_sampleFifo->write(reinterpret_cast<quint8*>(m_sdrDaemonBuffer.readData(m_readLength)), m_readLength);
    m_samplesCount += m_readLengthSamples;

	if (m_tickCount < m_rateDivider)
	{
		m_tickCount++;
	}
	else
	{
	    int framesDecodingStatus;
        int minNbBlocks = m_sdrDaemonBuffer.getMinNbBlocks();
	    int minNbOriginalBlocks = m_sdrDaemonBuffer.getMinOriginalBlocks();
	    int nbOriginalBlocks = m_sdrDaemonBuffer.getCurrentMeta().m_nbOriginalBlocks;
	    int nbFECblocks = m_sdrDaemonBuffer.getCurrentMeta().m_nbFECBlocks;
		m_tickCount = 0;

		//framesDecodingStatus = (minNbOriginalBlocks == nbOriginalBlocks ? 2 : (minNbOriginalBlocks < nbOriginalBlocks - nbFECblocks ? 0 : 1));
		if (minNbBlocks < nbOriginalBlocks) {
			framesDecodingStatus = 0;
		} else if (minNbBlocks < nbOriginalBlocks + nbFECblocks) {
			framesDecodingStatus = 1;
		} else {
			framesDecodingStatus = 2;
		}

		SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming *report = SDRdaemonSourceInput::MsgReportSDRdaemonSourceStreamTiming::create(
			m_tv_sec,
			m_tv_usec,
			m_sdrDaemonBuffer.getBufferLengthInSecs(),
            m_sdrDaemonBuffer.getBufferGauge(),
            framesDecodingStatus,
            minNbBlocks == nbOriginalBlocks + nbFECblocks,
            minNbBlocks,
            minNbOriginalBlocks,
            m_sdrDaemonBuffer.getMaxNbRecovery(),
            m_sdrDaemonBuffer.getAvgNbBlocks(),
            m_sdrDaemonBuffer.getAvgOriginalBlocks(),
            m_sdrDaemonBuffer.getAvgNbRecovery(),
            nbOriginalBlocks,
            nbFECblocks);

            m_outputMessageQueueToGUI->push(report);
	}
}
