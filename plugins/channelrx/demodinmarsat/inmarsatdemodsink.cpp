///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "dsp/fftfactory.h"
#include "dsp/dspengine.h"
#include "util/db.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "inmarsatdemod.h"
#include "inmarsatdemodsink.h"

AGC::AGC() :
    m_gain(1.0f)
{
}

Complex AGC::processOneSample(const Complex& iq, bool locked)
{
    Complex z = m_gain * iq;

    m_agcMovingAverage(abs(z));
    //qDebug() << "abs" << abs(z) << "m_agcGain" << m_agcGain << "m_agcMovingAverage.instantAverage()" << m_agcMovingAverage.instantAverage();

    Real agcRef = 1.0f; // Target amplitude
    Real agcMu = locked ? 0.03 : 0.3; // How fast we want to respond to changes in average
    Real agcEr = agcRef - m_agcMovingAverage.instantAverage(); // Error between target and average

    m_gain += agcMu * agcEr;
    m_gain = std::clamp(m_gain, 0.01f, 10000.0f);

    return z;
}

FrequencyOffsetEstimate::FrequencyOffsetEstimate() :
    m_fftSequence(-1),
    m_fft(nullptr),
    m_fftCounter(0),
    m_fftSize(2048),    // 23Hz per bin - which is roughly pull in range of costas loop
    m_freqOffsetBin(-1),
    m_freqOffsetHz(0.0f),
    m_currentFreqOffsetHz(0.0f),
    m_freqMagSq(0.0f),
    m_currentFreqMagSq(0.0)
{
    FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
    m_fftSequence = fftFactory->getEngine(m_fftSize, false, &m_fft);
    m_fftCounter = 0;
    m_fftWindow.create(FFTWindow::Rectangle, m_fftSize);
}

FrequencyOffsetEstimate::~FrequencyOffsetEstimate()
{
    if (m_fftSequence >= 0)
    {
        FFTFactory *fftFactory = DSPEngine::instance()->getFFTFactory();
        fftFactory->releaseEngine(m_fftSize, false, m_fftSequence);
    }
}

void FrequencyOffsetEstimate::processOneSample(Complex& iq, bool locked)
{
    // Square signal to remove modulation
    Complex cSquared = iq * iq;

    // FFT of squared signal for coarse frequency offset estimation
    m_fft->in()[m_fftCounter] = cSquared;
    m_fftCounter++;
    if (m_fftCounter == m_fftSize)
    {
        // Apply windowing function
        m_fftWindow.apply(m_fft->in());

        // Perform FFT
        m_fft->transform();

        // Find peak
        int maxIdx = -1;
        Real maxVal = 0;
        for (int i = 0; i < m_fftSize; i++)
        {
            Complex co = m_fft->out()[i];
            Real v = co.real() * co.real()  + co.imag() * co.imag();
            Real magsq = v / (m_fftSize * m_fftSize);
            if (magsq > maxVal)
            {
                maxIdx = i;
                maxVal = magsq;
            }
        }

        // Calculate power for current peak bin and frequency we're locked too
        m_currentFreqMagSq = magSq(maxIdx);
        m_freqMagSq = magSq(m_freqOffsetBin);

        Real hzPerBin = InmarsatDemodSettings::CHANNEL_SAMPLE_RATE / (Real) m_fftSize;
        int idx = maxIdx;
        if (maxIdx > m_fftSize / 2) {
            idx -= m_fftSize; // Negative freqs are in second half
        }
        m_currentFreqOffsetHz = ((idx * hzPerBin) / 2); // Divide by two, as the squaring operation doubles the freq
        //qDebug() << "Max val" << maxVal << "maxIdx" << maxIdx << "freqOffset" << m_freqOffsetHz;

        Real magRatio = sqrt(m_currentFreqMagSq) / sqrt(m_freqMagSq);

        // Don't change frequency if Costas loop is locked, unless there's a big
        // signal more than 2 bins away. Costas loop locked signal can falsely indicate lock
        // if offset is multiple of data rate.
        if (!locked || ((magRatio >= 3) && (abs(maxIdx - m_freqOffsetBin) >= 2)))
        {
            if (m_freqOffsetBin != maxIdx) // Only update if changed, to reduce NCO debug messages
            {
                m_freqOffsetBin = maxIdx;
                m_freqOffsetHz = m_currentFreqOffsetHz;
                m_freqOffsetNCO.setFreq(-m_freqOffsetHz, InmarsatDemodSettings::CHANNEL_SAMPLE_RATE);
                //qDebug() << "Setting CFO" << m_freqOffsetHz << "(" << CalcDb::dbPower(m_currentFreqMagSq) << "/" << CalcDb::dbPower(m_freqMagSq) << ")";
            }
        }

        m_fftCounter = 0;
    }

    // Correct for coarse frequency offset
    iq *= m_freqOffsetNCO.nextIQ();
}

