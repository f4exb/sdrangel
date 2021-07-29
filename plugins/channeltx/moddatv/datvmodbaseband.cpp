///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021 Jon Beniston, M7RCE                                        //
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

#include "dsp/upchannelizer.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"

#include "datvmodbaseband.h"
#include "datvmod.h"

DATVModBaseband::DATVModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &DATVModBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

DATVModBaseband::~DATVModBaseband()
{
    delete m_channelizer;
}

void DATVModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void DATVModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
{
    unsigned int part1Begin, part1End, part2Begin, part2End;
    m_sampleFifo.read(nbSamples, part1Begin, part1End, part2Begin, part2End);
    SampleVector& data = m_sampleFifo.getData();

    if (part1Begin != part1End)
    {
        std::copy(
            data.begin() + part1Begin,
            data.begin() + part1End,
            begin
        );
    }

    unsigned int shift = part1End - part1Begin;

    if (part2Begin != part2End)
    {
        std::copy(
            data.begin() + part2Begin,
            data.begin() + part2End,
            begin + shift
        );
    }
}

void DATVModBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);
    SampleVector& data = m_sampleFifo.getData();
    unsigned int ipart1begin;
    unsigned int ipart1end;
    unsigned int ipart2begin;
    unsigned int ipart2end;
    qreal rmsLevel, peakLevel;
    int numSamples;

    unsigned int remainder = m_sampleFifo.remainder();

    while ((remainder > 0) && (m_inputMessageQueue.size() == 0))
    {
        m_sampleFifo.write(remainder, ipart1begin, ipart1end, ipart2begin, ipart2end);

        if (ipart1begin != ipart1end) { // first part of FIFO data
            processFifo(data, ipart1begin, ipart1end);
        }

        if (ipart2begin != ipart2end) { // second part of FIFO data (used when block wraps around)
            processFifo(data, ipart2begin, ipart2end);
        }

        remainder = m_sampleFifo.remainder();
    }

    m_source.getLevels(rmsLevel, peakLevel, numSamples);
    emit levelChanged(rmsLevel, peakLevel, numSamples);
}

void DATVModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void DATVModBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool DATVModBaseband::handleMessage(const Message& cmd)
{
    if (DATVMod::MsgConfigureDATVMod::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DATVMod::MsgConfigureDATVMod& cfg = (DATVMod::MsgConfigureDATVMod&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DATVMod::MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DATVMod::MsgConfigureChannelizer& cfg = (DATVMod::MsgConfigureChannelizer&) cmd;
        qDebug() << "DATVModBaseband::handleMessage: MsgConfigureChannelizer"
            << "(requested) sourceSampleRate: " << cfg.getSourceSampleRate()
            << "(requested) sourceCenterFrequency: " << cfg.getSourceCenterFrequency();
        m_channelizer->setChannelization(cfg.getSourceSampleRate(), cfg.getSourceCenterFrequency());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "DATVModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(4*SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

        return true;
    }
    else if (DATVMod::MsgConfigureTsFileName::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);

        DATVMod::MsgConfigureTsFileName& cfg = (DATVMod::MsgConfigureTsFileName&) cmd;
        m_source.openTsFile(cfg.getFileName());

        return true;
    }
    else if (DATVMod::MsgConfigureTsFileSourceSeek::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);

        DATVMod::MsgConfigureTsFileSourceSeek& cfg = (DATVMod::MsgConfigureTsFileSourceSeek&) cmd;
        m_source.seekTsFileStream(cfg.getPercentage());

        return true;
    }
    else if (DATVMod::MsgConfigureTsFileSourceStreamTiming::match(cmd))
    {
        m_source.reportTsFileSourceStreamTiming();
        return true;
    }
    else if (DATVMod::MsgGetUDPBitrate::match(cmd))
    {
        m_source.reportUDPBitrate();
        return true;
    }
    else if (DATVMod::MsgGetUDPBufferUtilization::match(cmd))
    {
        m_source.reportUDPBufferUtilization();
        return true;
    }
    else
    {
        return false;
    }
}

void DATVModBaseband::applySettings(const DATVModSettings& settings, bool force)
{
    m_source.applySettings(settings, force);
    m_settings = settings;
}

int DATVModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}
