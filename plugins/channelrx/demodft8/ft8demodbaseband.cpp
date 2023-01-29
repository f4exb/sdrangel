///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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

#include <QDebug>
#include <QThread>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"
#include "maincore.h"

#include "ft8demodworker.h"
#include "ft8demodbaseband.h"

MESSAGE_CLASS_DEFINITION(FT8DemodBaseband::MsgConfigureFT8DemodBaseband, Message)

FT8DemodBaseband::FT8DemodBaseband() :
    m_channelizer(&m_sink),
    m_messageQueueToGUI(nullptr),
    m_spectrumVis(nullptr),
    m_deviceCenterFrequency(0)
{
    qDebug("FT8DemodBaseband::FT8DemodBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_ft8WorkerBuffer = new int16_t[FT8DemodSettings::m_ft8SampleRate*15];

    m_workerThread = new QThread();
    m_ft8DemodWorker = new FT8DemodWorker();
    m_ft8DemodWorker->moveToThread(m_workerThread);

    QObject::connect(
        m_workerThread,
        &QThread::finished,
        m_ft8DemodWorker,
        &QObject::deleteLater
    );
    QObject::connect(
        m_workerThread,
        &QThread::finished,
        m_ft8DemodWorker,
        &QThread::deleteLater
    );
    QObject::connect(
        this,
        &FT8DemodBaseband::bufferReady,
        m_ft8DemodWorker,
        &FT8DemodWorker::processBuffer,
        Qt::QueuedConnection
    );

    m_workerThread->start();

    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &FT8DemodBaseband::handleData,
        Qt::QueuedConnection
    );

    m_channelSampleRate = 0;
    m_sink.setFT8Buffer(&m_ft8Buffer);

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick()));
}

FT8DemodBaseband::~FT8DemodBaseband()
{
    m_workerThread->exit();
	m_workerThread->wait();
    delete[] m_ft8WorkerBuffer;
}

void FT8DemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
    m_channelSampleRate = 0;
}

void FT8DemodBaseband::setMessageQueueToGUI(MessageQueue *messageQueue)
{
    m_messageQueueToGUI = messageQueue;
    m_ft8DemodWorker->setReportingMessageQueue(m_messageQueueToGUI);
}

void FT8DemodBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
    m_ft8DemodWorker->setChannel(channel);
}

void FT8DemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void FT8DemodBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0))
    {
		SampleVector::iterator part1begin;
		SampleVector::iterator part1end;
		SampleVector::iterator part2begin;
		SampleVector::iterator part2end;

        std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

		// first part of FIFO data
        if (part1begin != part1end) {
            m_channelizer.feed(part1begin, part1end);
        }

		// second part of FIFO data (used when block wraps around)
		if(part2begin != part2end) {
            m_channelizer.feed(part2begin, part2end);
        }

		m_sampleFifo.readCommit((unsigned int) count);
    }

    qreal rmsLevel, peakLevel;
    int numSamples;
    m_sink.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void FT8DemodBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool FT8DemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureFT8DemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureFT8DemodBaseband& cfg = (MsgConfigureFT8DemodBaseband&) cmd;
        qDebug() << "FT8DemodBaseband::handleMessage: MsgConfigureFT8DemodBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "FT8DemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer.setBasebandSampleRate(notif.getSampleRate());
        m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());

        if (m_channelSampleRate != m_channelizer.getChannelSampleRate())
        {
            m_sink.applyFT8SampleRate(); // reapply when channel sample rate changes
            m_channelSampleRate = m_channelizer.getChannelSampleRate();
        }

        if (notif.getCenterFrequency() != m_deviceCenterFrequency)
        {
            m_ft8DemodWorker->invalidateSequence();
            m_deviceCenterFrequency = notif.getCenterFrequency();
            m_ft8DemodWorker->setBaseFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
        }

		return true;
    }
    else
    {
        return false;
    }
}

