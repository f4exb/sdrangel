///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// (c) 2014 Modified by John Greb
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


#include <stdio.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGSSBDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGSSBDemodReport.h"

#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"

#include "ssbdemod.h"

MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemod, Message)
MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureSSBDemodPrivate, Message)
MESSAGE_CLASS_DEFINITION(SSBDemod::MsgConfigureChannelizer, Message)

const QString SSBDemod::m_channelIdURI = "sdrangel.channel.ssbdemod";
const QString SSBDemod::m_channelId = "SSBDemod";

SSBDemod::SSBDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_audioBinaual(false),
        m_audioFlipChannels(false),
        m_dsb(false),
        m_audioMute(false),
        m_agc(12000, agcTarget, 1e-2),
        m_agcActive(false),
        m_agcClamping(false),
        m_agcNbSamples(12000),
        m_agcPowerThreshold(1e-2),
        m_agcThresholdGate(0),
        m_squelchDelayLine(2*48000),
        m_audioActive(false),
        m_sampleSink(0),
        m_audioFifo(24000),
        m_settingsMutex(QMutex::Recursive)
{
	setObjectName(m_channelId);

	m_Bandwidth = 5000;
	m_LowCutoff = 300;
	m_volume = 2.0;
	m_spanLog2 = 3;
	m_inputSampleRate = 48000;
	m_inputFrequencyOffset = 0;

    DSPEngine::instance()->getAudioDeviceManager()->addAudioSink(&m_audioFifo, getInputMessageQueue());
    m_audioSampleRate = DSPEngine::instance()->getAudioDeviceManager()->getOutputSampleRate();

	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;
	m_undersampleCount = 0;
	m_sum = 0;

	m_usb = true;
	m_magsq = 0.0f;
	m_magsqSum = 0.0f;
	m_magsqPeak = 0.0f;
	m_magsqCount = 0;

	m_agc.setClampMax(SDR_RX_SCALED/100.0);
	m_agc.setClamping(m_agcClamping);

	SSBFilter = new fftfilt(m_LowCutoff / m_audioSampleRate, m_Bandwidth / m_audioSampleRate, ssbFftLen);
	DSBFilter = new fftfilt((2.0f * m_Bandwidth) / m_audioSampleRate, 2 * ssbFftLen);

    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addChannelSink(m_threadedChannelizer);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

SSBDemod::~SSBDemod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	DSPEngine::instance()->getAudioDeviceManager()->removeAudioSink(&m_audioFifo);

	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete SSBFilter;
    delete DSBFilter;
}

void SSBDemod::configure(MessageQueue* messageQueue,
		Real Bandwidth,
		Real LowCutoff,
		Real volume,
		int spanLog2,
		bool audioBinaural,
		bool audioFlipChannel,
		bool dsb,
		bool audioMute,
		bool agc,
		bool agcClamping,
        int agcTimeLog2,
        int agcPowerThreshold,
        int agcThresholdGate)
{
	Message* cmd = MsgConfigureSSBDemodPrivate::create(
	        Bandwidth,
	        LowCutoff,
	        volume,
	        spanLog2,
	        audioBinaural,
	        audioFlipChannel,
	        dsb,
	        audioMute,
	        agc,
	        agcClamping,
	        agcTimeLog2,
	        agcPowerThreshold,
	        agcThresholdGate);
	messageQueue->push(cmd);
}

void SSBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly)
{
    (void) positiveOnly;
    Complex ci;
	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }

	m_settingsMutex.unlock();
}

