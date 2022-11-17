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
#include "util/popcount.h"
#include "maincore.h"

#include "pagerdemod.h"
#include "pagerdemodsink.h"

PagerDemodSink::PagerDemodSink(PagerDemod *pagerDemod) :
        m_scopeSink(nullptr),
        m_pagerDemod(pagerDemod),
        m_channelSampleRate(PagerDemodSettings::m_channelSampleRate),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_dcOffset(0.0f),
        m_dataPrev(0),
        m_inverted(false),
        m_bit(0),
        m_gotSOP(false),
        m_bits(0),
        m_bitCount(0),
        m_syncCount(75),
        m_batchNumber(0),
        m_wordCount(0),
        m_addressValid(0),
        m_sampleBufferIndex(0)
{
    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
    m_sampleBuffer.resize(m_sampleBufferSize);
}

PagerDemodSink::~PagerDemodSink()
{
}

void PagerDemodSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[m_sampleBufferIndex++] = sample;

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;
            vbegin.push_back(m_sampleBuffer.begin());
            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void PagerDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

// XOR bits together for parity check
int PagerDemodSink::xorBits(quint32 word, int firstBit, int lastBit)
{
    int x = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        x ^= (word >> i) & 1;
    }
    return x;
}

// Check for even parity
bool PagerDemodSink::evenParity(quint32 word, int firstBit, int lastBit, int parityBit)
{
    return xorBits(word, firstBit, lastBit) == parityBit;
}

// Reverse order of bits
quint32 PagerDemodSink::reverse(quint32 x)
{
    x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
    x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
    x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
    x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
    return((x >> 16) | (x << 16));
}

// Calculate BCH parity and even parity bits
quint32 PagerDemodSink::bchEncode(const quint32 cw)
{
	quint32 bit = 0;
	quint32 localCW = cw & 0xFFFFF800;	// Mask off BCH parity and even parity bits
	quint32 cwE = localCW;

	// Calculate BCH bits
	for (bit = 1; bit <= 21; bit++)
    {
		if (cwE & 0x80000000) {
			cwE ^= 0xED200000;
		}
		cwE <<= 1;
	}
	localCW |= (cwE >> 21);

	return localCW;
}

// Use BCH decoding to try to fix any bit errors
// Returns true if able to be decode/repair successfull
// See: https://www.eevblog.com/forum/microcontrollers/practical-guides-to-bch-fec/
bool PagerDemodSink::bchDecode(const quint32 cw, quint32& correctedCW)
{
	// Calculate syndrome
	// We do this by recalculating the BCH parity bits and XORing them against the received ones
	quint32 syndrome = ((bchEncode(cw) ^ cw) >> 1) & 0x3FF;

	if (syndrome == 0)
    {
		// Syndrome of zero indicates no repair required
        correctedCW = cw;
		return true;
	}

	// Meggitt decoder

	quint32 result = 0;
	quint32 damagedCW = cw;

	// Calculate BCH bits
	for (quint32 xbit = 0; xbit < 31; xbit++)
    {
		// Produce the next corrected bit in the high bit of the result
		result <<= 1;
		if ((syndrome == 0x3B4) ||		// 0x3B4: Syndrome when a single error is detected in the MSB
			(syndrome == 0x26E)	||		// 0x26E: Two adjacent errors
			(syndrome == 0x359) ||		// 0x359: Two errors, one OK bit between
			(syndrome == 0x076) ||		// 0x076: Two errors, two OK bits between
			(syndrome == 0x255) ||		// 0x255: Two errors, three OK bits between
			(syndrome == 0x0F0) ||		// 0x0F0: Two errors, four OK bits between
			(syndrome == 0x216) ||
			(syndrome == 0x365) ||
			(syndrome == 0x068) ||
			(syndrome == 0x25A) ||
			(syndrome == 0x343) ||
			(syndrome == 0x07B) ||
			(syndrome == 0x1E7) ||
			(syndrome == 0x129) ||
			(syndrome == 0x14E) ||
			(syndrome == 0x2C9) ||
			(syndrome == 0x0BE) ||
			(syndrome == 0x231) ||
			(syndrome == 0x0C2) ||
			(syndrome == 0x20F) ||
			(syndrome == 0x0DD) ||
			(syndrome == 0x1B4) ||
			(syndrome == 0x2B4) ||
			(syndrome == 0x334) ||
			(syndrome == 0x3F4) ||
			(syndrome == 0x394) ||
			(syndrome == 0x3A4) ||
			(syndrome == 0x3BC) ||
			(syndrome == 0x3B0) ||
			(syndrome == 0x3B6) ||
			(syndrome == 0x3B5)
		   )
        {
			// Syndrome matches an error in the MSB
			// Correct that error and adjust the syndrome to account for it
			syndrome ^= 0x3B4;
			result |= (~damagedCW & 0x80000000) >> 30;
		}
        else
        {
			// No error
			result |= (damagedCW & 0x80000000) >> 30;
		}
		damagedCW <<= 1;

		// Handle syndrome shift register feedback
		if (syndrome & 0x200)
        {
			syndrome <<= 1;
			syndrome ^= 0x769;  // 0x769 = POCSAG generator polynomial -- x^10 + x^9 + x^8 + x^6 + x^5 + x^3 + 1
		}
        else
        {
			syndrome <<= 1;
		}
		// Mask off bits which fall off the end of the syndrome shift register
		syndrome &= 0x3FF;
	}

	// Check if error correction was successful
	if (syndrome != 0)
    {
		// Syndrome nonzero at end indicates uncorrectable errors
        correctedCW = cw;
		return false;
	}

	correctedCW = result;
	return true;
}