Real FrequencyOffsetEstimate::magSq(int bin) const
{
    Complex c = m_fft->out()[bin];
    Real v = c.real() * c.real() + c.imag() * c.imag();
    return v / (m_fftSize * m_fftSize);
}

SymbolBuffer::SymbolBuffer(int size) :
    m_totalError(0),
    m_idx(0),
    m_count(0)
{
    m_bits.resize(size);
    m_symbols.resize(size);
    m_error.resize(size);
}

void SymbolBuffer::push(quint8 bit, Complex symbol)
{
    m_bits[m_idx] = bit;
    m_symbols[m_idx] = symbol;

    m_totalError -= m_error[m_idx];
    Complex error;
    if (bit) {
        error = symbol - Complex(1.0f, 0.0f);
    } else {
        error = symbol - Complex(-1.0f, 0.0f);
    }
    m_error[m_idx] = error.real() * error.real() + error.imag() * error.imag();
    m_totalError += m_error[m_idx];

    m_idx = (m_idx + 1) % m_bits.size();
    if (m_count <  m_bits.size()) {
        m_count++;
    }
}

bool SymbolBuffer::checkUW() const
{
    qulonglong bits1 = 0;
    qulonglong bits2 = 0;

    for (int i = 0; i < 64; i++)
    {
        int idx1 = (m_idx + i * 162) % m_bits.size();
        int idx2 = (m_idx + i * 162 + 1) % m_bits.size();
        bits1 |= (qulonglong)m_bits[idx1] << (63-i);
        bits2 |= (qulonglong)m_bits[idx2] << (63-i);
    }
    return   (   (bits1 == 0x07eacdda4e2f28c2)
              || (bits1 == ~0x07eacdda4e2f28c2)
             )
          && (bits1 == bits2);
}

Complex SymbolBuffer::getSymbol(int idx) const
{
    return m_symbols[(m_idx + idx) % m_bits.size()];
}

// Calculate RMS EVM
float SymbolBuffer::evm() const
{
    return sqrt((float) (m_totalError / m_count));
}

Equalizer::Equalizer(int samplesPerSymbol) :
    m_samplesPerSymbol(samplesPerSymbol),
    m_idx(0)
{
    int taps = m_samplesPerSymbol * 8 + 1;
    m_delay.resize(taps);
    m_taps.resize(taps);
    for (int i = 0; i < taps; i++)
    {
        m_delay[i] = 0.0f;
        m_taps[i] = 0.0f;
    }
}

void Equalizer::printTaps() const
{
    printf("taps=[");
    for (int i = 0; i < m_taps.size(); i++) {
        printf("%f+i*%f ", m_taps[i].real(), m_taps[i].imag());
    }
    printf("];\n");
}

CMAEqualizer::CMAEqualizer(int samplesPerSymbol) :
    Equalizer(samplesPerSymbol)
{
    m_taps[m_delay.size()/2] = 1.0f;
}

Complex CMAEqualizer::processOneSample(Complex x, bool update, bool training)
{
    (void) training;

    for (int i = m_taps.size() - 1; i >  0; i--) {
        m_delay[i] = m_delay[i-1];
    }
    m_delay[0] = x;

    Complex y = 0;
    for (int i = 0; i < m_taps.size(); i++) {
        y += std::conj(m_taps[i]) * m_delay[i];
    }

    Real mod = abs(y);
    Real R = 1.0f;
    Real error = mod * mod - R * R;
    //printf("x=%f+i%f y=%f+i%f d=%f error=%f+i%f\n", x.real(), x.imag(), y.real(), y.imag(), d.real(), m_error.real(), m_error.imag());
    if (update)
    {
        Real mu = 0.001f;
        for (int l = 0; l < m_taps.size(); l++) {
            m_taps[l] = m_taps[l] - mu * std::conj(y) * m_delay[l] * error;
        }
    }
    m_error = error;

    return y;
}