void SSBDemod::processOneSample(Complex &ci)
{
	fftfilt::cmplx *sideband;
	int n_out = 0;
	int decim = 1<<(m_spanLog2 - 1);
	unsigned char decim_mask = decim - 1; // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)

    if (m_dsb) {
        n_out = DSBFilter->runDSB(ci, &sideband);
    } else {
        n_out = SSBFilter->runSSB(ci, &sideband, m_usb);
    }

    for (int i = 0; i < n_out; i++)
    {
        // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
        // smart decimation with bit gain using float arithmetic (23 bits significand)

        m_sum += sideband[i];

        if (!(m_undersampleCount++ & decim_mask))
        {
            Real avgr = m_sum.real() / decim;
            Real avgi = m_sum.imag() / decim;
            m_magsq = (avgr * avgr + avgi * avgi) / (SDR_RX_SCALED*SDR_RX_SCALED);

            m_magsqSum += m_magsq;

            if (m_magsq > m_magsqPeak)
            {
                m_magsqPeak = m_magsq;
            }

            m_magsqCount++;

            if (!m_dsb & !m_usb)
            { // invert spectrum for LSB
                m_sampleBuffer.push_back(Sample(avgi, avgr));
            }
            else
            {
                m_sampleBuffer.push_back(Sample(avgr, avgi));
            }

            m_sum.real(0.0);
            m_sum.imag(0.0);
        }

        float agcVal = m_agcActive ? m_agc.feedAndGetValue(sideband[i]) : 0.1;
        fftfilt::cmplx& delayedSample = m_squelchDelayLine.readBack(m_agc.getStepDownDelay());
        m_audioActive = delayedSample.real() != 0.0;
        m_squelchDelayLine.write(sideband[i]*agcVal);

        if (m_audioMute)
        {
            m_audioBuffer[m_audioBufferFill].r = 0;
            m_audioBuffer[m_audioBufferFill].l = 0;
        }
        else
        {
            fftfilt::cmplx z = m_agcActive ? delayedSample * m_agc.getStepValue() : delayedSample;

            if (m_audioBinaual)
            {
                if (m_audioFlipChannels)
                {
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(z.imag() * m_volume);
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(z.real() * m_volume);
                }
                else
                {
                    m_audioBuffer[m_audioBufferFill].r = (qint16)(z.real() * m_volume);
                    m_audioBuffer[m_audioBufferFill].l = (qint16)(z.imag() * m_volume);
                }
            }
            else
            {
                Real demod = (z.real() + z.imag()) * 0.7;
                qint16 sample = (qint16)(demod * m_volume);
                m_audioBuffer[m_audioBufferFill].l = sample;
                m_audioBuffer[m_audioBufferFill].r = sample;
            }
        }

        ++m_audioBufferFill;

        if (m_audioBufferFill >= m_audioBuffer.size())
        {
            uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

            if (res != m_audioBufferFill)
            {
                qDebug("SSBDemod::feed: %u/%u samples written", res, m_audioBufferFill);
            }

            m_audioBufferFill = 0;
        }
    }

	uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

	if (res != m_audioBufferFill)
	{
        qDebug("SSBDemod::feed: %u/%u tail samples written", res, m_audioBufferFill);
	}

	m_audioBufferFill = 0;

	if (m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), !m_dsb);
	}

	m_sampleBuffer.clear();

}

