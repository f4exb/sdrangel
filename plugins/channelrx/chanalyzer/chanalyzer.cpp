///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include <QTime>
#include <QDebug>
#include <stdio.h>

#include "device/devicesourceapi.h"
#include "audio/audiooutput.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"
#include "chanalyzer.h"

MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelAnalyzer, Message)
MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelAnalyzerOld, Message)
MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(ChannelAnalyzer::MsgReportChannelSampleRateChanged, Message)

const QString ChannelAnalyzer::m_channelIdURI = "sdrangel.channel.chanalyzer";
const QString ChannelAnalyzer::m_channelId = "ChannelAnalyzer";

ChannelAnalyzer::ChannelAnalyzer(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_sampleSink(0),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

	m_undersampleCount = 0;
	m_sum = 0;
	m_usb = true;
	m_magsq = 0;
	m_useInterpolator = false;
	m_interpolatorDistance = 1.0f;
	m_interpolatorDistanceRemain = 0.0f;
	m_inputSampleRate = 48000;
	m_inputFrequencyOffset = 0;
	SSBFilter = new fftfilt(m_settings.m_lowCutoff / m_inputSampleRate, m_settings.m_bandwidth / m_inputSampleRate, ssbFftLen);
	DSBFilter = new fftfilt(m_settings.m_bandwidth / m_inputSampleRate, 2*ssbFftLen);
	RRCFilter = new fftfilt(m_settings.m_bandwidth / m_inputSampleRate, 2*ssbFftLen);
	m_corr = new fftcorr(8*ssbFftLen); // 8k for 4k effective samples
	m_pll.computeCoefficients(0.002f, 0.5f, 10.0f); // bandwidth, damping factor, loop gain

	applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
	applySettings(m_settings, true);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);
}

ChannelAnalyzer::~ChannelAnalyzer()
{
	m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
    delete SSBFilter;
    delete DSBFilter;
    delete RRCFilter;
}

void ChannelAnalyzer::configure(MessageQueue* messageQueue,
		int channelSampleRate,
		Real Bandwidth,
		Real LowCutoff,
		int  spanLog2,
		bool ssb,
		bool pll,
		bool fll,
		unsigned int pllPskOrder)
{
    Message* cmd = MsgConfigureChannelAnalyzerOld::create(channelSampleRate, Bandwidth, LowCutoff, spanLog2, ssb, pll, fll, pllPskOrder);
	messageQueue->push(cmd);
}

void ChannelAnalyzer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly __attribute__((unused)))
{
	fftfilt::cmplx *sideband = 0;
	Complex ci;

	m_settingsMutex.lock();

	for(SampleVector::const_iterator it = begin; it < end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_useInterpolator)
		{
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci, sideband);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
		}
		else
		{
	        processOneSample(c, sideband);
		}
	}

	if(m_sampleSink != 0)
	{
		m_sampleSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), m_settings.m_ssb); // m_ssb = positive only
	}

	m_sampleBuffer.clear();

	m_settingsMutex.unlock();
}

