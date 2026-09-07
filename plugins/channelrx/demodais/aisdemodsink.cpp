///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>               //
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Some code by AI                                                               //
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

#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "aisdemod.h"
#include "aisdemodsink.h"

AISDemodSink::AISDemodSink(AISDemod *aisDemod) :
        m_scopeSink(nullptr),
        m_aisDemod(aisDemod),
        m_channel(nullptr),
        m_channelSampleRate(AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_rxBuf(nullptr),
        m_train(nullptr),
        m_trainEnergy(0.0f),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;
    m_sampleCounter = 0;
    m_lastAttemptPos = 0;
    m_haveAttempted = false;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;
    for (int i = 0; i < AISDemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }

    applySettings(m_settings, QStringList(), true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AISDemodSink::~AISDemodSink()
{
    delete[] m_rxBuf;
    delete[] m_train;
}

void AISDemodSink::sampleToScope(Complex sample, Real magsq, Real fmDemod, Real filt, Real rxBuf, Real corr, Real thresholdMet, Real dcOffset, Real crcValid)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(fmDemod, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(filt, 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(rxBuf, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(corr, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(thresholdMet, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(dcOffset, 0.0f);
        m_sampleBuffer[8][m_sampleBufferIndex] = Complex(crcValid, 0.0f);
        m_sampleBufferIndex++;
        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < AISDemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void AISDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

// Per burst carrier frequency offset, radians per sample.
//
// The training sequence has equal numbers of ones and zeros, so its mean phase increment
// is the carrier offset and nothing else. Summing the products before taking the argument
// keeps the estimate out of the noisy per sample phase.
//
// This only has to get the MLSE's phase tracking loop into its pull in range - the loop
// removes what is left, including any drift the estimate cannot see.
double AISDemodSink::estimateFrequency() const
{
    std::complex<double> sum(0.0, 0.0);

    for (int i = 1; i < m_correlationLength; i++)
    {
        int j = (m_rxBufIdx + i) % m_rxBufLength;
        int k = (j + m_rxBufLength - 1) % m_rxBufLength;
        sum += m_iqBuf[j] * std::conj(m_iqBuf[k]);
    }

    return std::arg(sum);
}

// Run the sequence detector over n symbols starting at buffer position x, leaving the
// symbol decisions in m_symSoft
void AISDemodSink::computeSymbols(int x, int n)
{
    const double dphi = estimateFrequency();
    const int sps = m_samplesPerSymbol;
    const int half = sps / 2;
    const int len = m_rxBufLength;
    const std::complex<double> *iq = m_iqBuf.data();

    // The phase loop starts with no idea of the carrier phase, so run it over some of the
    // preamble first and throw those symbols away. x sits 18 symbols into the 24 bit
    // training sequence, so there is room for this - but only that much, so clamp rather
    // than reading backwards off the start of the buffer.
    const int xOffset = ((x - m_rxBufIdx) % len + len) % len;
    const int warmup = std::min(AISDEMOD_MLSE_WARMUP, (xOffset - half) / sps);

    m_mlse.decode(n + warmup,
        [x, sps, half, len, iq, dphi, warmup](int k, int m) -> std::complex<double>
        {
            int off = (k - warmup)*sps - half + m;
            int idx = ((x + off) % len + len) % len;
            double a = -dphi * off;
            return iq[idx] * std::complex<double>(cos(a), sin(a));
        },
        m_mlseSoft);

    m_symSoft.assign(m_mlseSoft.begin() + warmup, m_mlseSoft.end());
}

void AISDemodSink::processOneSample(Complex &ci)
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
    // AIS spec allows up to +-1kHz offset
    // We need to remove this, otherwise it may effect the sampling
    // To calculate what it is, we sum the training sequence, which should be zero

    // Clip, as large noise can result in high correlation
    // Don't clip to 1.0 - as there may be some DC offset (1k/4.8k max dev=0.2)
    Real filtClipped;
    filtClipped = std::fmax(-1.4, std::fmin(1.4, filt));

    // Buffer filtered samples. We buffer enough samples for a max length message
    // before trying to demod, so false triggering can't make us miss anything
    m_rxBuf[m_rxBufIdx] = filtClipped;
    m_iqBuf[m_rxBufIdx] = std::complex<double>(ci.real() / SDR_RX_SCALEF, ci.imag() / SDR_RX_SCALEF);
    m_rxBufIdx = (m_rxBufIdx + 1) % m_rxBufLength;
    m_sampleCounter++;
    m_rxBufCnt = std::min(m_rxBufCnt + 1, m_rxBufLength);

    Real metric = 0.0f;
    bool scopeCRCValid = false;
    bool scopeCRCInvalid = false;
    Real dcOffset = 0.0f;
    bool thresholdMet = false;
    if (m_rxBufCnt >= m_rxBufLength)
    {
        Real trainingSum = 0.0f;

        // Matched filter for the preamble, on the complex baseband. Correlating the
        // discriminator output instead - as this used to - means detecting with a statistic
        // that has already fallen off the FM threshold at the levels the sequence detector
        // can still demodulate, which is why noise used to trigger it for several percent of
        // all samples. Blocks are combined non-coherently because neither carrier phase nor
        // frequency is known; see AISDEMOD_IQ_BLOCKS.
        if ((m_sampleCounter % AISDemodSettings::AISDEMOD_IQ_DECIM) == 0)
        {
            const int blocks = AISDemodSettings::AISDEMOD_IQ_BLOCKS;
            const int blockLen = m_correlationLength / blocks;
            double sumMag = 0.0;
            double iqEnergy = 0.0;

            for (int b = 0; b < blocks; b++)
            {
                std::complex<double> acc(0.0, 0.0);

                for (int i = b*blockLen; i < (b+1)*blockLen; i++)
                {
                    int j = (m_rxBufIdx + i) % m_rxBufLength;
                    acc += m_iqBuf[j] * std::conj(m_trainIQ[i]);
                    iqEnergy += std::norm(m_iqBuf[j]);
                    trainingSum += m_rxBuf[j];
                }

                sumMag += std::abs(acc);
            }

            // |m_trainIQ| is 1, so a clean match of amplitude A gives sumMag = A*N against
            // sqrt(iqEnergy*N) = A*N, i.e. 1. Noise lands near 0.886/sqrt(blockLen).
            metric = (Real) (sumMag / (sqrt(iqEnergy * (double) (blockLen*blocks)) + 1e-12));
            thresholdMet = metric >= m_settings.m_correlationThreshold;
        }

        if (thresholdMet)
        {
            // Mean of the preamble, which is the carrier offset. Only reported to the
            // scope now - the sequence detector estimates and tracks it for itself.
            dcOffset = trainingSum/m_correlationLength;

            // Start demod after (most of) preamble
            // Symbol centres sit at m_samplesPerSymbol/2 - 1 past the training symbol
            // boundary: the transmit pulse shaping delay and the receive one cancel,
            // leaving the half symbol offset and the one sample the phase discriminator
            // consumes. Do not write this as a constant - it was +4, which is only
            // correct at 10 samples per symbol, and the MLSE will not tolerate a quarter
            // symbol error the way the old slicer did.
            int base = (m_rxBufIdx + m_correlationLength*3/4
                        + m_samplesPerSymbol/2 - 1) % m_rxBufLength;

            // Try the nominal timing first, then progressively larger offsets either side
            for (int phase = 0; phase <= 2*AISDEMOD_TIMING_PHASES; phase++)
            {
            // Only the alignment that is kept should mark the scope trace. deframe()
            // only ever sets these, so a bad CRC from an alignment that is about to be
            // retried would otherwise stay set even when a later phase succeeds.
            scopeCRCValid = false;
            scopeCRCInvalid = false;

            int d = (phase + 1) / 2;

            if (phase & 1) {
                d = -d;
            }

            // Consecutive triggers overlap by all but one alignment, and re-demodulating a
            // position almost always gives the same answer, so skipping the repeats is worth
            // about 2.5x the CPU of the whole retry stage. Not quite free: the look ahead
            // available from a position grows as samples arrive, so a multi slot frame that
            // ran out of buffer on the first attempt can succeed on a later one. Measured at
            // one message in 1550 on a clean recording and none at the noise floor.
            qint64 pos = (qint64) m_sampleCounter + d;

            if (m_haveAttempted && (pos <= m_lastAttemptPos)) {
                continue;
            }

            m_lastAttemptPos = pos;
            m_haveAttempted = true;

            int x = (base + d + m_rxBufLength) % m_rxBufLength;

            int endSampleIdx = 0;

            // Decode only far enough to tell whether there is a start flag. Nearly all
            // triggers are noise and abort here, which is what makes the sequence
            // detector affordable.
            computeSymbols(x, AISDEMOD_SOP_SYMBOLS + 8);

            DemodResult result = deframe(endSampleIdx, scopeCRCValid, scopeCRCInvalid);

            // Extend on demand. Most messages fit one slot, but type 5 and the binary
            // messages take two or more, and decoding every candidate to the full
            // buffer length would cost far more than decoding the few long ones twice.
            //
            // The look ahead must not run off the end of the circular buffer: symbol n-1
            // reads up to n*samplesPerSymbol - samplesPerSymbol/2 - 1 samples beyond x,
            // and anything past the end wraps onto pre-trigger samples, which the
            // Viterbi traceback would then start from. So cap the span at what is
            // genuinely buffered, and make sure that capped span is actually attempted
            // rather than being skipped by the doubling.
            int xOffset = ((x - m_rxBufIdx) % m_rxBufLength + m_rxBufLength) % m_rxBufLength;
            int maxSymbols = (m_rxBufLength - xOffset + m_samplesPerSymbol/2) / m_samplesPerSymbol;

            // Never extend to fewer symbols than the first stage already decoded, or the
            // deframer would run out before it reaches the start flag it just found
            const int firstSpan = std::max(AISDEMOD_MLSE_SYMBOLS, AISDEMOD_SOP_SYMBOLS + 8);

            for (int span = firstSpan; result == FrameTruncated; span *= 2)
            {
                int symbols = std::min(span, maxSymbols);

                computeSymbols(x, symbols);
                result = deframe(endSampleIdx, scopeCRCValid, scopeCRCInvalid);

                if (symbols >= maxSymbols) {
                    break;
                }
            }

            if (result == FrameGood)
            {
                // Skip over received packet, so we don't try to re-demodulate it
                m_rxBufCnt -= endSampleIdx;
                break;
            }
            }
        }
    }

    // Select signals to feed to scope
    sampleToScope(ci / SDR_RX_SCALEF, magsq, fmDemod, filt, m_rxBuf[m_rxBufIdx],
        metric,
        thresholdMet, dcOffset, scopeCRCValid ? 1.0 : (scopeCRCInvalid ? -1.0 : 0));

    // Send demod signal to Demod Analyzer feature
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

// HDLC deframing of the symbol decisions computeSymbols() has left in m_symSoft
AISDemodSink::DemodResult AISDemodSink::deframe(int& endSampleIdx,
                                                bool& crcValid, bool& crcInvalid)
{
    bool gotSOP = false;
    int bits = 0;
    int bitCount = 0;
    int onesCount = 0;
    int byteCount = 0;
    int symbolPrev = 0;
    int symbolIdx = 0;
    int totalBitCount = 0; // Count of bits after start flag, before bit stuffing removal, including stop flag

    for (int sampleIdx = 0; sampleIdx < m_rxBufLength; sampleIdx += m_samplesPerSymbol)
    {
        if (symbolIdx >= (int) m_symSoft.size()) {
            return gotSOP ? FrameTruncated : FrameNone;
        }

        // Sign of the Viterbi's decision for this symbol
        int symbol = m_symSoft[symbolIdx] >= 0.0f ? 1 : 0;

        symbolIdx++;

        // HDLC deframing

        // NRZI decoding
        int bit;
        if (symbol != symbolPrev) {
            bit = 0;
        } else {
            bit = 1;
        }
        symbolPrev = symbol;

        // Store in shift reg
        bits |= bit << bitCount;
        bitCount++;

        if (bit == 1)
        {
            onesCount++;
            // Shouldn't ever get 7 1s in a row
            if ((onesCount == 7) && gotSOP) {
                return FrameNone;
            }
        }
        else if (bit == 0)
        {
            if (onesCount == 5)
            {
                // Remove bit-stuffing (5 1s followed by a 0)
                bitCount--;
            }
            else if (onesCount == 6)
            {
                // Start/end of packet
                if (gotSOP && (bitCount == 8) && (bits == 0x7e) && (byteCount >= 3))
                {
                    // End of packet
                    // Check CRC is valid
                    m_crc.init();
                    m_crc.calculate(m_bytes, byteCount - 2);
                    uint16_t calcCrc = m_crc.get();
                    uint16_t rxCrc = m_bytes[byteCount-2] | (m_bytes[byteCount-1] << 8);
                    if (calcCrc == rxCrc)
                    {
                        crcValid = true;
                        // Don't include CRC
                        sendMessage(QByteArray((char *) m_bytes, byteCount - 2), totalBitCount);
                        endSampleIdx = sampleIdx;
                        return FrameGood;
                    }
                    else
                    {
                        //qDebug() << QString("CRC mismatch: %1 %2").arg(calcCrc, 4, 16, QLatin1Char('0')).arg(rxCrc, 4, 16, QLatin1Char('0'));
                        crcInvalid = true;
                        return FrameBadCrc;
                    }
                }
                else if (gotSOP)
                {
                    // Repeated start flag without data or misalignment, something not right
                    return FrameNone;
                }
                else
                {
                    // Start of packet
                    gotSOP = true;
                    bits = 0;
                    bitCount = 0;
                    byteCount = 0;
                    totalBitCount = 0;
                }
            }
            onesCount = 0;
        }

        if (gotSOP)
        {
            totalBitCount++;
            if (bitCount == 8)
            {
                // Could also check count according to message ID as that varies
                if (byteCount >= AISDEMOD_MAX_BYTES)
                {
                    // Too many bytes
                    return FrameNone;
                }
                else
                {
                    // Got a complete byte
                    m_bytes[byteCount] = bits;
                    byteCount++;
                }
                bits = 0;
                bitCount = 0;
            }
        }

        // Abort demod if we haven't found start flag within a couple of bytes of presumed preamble
        if (!gotSOP && (sampleIdx >= AISDEMOD_SOP_SYMBOLS * m_samplesPerSymbol)) {
            return FrameNone;
        }
    }

    return gotSOP ? FrameTruncated : FrameNone;
}

void AISDemodSink::sendMessage(const QByteArray& rxPacket, int totalBitCount)
{
    if (!getMessageQueueToChannel()) {
        return;
    }

    //qDebug() << "RX: " << rxPacket.toHex();

    // Calculate slot number based on time of start of transmission
    // This is unlikely to be accurate in absolute terms, given we don't know latency from SDR or buffering within SDRangel
    // But can be used to get an idea of congestion
    QDateTime currentTime = QDateTime::currentDateTime();
    if (m_settings.m_useFileTime)
    {
        QString hardwareId = m_aisDemod->getDeviceAPI()->getHardwareId();

        if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
        {
            QString dateTimeStr;
            int deviceIdx = m_aisDemod->getDeviceSetIndex();

            if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr))
            {
                QDateTime fileTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);

                // An unparseable timestamp gives an invalid QDateTime, whose time().second()
                // is -1. That makes ms and then the slot number negative, which the slot map
                // indexes with and the log reports. Fall back to the wall clock instead.
                if (fileTime.isValid()) {
                    currentTime = fileTime;
                } else {
                    qDebug() << "AISDemodSink::sendMessage: could not parse absoluteTime" << dateTimeStr;
                }
            }
        }
    }

    int txTimeMs = (totalBitCount + 8 + 24 + 8) * (1000.0 / m_settings.m_baud); // Add ramp up, preamble and start-flag
    QDateTime startDateTime = currentTime.addMSecs(-txTimeMs);
    int ms = startDateTime.time().second() * 1000 + startDateTime.time().msec();
    float slotTime = 60.0f * 1000.0f / 2250.0f; // 2250 slots per minute, 26.6ms per slot

    if (ms < 0) {
        ms = 0;
    }

    int slot = ms / slotTime;
    int totalSlots = std::ceil(txTimeMs / slotTime);
    AISDemod::MsgMessage *msg = AISDemod::MsgMessage::create(rxPacket, currentTime, slot, totalSlots);
    getMessageQueueToChannel()->push(msg);
}

void AISDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AISDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    m_samplesPerSymbol = AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / m_settings.m_baud;
    qDebug() << "AISDemodSink::applyChannelSettings: m_samplesPerSymbol: " << m_samplesPerSymbol;
}

void AISDemodSink::applySettings(const AISDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "AISDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth")) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }
    if ((settingsKeys.contains("fmDeviation")) || force)
    {
        m_phaseDiscri.setFMScaling(AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / (2.0f * settings.m_fmDeviation));
    }

    if ((settingsKeys.contains("baud")) || force)
    {
        // Clamp before dividing: baud is written by the web API and read from the config
    // blob without validation, and 0 divides while anything over the channel rate leaves
    // m_samplesPerSymbol at 0, which makes m_rxBufLength 0 and the next sample a % 0.
    int baud = settings.m_baud;

    if ((baud < 1) || (baud > AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE)) {
        baud = 9600;
    }

    m_samplesPerSymbol = AISDemodSettings::AISDEMOD_CHANNEL_SAMPLE_RATE / baud;
        qDebug() << "AISDemodSink::applySettings: m_samplesPerSymbol: " << m_samplesPerSymbol << " baud " << settings.m_baud;
        m_pulseShape.create(0.5, 3, m_samplesPerSymbol);

        // Receive buffer, long enough for one max length message
        delete[] m_rxBuf;
        m_rxBufLength = AISDEMOD_MAX_BYTES*8*m_samplesPerSymbol;
        m_rxBuf = new Real[m_rxBufLength];
        m_rxBufIdx = 0;
        m_rxBufCnt = 0;

        // The complex baseband the sequence detector works on, kept in step with m_rxBuf
        m_iqBuf.assign(m_rxBufLength, std::complex<double>(0.0, 0.0));

        m_mlse.create(m_samplesPerSymbol, AISDemodSettings::AISDEMOD_MLSE_SPAN,
                      AISDemodSettings::AISDEMOD_MLSE_BT);
        m_mlse.setLoopGains(AISDEMOD_MLSE_PHASE_GAIN, AISDEMOD_MLSE_FREQ_GAIN);

        // Create 24-bit training sequence for correlation
        delete[] m_train;
        m_correlationLength = 24*m_samplesPerSymbol;
        m_train = new Real[m_correlationLength]();
        const int trainNRZ[24] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};

        // Pulse shape filter takes a few symbols before outputting expected shape
        for (int j = 0; j < m_samplesPerSymbol; j++)
            m_pulseShape.filter(-1.0f);
        for (int j = 0; j < m_samplesPerSymbol; j++)
            m_pulseShape.filter(1.0f);
        for (int i = 0; i < 24; i++)
        {
            for (int j = 0; j < m_samplesPerSymbol; j++)
            {
                m_train[i*m_samplesPerSymbol+j] = m_pulseShape.filter(trainNRZ[i] * 2.0f - 1.0f);
            }
        }

        // The transmitted preamble, for matched filtering on the complex baseband.
        // m_train is the Gaussian filtered NRZ, i.e. the expected instantaneous
        // frequency; integrating gives phase, and GMSK with h=1/2 puts +-pi/2 in each
        // symbol while m_train sums to +-samplesPerSymbol over one.
        m_trainIQ.assign(m_correlationLength, std::complex<double>(0.0, 0.0));
        {
            double phase = 0.0;
            double k = M_PI / (2.0 * m_samplesPerSymbol);

            for (int i = 0; i < m_correlationLength; i++)
            {
                phase += k * m_train[i];
                m_trainIQ[i] = std::complex<double>(std::cos(phase), std::sin(phase));
            }
        }

        m_trainEnergy = 0.0f;
        for (int i = 0; i < m_correlationLength; i++) {
            m_trainEnergy += m_train[i] * m_train[i];
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
