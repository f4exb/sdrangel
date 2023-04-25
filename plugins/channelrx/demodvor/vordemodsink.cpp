///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "audio/audiooutputdevice.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "util/morse.h"
#include "util/units.h"

#include "vordemodreport.h"
#include "vordemodsettings.h"
#include "vordemodsink.h"

VORDemodSCSink::VORDemodSCSink() :
        m_channelFrequencyOffset(0),
        m_channelSampleRate(VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE),
        m_audioSampleRate(48000),
        m_squelchCount(0),
        m_squelchOpen(false),
        m_squelchDelayLine(9600),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_volumeAGC(0.003),
        m_audioFifo(48000),
        m_refPrev(0.0f),
        m_varGoertzel(30, VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE),
        m_refGoertzel(30, VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE)
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

	m_magsq = 0.0;

	applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

VORDemodSCSink::~VORDemodSCSink()
{
}

void VORDemodSCSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void VORDemodSCSink::processOneAudioSample(Complex &ci)
{
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;

    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;

    m_squelchDelayLine.write(magsq);

    if (m_magsq < m_squelchLevel)
    {
        if (m_squelchCount > 0) {
            m_squelchCount--;
        }
    }
    else
    {
        if (m_squelchCount < (unsigned int)m_audioSampleRate / 10) {
            m_squelchCount++;
        }
    }

    qint16 sample;

    m_squelchOpen = (m_squelchCount >= (unsigned int)m_audioSampleRate / 20);

    if (m_squelchOpen && !m_settings.m_audioMute)
    {
        Real demod;

        {
            demod = sqrt(m_squelchDelayLine.readBack(m_audioSampleRate/20));
            m_volumeAGC.feed(demod);
            demod = (demod - m_volumeAGC.getValue()) / m_volumeAGC.getValue();
        }

        demod = m_bandpass.filter(demod);

        Real attack = (m_squelchCount - 0.05f * m_audioSampleRate) / (0.05f * m_audioSampleRate);
        sample = demod * StepFunctions::smootherstep(attack) * (m_audioSampleRate/24) * m_settings.m_volume;
    }
    else
    {
        sample = 0;
    }

    m_audioBuffer[m_audioBufferFill].l = sample;
    m_audioBuffer[m_audioBufferFill].r = sample;
    ++m_audioBufferFill;

    if (m_audioBufferFill >= m_audioBuffer.size())
    {
        uint res = m_audioFifo.write((const quint8*)&m_audioBuffer[0], m_audioBufferFill);

        if (res != m_audioBufferFill)
        {
            qDebug("VORDemodSCSink::processOneAudioSample: %u/%u audio samples written", res, m_audioBufferFill);
            m_audioFifo.clear();
        }

        m_audioBufferFill = 0;
    }
}


void VORDemodSCSink::processOneSample(Complex &ci)
{
    Complex ca;

    // Resample as audio
    if (m_audioInterpolatorDistance < 1.0f) // interpolate
    {
        while (!m_audioInterpolator.interpolate(&m_audioInterpolatorDistanceRemain, ci, &ca))
        {
            processOneAudioSample(ca);
            m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
        }
    }
    else // decimate
    {
        if (m_audioInterpolator.decimate(&m_audioInterpolatorDistanceRemain, ci, &ca))
        {
            processOneAudioSample(ca);
            m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
        }
    }

    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;

    // AM Demod
    Real mag = std::sqrt(magsq);

    // Calculate phase of 30Hz variable AM signal
    double varPhase;
    double varMag;
    if (m_varGoertzel.size() == VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE - 1)
    {
        m_varGoertzel.goertzel(mag);
        varPhase = Units::radiansToDegrees(m_varGoertzel.phase());
        varMag = m_varGoertzel.mag();
        m_varGoertzel.reset();
    }
    else
        m_varGoertzel.filter(mag);

    Complex magc(mag, 0.0);

    // Mix reference sub-carrier down to 0Hz
    Complex fm0 = magc;
    fm0 *= m_ncoRef.nextIQ();
    // Filter other signals
    Complex fmfilt = m_lowpassRef.filter(fm0);

    // FM demod
    Real phi = std::arg(std::conj(m_refPrev) * fmfilt);
    m_refPrev = fmfilt;

    // Calculate phase of 30Hz reference FM signal
    if (m_refGoertzel.size() == VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE - 1)
    {
        m_refGoertzel.goertzel(phi);
        float phaseDeg = Units::radiansToDegrees(m_refGoertzel.phase());
        double refMag = m_refGoertzel.mag();
        int groupDelay = (301-1)/2;
        float filterPhaseShift = 360.0*30.0*groupDelay/VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE;
        float shiftedPhase = phaseDeg + filterPhaseShift;

        // Calculate difference in phase, which is the radial
        float phaseDifference = shiftedPhase - varPhase;
        if (phaseDifference < 0.0)
            phaseDifference += 360.0;
        else if (phaseDifference >= 360.0)
            phaseDifference -= 360.0;

        // qDebug() << "Ref phase: " << phaseDeg << " var phase " << varPhase;

        if (getMessageQueueToChannel())
        {
            VORDemodReport::MsgReportRadial *msg = VORDemodReport::MsgReportRadial::create(phaseDifference, refMag, varMag);
            getMessageQueueToChannel()->push(msg);
        }

        m_refGoertzel.reset();
    }
    else
        m_refGoertzel.filter(phi);

    // Decode Morse ident
    m_morseDemod.processOneSample(magc);
}

