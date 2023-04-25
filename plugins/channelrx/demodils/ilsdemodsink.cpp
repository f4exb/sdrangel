///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "dsp/scopevis.h"
#include "util/stepfunctions.h"
#include "util/db.h"
#include "util/morse.h"
#include "util/units.h"
#include "maincore.h"

#include "ilsdemod.h"
#include "ilsdemodsink.h"

ILSDemodSink::ILSDemodSink(ILSDemod *ilsDemod) :
        m_spectrumSink(nullptr),
        m_scopeSink(nullptr),
        m_ilsDemod(ilsDemod),
        m_channel(nullptr),
        m_channelSampleRate(ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_audioSampleRate(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_fftSequence(-1),
        m_fft(nullptr),
        m_fftCounter(0),
        m_squelchLevel(0.001f),
        m_squelchCount(0),
        m_squelchOpen(false),
        m_squelchDelayLine(9600),
        m_volumeAGC(0.003),
        m_audioFifo(48000),
        m_sampleBufferIndex(0)
{
	m_audioBuffer.resize(1<<14);
	m_audioBufferFill = 0;

    m_magsq = 0.0;

    m_sampleBuffer.resize(m_sampleBufferSize);
    m_spectrumSampleBuffer.resize(m_sampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    if (m_fftSequence >= 0) {
        fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
    }
    m_fftSequence = fftFactory->getEngine(m_fftSize, false, &m_fft);
    m_fftCounter = 0;
    m_fftWindow.create(FFTWindow::Flattop, m_fftSize);
}

ILSDemodSink::~ILSDemodSink()
{
}

void ILSDemodSink::sampleToScope(Complex sample, Real demod)
{
    Real r = std::real(sample) * SDR_RX_SCALEF;
    Real i = std::imag(sample) * SDR_RX_SCALEF;
    m_sampleBuffer[m_sampleBufferIndex] = Sample(r, i);
    m_spectrumSampleBuffer[m_sampleBufferIndex] = Sample(demod * SDR_RX_SCALEF, 0);
    m_sampleBufferIndex++;

    if (m_sampleBufferIndex == m_sampleBufferSize)
    {
        if (m_scopeSink)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_sampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_sampleBufferSize);
        }
        if (m_spectrumSink)
        {
		    m_spectrumSink->feed(m_spectrumSampleBuffer.begin(), m_spectrumSampleBuffer.end(), false);
        }
        m_sampleBufferIndex = 0;
    }
}

void ILSDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void ILSDemodSink::processOneSample(Complex &ci)
{
    Complex ca;

    // Calculate average and peak levels for level meter
    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();;
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    ci /= SDR_RX_SCALEF;

    // AM demodulation
    Complex demod = std::abs(ci);

    // Resample as audio
    if (m_audioInterpolatorDistance < 1.0f) // interpolate
    {
        while (!m_audioInterpolator.interpolate(&m_audioInterpolatorDistanceRemain, demod, &ca))
        {
            processOneAudioSample(ca);
            m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
        }
    }
    else // decimate
    {
        if (m_audioInterpolator.decimate(&m_audioInterpolatorDistanceRemain, demod, &ca))
        {
            processOneAudioSample(ca);
            m_audioInterpolatorDistanceRemain += m_audioInterpolatorDistance;
        }
    }

    // Decimate again for spectral analysis
    Complex demodDecim;
    if (m_decimator.decimate(demod, demodDecim))
    {

        // Use FFT to calculate sidebands modulation depth
        m_fft->in()[m_fftCounter] = demodDecim;
        m_fftCounter++;
        if (m_fftCounter == m_fftSize)
        {
            calcDDM();
            m_fftCounter = 0;

            // Send results to GUI
            if (getMessageQueueToChannel())
            {
                Real modDepth90, modDepth150, sdm, ddm;
                if (m_settings.m_average)
                {
                    modDepth90 = m_modDepth90Average.instantAverage();
                    modDepth150 = m_modDepth150Average.instantAverage();
                    sdm = m_sdmAverage.instantAverage();
                    ddm = m_ddmAverage.instantAverage();
                }
                else
                {
                    modDepth90 = m_modDepth90;
                    modDepth150 = m_modDepth150;
                    sdm = m_sdm;
                    ddm = m_ddm;
                }

                Real angle;
                if (m_settings.m_mode == ILSDemodSettings::LOC)
                {
                    // For localiser, angle depends on runway length
                    // At ILS datum (over threshold) (or ILS point B for short runways (<=1200m), which is 1050m from threshold)
                    // the displacement sensitivity is 0.00145 DDM/metre (3.1.3.7)
                    // The points at which DDM is 0.155 (i.e a displacement of 0.155/0.00154=~105m) define the course sector (3.1.3.7.3 Note 1)
                    // And this must be <= 6 degrees (typically between 3-6degrees) (3.1.3.7.1)
                    // Localilzer to threshold distances (geometric angle)
                    // EGKK 3150m (3.8deg), EGKB 1840m (6.5deg), EGLL 3960m (3.0deg), EGLC 1570m(27) 1510m(09) (7.6/8deg) EGJJ 1710m (7deg)
                    // FAS data for EGJJ https://nats-uk.ead-it.com/cms-nats/export/sites/default/en/Publications/AIP/Current-AIRAC/graphics/196515.pdf
                    // LTP (Landing threshold point) 491231.8010N  0021105.6645W    =  49.20883361 -2.18490681
                    // FPAP                          491224.8745N  0021228.7365W    =  49.20690958 -2.20798236
                    // Length offset                 136m  (distance from near threshold??)
                    // LTP-FPAP=1690m   D=1690+305=1995   (GARP is 305m/1000ft from FPAP)
                    // EGJJ angle for 1995m = 6deg
                    angle = ddm / 0.155f * (m_settings.m_courseWidth / 2.0f);
                }
                else
                {
                    // For glide slope, sector is 0.175 DDM = 0.7 degrees
                    // Displacement sensitivity 0.0875 at 0.12*theta (0.12*3=0.36deg) (3.1.5.6.2)
                    // GP coverage is from 0.45*theta to 1.75*theta (5.25-1.35=4.9deg for 3deg GP)
                    angle = 0.12f * m_settings.m_glidePath * ddm / 0.0875f;
                }

                ILSDemod::MsgAngleEstimate *msg = ILSDemod::MsgAngleEstimate::create(m_powerCarrier, m_power90, m_power150, modDepth90, modDepth150, sdm, ddm, angle);
                getMessageQueueToChannel()->push(msg);
            }
        }

        // Select signals to feed to scope
        Complex scopeSample;
        switch (m_settings.m_scopeCh1)
        {
        case 0:
            scopeSample.real(ci.real());
            break;
        case 1:
            scopeSample.real(ci.imag());
            break;
        case 2:
            scopeSample.real(demod.real());
            break;
        }
        switch (m_settings.m_scopeCh2)
        {
        case 0:
            scopeSample.imag(ci.real());
            break;
        case 1:
            scopeSample.imag(ci.imag());
            break;
        case 2:
            scopeSample.imag(demod.real());
            break;
        }
        sampleToScope(scopeSample, demod.real());
    }

}

void ILSDemodSink::processOneAudioSample(Complex &ci)
{
    Real re = ci.real();
    Real im = ci.imag();
    Real magsq = re*re + im*im;
    m_audioMovingAverage(magsq);
    double magsqAvg = m_movingAverage.asDouble();

    m_squelchDelayLine.write(magsq);

    if (magsqAvg < m_squelchLevel)
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
            qDebug("ILSDemodSink::processOneAudioSample: %u/%u audio samples written", res, m_audioBufferFill);
            m_audioFifo.clear();
        }

        m_audioBufferFill = 0;
    }

    m_morseDemod.processOneSample(ci);
}