LMSEqualizer::LMSEqualizer(int samplesPerSymbol) :
    Equalizer(samplesPerSymbol)
{
}

Complex LMSEqualizer::processOneSample(Complex x, bool update, bool training)
{
    for (int i = m_taps.size() - 1; i > 0; i--) {
        m_delay[i] = m_delay[i-1];
    }
    m_delay[0] = x;

    Complex y = 0;
    for (int i = 0; i < m_taps.size(); i++) {
        y += std::conj(m_taps[i]) * m_delay[i]; // Could avoid conj here, by instead using mu * std::conj(m_delay[l]) * m_error below.
    }

    Complex d = (training ? x.real() : y.real()) >= 0.0f ? Complex(1.0f, 0.0f) : Complex(-1.0f, 0.0f);
    m_error = d - y;
    if (update)
    {
        Real mu = 0.1f;
        for (int l = 0; l < m_taps.size(); l++) {
            m_taps[l] = m_taps[l] + mu * m_delay[l] * std::conj(m_error);
        }
    }

    return y;
}

InmarsatDemodSink::InmarsatDemodSink(InmarsatDemod *stdCDemod) :
    m_scopeSink(nullptr),
    m_inmarsatDemod(stdCDemod),
    m_channelSampleRate(InmarsatDemodSettings::CHANNEL_SAMPLE_RATE),
    m_channelFrequencyOffset(0),
    m_magsqSum(0.0f),
    m_magsqPeak(0.0f),
    m_magsqCount(0),
    m_messageQueueToChannel(nullptr),
    m_decoder(9),
    m_symbolCounter(0),
    m_syncedToUW(false),
    m_equalizer(nullptr),
    m_costasLoop(10*2*M_PI/100.0, 2),
    m_rrcBufferIndex(0),
    m_sampleBufferIndexA(0),
    m_sampleBufferIndexB(0)
{
    m_magsq = 0.0;

    m_rrcFilter = new fftfilt(m_settings.m_rfBandwidth / (float) m_channelSampleRate, RRC_FILTER_SIZE);

    m_rrcI.create(m_settings.m_rrcRolloff, 5, SAMPLES_PER_SYMBOL, RootRaisedCosine<Real>::Gain);
    m_rrcQ.create(m_settings.m_rrcRolloff, 5, SAMPLES_PER_SYMBOL, RootRaisedCosine<Real>::Gain);

    m_sampleIdx = 0;

    m_adjustedSPS = m_maxSamplesPerSymbol; // ok: 20, 15,  fail: 10, 9, 5
    m_adjustment = 0;
    m_totalSampleCount = 0;
    m_error = 0;
    m_errorSum = 0;
    m_mu = 0.0;
    m_bit = 0;
    m_bitCount = 0;
    m_filteredSamples.enqueue(0); // Prev sample for first iteration

    for (int i = 0; i < InmarsatDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(SAMPLE_BUFFER_SIZE);
    }

    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

InmarsatDemodSink::~InmarsatDemodSink()
{
    delete m_rrcFilter;
    m_rrcFilter = nullptr;
    delete m_equalizer;
    m_equalizer = nullptr;
}

void InmarsatDemodSink::sampleToScopeA(Complex sample, Real magsq, Complex postAGC, Real agcGain, Real agcAvg, Complex postCFO)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndexA] = sample;
        m_sampleBuffer[1][m_sampleBufferIndexA] = Complex(magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndexA] = postAGC;
        m_sampleBuffer[3][m_sampleBufferIndexA] = Complex(agcGain, agcAvg);
        m_sampleBuffer[4][m_sampleBufferIndexA] = postCFO;
        m_sampleBufferIndexA++;
    }
}

