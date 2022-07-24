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
#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "util/stepfunctions.h"
#include "util/reedsolomon.h"
#include "maincore.h"

#include "radiosondedemod.h"
#include "radiosondedemodsink.h"

const uint8_t RadiosondeDemodSink::m_descramble[64] = {
    0x96, 0x83, 0x3E, 0x51, 0xB1, 0x49, 0x08, 0x98,
    0x32, 0x05, 0x59, 0x0E, 0xF9, 0x44, 0xC6, 0x26,
    0x21, 0x60, 0xC2, 0xEA, 0x79, 0x5D, 0x6D, 0xA1,
    0x54, 0x69, 0x47, 0x0C, 0xDC, 0xE8, 0x5C, 0xF1,
    0xF7, 0x76, 0x82, 0x7F, 0x07, 0x99, 0xA2, 0x2C,
    0x93, 0x7C, 0x30, 0x63, 0xF5, 0x10, 0x2E, 0x61,
    0xD0, 0xBC, 0xB4, 0xB6, 0x06, 0xAA, 0xF4, 0x23,
    0x78, 0x6E, 0x3B, 0xAE, 0xBF, 0x7B, 0x4C, 0xC1
};

RadiosondeDemodSink::RadiosondeDemodSink(RadiosondeDemod *radiosondeDemod) :
        m_scopeSink(nullptr),
        m_radiosondeDemod(radiosondeDemod),
        m_channelSampleRate(RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_rxBuf(nullptr),
        m_train(nullptr),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    m_sampleBuffer.resize(m_sampleBufferSize);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

RadiosondeDemodSink::~RadiosondeDemodSink()
{
    delete[] m_rxBuf;
    delete[] m_train;
}

void RadiosondeDemodSink::sampleToScope(Complex sample)
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

void RadiosondeDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void RadiosondeDemodSink::processOneSample(Complex &ci)
{
    // FM demodulation
    double magsqRaw;
    Real deviation;
    Real fmDemod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

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

    // Gaussian filter
    Real filt = m_pulseShape.filter(fmDemod);

    // An input frequency offset corresponds to a DC offset after FM demodulation
    // What frequency offset is RS41 specified too?
    // We need to remove this, otherwise it may effect the sampling
    // To calculate what it is, we sum the training sequence, which should be zero

    // Clip, as large noise can result in high correlation
    // Don't clip to 1.0 - as there may be some DC offset (1k/4.8k max dev=0.2)
    Real filtClipped;
    filtClipped = std::fmax(-1.4, std::fmin(1.4, filt));

    // Buffer filtered samples. We buffer enough samples for a max length message
    // before trying to demod, so false triggering can't make us miss anything
    m_rxBuf[m_rxBufIdx] = filtClipped;
    m_rxBufIdx = (m_rxBufIdx + 1) % m_rxBufLength;
    m_rxBufCnt = std::min(m_rxBufCnt + 1, m_rxBufLength);

    Real corr = 0.0f;
    bool scopeCRCValid = false;
    bool scopeCRCInvalid = false;
    Real dcOffset = 0.0f;
    bool thresholdMet = false;
    bool gotSOP = false;

    if ((m_rxBufCnt >= m_rxBufLength))
    {
        // Correlate with training sequence
        corr = correlate(m_rxBufIdx);

        // If we meet threshold, try to demod
        // Take abs value, to account for both initial phases
        thresholdMet = fabs(corr) >= m_settings.m_correlationThreshold;
        if (thresholdMet)
        {
            // Try to see if starting at a later sample improves correlation
            int maxCorrOffset = 0;
            Real maxCorr;
            do
            {
                maxCorr = fabs(corr);
                maxCorrOffset++;
                corr = correlate(m_rxBufIdx + maxCorrOffset);
            }
            while (fabs(corr) > maxCorr);
            maxCorrOffset--;

            // Calculate mean of preamble as DC offset (as it should be 0 on an ideal signal)
            Real trainingSum = 0.0f;
            for (int i = 0; i < m_correlationLength; i++)
            {
                int j = (m_rxBufIdx + maxCorrOffset + i) % m_rxBufLength;
                trainingSum += m_rxBuf[j];
            }
            dcOffset = trainingSum/m_correlationLength;

            // Start demod after (most of) preamble
            int x = (m_rxBufIdx + maxCorrOffset + m_correlationLength*3/4 + 0) % m_rxBufLength;

            // Attempt to demodulate
            uint64_t bits = 0;
            int bitCount = 0;
            int byteCount = 0;
            QList<int> sampleIdxs;
            for (int sampleIdx = 0; sampleIdx < m_rxBufLength; sampleIdx += m_samplesPerSymbol)
            {
                // Sum and slice
                // Summing 3 samples seems to give a very small improvement vs just using 1
                int sampleCnt = 3;
                int sampleOffset = -1;
                Real sampleSum = 0.0f;
                for (int i = 0; i < sampleCnt; i++) {
                    sampleSum += m_rxBuf[(x + sampleOffset + i) % m_rxBufLength] - dcOffset;
                    sampleIdxs.append((x + sampleOffset + i) % m_rxBufLength);
                }
                int symbol = sampleSum >= 0.0f ? 1 : 0;

                // Move to next symbol
                x = (x + m_samplesPerSymbol) % m_rxBufLength;

                // Symbols map directly to bits
                int bit = symbol;

                // Store in shift reg (little endian)
                bits |= ((uint64_t)bit) << bitCount;
                bitCount++;

                if (gotSOP)
                {
                    if (bitCount == 8)
                    {
                        // Got a complete byte
                        m_bytes[byteCount] = bits;
                        byteCount++;
                        bits = 0;
                        bitCount = 0;

                        if (byteCount >= RS41_LENGTH_STD)
                        {
                            // Get expected length of frame
                            uint8_t frameType = m_bytes[RS41_OFFSET_FRAME_TYPE] ^ m_descramble[RS41_OFFSET_FRAME_TYPE];
                            int length = RS41Frame::getFrameLength(frameType);

                            // Have we received a complete frame?
                            if (byteCount == length)
                            {
                                bool ok = processFrame(length, corr, sampleIdx);
                                scopeCRCValid = ok;
                                scopeCRCInvalid = !ok;
                                break;
                            }
                        }
                    }
                }
                else if (bits == 0xf812962211cab610ULL) // Scrambled header
                {
                    // Start of packet
                    gotSOP = true;
                    bits = 0;
                    bitCount = 0;
                    m_bytes[0] = 0x10;
                    m_bytes[1] = 0xb6;
                    m_bytes[2] = 0xca;
                    m_bytes[3] = 0x11;
                    m_bytes[4] = 0x22;
                    m_bytes[5] = 0x96;
                    m_bytes[6] = 0x12;
                    m_bytes[7] = 0xf8;
                    byteCount = 8;
                }
                else
                {
                    if (bitCount == 64)
                    {
                        bits >>= 1;
                        bitCount--;
                    }
                    if (sampleIdx >= 16 * 8 * m_samplesPerSymbol)
                    {
                        // Too many bits without receving header
                        break;
                    }
                }
            }
          //  printf("\n");
        }
    }

    // Select signals to feed to scope
    Complex scopeSample;
    switch (m_settings.m_scopeCh1)
    {
    case 0:
        scopeSample.real(ci.real() / SDR_RX_SCALEF);
        break;
    case 1:
        scopeSample.real(ci.imag() / SDR_RX_SCALEF);
        break;
    case 2:
        scopeSample.real(magsq);
        break;
    case 3:
        scopeSample.real(fmDemod);
        break;
    case 4:
        scopeSample.real(filt);
        break;
    case 5:
        scopeSample.real(m_rxBuf[m_rxBufIdx]);
        break;
    case 6:
        scopeSample.real(corr / 100.0);
        break;
    case 7:
        scopeSample.real(thresholdMet);
        break;
    case 8:
        scopeSample.real(gotSOP);
        break;
    case 9:
        scopeSample.real(dcOffset);
        break;
    case 10:
        scopeSample.real(scopeCRCValid ? 1.0 : (scopeCRCInvalid ? -1.0 : 0));
        break;
    }
    switch (m_settings.m_scopeCh2)
    {
    case 0:
        scopeSample.imag(ci.real() / SDR_RX_SCALEF);
        break;
    case 1:
        scopeSample.imag(ci.imag() / SDR_RX_SCALEF);
        break;
    case 2:
        scopeSample.imag(magsq);
        break;
    case 3:
        scopeSample.imag(fmDemod);
        break;
    case 4:
        scopeSample.imag(filt);
        break;
    case 5:
        scopeSample.imag(m_rxBuf[m_rxBufIdx]);
        break;
    case 6:
        scopeSample.imag(corr / 100.0);
        break;
    case 7:
        scopeSample.imag(thresholdMet);
        break;
    case 8:
        scopeSample.imag(gotSOP);
        break;
    case 9:
        scopeSample.imag(dcOffset);
        break;
    case 10:
        scopeSample.imag(scopeCRCValid ? 1.0 : (scopeCRCInvalid ? -1.0 : 0));
        break;
    }
    sampleToScope(scopeSample);

    // Send demod signal to Demod Analzyer feature
    m_demodBuffer[m_demodBufferFill++] = fmDemod * std::numeric_limits<int16_t>::max();

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        if (dataPipes.size() > 0)
        {
            QList<ObjectPipe*>::iterator it = dataPipes.begin();

            for (; it != dataPipes.end(); ++it)
            {
                DataFifo *fifo = qobject_cast<DataFifo*>((*it)->m_element);

                if (fifo) {
                    fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
                }
            }
        }

        m_demodBufferFill = 0;
    }
}