Real ILSDemodSink::magSq(int bin) const
{
    Complex c = m_fft->out()[bin];
    Real v = c.real() * c.real() + c.imag() * c.imag();
    Real magsq = v / (m_fftSize * m_fftSize);
    return magsq;
}

// Calculate the difference in the depth of modulation (DDM)
void ILSDemodSink::calcDDM()
{
    // 3.1.3.5.3 - the modulating tones shall be 90 Hz and 150 Hz within plus or minus 2.5 per cent
    // At 88/92Hz, some energy is lost in adjacent bin, so we use flat top windowing for accurate
    // amplitude measurement, which is what is needed for calculating depth of modulation
    m_fftWindow.apply(m_fft->in());

    // Perform FFT
    m_fft->transform();

    // Convert bin to frequency offset
    double frequencyResolution = ILSDemodSettings::ILSDEMOD_SPECTRUM_SAMPLE_RATE / (double)m_fftSize;
    int bin90 = 90.0 / frequencyResolution;
    int bin150 = 150.0 / frequencyResolution;

    double mag90, mag150;
    double magSqCarrier = magSq(0);
    double magCarrier = sqrt(magSqCarrier);

    // Add both sidebands
    mag90 = sqrt(magSq(bin90)) + sqrt(magSq(m_fftSize-bin90));
    mag150 = sqrt(magSq(bin150)) + sqrt(magSq(m_fftSize-bin150));

    // Calculate power in dB
    m_powerCarrier = CalcDb::dbPower(magSqCarrier);
    m_power90 =  CalcDb::dbPower(mag90 * mag90);
    m_power150 =  CalcDb::dbPower(mag150 * mag150);

    // Calculate modulation depth as % of carrier
    m_modDepth90 = mag90 / magCarrier * 100.0;
    m_modDepth150 = mag150 / magCarrier * 100.0;

    // Calculate modulation depth difference (https://www.youtube.com/watch?v=71iww_ERoYc)
    m_ddm = (m_modDepth90 - m_modDepth150) / 100.0;

    // Calculate sum of difference of modulation
    m_sdm = (m_modDepth90 + m_modDepth150) / 100.0;

    // Calculate moving averages
    m_modDepth90Average(m_modDepth90);
    m_modDepth150Average(m_modDepth150);
    m_sdmAverage(m_sdm);
    m_ddmAverage(m_ddm);
}

void ILSDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "ILSDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;

}

void ILSDemodSink::applySettings(const ILSDemodSettings& settings, bool force)
{
    qDebug() << "ILSDemodSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_volume: " << settings.m_volume
            << " m_squelch: " << settings.m_squelch
            << " m_audioMute: " << settings.m_audioMute
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " force: " << force;

    if ((m_settings.m_squelch != settings.m_squelch) || force) {
        m_squelchLevel = CalcDb::powerFromdB(settings.m_squelch);
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_identThreshold != m_settings.m_identThreshold) || force) {
        m_morseDemod.applySettings(settings.m_identThreshold);
    }

    if (force)
    {
        m_modDepth90Average.reset();
        m_modDepth150Average.reset();
        m_ddmAverage.reset();
        m_decimator.setLog2Decim(ILSDemodSettings::ILSDEMOD_SPECTRUM_DECIM_LOG2);
    }

    m_settings = settings;
}

void ILSDemodSink::applyAudioSampleRate(int sampleRate)
{
    if (sampleRate < 0)
    {
        qWarning("ILSDemodSink::applyAudioSampleRate: invalid sample rate: %d", sampleRate);
        return;
    }

    qDebug("ILSDemodSink::applyAudioSampleRate: sampleRate: %d channelSampleRate: %d", sampleRate, ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE);

    if (sampleRate != m_audioSampleRate)
    {
        m_audioInterpolator.create(16, ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE, 3500.0f);
        m_audioInterpolatorDistanceRemain = 0;
        m_audioInterpolatorDistance = (Real) ILSDemodSettings::ILSDEMOD_CHANNEL_SAMPLE_RATE / (Real) sampleRate;
        m_bandpass.create(301, sampleRate, 300.0f, 3000.0f);
        //m_bandpass.printTaps("audio_bpf");
        m_audioFifo.setSize(sampleRate);
        m_squelchDelayLine.resize(sampleRate/5);

        m_volumeAGC.resizeNew(sampleRate/10, 0.003f);
        m_morseDemod.applyChannelSettings(sampleRate);
    }

    m_audioSampleRate = sampleRate;
}