void FT8DemodBaseband::applySettings(const FT8DemodSettings& settings, bool force)
{
    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        m_ft8DemodWorker->invalidateSequence();
        m_ft8DemodWorker->setBaseFrequency(m_deviceCenterFrequency + settings.m_inputFrequencyOffset);
        m_channelizer.setChannelization(FT8DemodSettings::m_ft8SampleRate, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());

        if (m_channelSampleRate != m_channelizer.getChannelSampleRate())
        {
            m_sink.applyFT8SampleRate(); // reapply when channel sample rate changes
            m_channelSampleRate = m_channelizer.getChannelSampleRate();
        }
    }

    if ((settings.m_filterBank[settings.m_filterIndex].m_spanLog2 != m_settings.m_filterBank[settings.m_filterIndex].m_spanLog2) || force)
    {
        if (m_spectrumVis)
        {
            DSPSignalNotification *msg = new DSPSignalNotification(FT8DemodSettings::m_ft8SampleRate/(1<<settings.m_filterBank[settings.m_filterIndex].m_spanLog2), 0);
            m_spectrumVis->getInputMessageQueue()->push(msg);
        }
    }

    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_lowCutoff != settings.m_filterBank[settings.m_filterIndex].m_lowCutoff) || force) {
        m_ft8DemodWorker->setLowFrequency(settings.m_filterBank[settings.m_filterIndex].m_lowCutoff);
    }

    if ((m_settings.m_filterBank[m_settings.m_filterIndex].m_rfBandwidth != settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth) || force) {
        m_ft8DemodWorker->setHighFrequency(settings.m_filterBank[settings.m_filterIndex].m_rfBandwidth);
    }

    if ((settings.m_recordWav != m_settings.m_recordWav) || force) {
        m_ft8DemodWorker->setRecordSamples(settings.m_recordWav);
    }

    if ((settings.m_logMessages != m_settings.m_logMessages) || force) {
        m_ft8DemodWorker->setLogMessages(settings.m_logMessages);
    }

    if ((settings.m_nbDecoderThreads != m_settings.m_nbDecoderThreads) || force) {
        m_ft8DemodWorker->setNbDecoderThreads(settings.m_nbDecoderThreads);
    }

    if ((settings.m_decoderTimeBudget != m_settings.m_decoderTimeBudget) || force) {
        m_ft8DemodWorker->setDecoderTimeBudget(settings.m_decoderTimeBudget);
    }

    if ((settings.m_useOSD != m_settings.m_useOSD) || force) {
        m_ft8DemodWorker->setUseOSD(settings.m_useOSD);
    }

    if ((settings.m_osdDepth != m_settings.m_osdDepth) || force) {
        m_ft8DemodWorker->setOSDDepth(settings.m_osdDepth);
    }

    if ((settings.m_osdLDPCThreshold != m_settings.m_osdLDPCThreshold) || force) {
        m_ft8DemodWorker->setOSDLDPCThreshold(settings.m_osdLDPCThreshold);
    }

    if ((settings.m_verifyOSD != m_settings.m_verifyOSD) || force) {
        m_ft8DemodWorker->setVerifyOSD(settings.m_verifyOSD);
    }

    m_sink.applySettings(settings, force);
    m_settings = settings;
}

int FT8DemodBaseband::getChannelSampleRate() const
{
    return m_channelizer.getChannelSampleRate();
}


void FT8DemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer.setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer.getChannelSampleRate(), m_channelizer.getChannelFrequencyOffset());
}

void FT8DemodBaseband::tick()
{
    QDateTime nowUTC = QDateTime::currentDateTimeUtc();

    if (nowUTC.time().second() % 15 < 14)
    {
        if (m_tickCount++ == 0)
        {
            QDateTime periodTs = nowUTC.addSecs(-15);
            // qDebug("FT8DemodBaseband::tick: %s", qPrintable(nowUTC.toString("yyyy-MM-dd HH:mm:ss")));
            m_ft8Buffer.getCurrentBuffer(m_ft8WorkerBuffer);
            emit bufferReady(m_ft8WorkerBuffer, periodTs);
            periodTs = nowUTC;
        }
    }
    else
    {
        m_tickCount = 0;
    }
}