void SSBDemod::start()
{
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void SSBDemod::stop()
{
}

bool SSBDemod::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
		qDebug("SSBDemod::handleMessage: MsgChannelizerNotification: m_sampleRate");

		applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "SSBDemod::handleMessage: MsgConfigureChannelizer: sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
            cfg.getSampleRate(),
            cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureSSBDemod::match(cmd))
    {
        MsgConfigureSSBDemod& cfg = (MsgConfigureSSBDemod&) cmd;
        qDebug("SSBDemod::handleMessage: MsgConfigureSSBDemod");

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (BasebandSampleSink::MsgThreadedSink::match(cmd))
    {
        BasebandSampleSink::MsgThreadedSink& cfg = (BasebandSampleSink::MsgThreadedSink&) cmd;
        const QThread *thread = cfg.getThread();
        qDebug("SSBDemod::handleMessage: BasebandSampleSink::MsgThreadedSink: %p", thread);
        return true;
    }
    else if (DSPConfigureAudio::match(cmd))
    {
        DSPConfigureAudio& cfg = (DSPConfigureAudio&) cmd;
        uint32_t sampleRate = cfg.getSampleRate();

        qDebug() << "SSBDemod::handleMessage: DSPConfigureAudio:"
                << " sampleRate: " << sampleRate;

        if (sampleRate != m_audioSampleRate) {
            applyAudioSampleRate(sampleRate);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        return true;
    }
	else
	{
		if(m_sampleSink != 0)
		{
		   return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void SSBDemod::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "SSBDemod::applyChannelSettings:"
            << " inputSampleRate: " << inputSampleRate
            << " inputFrequencyOffset: " << inputFrequencyOffset;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_inputSampleRate != inputSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, inputSampleRate);
    }

    if ((m_inputSampleRate != inputSampleRate) || force)
    {
        m_settingsMutex.lock();
        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > inputSampleRate ? inputSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, inputSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_audioSampleRate;
        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void SSBDemod::applyAudioSampleRate(int sampleRate)
{
    qDebug("SSBDemod::applyAudioSampleRate: %d", sampleRate);

    MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
            sampleRate, m_settings.m_inputFrequencyOffset);
    m_inputMessageQueue.push(channelConfigMsg);

    m_settingsMutex.lock();

    Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_inputSampleRate ? m_inputSampleRate : (m_Bandwidth * 1.5f);
    m_interpolator.create(16, m_inputSampleRate, interpolatorBandwidth, 2.0f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_inputSampleRate / (Real) sampleRate;

    SSBFilter->create_filter(m_LowCutoff / (float) sampleRate, m_Bandwidth / (float) sampleRate);
    DSBFilter->create_dsb_filter((2.0f * m_Bandwidth) / (float) sampleRate);

    int agcNbSamples = (sampleRate / 1000) * (1<<m_settings.m_agcTimeLog2);
    int agcThresholdGate = (sampleRate / 1000) * m_settings.m_agcThresholdGate; // ms

    if (m_agcNbSamples != agcNbSamples)
    {
        m_agc.resize(agcNbSamples, agcNbSamples/2, agcTarget);
        m_agc.setStepDownDelay(agcNbSamples);
        m_agcNbSamples = agcNbSamples;
    }

    if (m_agcThresholdGate != agcThresholdGate)
    {
        m_agc.setGate(agcThresholdGate);
        m_agcThresholdGate = agcThresholdGate;
    }

    m_audioFifo.setSize(sampleRate);

    m_settingsMutex.unlock();

    m_audioSampleRate = sampleRate;

    if (m_guiMessageQueue) // forward to GUI if any
    {
        DSPConfigureAudio *cfg = new DSPConfigureAudio(m_audioSampleRate);
        m_guiMessageQueue->push(cfg);
    }
}

void SSBDemod::applySettings(const SSBDemodSettings& settings, bool force)
{
    qDebug() << "SSBDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_volume: " << settings.m_volume
            << " m_spanLog2: " << settings.m_spanLog2
            << " m_audioBinaual: " << settings.m_audioBinaural
            << " m_audioFlipChannels: " << settings.m_audioFlipChannels
            << " m_dsb: " << settings.m_dsb
            << " m_audioMute: " << settings.m_audioMute
            << " m_agcActive: " << settings.m_agc
            << " m_agcClamping: " << settings.m_agcClamping
            << " m_agcTimeLog2: " << settings.m_agcTimeLog2
            << " agcPowerThreshold: " << settings.m_agcPowerThreshold
            << " agcThresholdGate: " << settings.m_agcThresholdGate
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if((m_settings.m_inputFrequencyOffset != settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if((m_settings.m_lowCutoff != settings.m_lowCutoff) || force) {
        reverseAPIKeys.append("lowCutoff");
    }

    if((m_settings.m_rfBandwidth != settings.m_rfBandwidth) ||
        (m_settings.m_lowCutoff != settings.m_lowCutoff) || force)
    {
        float band, lowCutoff;

        band = settings.m_rfBandwidth;
        lowCutoff = settings.m_lowCutoff;

        if (band < 0) {
            band = -band;
            lowCutoff = -lowCutoff;
            m_usb = false;
        } else {
            m_usb = true;
        }

        if (band < 100.0f)
        {
            band = 100.0f;
            lowCutoff = 0;
        }

        m_Bandwidth = band;
        m_LowCutoff = lowCutoff;

        m_settingsMutex.lock();
        Real interpolatorBandwidth = (m_Bandwidth * 1.5f) > m_inputSampleRate ? m_inputSampleRate : (m_Bandwidth * 1.5f);
        m_interpolator.create(16, m_inputSampleRate, interpolatorBandwidth, 2.0f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) m_inputSampleRate / (Real) m_audioSampleRate;
        SSBFilter->create_filter(m_LowCutoff / (float) m_audioSampleRate, m_Bandwidth / (float) m_audioSampleRate);
        DSBFilter->create_dsb_filter((2.0f * m_Bandwidth) / (float) m_audioSampleRate);
        m_settingsMutex.unlock();
    }

    if ((m_settings.m_volume != settings.m_volume) || force)
    {
        reverseAPIKeys.append("volume");
        m_volume = settings.m_volume;
        m_volume /= 4.0; // for 3276.8
    }

    if ((m_settings.m_agcTimeLog2 != settings.m_agcTimeLog2) || force) {
        reverseAPIKeys.append("agcTimeLog2");
    }
    if ((m_settings.m_agcPowerThreshold != settings.m_agcPowerThreshold) || force) {
        reverseAPIKeys.append("agcPowerThreshold");
    }
    if ((m_settings.m_agcThresholdGate != settings.m_agcThresholdGate) || force) {
        reverseAPIKeys.append("agcThresholdGate");
    }
    if ((m_settings.m_agcClamping != settings.m_agcClamping) || force) {
        reverseAPIKeys.append("agcClamping");
    }

    if ((m_settings.m_agcTimeLog2 != settings.m_agcTimeLog2) ||
        (m_settings.m_agcPowerThreshold != settings.m_agcPowerThreshold) ||
        (m_settings.m_agcThresholdGate != settings.m_agcThresholdGate) ||
        (m_settings.m_agcClamping != settings.m_agcClamping) || force)
    {
        int agcNbSamples = (m_audioSampleRate / 1000) * (1<<settings.m_agcTimeLog2);
        m_agc.setThresholdEnable(settings.m_agcPowerThreshold != -SSBDemodSettings::m_minPowerThresholdDB);
        double agcPowerThreshold = CalcDb::powerFromdB(settings.m_agcPowerThreshold) * (SDR_RX_SCALED*SDR_RX_SCALED);
        int agcThresholdGate = (m_audioSampleRate / 1000) * settings.m_agcThresholdGate; // ms
        bool agcClamping = settings.m_agcClamping;

        if (m_agcNbSamples != agcNbSamples)
        {
            m_settingsMutex.lock();
            m_agc.resize(agcNbSamples, agcNbSamples/2, agcTarget);
            m_agc.setStepDownDelay(agcNbSamples);
            m_agcNbSamples = agcNbSamples;
            m_settingsMutex.unlock();
        }

        if (m_agcPowerThreshold != agcPowerThreshold)
        {
            m_agc.setThreshold(agcPowerThreshold);
            m_agcPowerThreshold = agcPowerThreshold;
        }

        if (m_agcThresholdGate != agcThresholdGate)
        {
            m_agc.setGate(agcThresholdGate);
            m_agcThresholdGate = agcThresholdGate;
        }

        if (m_agcClamping != agcClamping)
        {
            m_agc.setClamping(agcClamping);
            m_agcClamping = agcClamping;
        }

        qDebug() << "SBDemod::applySettings: AGC:"
            << " agcNbSamples: " << agcNbSamples
            << " agcPowerThreshold: " << agcPowerThreshold
            << " agcThresholdGate: " << agcThresholdGate
            << " agcClamping: " << agcClamping;
    }

    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force)
    {
        reverseAPIKeys.append("audioDeviceName");
        AudioDeviceManager *audioDeviceManager = DSPEngine::instance()->getAudioDeviceManager();
        int audioDeviceIndex = audioDeviceManager->getOutputDeviceIndex(settings.m_audioDeviceName);
        audioDeviceManager->addAudioSink(&m_audioFifo, getInputMessageQueue(), audioDeviceIndex);
        uint32_t audioSampleRate = audioDeviceManager->getOutputSampleRate(audioDeviceIndex);

        if (m_audioSampleRate != audioSampleRate) {
            applyAudioSampleRate(audioSampleRate);
        }
    }

    if ((m_settings.m_spanLog2 != settings.m_spanLog2) || force) {
        reverseAPIKeys.append("spanLog2");
    }
    if ((m_settings.m_audioBinaural != settings.m_audioBinaural) || force) {
        reverseAPIKeys.append("audioBinaural");
    }
    if ((m_settings.m_audioFlipChannels != settings.m_audioFlipChannels) || force) {
        reverseAPIKeys.append("audioFlipChannels");
    }
    if ((m_settings.m_dsb != settings.m_dsb) || force) {
        reverseAPIKeys.append("dsb");
    }
    if ((m_settings.m_audioMute != settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((m_settings.m_agc != settings.m_agc) || force) {
        reverseAPIKeys.append("agc");
    }

    m_spanLog2 = settings.m_spanLog2;
    m_audioBinaual = settings.m_audioBinaural;
    m_audioFlipChannels = settings.m_audioFlipChannels;
    m_dsb = settings.m_dsb;
    m_audioMute = settings.m_audioMute;
    m_agcActive = settings.m_agc;

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

QByteArray SSBDemod::serialize() const
{
    return m_settings.serialize();
}

bool SSBDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

int SSBDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
    response.getSsbDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int SSBDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    SSBDemodSettings settings = m_settings;
    bool frequencyOffsetChanged = false;

    if (channelSettingsKeys.contains("inputFrequencyOffset"))
    {
        settings.m_inputFrequencyOffset = response.getSsbDemodSettings()->getInputFrequencyOffset();
        frequencyOffsetChanged = true;
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getSsbDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("lowCutoff")) {
        settings.m_lowCutoff = response.getSsbDemodSettings()->getLowCutoff();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getSsbDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("spanLog2")) {
        settings.m_spanLog2 = response.getSsbDemodSettings()->getSpanLog2();
    }
    if (channelSettingsKeys.contains("audioBinaural")) {
        settings.m_audioBinaural = response.getSsbDemodSettings()->getAudioBinaural() != 0;
    }
    if (channelSettingsKeys.contains("audioFlipChannels")) {
        settings.m_audioFlipChannels = response.getSsbDemodSettings()->getAudioFlipChannels() != 0;
    }
    if (channelSettingsKeys.contains("dsb")) {
        settings.m_dsb = response.getSsbDemodSettings()->getDsb() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getSsbDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("agc")) {
        settings.m_agc = response.getSsbDemodSettings()->getAgc() != 0;
    }
    if (channelSettingsKeys.contains("agcClamping")) {
        settings.m_agcClamping = response.getSsbDemodSettings()->getAgcClamping() != 0;
    }
    if (channelSettingsKeys.contains("agcTimeLog2")) {
        settings.m_agcTimeLog2 = response.getSsbDemodSettings()->getAgcTimeLog2();
    }
    if (channelSettingsKeys.contains("agcPowerThreshold")) {
        settings.m_agcPowerThreshold = response.getSsbDemodSettings()->getAgcPowerThreshold();
    }
    if (channelSettingsKeys.contains("agcThresholdGate")) {
        settings.m_agcThresholdGate = response.getSsbDemodSettings()->getAgcThresholdGate();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getSsbDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getSsbDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getSsbDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getSsbDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getSsbDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getSsbDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getSsbDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getSsbDemodSettings()->getReverseApiChannelIndex();
    }

    if (frequencyOffsetChanged)
    {
        MsgConfigureChannelizer* channelConfigMsg = MsgConfigureChannelizer::create(
                m_audioSampleRate, settings.m_inputFrequencyOffset);
        m_inputMessageQueue.push(channelConfigMsg);
    }

    MsgConfigureSSBDemod *msg = MsgConfigureSSBDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("SSBDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSSBDemod *msgToGUI = MsgConfigureSSBDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int SSBDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setSsbDemodReport(new SWGSDRangel::SWGSSBDemodReport());
    response.getSsbDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void SSBDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SSBDemodSettings& settings)
{
    response.getSsbDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getSsbDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getSsbDemodSettings()->setLowCutoff(settings.m_lowCutoff);
    response.getSsbDemodSettings()->setVolume(settings.m_volume);
    response.getSsbDemodSettings()->setSpanLog2(settings.m_spanLog2);
    response.getSsbDemodSettings()->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    response.getSsbDemodSettings()->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    response.getSsbDemodSettings()->setDsb(settings.m_dsb ? 1 : 0);
    response.getSsbDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getSsbDemodSettings()->setAgc(settings.m_agc ? 1 : 0);
    response.getSsbDemodSettings()->setAgcClamping(settings.m_agcClamping ? 1 : 0);
    response.getSsbDemodSettings()->setAgcTimeLog2(settings.m_agcTimeLog2);
    response.getSsbDemodSettings()->setAgcPowerThreshold(settings.m_agcPowerThreshold);
    response.getSsbDemodSettings()->setAgcThresholdGate(settings.m_agcThresholdGate);
    response.getSsbDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getSsbDemodSettings()->getTitle()) {
        *response.getSsbDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getSsbDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getSsbDemodSettings()->getAudioDeviceName()) {
        *response.getSsbDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getSsbDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getSsbDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getSsbDemodSettings()->getReverseApiAddress()) {
        *response.getSsbDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getSsbDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getSsbDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getSsbDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getSsbDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void SSBDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getSsbDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    response.getSsbDemodReport()->setSquelch(m_audioActive ? 1 : 0);
    response.getSsbDemodReport()->setAudioSampleRate(m_audioSampleRate);
    response.getSsbDemodReport()->setChannelSampleRate(m_inputSampleRate);
}

void SSBDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const SSBDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("SSBDemod"));
    swgChannelSettings->setSsbDemodSettings(new SWGSDRangel::SWGSSBDemodSettings());
    SWGSDRangel::SWGSSBDemodSettings *swgSSBDemodSettings = swgChannelSettings->getSsbDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgSSBDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgSSBDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("lowCutoff") || force) {
        swgSSBDemodSettings->setLowCutoff(settings.m_lowCutoff);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgSSBDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("spanLog2") || force) {
        swgSSBDemodSettings->setSpanLog2(settings.m_spanLog2);
    }
    if (channelSettingsKeys.contains("audioBinaural") || force) {
        swgSSBDemodSettings->setAudioBinaural(settings.m_audioBinaural ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioFlipChannels") || force) {
        swgSSBDemodSettings->setAudioFlipChannels(settings.m_audioFlipChannels ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dsb") || force) {
        swgSSBDemodSettings->setDsb(settings.m_dsb ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgSSBDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agc") || force) {
        swgSSBDemodSettings->setAgc(settings.m_agc ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agcClamping") || force) {
        swgSSBDemodSettings->setAgcClamping(settings.m_agcClamping ? 1 : 0);
    }
    if (channelSettingsKeys.contains("agcTimeLog2") || force) {
        swgSSBDemodSettings->setAgcTimeLog2(settings.m_agcTimeLog2);
    }
    if (channelSettingsKeys.contains("agcPowerThreshold") || force) {
        swgSSBDemodSettings->setAgcPowerThreshold(settings.m_agcPowerThreshold);
    }
    if (channelSettingsKeys.contains("agcThresholdGate") || force) {
        swgSSBDemodSettings->setAgcThresholdGate(settings.m_agcThresholdGate);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSSBDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgSSBDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgSSBDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void SSBDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "SSBDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("SSBDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
