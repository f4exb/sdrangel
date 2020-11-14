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

#include <QTime>
#include <QTimer>
#include <QDebug>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/fftfilt.h"
#include "dsp/spectrumvis.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "util/messagequeue.h"

#include "freqtrackerreport.h"
#include "freqtrackersink.h"

FreqTrackerSink::FreqTrackerSink() :
        m_channelSampleRate(48000),
        m_inputFrequencyOffset(0),
        m_sinkSampleRate(48000),
        m_spectrumSink(nullptr),
        m_sampleBufferCount(0),
      	m_undersampleCount(0),
        m_squelchOpen(false),
        m_squelchGate(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_timerConnected(false),
        m_tickCount(0),
        m_lastCorrAbs(0),
        m_avgDeltaFreq(0.0),
        m_messageQueueToInput(nullptr)
{
#ifdef USE_INTERNAL_TIMER
#warning "Uses internal timer"
    m_timer = new QTimer();
    m_timer->start(50);
#else
    m_timer = &DSPEngine::instance()->getMasterTimer();
#endif
	m_magsq = 0.0;
    m_sampleBufferSize = m_sinkSampleRate / 20; // 50 ms
    m_sampleBuffer.resize(m_sampleBufferSize);
    m_sum = Complex{0.0, 0.0};

    m_rrcFilter = new fftfilt(m_settings.m_rfBandwidth / m_sinkSampleRate, 2*1024);
    m_pll.computeCoefficients(0.002f, 0.5f, 10.0f); // bandwidth, damping factor, loop gain
    applyChannelSettings(m_channelSampleRate, m_inputFrequencyOffset, true);
}

FreqTrackerSink::~FreqTrackerSink()
{
    disconnectTimer();
#ifdef USE_INTERNAL_TIMER
    m_timer->stop();
    delete m_timer;
#endif
    delete m_rrcFilter;
}

void FreqTrackerSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
	{
		Complex c(it->real(), it->imag());
		c *= m_nco.nextIQ();

		if (m_interpolatorDistance < 1.0f) // interpolate
		{
            processOneSample(ci);

		    while (m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci)) {
                processOneSample(ci);
            }

            m_interpolatorDistanceRemain += m_interpolatorDistance;
		}
		else // decimate
		{
	        if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
	        {
	            processOneSample(ci);
	            m_interpolatorDistanceRemain += m_interpolatorDistance;
	        }
		}
	}
}

void FreqTrackerSink::processOneSample(Complex &ci)
{
    fftfilt::cmplx *sideband;
    int n_out;
	int decim = 1<<m_settings.m_spanLog2;
    m_sum += ci;

    if (m_undersampleCount++ == decim)
    {
        Real avgr = m_sum.real() / decim;
        Real avgi = m_sum.imag() / decim;
        m_sampleBuffer[m_sampleBufferCount++] = Sample(avgr, avgi);
        m_sum.real(0.0);
        m_sum.imag(0.0);
        m_undersampleCount = 0;
    }

    if (m_settings.m_rrc)
    {
        n_out = m_rrcFilter->runFilt(ci, &sideband);
    }
    else
    {
        n_out = 1;
        sideband = &ci;
    }

    for (int i = 0; i < n_out; i++)
    {
        Real re = sideband[i].real() / SDR_RX_SCALEF;
        Real im = sideband[i].imag() / SDR_RX_SCALEF;
        Real magsq = re*re + im*im;
        m_movingAverage(magsq);
        m_magsq = m_movingAverage.asDouble();
        m_magsqSum += magsq;

        if (magsq > m_magsqPeak)
        {
            m_magsqPeak = magsq;
        }

        m_magsqCount++;

        if (m_magsq < m_squelchLevel)
        {
            if (m_squelchGate > 0)
            {
                if (m_squelchCount > 0) {
                    m_squelchCount--;
                }

                m_squelchOpen = m_squelchCount >= m_squelchGate;
            }
            else
            {
                m_squelchOpen = false;
            }
        }
        else
        {
            if (m_squelchGate > 0)
            {
                if (m_squelchCount < 2*m_squelchGate) {
                    m_squelchCount++;
                }

                m_squelchOpen = m_squelchCount >= m_squelchGate;
            }
            else
            {
                m_squelchOpen = true;
            }
        }

        if (m_squelchOpen)
        {
            if (m_settings.m_trackerType == FreqTrackerSettings::TrackerFLL) {
                m_fll.feed(re, im);
            } else if (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL) {
                m_pll.feed(re, im);
            }
        }

    }

    if (m_spectrumSink && (m_sampleBufferCount == m_sampleBufferSize))
    {
        m_spectrumSink->feed(m_sampleBuffer.begin(), m_sampleBuffer.end(), false);
        m_sampleBufferCount = 0;
    }

}

