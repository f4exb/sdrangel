///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "droneiddemodbaseband.h"
#include "droneiddemodworker.h"

MESSAGE_CLASS_DEFINITION(DroneIDDemodBaseband::MsgConfigureDroneIDDemodBaseband, Message)

DroneIDDemodBaseband::DroneIDDemodBaseband(DroneIDDemod *droneIDDemod) :
    m_sink(droneIDDemod),
    m_running(false)
{
    qDebug("DroneIDDemodBaseband::DroneIDDemodBaseband");

    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE));
    m_channelizer = new DownChannelizer(&m_sink);

    m_worker = new DroneIDDemodWorker();
    m_workerThread.setObjectName("DroneIDDecode");
    m_worker->moveToThread(&m_workerThread);
    m_sink.setWorker(m_worker);
}

DroneIDDemodBaseband::~DroneIDDemodBaseband()
{
    m_inputMessageQueue.clear();

    if (m_workerThread.isRunning())
    {
        m_workerThread.quit();
        m_workerThread.wait();
    }

    delete m_worker;
    delete m_channelizer;
}

void DroneIDDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
    m_sink.reset();
}

void DroneIDDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DroneIDDemodBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_workerThread.start();
    m_running = true;
}

void DroneIDDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &DroneIDDemodBaseband::handleData
    );
    m_workerThread.quit();
    m_workerThread.wait();
    m_running = false;
}

void DroneIDDemodBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void DroneIDDemodBaseband::setMessageQueueToChannel(MessageQueue *messageQueue)
{
    m_worker->setMessageQueueToChannel(messageQueue);
}

void DroneIDDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void DroneIDDemodBaseband::handleData()
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
            m_channelizer->feed(part1begin, part1end);
        }

        // second part of FIFO data (used when block wraps around)
        if(part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void DroneIDDemodBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (!handleMessage(*message)) {
            qDebug("%s: unhandled message: %s", Q_FUNC_INFO, message->getIdentifier());
        }
        delete message;
    }
}

bool DroneIDDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureDroneIDDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureDroneIDDemodBaseband& cfg = (MsgConfigureDroneIDDemodBaseband&) cmd;
        qDebug() << "DroneIDDemodBaseband::handleMessage: MsgConfigureDroneIDDemodBaseband";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DroneIDDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        m_sink.setCenterFrequency(notif.getCenterFrequency());

        return true;
    }
    else
    {
        return false;
    }
}

void DroneIDDemodBaseband::applySettings(const QStringList& settingsKeys, const DroneIDDemodSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) || force)
    {
        m_channelizer->setChannelization(DroneIDDemodSettings::DRONEIDDEMOD_CHANNEL_SAMPLE_RATE, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    if (settingsKeys.contains("turboIterations") || force) {
        m_worker->setMaxIterations(settings.m_turboIterations);
    }

    m_sink.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int DroneIDDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void DroneIDDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}