void ChannelAnalyzer::processOneSample(Complex& c, fftfilt::cmplx *sideband)
{
    int n_out;
    int decim = 1<<m_settings.m_spanLog2;

    if (m_settings.m_ssb)
    {
        n_out = SSBFilter->runSSB(c, &sideband, m_usb);
    }
    else
    {
        if (m_settings.m_rrc) {
            n_out = RRCFilter->runFilt(c, &sideband);
        } else {
            n_out = DSBFilter->runDSB(c, &sideband);
        }
    }

    for (int i = 0; i < n_out; i++)
    {
        // Downsample by 2^(m_scaleLog2 - 1) for SSB band spectrum display
        // smart decimation with bit gain using float arithmetic (23 bits significand)

        m_sum += sideband[i];

        if (!(m_undersampleCount++ & (decim - 1))) // counter LSB bit mask for decimation by 2^(m_scaleLog2 - 1)
        {
            m_sum /= decim;
            Real re = m_sum.real() / SDR_RX_SCALEF;
            Real im = m_sum.imag() / SDR_RX_SCALEF;
            m_magsq = re*re + im*im;
            m_channelPowerAvg(m_magsq);
            std::complex<float> mix;

            if (m_settings.m_pll)
            {
                if (m_settings.m_fll)
                {
                    m_fll.feed(re, im);
                    // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                    mix = m_sum * std::conj(m_fll.getComplex());
                }
                else
                {
                    m_pll.feed(re, im);
                    // Use -fPLL to mix (exchange PLL real and image in the complex multiplication)
                    mix = m_sum * std::conj(m_pll.getComplex());
                }
            }

            feedOneSample(m_settings.m_pll ? mix : m_sum, m_settings.m_fll ? m_fll.getComplex() : m_pll.getComplex());
            m_sum = 0;
        }
    }
}

void ChannelAnalyzer::start()
{
    applyChannelSettings(m_inputSampleRate, m_inputFrequencyOffset, true);
}

void ChannelAnalyzer::stop()
{
}

bool ChannelAnalyzer::handleMessage(const Message& cmd)
{
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;
	    qDebug() << "ChannelAnalyzer::handleMessage: DownChannelizer::MsgChannelizerNotification:"
	            << " sampleRate: " << notif.getSampleRate()
	            << " frequencyOffset: " << notif.getFrequencyOffset();

        applyChannelSettings(notif.getSampleRate(), notif.getFrequencyOffset());

		if (getMessageQueueToGUI())
		{
            MsgReportChannelSampleRateChanged *msg = MsgReportChannelSampleRateChanged::create();
            getMessageQueueToGUI()->push(msg);
		}

	    return true;
	}
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        qDebug() << "ChannelAnalyzer::handleMessage: MsgConfigureChannelizer:"
                << " sampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_channelizer->configure(m_channelizer->getInputMessageQueue(),
                cfg.getSampleRate(),
                cfg.getCenterFrequency());

        return true;
    }
    else if (MsgConfigureChannelAnalyzer::match(cmd))
    {
        qDebug("ChannelAnalyzer::handleMessage: MsgConfigureChannelAnalyzer");
        MsgConfigureChannelAnalyzer& cfg = (MsgConfigureChannelAnalyzer&) cmd;

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
	else
	{
		if (m_sampleSink != 0)
		{
		   return m_sampleSink->handleMessage(cmd);
		}
		else
		{
			return false;
		}
	}
}

void ChannelAnalyzer::applyChannelSettings(int inputSampleRate, int inputFrequencyOffset, bool force)
{
    qDebug() << "ChannelAnalyzer::applyChannelSettings:"
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

        m_interpolator.create(16, inputSampleRate, inputSampleRate / 2.2f);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) inputSampleRate / (Real) m_settings.m_downSampleRate;

        if (!m_settings.m_downSample)
        {
            setFilters(inputSampleRate, m_settings.m_bandwidth, m_settings.m_lowCutoff);
            m_pll.setSampleRate(inputSampleRate / (1<<m_settings.m_spanLog2));
            m_fll.setSampleRate(inputSampleRate / (1<<m_settings.m_spanLog2));
        }

        m_settingsMutex.unlock();
    }

    m_inputSampleRate = inputSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;
}

void ChannelAnalyzer::setFilters(int sampleRate, float bandwidth, float lowCutoff)
{
    qDebug("ChannelAnalyzer::setFilters: sampleRate: %d bandwidth: %f lowCutoff: %f",
            sampleRate, bandwidth, lowCutoff);

    if (bandwidth < 0)
    {
        bandwidth = -bandwidth;
        lowCutoff = -lowCutoff;
        m_usb = false;
    }
    else
    {
        m_usb = true;
    }

    if (bandwidth < 100.0f)
    {
        bandwidth = 100.0f;
        lowCutoff = 0;
    }

    SSBFilter->create_filter(lowCutoff / sampleRate, bandwidth / sampleRate);
    DSBFilter->create_dsb_filter(bandwidth / sampleRate);
    RRCFilter->create_rrc_filter(bandwidth / sampleRate, m_settings.m_rrcRolloff / 100.0);
}