void VORDemodSCSink::setMessageQueueToChannel(MessageQueue *messageQueue)
{
    m_messageQueueToChannel = messageQueue;
    m_morseDemod.setMessageQueueToChannel(messageQueue);
}

void VORDemodSCSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "VORDemodSCSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, VORDemodSettings::VORDEMOD_CHANNEL_BANDWIDTH);
        m_interpolatorDistanceRemain = 0;
        m_interpolatorDistance = (Real) channelSampleRate / (Real) VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE;

        m_ncoRef.setFreq(-9960, VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE);
        m_lowpassRef.create(301, VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE, 600.0f); // Max deviation is 480Hz

        m_morseDemod.applyChannelSettings(VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE);
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void VORDemodSCSink::applySettings(const VORDemodSettings& settings, bool force)
{
    qDebug() << "VORDemodSCSink::applySettings:"
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_identBandpassEnable: " << settings.m_identBandpassEnable
            << " force: " << force;

    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
    }

    if (m_settings.m_navId != settings.m_navId)
    {
        // Reset state when navId changes, so we don't report old ident for new navId
        m_morseDemod.reset();
        m_refGoertzel.reset();
        m_varGoertzel.reset();
    }

    if ((m_settings.m_identBandpassEnable != settings.m_identBandpassEnable) || force)
    {
        if (settings.m_identBandpassEnable) {
            m_bandpass.create(1001, m_audioSampleRate, 970.0f, 1070.0f);
        } else {
            m_bandpass.create(301, m_audioSampleRate, 300.0f, 3000.0f);
        }
        //m_bandpass.printTaps("audio_bpf");
    }

    m_settings = settings;
    m_morseDemod.applySettings(m_settings.m_identThreshold);
}

void VORDemodSCSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("VORDemodSCSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("VORDemodSCSink::applyAudioSampleRate: sampleRate: %d m_channelSampleRate: %d", sampleRate, m_channelSampleRate);

    // (ICAO Annex 10 3.3.6.3) - Optional voice audio is 300Hz to 3kHz
    m_audioInterpolator.create(16, VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE, 3000.0f);
    m_audioInterpolatorDistanceRemain = 0;
    m_audioInterpolatorDistance = (Real) VORDemodSettings::VORDEMOD_CHANNEL_SAMPLE_RATE / (Real) sampleRate;
    if (m_settings.m_identBandpassEnable) {
        m_bandpass.create(1001, sampleRate, 970.0f, 1070.0f);
    } else {
        m_bandpass.create(301, sampleRate, 300.0f, 3000.0f);
    }
    //m_bandpass.printTaps("audio_bpf");
    m_audioFifo.setSize(sampleRate);
    m_squelchDelayLine.resize(sampleRate/5);

    m_volumeAGC.resizeNew(sampleRate/10, 0.003f);

    m_audioSampleRate = sampleRate;
}
