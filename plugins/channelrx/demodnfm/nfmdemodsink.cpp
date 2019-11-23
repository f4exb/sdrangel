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

#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>

#include "util/stepfunctions.h"
#include "util/db.h"
#include "audio/audiooutput.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"

#include "nfmdemodreport.h"
#include "nfmdemodsink.h"

const double NFMDemodSink::afSqTones[] = {1000.0, 6000.0}; // {1200.0, 8000.0};
const double NFMDemodSink::afSqTones_lowrate[] = {1000.0, 3500.0};

NFMDemodSink::NFMDemodSink() :
        m_channelSampleRate(48000),
        m_channelFrequencyOffset(0),
        m_audioSampleRate(48000),
        m_audioBufferFill(0),
        m_audioFifo(48000),
        m_ctcssIndex(0),
        m_sampleCount(0),
        m_squelchCount(0),
        m_squelchGate(4800),
        m_squelchLevel(-990),
        m_squelchOpen(false),
        m_afSquelchOpen(false),
        m_magsq(0.0f),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_afSquelch(),
        m_squelchDelayLine(24000),
        m_messageQueueToGUI(nullptr)
{
	m_agcLevel = 1.0;
    m_audioBuffer.resize(1<<14);

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

NFMDemodSink::~NFMDemodSink()
{
}

void NFMDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
	Complex ci;

	for (SampleVector::const_iterator it = begin; it != end; ++it)
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

void NFMDemodSink::processOneSample(Complex &ci)
{
    qint16 sample;

    double magsqRaw; // = ci.real()*ci.real() + c.imag()*c.imag();
    Real deviation;

    Real demod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;
    m_sampleCount++;

    // AF processing

    if (m_settings.m_deltaSquelch)
    {
        if (m_afSquelch.analyze(demod * m_discriCompensation))
        {
            m_afSquelchOpen = m_afSquelch.evaluate(); // ? m_squelchGate + m_squelchDecay : 0;

            if (!m_afSquelchOpen) {
                m_squelchDelayLine.zeroBack(m_audioSampleRate/10); // zero out evaluation period
            }
        }

        if (m_afSquelchOpen)
        {
            m_squelchDelayLine.write(demod * m_discriCompensation);

            if (m_squelchCount < 2*m_squelchGate) {
                m_squelchCount++;
            }
        }
        else
        {
            m_squelchDelayLine.write(0);

            if (m_squelchCount > 0) {
                m_squelchCount--;
            }
        }
    }
    else
    {
        if ((Real) m_movingAverage < m_squelchLevel)
        {
            m_squelchDelayLine.write(0);

            if (m_squelchCount > 0) {
                m_squelchCount--;
            }
        }
        else
        {
            m_squelchDelayLine.write(demod * m_discriCompensation);

            if (m_squelchCount < 2*m_squelchGate) {
                m_squelchCount++;
            }
        }
    }

    m_squelchOpen = (m_squelchCount > m_squelchGate);

    if (m_settings.m_audioMute)
    {
        sample = 0;
    }
    else
    {
        if (m_squelchOpen)
        {
            if (m_settings.m_ctcssOn)
            {
                Real ctcss_sample = m_ctcssLowpass.filter(demod * m_discriCompensation);

                if ((m_sampleCount & 7) == 7) // decimate 48k -> 6k
                {
                    if (m_ctcssDetector.analyze(&ctcss_sample))
                    {
                        int maxToneIndex;

                        if (m_ctcssDetector.getDetectedTone(maxToneIndex))
                        {
                            if (maxToneIndex+1 != m_ctcssIndex)
                            {
                                if (getMessageQueueToGUI())
                                {
                                    NFMDemodReport::MsgReportCTCSSFreq *msg = NFMDemodReport::MsgReportCTCSSFreq::create(m_ctcssDetector.getToneSet()[maxToneIndex]);
                                    getMessageQueueToGUI()->push(msg);
                                }

                                m_ctcssIndex = maxToneIndex+1;
                            }
                        }
                        else
                        {
                            if (m_ctcssIndex != 0)
                            {
                                if (getMessageQueueToGUI())
                                {
                                    NFMDemodReport::MsgReportCTCSSFreq *msg = NFMDemodReport::MsgReportCTCSSFreq::create(0);
                                    getMessageQueueToGUI()->push(msg);
                                }

                                m_ctcssIndex = 0;
                            }
                        }
                    }
                }
            }

            if (m_settings.m_ctcssOn && m_ctcssIndexSelected && (m_ctcssIndexSelected != m_ctcssIndex))
            {
                sample = 0;
            }
            else
            {
                if (m_settings.m_highPass) {
                    sample = m_bandpass.filter(m_squelchDelayLine.readBack(m_squelchGate)) * m_settings.m_volume;
                } else {
                    sample = m_lowpass.filter(m_squelchDelayLine.readBack(m_squelchGate)) * m_settings.m_volume * 301.0f;
                }
            }
        }
        else
        {
            if (m_ctcssIndex != 0)
            {
                if (getMessageQueueToGUI())
                {
                    NFMDemodReport::MsgReportCTCSSFreq *msg = NFMDemodReport::MsgReportCTCSSFreq::create(0);
                    getMessageQueueToGUI()->push(msg);
                }

                m_ctcssIndex = 0;
            }

            sample = 0;
        }
    }

    m_audioBuffer[m_audioBufferFill].l = sample;
    m_audioBuffer[m_audioBufferFill].r = sample;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill)
        {
            qDebug("NFMDemodSink::feed: %u/%u audio samples written", res, m_audioBufferFill);
            qDebug("NFMDemodSink::feed: m_audioSampleRate: %u m_channelSampleRate: %d", m_audioSampleRate, m_channelSampleRate);
        }

        m_audioBufferFill = 0;
    }
}


void NFMDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "NFMDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((channelFrequencyOffset != m_channelFrequencyOffset) ||
        (channelSampleRate != m_channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((channelSampleRate != m_channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) channelSampleRate / (Real) m_audioSampleRate;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void NFMDemodSink::applySettings(const NFMDemodSettings& settings, bool force)
{
    qDebug() << "NFMDemodSink::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volume: " << settings.m_volume
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_deltaSquelch: " << settings.m_deltaSquelch
            << " m_squelch: " << settings.m_squelch
            << " m_ctcssIndex: " << settings.m_ctcssIndex
            << " m_ctcssOn: " << settings.m_ctcssOn
            << " m_highPass: " << settings.m_highPass
            << " m_audioMute: " << settings.m_audioMute
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance =  (Real) m_channelSampleRate / (Real) m_audioSampleRate;
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling((8.0f*m_audioSampleRate) / static_cast<float>(settings.m_fmDeviation)); // integrate 4x factor
    }

    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force)
    {
        m_bandpass.create(301, m_audioSampleRate, 300.0, settings.m_afBandwidth);
        m_lowpass.create(301, m_audioSampleRate, settings.m_afBandwidth);
    }

    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force)
    {
        m_squelchGate = (m_audioSampleRate / 100) * settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_squelch != m_settings.m_squelch) ||
        (settings.m_deltaSquelch != m_settings.m_deltaSquelch) || force)
    {
        if (settings.m_deltaSquelch)
        { // input is a value in negative centis
            m_squelchLevel = (- settings.m_squelch) / 100.0;
            m_afSquelch.setThreshold(m_squelchLevel);
            m_afSquelch.reset();
        }
        else
        { // input is a value in deci-Bels
            m_squelchLevel = std::pow(10.0, settings.m_squelch / 10.0);
            m_movingAverage.reset();
        }

        m_squelchCount = 0; // reset squelch open counter
    }

    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force) {
        setSelectedCtcssIndex(settings.m_ctcssIndex);
    }

    m_settings = settings;
}

void NFMDemodSink::applyAudioSampleRate(unsigned int sampleRate)
{
    qDebug("NFMDemodSink::applyAudioSampleRate: %u m_channelSampleRate: %d", sampleRate, m_channelSampleRate);

    m_ctcssLowpass.create(301, sampleRate, 250.0);
    m_bandpass.create(301, sampleRate, 300.0, m_settings.m_afBandwidth);
    m_lowpass.create(301, sampleRate, m_settings.m_afBandwidth);
    m_squelchGate = (sampleRate / 100) * m_settings.m_squelchGate; // gate is given in 10s of ms at 48000 Hz audio sample rate
    m_squelchCount = 0; // reset squelch open counter
    m_ctcssDetector.setCoefficients(sampleRate/16, sampleRate/8.0f); // 0.5s / 2 Hz resolution

    if (sampleRate < 16000) {
        m_afSquelch.setCoefficients(sampleRate/2000, 600, sampleRate, 200, 0, afSqTones_lowrate); // 0.5ms test period, 300ms average span, audio SR, 100ms attack, no decay

    } else {
        m_afSquelch.setCoefficients(sampleRate/2000, 600, sampleRate, 200, 0, afSqTones); // 0.5ms test period, 300ms average span, audio SR, 100ms attack, no decay
    }

    m_discriCompensation = (sampleRate/48000.0f);
    m_discriCompensation *= sqrt(m_discriCompensation);

    m_phaseDiscri.setFMScaling(sampleRate / static_cast<float>(m_settings.m_fmDeviation));
    m_audioFifo.setSize(sampleRate);
    m_squelchDelayLine.resize(sampleRate/2);

    m_audioSampleRate = sampleRate;
}