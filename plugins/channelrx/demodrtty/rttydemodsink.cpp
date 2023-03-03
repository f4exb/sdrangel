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
#include <QRegularExpression>

#include <complex.h>

#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "maincore.h"

#include "rttydemod.h"
#include "rttydemodsink.h"

RttyDemodSink::RttyDemodSink(RttyDemod *packetDemod) :
        m_rttyDemod(packetDemod),
        m_channelSampleRate(RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_expLength(600),
        m_prods1(nullptr),
        m_prods2(nullptr),
        m_exp(nullptr),
        m_sampleIdx(0),
        m_clockHistogram(100),
        m_shiftEstMag(m_fftSize),
        m_fftSequence(-1),
        m_fft(nullptr),
        m_fftCounter(0),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_sampleBuffer.resize(m_sampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    if (m_fftSequence >= 0) {
        fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
    }
    m_fftSequence = fftFactory->getEngine(m_fftSize, false, &m_fft);
    m_fftCounter = 0;
}

RttyDemodSink::~RttyDemodSink()
{
    delete[] m_exp;
    delete[] m_prods1;
    delete[] m_prods2;
}

void RttyDemodSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        Real r = std::real(sample) * SDR_RX_SCALEF;
        Real i = std::imag(sample) * SDR_RX_SCALEF;
        m_sampleBuffer[m_sampleBufferIndex++] = Sample(r, i);

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<SampleVector::const_iterator> vbegin;
            vbegin.push_back(m_sampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void RttyDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void RttyDemodSink::processOneSample(Complex &ci)
{
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

    // Sum power while data is being received
    if (m_gotSOP)
    {
        m_rssiMagSqSum += magsq;
        m_rssiMagSqCount++;
    }

    ci /= SDR_RX_SCALEF;

    // Use FFT to estimate frequency shift
    m_fft->in()[m_fftCounter] = ci;
    m_fftCounter++;
    if (m_fftCounter == m_fftSize)
    {
        estimateFrequencyShift();
        m_fftCounter = 0;
    }

    // Correlate with expected mark and space frequencies
    Complex exp = m_exp[m_expIdx];
    m_expIdx = (m_expIdx + 1) % m_expLength;
    //Complex exp = m_exp[m_sampleIdx];
    //qDebug() << "IQ " << real(ci) << imag(ci);
    Complex corr1 = ci * exp;
    Complex corr2 = ci * std::conj(exp);

    // Filter
    Real abs1, abs2;
    Real abs1Filt, abs2Filt;
    if (m_settings.m_filter == RttyDemodSettings::LOWPASS)
    {
        // Low pass filter
        abs1Filt = abs1 = std::abs(m_lowpassComplex1.filter(corr1));
        abs2Filt = abs2 = std::abs(m_lowpassComplex2.filter(corr2));
    }
    else if (   (m_settings.m_filter == RttyDemodSettings::COSINE_B_1)
             || (m_settings.m_filter == RttyDemodSettings::COSINE_B_0_75)
             || (m_settings.m_filter == RttyDemodSettings::COSINE_B_0_5)
            )
    {
        // Rasised cosine filter
        abs1Filt = abs1 = std::abs(m_raisedCosine1.filter(corr1));
        abs2Filt = abs2 = std::abs(m_raisedCosine2.filter(corr2));
    }
    else
    {
        // Moving average

        // Calculating moving average (well windowed sum)
        Complex old1 = m_prods1[m_sampleIdx];
        Complex old2 = m_prods2[m_sampleIdx];
        m_prods1[m_sampleIdx] = corr1;
        m_prods2[m_sampleIdx] = corr2;
        m_sum1 += m_prods1[m_sampleIdx] - old1;
        m_sum2 += m_prods2[m_sampleIdx] - old2;
        m_sampleIdx = (m_sampleIdx + 1) % m_samplesPerBit;

        // Square signals (by calculating absolute value of complex signal)
        abs1 = std::abs(m_sum1);
        abs2 = std::abs(m_sum2);

        // Apply optional low-pass filter to try to avoid extra zero-crassings above the baud rate
        if (m_settings.m_filter == RttyDemodSettings::FILTERED_MAV)
        {
            abs1Filt = m_lowpass1.filter(abs1);
            abs2Filt = m_lowpass2.filter(abs2);
        }
        else
        {
            abs1Filt = abs1;
            abs2Filt = abs2;
        }
    }

    // Envelope calculation
    m_movMax1(abs1Filt);
    m_movMax2(abs2Filt);
    Real env1 = m_movMax1.getMaximum();
    Real env2 = m_movMax2.getMaximum();

    // Automatic threshold correction to compensate for frequency selective fading
    // http://www.w7ay.net/site/Technical/ATC/index.html
    Real bias1 = abs1Filt - 0.5 * env1;
    Real bias2 = abs2Filt - 0.5 * env2;
    Real unbiasedData = abs1Filt - abs2Filt;
    Real biasedData = bias1 - bias2;

    // Save current data for edge detection
    m_dataPrev = m_data;
    // Set data according to stongest correlation
    if (m_settings.m_spaceHigh) {
        m_data = m_settings.m_atc ? biasedData < 0 : unbiasedData < 0;
    } else {
        m_data = m_settings.m_atc ? biasedData > 0 : unbiasedData > 0;
    }

    if (!m_gotSOP)
    {
        // Look for falling edge which indicates start bit
        if (!m_data && m_dataPrev)
        {
            m_gotSOP = true;
            m_bits = 0;
            m_bitCount = 0;
            m_clockCount = 0;
            m_clock = false;
            m_cycleCount = 0;
        }
    }
    else
    {
        // Sample in middle of symbol
        if (m_clockCount == m_samplesPerBit/2)
        {
            receiveBit(m_data);
            m_clock = true;
        }
        m_clockCount = (m_clockCount + 1) % m_samplesPerBit;
        if (m_clockCount == 0) {
            m_clock = false;
        }

        // Count cycles between edges, to estimate baud rate
        m_cycleCount++;
        if (m_data != m_dataPrev)
        {
            if (m_cycleCount < m_clockHistogram.size())
            {
                m_clockHistogram[m_cycleCount]++;
                m_edgeCount++;

                // Every 100 edges, calculate estimate
                if (m_edgeCount == 100) {
                    estimateBaudRate();
                }
            }
            m_cycleCount = 0;
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
        scopeSample.real(magsq);
        break;
    case 3:
        scopeSample.real(m_sampleIdx);
        break;
    case 4:
        scopeSample.real(abs(m_sum1));
        break;
    case 5:
        scopeSample.real(abs(m_sum2));
        break;
    case 6:
        scopeSample.real(m_bit);
        break;
    case 7:
        scopeSample.real(m_bitCount);
        break;
    case 8:
        scopeSample.real(m_gotSOP);
        break;
    case 9:
        scopeSample.real(real(exp));
        break;
    case 10:
        scopeSample.real(imag(exp));
        break;
    case 11:
        scopeSample.real(abs1Filt);
        break;
    case 12:
        scopeSample.real(abs2Filt);
        break;
    case 13:
        scopeSample.real(abs2 - abs1);
        break;
    case 14:
        scopeSample.real(abs2Filt - abs1Filt);
        break;
    case 15:
        scopeSample.real(m_data);
        break;
    case 16:
        scopeSample.real(m_clock);
        break;
    case 17:
        scopeSample.real(env1);
        break;
    case 18:
        scopeSample.real(env2);
        break;
    case 19:
        scopeSample.real(bias1);
        break;
    case 20:
        scopeSample.real(bias2);
        break;
    case 21:
        scopeSample.real(unbiasedData);
        break;
    case 22:
        scopeSample.real(biasedData);
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
        scopeSample.imag(magsq);
        break;
    case 3:
        scopeSample.imag(m_sampleIdx);
        break;
    case 4:
        scopeSample.imag(abs(m_sum1));
        break;
    case 5:
        scopeSample.imag(abs(m_sum2));
        break;
    case 6:
        scopeSample.imag(m_bit);
        break;
    case 7:
        scopeSample.imag(m_bitCount);
        break;
    case 8:
        scopeSample.imag(m_gotSOP);
        break;
    case 9:
        scopeSample.imag(real(exp));
        break;
    case 10:
        scopeSample.imag(imag(exp));
        break;
    case 11:
        scopeSample.imag(abs1Filt);
        break;
    case 12:
        scopeSample.imag(abs2Filt);
        break;
    case 13:
        scopeSample.imag(abs2 - abs1);
        break;
    case 14:
        scopeSample.imag(abs2Filt - abs1Filt);
        break;
    case 15:
        scopeSample.imag(m_data);
        break;
    case 16:
        scopeSample.imag(m_clock);
        break;
    case 17:
        scopeSample.imag(env1);
        break;
    case 18:
        scopeSample.imag(env2);
        break;
    case 19:
        scopeSample.imag(bias1);
        break;
    case 20:
        scopeSample.imag(bias2);
        break;
    case 21:
        scopeSample.imag(unbiasedData);
        break;
    case 22:
        scopeSample.imag(biasedData);
        break;
    }
    sampleToScope(scopeSample);
}

void RttyDemodSink::estimateFrequencyShift()
{
    // Perform FFT
    m_fft->transform();
    // Calculate magnitude
    for (int i = 0; i < m_fftSize; i++)
    {
        Complex c = m_fft->out()[i];
        Real v = c.real() * c.real() + c.imag() * c.imag();
        Real magsq = v / (m_fftSize * m_fftSize);
        m_shiftEstMag[i] = magsq;
    }
    // Fink peaks in each half
    Real peak1 = m_shiftEstMag[0];
    int peak1Bin = 0;
    for (int i = 1; i < m_fftSize/2; i++)
    {
        if (m_shiftEstMag[i] > peak1)
        {
            peak1 = m_shiftEstMag[i];
            peak1Bin = i;
        }
    }
    Real peak2 = m_shiftEstMag[m_fftSize/2];
    int peak2Bin = m_fftSize/2;
    for (int i = m_fftSize/2+1; i < m_fftSize; i++)
    {
        if (m_shiftEstMag[i] > peak2)
        {
            peak2 = m_shiftEstMag[i];
            peak2Bin = i;
        }
    }
    // Convert bin to frequency offset
    double frequencyResolution = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / (double)m_fftSize;
    double freq1 = frequencyResolution * peak1Bin;
    double freq2 = -frequencyResolution * (m_fftSize - peak2Bin);
    m_freq1Average(freq1);
    m_freq2Average(freq2);
    //int shift = m_freq1Average.instantAverage() - m_freq2Average.instantAverage();
    //qDebug() << "Freq est " << freq1 << freq2 << shift;
}

int RttyDemodSink::estimateBaudRate()
{
    // Find most frequent entry in histogram
    auto histMax = max_element(m_clockHistogram.begin(), m_clockHistogram.end());
    int index = std::distance(m_clockHistogram.begin(), histMax);

    // Calculate baud rate as weighted average
    Real baud1 = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / (Real)(index-1);
    int count1 = m_clockHistogram[index-1];
    Real baud2 = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / (Real)(index);
    int count2 = m_clockHistogram[index];
    Real baud3 = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / (Real)(index+1);
    int count3 = m_clockHistogram[index+1];
    Real total = count1 + count2 + count3;
    Real estBaud = count1/total*baud1 + count2/total*baud2 + count3/total*baud3;
    m_baudRateAverage(estBaud);

    // Send estimate to GUI
    if (getMessageQueueToChannel())
    {
        int estFrequencyShift = m_freq1Average.instantAverage() - m_freq2Average.instantAverage();
        RttyDemod::MsgModeEstimate *msg = RttyDemod::MsgModeEstimate::create(m_baudRateAverage.instantAverage(), estFrequencyShift);
        getMessageQueueToChannel()->push(msg);
    }

    // Restart estimation
    std::fill(m_clockHistogram.begin(), m_clockHistogram.end(), 0);
    m_edgeCount = 0;

    return estBaud;
}

void RttyDemodSink::receiveBit(bool bit)
{
    m_bit = bit;

    // Store in shift reg.
    if (m_settings.m_msbFirst) {
        m_bits = (m_bit & 0x1) | (m_bits << 1);
    } else {
        m_bits = (m_bit << 6) | (m_bits >> 1);
    }
    m_bitCount++;

    if (m_bitCount == 7)
    {
        if (   (!m_settings.m_msbFirst && ((m_bits & 0x40) != 0x40))
            || (m_settings.m_msbFirst && ((m_bits & 0x01) != 0x01)))
        {
            //qDebug() << "No stop bit";
        }
        else
        {
            QString c = m_rttyDecoder.decode((m_bits >> 1) & 0x1f);
            if ((c != '\0') && (c != '<') && (c != '>') && (c != '^'))
            {
                // Calculate average power over received byte
                float rssi = CalcDb::dbPower(m_rssiMagSqSum / m_rssiMagSqCount);
                if (rssi > m_settings.m_squelch)
                {
                    // Slow enough to send individually to be displayed
                    if (getMessageQueueToChannel())
                    {
                        RttyDemod::MsgCharacter *msg = RttyDemod::MsgCharacter::create(c);
                        getMessageQueueToChannel()->push(msg);
                    }
                }
            }
        }
        m_gotSOP = false;
    }
}

void RttyDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RttyDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void RttyDemodSink::init()
{
    m_sampleIdx = 0;
    m_expIdx = 0;
    m_sum1 = 0.0;
    m_sum2 = 0.0;
    for (int i = 0; i < m_samplesPerBit; i++)
    {
        m_prods1[i] = 0.0f;
        m_prods2[i] = 0.0f;
    }
    m_bit = 0;
    m_bits = 0;
    m_bitCount = 0;
    m_gotSOP = false;
    m_clockCount = 0;
    m_clock = 0;
    m_rssiMagSqSum = 0.0;
    m_rssiMagSqCount = 0;
    m_rttyDecoder.init();
}

void RttyDemodSink::applySettings(const RttyDemodSettings& settings, bool force)
{
    qDebug() << "RttyDemodSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_baudRate: " << settings.m_baudRate
            << " m_frequencyShift: " << settings.m_frequencyShift
            << " m_characterSet: " << settings.m_characterSet
            << " m_unshiftOnSpace: " << settings.m_unshiftOnSpace
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_baudRate != m_settings.m_baudRate) || (settings.m_filter != m_settings.m_filter) || force)
    {
        m_envelope1.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, 2);
        m_envelope2.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, 2);
        m_lowpass1.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, m_settings.m_baudRate * 1.1);
        m_lowpass2.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, m_settings.m_baudRate * 1.1);
        //m_lowpass1.printTaps("lpf");

        m_lowpassComplex1.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, m_settings.m_baudRate * 1.1);
        m_lowpassComplex2.create(301, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE, m_settings.m_baudRate * 1.1);
        //m_lowpass1.printTaps("lpfc");

        // http://w7ay.net/site/Technical/Extended%20Nyquist%20Filters/index.html
        // http://w7ay.net/site/Technical/EqualizedRaisedCosine/index.html
        float beta = 1.0f;
        float bw = 1.0f;
        if (settings.m_filter == RttyDemodSettings::COSINE_B_0_5) {
            beta = 0.5f;
        } else if (settings.m_filter == RttyDemodSettings::COSINE_B_0_75) {
            beta = 0.75f;
        } else if (settings.m_filter == RttyDemodSettings::COSINE_B_1_BW_0_75) {
            bw = 0.75f;
        } else if (settings.m_filter == RttyDemodSettings::COSINE_B_1_BW_1_25) {
            bw = 1.25f;
        }
        m_raisedCosine1.create(beta, 7, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE/(m_settings.m_baudRate/bw), false);
        m_raisedCosine2.create(beta, 7, RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE/(m_settings.m_baudRate/bw), false);
        //m_raisedCosine1.printTaps("rcos");
    }

    if ((settings.m_characterSet != m_settings.m_characterSet) || force) {
        m_rttyDecoder.setCharacterSet(settings.m_characterSet);
    }
    if ((settings.m_unshiftOnSpace != m_settings.m_unshiftOnSpace) || force) {
        m_rttyDecoder.setUnshiftOnSpace(settings.m_unshiftOnSpace);
    }

    if ((settings.m_baudRate != m_settings.m_baudRate) || (settings.m_frequencyShift != m_settings.m_frequencyShift) || force)
    {
        delete[] m_exp;
        delete[] m_prods1;
        delete[] m_prods2;
        m_samplesPerBit = RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE / settings.m_baudRate;
        m_exp = new Complex[m_expLength];
        m_prods1 = new Complex[m_samplesPerBit];
        m_prods2 = new Complex[m_samplesPerBit];
        Real f0 = 0.0f;
        for (int i = 0; i < m_expLength; i++)
        {
            m_exp[i] = Complex(cos(f0), sin(f0));
            f0 += 2.0f * (Real)M_PI * (settings.m_frequencyShift/2.0f) / RttyDemodSettings::RTTYDEMOD_CHANNEL_SAMPLE_RATE;
        }
        init();

        // Due to start and stop bits, we should get mark and space at least every 8 bits
        // while something is being transmitted
        m_movMax1.setSize(m_samplesPerBit * 8);
        m_movMax2.setSize(m_samplesPerBit * 8);

        m_edgeCount = 0;
        std::fill(m_clockHistogram.begin(), m_clockHistogram.end(), 0);

        m_baudRateAverage.reset();
        m_freq1Average.reset();
        m_freq2Average.reset();
    }

    m_settings = settings;
}