Real FreqTrackerSink::getFrequency() const
{
    if (m_settings.m_trackerType == FreqTrackerSettings::TrackerPLL) {
        return (m_pll.getFreq() * m_sinkSampleRate) / (2.0 * M_PI);
    } else if (m_settings.m_trackerType == FreqTrackerSettings::TrackerFLL) {
        return (m_fll.getFreq() * m_sinkSampleRate) / (2.0 * M_PI);
    } else {
        return 0;
    }
}

void FreqTrackerSink::applyChannelSettings(int sinkSampleRate, int channelSampleRate, int inputFrequencyOffset, bool force)
{
    if (!m_settings.m_tracking)
    {
        qDebug() << "FreqTracker::applyChannelSettings:"
                << " sinkSampleRate: " << sinkSampleRate
                << " channelSampleRate: " << channelSampleRate
                << " inputFrequencyOffset: " << inputFrequencyOffset;
    }

    bool useInterpolator = false;

    if ((m_inputFrequencyOffset != inputFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-inputFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate)
     || (m_sinkSampleRate != sinkSampleRate) || force)
     {
        m_pll.setSampleRate(sinkSampleRate);
        m_fll.setSampleRate(sinkSampleRate);
        useInterpolator = true;
    }

    m_sinkSampleRate = sinkSampleRate;
    m_channelSampleRate = channelSampleRate;
    m_inputFrequencyOffset = inputFrequencyOffset;

    if (useInterpolator) {
        setInterpolator();
    }

    m_sampleBufferSize = (m_sinkSampleRate/(1<<m_settings.m_spanLog2)) / 20; // 50 ms
    m_sampleBuffer.resize(m_sampleBufferSize);
    m_sampleBufferCount = 0;
    m_undersampleCount = 0;
}

void FreqTrackerSink::applySettings(const FreqTrackerSettings& settings, bool force)
{
    if (!settings.m_tracking)
    {
        qDebug() << "FreqTrackerSink::applySettings:"
                << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
                << " m_rfBandwidth: " << settings.m_rfBandwidth
                << " m_log2Decim: " << settings.m_log2Decim
                << " m_squelch: " << settings.m_squelch
                << " m_rgbColor: " << settings.m_rgbColor
                << " m_title: " << settings.m_title
                << " m_alphaEMA: " << settings.m_alphaEMA
                << " m_tracking: " << settings.m_tracking
                << " m_trackerType: " << settings.m_trackerType
                << " m_pllPskOrder: " << settings.m_pllPskOrder
                << " m_rrc: " << settings.m_rrc
                << " m_rrcRolloff: " << settings.m_rrcRolloff
                << " m_streamIndex: " << settings.m_streamIndex
                << " m_useReverseAPI: " << settings.m_useReverseAPI
                << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
                << " m_reverseAPIPort: " << settings.m_reverseAPIPort
                << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
                << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
                << " force: " << force;
    }


    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
    }

    if ((m_settings.m_tracking != settings.m_tracking) || force)
    {
        m_avgDeltaFreq = 0.0;
        m_lastCorrAbs = 0;

        if (settings.m_tracking)
        {
            m_pll.reset();
            m_fll.reset();
        }
    }

    if ((m_settings.m_trackerType != settings.m_trackerType) || force)
    {
        m_lastCorrAbs = 0;
        m_avgDeltaFreq = 0.0;

        if (settings.m_trackerType == FreqTrackerSettings::TrackerFLL) {
            m_fll.reset();
        } else if (settings.m_trackerType == FreqTrackerSettings::TrackerPLL) {
            m_pll.reset();
        }

        if (settings.m_trackerType == FreqTrackerSettings::TrackerNone) {
            disconnectTimer();
        } else {
            connectTimer();
        }
    }

    if ((m_settings.m_pllPskOrder != settings.m_pllPskOrder) || force)
    {
        if (settings.m_pllPskOrder < 32) {
            m_pll.setPskOrder(settings.m_pllPskOrder);
        }
    }

    bool useInterpolator = false;

    if ((m_settings.m_rrcRolloff != settings.m_rrcRolloff)
     || (m_settings.m_rfBandwidth != settings.m_rfBandwidth)
     || (m_settings.m_squelchGate != settings.m_squelchGate) || force) {
        useInterpolator = true;
    }

    if ((settings.m_spanLog2 != m_settings.m_spanLog2) || force)
    {
        m_sampleBufferSize = (m_sinkSampleRate/(1<<settings.m_spanLog2)) / 20; // 50 ms
        m_sampleBuffer.resize(m_sampleBufferSize);
        m_sampleBufferCount = 0;
        m_undersampleCount = 0;
    }

    m_settings = settings;

    if (useInterpolator) {
        setInterpolator();
    }
}

