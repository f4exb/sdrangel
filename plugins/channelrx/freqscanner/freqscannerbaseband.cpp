///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019-2021 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
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

#include "freqscannerbaseband.h"
#include "freqscanner.h"

MESSAGE_CLASS_DEFINITION(FreqScannerBaseband::MsgConfigureFreqScannerBaseband, Message)

FreqScannerBaseband::FreqScannerBaseband(FreqScanner *freqScanner) :
    m_freqScanner(freqScanner),
    m_messageQueueToGUI(nullptr)
{
    qDebug("FreqScannerBaseband::FreqScannerBaseband");

    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));

    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &FreqScannerBaseband::handleData,
        Qt::QueuedConnection
    );

    m_channelizer = new DownChannelizer(&m_sink);
    m_channelSampleRate = 0;
    m_scannerSampleRate = 0;

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

FreqScannerBaseband::~FreqScannerBaseband()
{
    m_inputMessageQueue.clear();
    delete m_channelizer;
}

void FreqScannerBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
    m_channelSampleRate = 0;
}

void FreqScannerBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void FreqScannerBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void FreqScannerBaseband::handleData()
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
        if (part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void FreqScannerBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool FreqScannerBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureFreqScannerBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureFreqScannerBaseband& cfg = (MsgConfigureFreqScannerBaseband&) cmd;
        qDebug() << "FreqScannerBaseband::handleMessage: MsgConfigureFreqScannerBaseband";

        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "FreqScannerBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        setBasebandSampleRate(notif.getSampleRate());
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(notif.getSampleRate()));
        if (m_channelSampleRate != m_channelizer->getChannelSampleRate()) {
            m_channelSampleRate = m_channelizer->getChannelSampleRate();
        }
        m_sink.setCenterFrequency(notif.getCenterFrequency());

        return true;
    }
    else
    {
        return false;
    }
}

void FreqScannerBaseband::applySettings(const FreqScannerSettings& settings, const QStringList& settingsKeys, bool force)
{
    if (settingsKeys.contains("channelBandwidth") || settingsKeys.contains("inputFrequencyOffset") || force)
    {
        int basebandSampleRate = m_channelizer->getBasebandSampleRate();
        if ((basebandSampleRate != 0) && (settings.m_channelBandwidth != 0)) {
            calcScannerSampleRate(basebandSampleRate, settings.m_channelBandwidth, settings.m_inputFrequencyOffset);
        }
    }

    m_sink.applySettings(settings, settingsKeys, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int FreqScannerBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void FreqScannerBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    if ((sampleRate != 0) && (m_settings.m_channelBandwidth != 0)) {
        calcScannerSampleRate(sampleRate, m_settings.m_channelBandwidth, m_settings.m_inputFrequencyOffset);
    }
}

void FreqScannerBaseband::calcScannerSampleRate(int basebandSampleRate, float rfBandwidth, int inputFrequencyOffset)
{
    int fftSize;
    int binsPerChannel;

    m_freqScanner->calcScannerSampleRate(rfBandwidth, basebandSampleRate, m_scannerSampleRate, fftSize, binsPerChannel);

    m_channelizer->setChannelization(m_scannerSampleRate, inputFrequencyOffset);
    m_channelSampleRate = m_channelizer->getChannelSampleRate();
    m_sink.applyChannelSettings(m_channelSampleRate, m_channelizer->getChannelFrequencyOffset(), m_scannerSampleRate, fftSize, binsPerChannel);
    qDebug() << "FreqScannerBaseband::calcScannerSampleRate"
        << "basebandSampleRate:" << basebandSampleRate
        << "channelSampleRate:" << m_channelSampleRate
        << "scannerSampleRate:" << m_scannerSampleRate
        << "rfBandwidth:" << rfBandwidth
        << "fftSize:" << fftSize
        << "binsPerChannel:" << binsPerChannel;

    if (getMessageQueueToGUI())
    {
        FreqScanner::MsgReportScanRange* msg = FreqScanner::MsgReportScanRange::create(inputFrequencyOffset, m_scannerSampleRate, fftSize);
        getMessageQueueToGUI()->push(msg);
    }
}