// Correlate received signal with training sequence
// Note that DC offset doesn't matter for this
Real RadiosondeDemodSink::correlate(int idx) const
{
    Real corr = 0.0f;
    for (int i = 0; i < m_correlationLength; i++)
    {
        int j = (idx + i) % m_rxBufLength;
        corr += m_train[i] * m_rxBuf[j];
    }
    return corr;
}

bool RadiosondeDemodSink::processFrame(int length, float corr, int sampleIdx)
{
    // Descramble
    for (int i = 0; i < length; i++) {
        m_bytes[i] = m_bytes[i] ^ m_descramble[i & 0x3f];
    }

    // Reed-Solomon Error Correction
    int errorsCorrected = reedSolomonErrorCorrection();
    if (errorsCorrected >= 0)
    {
        // Check per-block CRCs are correct
        if (checkCRCs(length))
        {
            if (getMessageQueueToChannel())
            {
                QByteArray rxPacket((char *)m_bytes, length);
                RadiosondeDemod::MsgMessage *msg = RadiosondeDemod::MsgMessage::create(rxPacket, errorsCorrected, corr);
                getMessageQueueToChannel()->push(msg);
            }

            // Skip over received packet, so we don't try to re-demodulate it
            m_rxBufCnt -= sampleIdx;
            return true;
        }
    }
    return false;
}