void FreqTrackerSink::setInterpolator()
{
    qDebug("FreqTrackerSink::setInterpolator: m_sinkSampleRate: %u m_channelSampleRate: %d",
        m_sinkSampleRate, m_channelSampleRate);

    m_interpolator.create(16, m_channelSampleRate, m_settings.m_rfBandwidth / 2.2f);
    m_interpolatorDistanceRemain = 0;
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_sinkSampleRate;
    m_rrcFilter->create_rrc_filter(m_settings.m_rfBandwidth / m_sinkSampleRate, m_settings.m_rrcRolloff / 100.0);
    m_squelchGate = (m_sinkSampleRate / 100) * m_settings.m_squelchGate; // gate is given in 10s of ms at channel sample rate
}

void FreqTrackerSink::connectTimer()
{
    if (!m_timerConnected)
    {
        m_tickCount = 0;
        connect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_timerConnected = true;
    }
}

void FreqTrackerSink::disconnectTimer()
{
    if (m_timerConnected)
    {
        disconnect(m_timer, SIGNAL(timeout()), this, SLOT(tick()));
        m_timerConnected = false;
    }
}

void FreqTrackerSink::tick()
{
    if (getSquelchOpen()) {
        m_avgDeltaFreq = m_settings.m_alphaEMA*getFrequency() + (1.0 - m_settings.m_alphaEMA)*m_avgDeltaFreq;
    }

    if (m_tickCount < 9)
    {
        m_tickCount++;
    }
    else
    {
        if ((m_settings.m_tracking) && getSquelchOpen())
        {
            int decayDivider = 200.0 * m_settings.m_alphaEMA;
            int decayAmount = m_sinkSampleRate < decayDivider ? 1 : m_sinkSampleRate / decayDivider;
            int trim = m_sinkSampleRate / 1000;

            if (m_lastCorrAbs < decayAmount)
            {
                m_lastCorrAbs = m_avgDeltaFreq < 0 ? -m_avgDeltaFreq : m_avgDeltaFreq;

                if (m_lastCorrAbs > trim)
                {
                    FreqTrackerSettings settings = m_settings;
                    settings.m_inputFrequencyOffset += m_avgDeltaFreq;
                    if (getMessageQueueToInput())
                    {
                        FreqTrackerReport::MsgSinkFrequencyOffsetNotification *msg
                            = FreqTrackerReport::MsgSinkFrequencyOffsetNotification::create(settings.m_inputFrequencyOffset);
                        getMessageQueueToInput()->push(msg);
                    }
                }
            }
            else
            {
                m_lastCorrAbs -= decayAmount;
            }
        }

        m_tickCount = 0;
    }
}
