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

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "maincore.h"

#include "aptdemod.h"
#include "aptdemodsink.h"

APTDemodSink::APTDemodSink(APTDemod *packetDemod) :
        m_aptDemod(packetDemod),
        m_channelSampleRate(APTDEMOD_AUDIO_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_imageWorkerMessageQueue(nullptr),
        m_samples(nullptr)
{
    m_magsq = 0.0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    m_samplesLength = APTDEMOD_AUDIO_SAMPLE_RATE * APT_MAX_HEIGHT / 2;   // APT broadcasts at 2 lines per second
    m_samples = new float[m_samplesLength];

    resetDecoder();
}

void APTDemodSink::resetDecoder()
{
    m_sampleCount = 0;
    m_writeIdx = 0;
    m_readIdx = 0;

    apt_init(APTDEMOD_AUDIO_SAMPLE_RATE);

    m_row = 0;
    m_zenith = 0;
}

APTDemodSink::~APTDemodSink()
{
    delete[] m_samples;
}

// callback from APT library to get audio samples
static int getsamples(void *context, float *samples, int count)
{
    APTDemodSink *sink = (APTDemodSink *)context;
    return sink->getSamples(samples, count);
}

int APTDemodSink::getSamples(float *samples, int count)
{
    for (int i = 0; i < count; i++)
    {
        if ((m_sampleCount > 0) && (m_readIdx < m_samplesLength))
        {
            *samples++ = m_samples[m_readIdx++];
            m_sampleCount--;
        }
        else
            return i;
    }

    return count;
}

void APTDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

    // Have we enough samples to decode one line?
    // 2 lines per second
    if (m_sampleCount >= APTDEMOD_AUDIO_SAMPLE_RATE)
    {
        if (getImageWorkerMessageQueue())
        {
            float *pixels = new float[APT_PROW_WIDTH];
            apt_getpixelrow(pixels, m_row, &m_zenith, m_row == 0, getsamples, this);
            getImageWorkerMessageQueue()->push(APTDemod::MsgPixels::create(pixels, m_zenith));
        }

        m_row++;
    }
}


void APTDemodSink::processOneSample(Complex &ci)
{
    Complex ca;

    // FM demodulation
    double magsqRaw;
    Real deviation;
    Real fmDemod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

    // Add to sample buffer, if there's space and decoding is enabled
    if ((m_writeIdx < m_samplesLength) && m_settings.m_decodeEnabled)
    {
        m_samples[m_writeIdx++] = fmDemod;
        m_sampleCount++;
    }

    // Calculate average and peak levels for level meter
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;
}

void APTDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "APTDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth, 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) APTDEMOD_AUDIO_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void APTDemodSink::applySettings(const APTDemodSettings& settings, bool force)
{
    qDebug() << "APTDemodSink::applySettings:"
             << " m_rfBandwidth: " << settings.m_rfBandwidth
             << " m_fmDeviation: " << settings.m_fmDeviation
             << " m_decodeEnabled: " << settings.m_decodeEnabled
             << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth, 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) APTDEMOD_AUDIO_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(APTDEMOD_AUDIO_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    m_settings = settings;
}