// Decode a batch of codewords to addresses and messages
// Messages may be spreadout over multiple batches
// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.584-1-198607-S!!PDF-E.pdf
// https://www.itu.int/dms_pubrec/itu-r/rec/m/R-REC-M.584-2-199711-I!!PDF-E.pdf
void PagerDemodSink::decodeBatch()
{
    int i = 1;
    for (int frame = 0; frame < PAGERDEMOD_FRAMES_PER_BATCH; frame++)
    {
        for (int word = 0; word < PAGERDEMOD_CODEWORDS_PER_FRAME; word++)
        {
            bool addressCodeWord = ((m_codeWords[i] >> 31) & 1) == 0;

            // Stop decoding current message if we receive a new address
            if (addressCodeWord && m_addressValid)
            {
                m_numericMessage = m_numericMessage.trimmed(); // Remove trailing spaces
                if (getMessageQueueToChannel())
                {
                    // Convert from 7-bit to UTF-8 using user specified encoding
                    for (int i = 0; i < m_alphaMessage.size(); i++)
                    {
                        QChar c = m_alphaMessage[i];
                        int idx = m_settings.m_sevenbit.indexOf(c.toLatin1());
                        if (idx >= 0) {
                            c = QChar(m_settings.m_unicode[idx]);
                        }
                        m_alphaMessage[i] = c;
                    }
                    // Reverse reading order, if required
                    if (m_settings.m_reverse) {
                        std::reverse(m_alphaMessage.begin(), m_alphaMessage.end());
                    }
                    // Send to channel and GUI
                    PagerDemod::MsgPagerMessage *msg = PagerDemod::MsgPagerMessage::create(m_address, m_functionBits, m_alphaMessage, m_numericMessage, m_parityErrors, m_bchErrors);
                    getMessageQueueToChannel()->push(msg);
                }
                m_addressValid = false;
            }

            // Check parity bit
            bool parityError = !evenParity(m_codeWords[i], 1, 31, m_codeWords[i] & 0x1);

            if (m_codeWords[i] == PAGERDEMOD_POCSAG_IDLECODE)
            {
                // Idle
            }
            else if (addressCodeWord)
            {
                // Address
                m_functionBits = (m_codeWords[i] >> 11) & 0x3;
                int addressBits = (m_codeWords[i] >> 13) & 0x3ffff;
                m_address = (addressBits << 3) | frame;
                m_numericMessage = "";
                m_alphaMessage = "";
                m_alphaBitBufferBits = 0;
                m_alphaBitBuffer = 0;
                m_parityErrors = parityError ? 1 : 0;
                m_bchErrors = m_codeWordsBCHError[i] ? 1 : 0;
                m_addressValid = true;
            }
            else
            {
                // Message - decode as both numeric and ASCII - not all operators use functionBits to indidcate encoding
                int messageBits = (m_codeWords[i] >> 11) & 0xfffff;
                if (parityError) {
                    m_parityErrors++;
                }
                if (m_codeWordsBCHError[i]) {
                    m_bchErrors++;
                }

                // Numeric format
                for (int j = 16; j >= 0; j -= 4)
                {
                    quint32 numericBits = (messageBits >> j) & 0xf;
                    numericBits = reverse(numericBits) >> (32-4);
                    // Spec has 0xa as 'spare', but other decoders treat is as .
                    const char numericChars[] = {
                        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', 'U', ' ', '-', ')', '('
                    };
                    char numericChar = numericChars[numericBits];
                    m_numericMessage.append(numericChar);
                }

                // 7-bit ASCII alpnanumeric format
                m_alphaBitBuffer = (m_alphaBitBuffer << 20) | messageBits;
                m_alphaBitBufferBits += 20;
                while (m_alphaBitBufferBits >= 7)
                {
                    // Extract next 7-bit character from bit buffer
                    char c = (m_alphaBitBuffer >> (m_alphaBitBufferBits-7)) & 0x7f;
                    // Reverse bit ordering
                    c = reverse(c) >> (32-7);
                    // Add to received message string (excluding, null, end of text, end ot transmission)
                    if (c != 0 && c != 0x3 && c != 0x4) {
                        m_alphaMessage.append(c);
                    }
                    // Remove from bit buffer
                    m_alphaBitBufferBits -= 7;
                    if (m_alphaBitBufferBits == 0) {
                        m_alphaBitBuffer = 0;
                    } else {
                        m_alphaBitBuffer &= (1 << m_alphaBitBufferBits) - 1;
                    }
                }
            }

            // Move to next codeword
            i++;
        }
    }
}