// Reed Solomon error correction
// Returns number of errors corrected, or -1 if there are uncorrectable errors
int RadiosondeDemodSink::reedSolomonErrorCorrection()
{
    ReedSolomon::RS<RS41_RS_N,RS41_RS_K> rs;
    int errorsCorrected = 0;

    for (int i = 0; (i < RS41_RS_INTERLEAVE) && (errorsCorrected >= 0); i++)
    {
        // Deinterleave and reverse order
        uint8_t rsData[RS41_RS_N];

        memset(rsData, 0, RS41_RS_PAD);
        for (int j = 0; j < RS41_RS_DATA; j++) {
            rsData[RS41_RS_K-1-j] = m_bytes[RS41_OFFSET_FRAME_TYPE+j*RS41_RS_INTERLEAVE+i];
        }
        for (int j = 0; j < RS41_RS_2T; j++) {
            rsData[RS41_RS_N-1-j] = m_bytes[RS41_OFFSET_RS+i*RS41_RS_2T+j];
        }

        // Detect and correct errors
        int errors = rs.decode(&rsData[0], RS41_RS_K);    // FIXME: Indicate 0 padding?
        if (errors >= 0) {
            errorsCorrected += errors;
        } else {
            // Uncorrectable errors
            return -1;
        }

        // Restore corrected data
        for (int j = 0; j < RS41_RS_DATA; j++) {
            m_bytes[RS41_OFFSET_FRAME_TYPE+j*RS41_RS_INTERLEAVE+i] = rsData[RS41_RS_K-1-j];
        }

    }
    return errorsCorrected;
}

