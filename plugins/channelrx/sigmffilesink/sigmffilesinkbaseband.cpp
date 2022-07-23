///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#include <QTimer>
#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/spectrumvis.h"
#include "util/db.h"

#include "sigmffilesinkmessages.h"
#include "sigmffilesinkbaseband.h"

MESSAGE_CLASS_DEFINITION(SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkBaseband, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkWork, Message)

SigMFFileSinkBaseband::SigMFFileSinkBaseband() :
    m_channelizer(&m_sink),
    m_specMax(0),
    m_squelchLevel(0),
    m_squelchOpen(false),
    m_mutex(QMutex::Recursive)
{
    qDebug("SigMFFileSinkBaseband::SigMFFileSinkBaseband");
    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
}

SigMFFileSinkBaseband::~SigMFFileSinkBaseband()
{
    m_inputMessageQueue.clear();
    stopWork();
}

void SigMFFileSinkBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void SigMFFileSinkBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &SigMFFileSinkBaseband::handleData,
        Qt::QueuedConnection
    );
    QObject::connect(
        &m_inputMessageQueue,
        &MessageQueue::messageEnqueued,
        this,
        &SigMFFileSinkBaseband::handleInputMessages
    );
    m_timer = new QTimer();
    QObject::connect(
        m_timer,
        &QTimer::timeout,
        this,
        &SigMFFileSinkBaseband::tick
    );
    m_timer->start(200);
}

void SigMFFileSinkBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sink.stopRecording();
    QObject::disconnect(
        &m_inputMessageQueue,
        &MessageQueue::messageEnqueued,
        this,
        &SigMFFileSinkBaseband::handleInputMessages
    );
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &SigMFFileSinkBaseband::handleData
    );
    QObject::disconnect(
        m_timer,
        &QTimer::timeout,
        this,
        &SigMFFileSinkBaseband::tick
    );
    m_timer->stop();
    delete m_timer;
}

void SigMFFileSinkBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void SigMFFileSinkBaseband::handleData()
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
}

void SigMFFileSinkBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool SigMFFileSinkBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureSigMFFileSinkBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureSigMFFileSinkBaseband& cfg = (MsgConfigureSigMFFileSinkBaseband&) cmd;
        qDebug() << "SigMFFileSinkBaseband::handleMessage: MsgConfigureSigMFFileSinkBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "SigMFFileSinkBaseband::handleMessage: DSPSignalNotification:"
            << " basebandSampleRate: " << notif.getSampleRate()
            << " cnterFrequency: " << notif.getCenterFrequency();
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_centerFrequency = notif.getCenterFrequency();
        m_channelizer.setBasebandSampleRate(notif.getSampleRate());
        int desiredSampleRate = m_channelizer.getBasebandSampleRate() / (1<<m_settings.m_log2Decim);
        m_channelizer.setChannelization(desiredSampleRate, m_settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(
            m_channelizer.getChannelSampleRate(),
            desiredSampleRate,
            m_channelizer.getChannelFrequencyOffset(),
            m_centerFrequency + m_settings.m_inputFrequencyOffset);

		return true;
    }
    else if (MsgConfigureSigMFFileSinkWork::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
		MsgConfigureSigMFFileSinkWork& conf = (MsgConfigureSigMFFileSinkWork&) cmd;
        qDebug() << "SigMFFileSinkBaseband::handleMessage: MsgConfigureSigMFFileSinkWork: " << conf.isWorking();

        if (conf.isWorking()) {
            m_sink.startRecording();
        } else {
            m_sink.stopRecording();
        }

		return true;
    }
    else
    {
        return false;
    }
}

void SigMFFileSinkBaseband::applySettings(const SigMFFileSinkSettings& settings, bool force)
{
    qDebug() << "SigMFFileSinkBaseband::applySettings:"
        << "m_log2Decim:" << settings.m_log2Decim
        << "m_inputFrequencyOffset:" << settings.m_inputFrequencyOffset
        << "m_fileRecordName: " << settings.m_fileRecordName
        << "m_centerFrequency: " << m_centerFrequency
        << "force: " << force;

    if ((settings.m_log2Decim != m_settings.m_log2Decim)
     || (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force)
    {
        int desiredSampleRate = m_channelizer.getBasebandSampleRate() / (1<<settings.m_log2Decim);
        m_channelizer.setChannelization(desiredSampleRate, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(
            m_channelizer.getChannelSampleRate(),
            desiredSampleRate,
            m_channelizer.getChannelFrequencyOffset(),
            m_centerFrequency + settings.m_inputFrequencyOffset);
    }

    if ((settings.m_spectrumSquelchMode != m_settings.m_spectrumSquelchMode) || force) {
        if (!settings.m_spectrumSquelchMode) {
            m_squelchOpen = false;
        }
    }

    if ((settings.m_spectrumSquelch != m_settings.m_spectrumSquelch) || force) {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_spectrumSquelch);
    }

    m_sink.applySettings(settings, force);
    m_settings = settings;
}

int SigMFFileSinkBaseband::getChannelSampleRate() const
{
    return m_channelizer.getChannelSampleRate();
}

void SigMFFileSinkBaseband::tick()
{
    if (m_spectrumSink && m_settings.m_spectrumSquelchMode)
    {
        m_specMax = m_spectrumSink->getSpecMax();
        bool squelchOpen = m_specMax > m_squelchLevel;

        if (squelchOpen != m_squelchOpen)
        {
            if (m_messageQueueToGUI)
            {
                SigMFFileSinkMessages::MsgReportSquelch *msg = SigMFFileSinkMessages::MsgReportSquelch::create(squelchOpen);
                m_messageQueueToGUI->push(msg);
            }

            if (m_settings.m_squelchRecordingEnable) {
                m_sink.squelchRecording(squelchOpen);
            }
        }

        m_squelchOpen = squelchOpen;
    }
}