void PagerDemodSink::processOneSample(Complex &ci)
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

    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }

    m_magsqCount++;

    // Low pass filter
    Real filt = m_lowpassBaud.filter(fmDemod);

    // An input frequency offset corresponds to a DC offset after FM demodulation
    // To calculate what it is, we average part of the preamble, which should be zero
    if (!m_gotSOP)
    {
        m_preambleMovingAverage(filt);
        m_dcOffset = m_preambleMovingAverage.asDouble();
    }

    bool sample = false;

    // Slice data
    int data = (filt - m_dcOffset) >= 0.0;

    // Look for edge - A PLL here would be less susceptible to noise
    if (data != m_dataPrev)
    {
        // Center in middle of bit
        m_syncCount = m_samplesPerSymbol/2;
    }
    else
    {
        // Wait until centre of bit to sample it
        m_syncCount--;

        if (m_syncCount <= 0)
        {
            // According to a variety of places on the web, high frequency is a 0, low is 1.
            // While this seems to be correct in the UK, some IQ files I've obtained seem
            // to be reversed, so we support both.
            if (m_inverted) {
                m_bit = data;
            } else {
                m_bit = !data;
            }

            sample = true;
            // Store in shift reg. MSB transmitted first
            m_bits = (m_bits << 1) | m_bit;
            m_bitCount++;

            if (m_bitCount > 32) {
                m_bitCount = 32;
            }

            if ((m_bitCount == 32) && !m_gotSOP)
            {
                // Look for synccode that starts a batch - allow three errors that can be corrected
                if (m_bits == PAGERDEMOD_POCSAG_SYNCCODE)
                {
                    m_gotSOP = true;
                    m_inverted = false;
                }
                else if (m_bits == PAGERDEMOD_POCSAG_SYNCCODE_INV)
                {
                    m_gotSOP = true;
                    m_inverted = true;
                }
                else if (popcount(m_bits ^ PAGERDEMOD_POCSAG_SYNCCODE) >= 29)
                {
                    quint32 correctedCW;
                    if (bchDecode(m_bits, correctedCW) && (correctedCW == PAGERDEMOD_POCSAG_SYNCCODE))
                    {
                        m_gotSOP = true;
                        m_inverted = false;
                    }
                }
                else if (popcount(m_bits ^ PAGERDEMOD_POCSAG_SYNCCODE_INV) >= 29)
                {
                    quint32 correctedCW;
                    if (bchDecode(~m_bits, correctedCW) && (correctedCW == PAGERDEMOD_POCSAG_SYNCCODE))
                    {
                        m_gotSOP = true;
                        m_inverted = true;
                    }
                }

                if (m_gotSOP)
                {
                    // Reset demod state
                    m_bits = 0;
                    m_bitCount = 0;
                    m_codeWords[0] = PAGERDEMOD_POCSAG_SYNCCODE;
                    m_wordCount = 1;
                    m_addressValid = false;
                }
            }
            else if ((m_bitCount == 32) && m_gotSOP)
            {
                // Got a complete codeword - use BCH decoding to fix any bit errors
                quint32 correctedCW;
                m_codeWordsBCHError[m_wordCount] = !bchDecode(m_bits, correctedCW);
                m_codeWords[m_wordCount] = correctedCW;
                m_wordCount++;

                // Check for sync code at start of batch
                if ((m_wordCount == 1) && (correctedCW != PAGERDEMOD_POCSAG_SYNCCODE))
                {
                    m_gotSOP = false;
                    //m_thresholdMet = false;
                    m_addressValid = false;
                    m_inverted = false;
                }

                // Have we received a complete batch
                if (m_wordCount == PAGERDEMOD_BATCH_WORDS)
                {
                    // Decode it to addresses and messages
                    decodeBatch();

                    // Start a new batch
                    m_batchNumber++;
                    m_wordCount = 0;
                }

                m_bits = 0;
                m_bitCount = 0;
            }

            m_syncCount = m_samplesPerSymbol;
        }
    }

    // Save data for edge detection
    m_dataPrev = data;
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
        scopeSample.real(m_dcOffset);
        break;
    case 6:
        scopeSample.real(data);
        break;
    case 7:
        scopeSample.real(sample);
        break;
    case 8:
        scopeSample.real(m_bit);
        break;
    case 9:
        scopeSample.real(m_gotSOP);
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
        scopeSample.imag(m_dcOffset);
        break;
    case 6:
        scopeSample.imag(data);
        break;
    case 7:
        scopeSample.imag(sample);
        break;
    case 8:
        scopeSample.imag(m_bit);
        break;
    case 9:
        scopeSample.imag(m_gotSOP);
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

void PagerDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "PagerDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) PagerDemodSettings::m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void PagerDemodSink::applySettings(const PagerDemodSettings& settings, bool force)
{
    qDebug() << "PagerDemodSink::applySettings:"
            << " rfBandwidth: " << settings.m_rfBandwidth
            << " fmDeviation: " << settings.m_fmDeviation
            << " baud: " << settings.m_baud
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) PagerDemodSettings::m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
        m_lowpass.create(301, PagerDemodSettings::m_channelSampleRate, settings.m_rfBandwidth / 2.0f);
    }

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force)
    {
        m_phaseDiscri.setFMScaling(PagerDemodSettings::m_channelSampleRate / (2.0f * settings.m_fmDeviation));
    }

    if ((settings.m_baud != m_settings.m_baud) || force)
    {
        m_samplesPerSymbol = PagerDemodSettings::m_channelSampleRate / settings.m_baud;
        qDebug() << "PagerDemodSink::applySettings: m_samplesPerSymbol: " << m_samplesPerSymbol;

        // Signal is a square wave - so include several harmonics
        m_lowpassBaud.create(301, PagerDemodSettings::m_channelSampleRate, settings.m_baud * 5.0f);
    }

    m_settings = settings;
}