void ChannelAnalyzer::applySettings(const ChannelAnalyzerSettings& settings, bool force)
{
    qDebug() << "ChannelAnalyzer::applySettings:"
            << " m_downSample: " << settings.m_downSample
            << " m_downSampleRate: " << settings.m_downSampleRate
            << " m_rcc: " << settings.m_rrc
            << " m_rrcRolloff: " << settings.m_rrcRolloff / 100.0
            << " m_bandwidth: " << settings.m_bandwidth
            << " m_lowCutoff: " << settings.m_lowCutoff
            << " m_spanLog2: " << settings.m_spanLog2
            << " m_ssb: " << settings.m_ssb
            << " m_pll: " << settings.m_pll
            << " m_fll: " << settings.m_fll
            << " m_pllPskOrder: " << settings.m_pllPskOrder
            << " m_inputType: " << (int) settings.m_inputType;

    if ((settings.m_downSampleRate != m_settings.m_downSampleRate) || force)
    {
        m_settingsMutex.lock();
        m_interpolator.create(16, m_inputSampleRate, m_inputSampleRate / 2.2);
        m_interpolatorDistanceRemain = 0.0f;
        m_interpolatorDistance =  (Real) m_inputSampleRate / (Real) settings.m_downSampleRate;
        m_settingsMutex.unlock();
    }

    if ((settings.m_downSample != m_settings.m_downSample) || force)
    {
        int sampleRate = settings.m_downSample ? settings.m_downSampleRate : m_inputSampleRate;

        m_settingsMutex.lock();
        m_useInterpolator = settings.m_downSample;
        setFilters(sampleRate, settings.m_bandwidth, settings.m_lowCutoff);
        m_pll.setSampleRate(sampleRate / (1<<settings.m_spanLog2));
        m_fll.setSampleRate(sampleRate / (1<<settings.m_spanLog2));
        m_settingsMutex.unlock();
    }

    if ((settings.m_bandwidth != m_settings.m_bandwidth) ||
        (settings.m_lowCutoff != m_settings.m_lowCutoff)|| force)
    {
        m_settingsMutex.lock();
        setFilters(settings.m_downSample ? settings.m_downSampleRate : m_inputSampleRate, settings.m_bandwidth, settings.m_lowCutoff);
        m_settingsMutex.unlock();
    }

    if ((settings.m_rrcRolloff != m_settings.m_rrcRolloff) || force)
    {
        float sampleRate = settings.m_downSample ? (float) settings.m_downSampleRate : (float) m_inputSampleRate;
        m_settingsMutex.lock();
        RRCFilter->create_rrc_filter(settings.m_bandwidth / sampleRate, settings.m_rrcRolloff / 100.0);
        m_settingsMutex.unlock();
    }

    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force)
    {
        int sampleRate = (settings.m_downSample ? settings.m_downSampleRate : m_inputSampleRate) / (1<<m_settings.m_spanLog2);
        m_pll.setSampleRate(sampleRate);
        m_fll.setSampleRate(sampleRate);
    }

    if (settings.m_pll != m_settings.m_pll || force)
    {
        if (settings.m_pll)
        {
            m_pll.reset();
            m_fll.reset();
        }
    }

    if (settings.m_fll != m_settings.m_fll || force)
    {
        if (settings.m_fll) {
            m_fll.reset();
        }
    }

    if (settings.m_pllPskOrder != m_settings.m_pllPskOrder || force)
    {
        if (settings.m_pllPskOrder < 32) {
            m_pll.setPskOrder(settings.m_pllPskOrder);
        }
    }

    m_settings = settings;
}