void InmarsatDemodSink::sampleToScopeB(Complex rrc, Complex tedError, Complex tedErrorSum, Complex ref, Complex derot, Complex eq, Complex eqError, int bit, Real mu)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[5][m_sampleBufferIndexB] = rrc;
        m_sampleBuffer[6][m_sampleBufferIndexB] = tedError;
        m_sampleBuffer[7][m_sampleBufferIndexB] = tedErrorSum;
        m_sampleBuffer[8][m_sampleBufferIndexB] = ref;
        m_sampleBuffer[9][m_sampleBufferIndexB] = derot;
        m_sampleBuffer[10][m_sampleBufferIndexB] = eq;
        m_sampleBuffer[11][m_sampleBufferIndexB] = eqError;
        m_sampleBuffer[12][m_sampleBufferIndexB] = Complex(bit, mu);
        m_sampleBufferIndexB++;
        if (m_sampleBufferIndexB == SAMPLE_BUFFER_SIZE)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < InmarsatDemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, SAMPLE_BUFFER_SIZE);
            if (m_sampleBufferIndexA != m_sampleBufferIndexB) {
                qDebug() << "m_sampleBufferIndexA != m_sampleBufferIndexB" << m_sampleBufferIndexA << m_sampleBufferIndexB;
            }
            m_sampleBufferIndexA = 0;
            m_sampleBufferIndexB = 0;
        }
    }
}

void InmarsatDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

// Wrap to range [-0.5, 0.5)
static Real wrap(Real v)
{
    if (v > 0.0f) {
        return fmod(v + 0.5f, 1.0f) - 0.5f;
    } else {
        return fmod(v - 0.5f, 1.0f) + 0.5f;
    }
}

void InmarsatDemodSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // AGC

    Complex cScaled = Complex(re, im);
    Complex agcZ = m_agc.processOneSample(cScaled, m_locked);

    // Coarse frequency offset correction

    Complex cCFO = agcZ;
    m_frequencyOffsetEst.processOneSample(cCFO, m_locked);

    // Send signals to scope that are updated for each invocation of this method
    sampleToScopeA(cScaled, magsq, agcZ, m_agc.getGain(), m_agc.getAverage(), cCFO);

    // RRC Matched filter
    fftfilt::cmplx *rrcFilterOut = nullptr;
    int n_out = m_rrcFilter->runFilt(cCFO, &rrcFilterOut);

    m_rrcBuffer[m_rrcBufferIndex++] = cCFO;

    if (m_rrcBufferIndex == RRC_FILTER_SIZE/2)
    {
        n_out = m_rrcBufferIndex;
        m_rrcBufferIndex = 0;
    }
    else
    {
        n_out = 0;
    }

    for (int i = 0; i < n_out; i++)
    {
        //Complex rrc = rrcFilterOut[i];
        Complex rrc (m_rrcI.filter(m_rrcBuffer[i].real()), m_rrcQ.filter(m_rrcBuffer[i].imag()));

        // Symbol synchronizer

        m_totalSampleCount++;
        m_filteredSamples.enqueue(rrc);
        while (m_filteredSamples.size() > m_maxSamplesPerSymbol)
        {
            m_filteredSamples.dequeue();

            if (m_sampleIdx == m_adjustedSPS - 1)
            {
                // Gardner timing error detector

                int currentIdx = m_filteredSamples.size()-1;
                int midIdx = currentIdx - (SAMPLES_PER_SYMBOL/2.0f);
                int previousIdx = currentIdx - SAMPLES_PER_SYMBOL;

                //qDebug() << "diff" << (m_prevTotalSampleCount - m_totalSampleCount);
                //qDebug() << "m_totalSampleCount" << m_totalSampleCount << "m_prevTotalSampleCount" << m_prevTotalSampleCount << "diff" << (m_prevTotalSampleCount - m_totalSampleCount) << "currentIdx" << currentIdx << "previousIdx" << previousIdx << "midIdx" << midIdx << "mu" << m_mu;

                Complex previous = m_filteredSamples[previousIdx];
                Complex mid = m_filteredSamples[midIdx];
                Complex current = m_filteredSamples[currentIdx];

                m_error = (current.real() - previous.real()) * mid.real()
                    + (current.imag() - previous.imag()) * mid.imag();
                m_errorSum += m_error;

                // Symbol synchronizer adjustment

                m_mu = m_kp * m_error + m_ki * m_errorSum; // Positive error is late
                m_mu = wrap(m_mu);

                int adjustment = (int)round(m_mu * SAMPLES_PER_SYMBOL);

                //qDebug() << "m_mu" << m_mu << "m_error" << m_error << "m_errorSum" << m_errorSum << "adjustment" << adjustment;

                m_adjustedSPS = SAMPLES_PER_SYMBOL - m_adjustment; // Positve mu indicates late, so reduce time to next sample
                m_adjustment = adjustment;
                m_prevTotalSampleCount = m_totalSampleCount;

                // Costas loop for fine phase/freq correction - runs at symbol rate

                m_costasLoop.feed(current.real(), current.imag());
                m_ref = -std::conj(m_costasLoop.getComplex());
                m_derot = current * m_ref;

                // Determine whether Costas loop is locked - BPSK so Q part should average as zero when locked, and will be similar to I when not locked
                Real derotMag = abs(m_derot);
                Real iNorm = abs(m_derot.real()) / derotMag;
                Real qNorm = abs(m_derot.imag()) / derotMag;
                m_lockAverage(iNorm - qNorm);
                float lockThresh = 0.45;
                m_locked = m_lockAverage.instantAverage() > lockThresh;
                //qDebug() << "m_lockAverage.instantAverage()" << m_lockAverage.instantAverage();

                // Equalizer (runs at symbol rate)
                if (   (m_settings.m_equalizer == InmarsatDemodSettings::CMA)
                    || (m_settings.m_equalizer == InmarsatDemodSettings::LMS && m_syncedToUW)
                   )
                {
                    m_eq = m_equalizer->processOneSample(m_derot, true, false);
                } else {
                    m_eq = m_derot;
                }

                // Symbol to bit
                m_bit = m_eq.real() >= 0;
                m_bits[m_bitCount++] = m_bit;

                // Look for Unique Words to synchronize to start of frame
                m_symbolBuffer.push(m_bit, m_eq);
                if (m_symbolBuffer.checkUW())
                {
                    if (m_syncedToUW && (m_symbolCounter != m_symbolBuffer.size() - 1)) {
                        qDebug() << "Already synced" << m_symbolCounter << m_symbolBuffer.size();
                    }
                    m_symbolCounter = 0;
                    if (!m_syncedToUW && (m_settings.m_equalizer == InmarsatDemodSettings::LMS))
                    {
                        // Train equalizer on unique word symbols
                        for (int i = 0; i < 64; i++) {
                            m_equalizer->processOneSample(m_symbolBuffer.getSymbol(i * 162), true, true);
                        }
                        for (int i = 0; i < 64; i++) {
                            m_equalizer->processOneSample(m_symbolBuffer.getSymbol(i * 162 + 1), true, true);
                        }
                    }
                    m_syncedToUW = true;
                }
                else if (m_syncedToUW)
                {
                    // Allow 3 frames without UW before assuming sync loss (only currently effects whether we run LMS eq)
                    // Could allow a certain number of bit errors in Unique Words instead
                    if (m_symbolCounter < 3 * m_symbolBuffer.size())
                    {
                        m_symbolCounter++;
                    }
                    else
                    {
                        m_symbolCounter = 0;
                        m_syncedToUW = false;
                    }
                }

                // Pass demodulated bits to decoder
                if (m_bitCount == DEMODULATOR_SYMBOLSPERCHUNK)
                {
                    decodeBits();
                    m_bitCount = 0;
                }

                // Calculate EVM
                m_evm = m_symbolBuffer.evm();

                m_sampleIdx = 0;
            }
            else
            {
                m_sampleIdx++;
            }
        }

        // Send signals to scope that are updated only whenever the RRC filter output is updated
        sampleToScopeB(rrc, m_error, m_errorSum, m_ref, m_derot, m_eq, m_equalizer ? m_equalizer->getError() : Complex(0.0), m_bit*2-1, m_mu);
    }
}

