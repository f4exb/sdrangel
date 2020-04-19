///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "atvmodbaseband.h"

MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureATVModBaseband, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureImageFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureVideoFileName, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureVideoFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureVideoFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureCameraIndex, Message)
MESSAGE_CLASS_DEFINITION(ATVModBaseband::MsgConfigureCameraData, Message)

ATVModBaseband::ATVModBaseband() :
    m_mutex(QMutex::Recursive)
{
    m_sampleFifo.resize(SampleSourceFifo::getSizePolicy(48000));
    m_channelizer = new UpChannelizer(&m_source);

    qDebug("AMModBaseband::AMModBaseband");
    QObject::connect(
        &m_sampleFifo,
        &SampleSourceFifo::dataRead,
        this,
        &ATVModBaseband::handleData,
        Qt::QueuedConnection
    );

    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
}

ATVModBaseband::~ATVModBaseband()
{
    delete m_channelizer;
}

void ATVModBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_sampleFifo.reset();
}

void ATVModBaseband::pull(const SampleVector::iterator& begin, unsigned int nbSamples)
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

void ATVModBaseband::handleData()
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

void ATVModBaseband::processFifo(SampleVector& data, unsigned int iBegin, unsigned int iEnd)
{
    m_channelizer->prefetch(iEnd - iBegin);
    m_channelizer->pull(data.begin() + iBegin, iEnd - iBegin);
}

void ATVModBaseband::handleInputMessages()
{
	Message* message;

	while ((message = m_inputMessageQueue.pop()) != nullptr)
	{
		if (handleMessage(*message)) {
			delete message;
		}
	}
}

bool ATVModBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureATVModBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureATVModBaseband& cfg = (MsgConfigureATVModBaseband&) cmd;
        qDebug() << "AMModBaseband::handleMessage: MsgConfigureATVModBaseband";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "ATVModBaseband::handleMessage: MsgConfigureChannelizer"
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
        qDebug() << "ATVModBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        m_sampleFifo.resize(4*SampleSourceFifo::getSizePolicy(notif.getSampleRate()));
        m_channelizer->setBasebandSampleRate(notif.getSampleRate());
        m_source.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());

		return true;
    }
    else if (MsgConfigureImageFileName::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureImageFileName& cfg = (MsgConfigureImageFileName&) cmd;
        qDebug() << "ATVModBaseband::handleMessage: MsgConfigureImageFileName: fileNaem: " << cfg.getFileName();
        m_source.openImage(cfg.getFileName());

		return true;
    }
    else if (MsgConfigureVideoFileName::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureVideoFileName& cfg = (MsgConfigureVideoFileName&) cmd;
        qDebug() << "ATVModBaseband::handleMessage: MsgConfigureVideoFileName: fileName: " << cfg.getFileName();
        m_source.openVideo( cfg.getFileName());

		return true;
    }
    else if (MsgConfigureVideoFileSourceSeek::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureVideoFileSourceSeek& cfg = (MsgConfigureVideoFileSourceSeek&) cmd;
        qDebug() << "ATVModBaseband::handleMessage: MsgConfigureVideoFileName: precnetage: " << cfg.getPercentage();
        m_source.seekVideoFileStream(cfg.getPercentage());

		return true;
    }
    else if (MsgConfigureVideoFileSourceStreamTiming::match(cmd))
    {
        m_source.reportVideoFileSourceStreamTiming();
		return true;
    }
    else if (MsgConfigureCameraIndex::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
    	MsgConfigureCameraIndex& cfg = (MsgConfigureCameraIndex&) cmd;
    	uint32_t index = cfg.getIndex() & 0x7FFFFFF;
        m_source.configureCameraIndex(index);

		return true;
    }
    else if (MsgConfigureCameraData::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureCameraData& cfg = (MsgConfigureCameraData&) cmd;
        m_source.configureCameraData(cfg.getIndex(), cfg.getManualFPS(), cfg.getManualFPSEnable());

		return true;
    }
    else
    {
        return false;
    }
}

void ATVModBaseband::applySettings(const ATVModSettings& settings, bool force)
{
    qDebug() << "ATVModBaseband::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_rfOppBandwidth: " << settings.m_rfOppBandwidth
            << " m_atvStd: " << (int) settings.m_atvStd
            << " m_nbLines: " << settings.m_nbLines
            << " m_fps: " << settings.m_fps
            << " m_atvModInput: " << (int) settings.m_atvModInput
            << " m_uniformLevel: " << settings.m_uniformLevel
            << " m_atvModulation: " << (int) settings.m_atvModulation
            << " m_videoPlayLoop: " << settings.m_videoPlayLoop
            << " m_videoPlay: " << settings.m_videoPlay
            << " m_cameraPlay: " << settings.m_cameraPlay
            << " m_channelMute: " << settings.m_channelMute
            << " m_invertedVideo: " << settings.m_invertedVideo
            << " m_rfScalingFactor: " << settings.m_rfScalingFactor
            << " m_fmExcursion: " << settings.m_fmExcursion
            << " m_forceDecimator: " << settings.m_forceDecimator
            << " m_showOverlayText: " << settings.m_showOverlayText
            << " m_overlayText: " << settings.m_overlayText
            << " force: " << force;

    m_source.applySettings(settings, force);
    m_settings = settings;
}

int ATVModBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void ATVModBaseband::getCameraNumbers(std::vector<int>& numbers)
{
    m_source.getCameraNumbers(numbers);
}