// Check per-block CRCs
// We could pass partial frames that have some correct CRCs, but for now, whole frame has to be correct
bool RadiosondeDemodSink::checkCRCs(int length)
{
    for (int i = RS41_OFFSET_BLOCK_0; i < length; )
    {
        uint8_t blockLength = m_bytes[i+1];
        uint16_t rxCrc = m_bytes[i+2+blockLength] | (m_bytes[i+2+blockLength+1] << 8);
        // CRC doesn't include ID/len - so these can be wrong
        m_crc.init();
        m_crc.calculate(&m_bytes[i+2], blockLength);
        uint16_t calcCrc = m_crc.get();
        if (calcCrc != rxCrc) {
            return false;
        }
        i += blockLength+4;
    }
    return true;
}

void RadiosondeDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RadiosondeDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_samplesPerSymbol = RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE / m_settings.m_baud;
    qDebug() << "RadiosondeDemodSink::applyChannelSettings: m_samplesPerSymbol: " << m_samplesPerSymbol;
}

void RadiosondeDemodSink::applySettings(const RadiosondeDemodSettings& settings, bool force)
{
    qDebug() << "RadiosondeDemodSink::applySettings:"
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
        m_lowpass.create(301, RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE, settings.m_rfBandwidth / 2.0f);
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    if ((settings.m_baud != m_settings.m_baud) || force)
    {
        m_samplesPerSymbol = RadiosondeDemodSettings::RADIOSONDEDEMOD_CHANNEL_SAMPLE_RATE / settings.m_baud;
        qDebug() << "RadiosondeDemodSink::applySettings: m_samplesPerSymbol: " << m_samplesPerSymbol << " baud " << settings.m_baud;

        // What value to use for BT? RFIC is Si4032 - its datasheet only appears to support 0.5
        m_pulseShape.create(0.5, 3, m_samplesPerSymbol);

        // Recieve buffer, long enough for one max length message
        delete[] m_rxBuf;
        m_rxBufLength = RADIOSONDEDEMOD_MAX_BYTES*8*m_samplesPerSymbol;
        m_rxBuf = new Real[m_rxBufLength];
        m_rxBufIdx = 0;
        m_rxBufCnt = 0;

        // Create training sequence for correlation
        delete[] m_train;

        const int correlateBits = 200;            // Preamble is 320bits - leave some for AGC (and clock recovery eventually)
        m_correlationLength = correlateBits*m_samplesPerSymbol;      // Don't want to use header, as we want to calculate DC offset
        m_train = new Real[m_correlationLength]();

        // Pulse shape filter takes a few symbols before outputting expected shape
        for (int j = 0; j < m_samplesPerSymbol; j++) {
            m_pulseShape.filter(-1.0f);
        }
        for (int j = 0; j < m_samplesPerSymbol; j++) {
            m_pulseShape.filter(1.0f);
        }
        for (int i = 0; i < correlateBits; i++)
        {
            for (int j = 0; j < m_samplesPerSymbol; j++) {
                m_train[i*m_samplesPerSymbol+j] = -m_pulseShape.filter((i&1) * 2.0f - 1.0f);
            }
        }
    }

    m_settings = settings;
}