void InmarsatDemodSink::decodeBits()
{
    std::vector<inmarsatc::decoder::Decoder::decoder_result> decoderResults;

    decoderResults = m_decoder.decode(m_bits);

    if (decoderResults.size() > 0)
    {
        for (int j = 0; j < (int) decoderResults.size(); j++)
        {
            QByteArray rxPacket;
            rxPacket.resize(sizeof(inmarsatc::decoder::Decoder::decoder_result));
            memcpy(rxPacket.data(), &decoderResults[j], sizeof(inmarsatc::decoder::Decoder::decoder_result));

            if (getMessageQueueToChannel())
            {
                QDateTime dateTime = QDateTime::currentDateTime();
                if (m_settings.m_useFileTime)
                {
                    QString hardwareId = m_inmarsatDemod->getDeviceAPI()->getHardwareId();

                    if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
                    {
                        QString dateTimeStr;
                        int deviceIdx = m_inmarsatDemod->getDeviceSetIndex();

                        if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr)) {
                            dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);
                        }
                    }
                }

                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_inmarsatDemod, rxPacket, dateTime);
                getMessageQueueToChannel()->push(msg);
            }
        }
    }
}

void InmarsatDemodSink::getPLLStatus(bool &locked, Real &coarseFreqCurrent, Real &coarseFreqCurrentPower, Real &coarseFreq, Real &coarseFreqPower, Real &fineFreq, Real &evm, bool &synced) const
{
    locked = m_locked;
    coarseFreqCurrent = m_frequencyOffsetEst.getCurrentFreqHz();
    coarseFreqCurrentPower = m_frequencyOffsetEst.getCurrentFreqMagSq();
    coarseFreq = m_frequencyOffsetEst.getFreqHz();
    coarseFreqPower = m_frequencyOffsetEst.getFreqMagSq();
    fineFreq = m_costasLoop.getFreq() * COSTAS_LOOP_RATE / (2 * M_PI);
    evm = m_evm;
    synced = m_syncedToUW;
}

void InmarsatDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "InmarsatDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) InmarsatDemodSettings::CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void InmarsatDemodSink::applySettings(const InmarsatDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "InmarsatDemodSink::applySettings:"
        << settings.getDebugString(settingsKeys, force)
        << " force: " << force;

    if (settingsKeys.contains("rfBandwidth") || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) InmarsatDemodSettings::CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;

        // Limit costas loop frequency range to stay within RF bandwidth - it probably can't track this far anyway
        Real freqMaxHz = (settings.m_rfBandwidth / 2.0f - InmarsatDemodSettings::BAUD_RATE / 2.0f);
        //qDebug() << "InmarsatDemodSink::applySettings: Costas loop freq limit: +/-" << freqMaxHz << "Hz";
        Real freqMax = freqMaxHz * 2 * M_PI / COSTAS_LOOP_RATE;
        m_costasLoop.setMaxFreq(freqMax);
        m_costasLoop.setMinFreq(-freqMax);
    }

    if (settingsKeys.contains("rfBandwidth") || settingsKeys.contains("rrcRolloff") || force) {
        m_rrcFilter->create_rrc_filter(settings.m_rfBandwidth / (float) m_channelSampleRate, settings.m_rrcRolloff);
    }

    if (settingsKeys.contains("pllBandwidth") || force) {
        m_costasLoop.computeCoefficients(settings.m_pllBW);
    }

    if (settingsKeys.contains("ssBandwidth") || force)
    {
        /*
        float kd = 1.5f; // TED gain
        float zeta = 1.0f / sqrtf(2.0f); // Damping factor - critical damping
        float theta = (m_settings.m_ssBW / SAMPLES_PER_SYMBOL) / (zeta + (1.0f / (4.0f * zeta)));
        float denom = ((1.0f + 2.0f * zeta * theta + theta * theta) * kd);

        m_kp = (4.0f * zeta * theta) / denom;
        m_ki = (4.0f * theta * theta) / denom;

        qDebug() << "m_kp" << m_kp << "m_ki" << m_ki;
        */

        m_kp = 0.1f;
        m_ki = 0.01f;
    }

    if (settingsKeys.contains("equalizer") || force)
    {
        delete m_equalizer;
        m_equalizer = nullptr;
        if (settings.m_equalizer == InmarsatDemodSettings::CMA) {
            m_equalizer = new CMAEqualizer(1);
        } else if (settings.m_equalizer == InmarsatDemodSettings::LMS) {
            m_equalizer = new LMSEqualizer(1);
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
