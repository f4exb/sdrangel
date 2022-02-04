///////////////////////////////////////////////////////////////////////////////////
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

#define BOOST_CHRONO_HEADER_ONLY
#include <boost/chrono/chrono.hpp>

#include <QDebug>

#include "util/stepfunctions.h"
#include "util/db.h"
#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"

#include "adsbdemodreport.h"
#include "adsbdemodsink.h"
#include "adsbdemodsinkworker.h"
#include "adsbdemodsettings.h"
#include "adsb.h"

MESSAGE_CLASS_DEFINITION(ADSBDemodSinkWorker::MsgConfigureADSBDemodSinkWorker, Message)

void ADSBDemodSinkWorker::run()
{
    int readBuffer = 0;

    // Acquire first buffer
    m_sink->m_bufferRead[readBuffer].acquire();

    // Start recording how much time is spent processing in this method
    boost::chrono::steady_clock::time_point startPoint = boost::chrono::steady_clock::now();

    // Check for updated settings
    handleInputMessages();

    // samplesPerBit is only changed when the thread is stopped
    int samplesPerBit = m_settings.m_samplesPerBit;
    int samplesPerFrame = samplesPerBit*(ADS_B_PREAMBLE_BITS+ADS_B_ES_BITS);
    int samplesPerChip = samplesPerBit/ADS_B_CHIPS_PER_BIT;

    qDebug() << "ADSBDemodSinkWorker:: running with"
         << " samplesPerFrame: " << samplesPerFrame
         << " samplesPerChip: " << samplesPerChip
         << " samplesPerBit: " << samplesPerBit
         << " correlateFullPreamble: " << m_settings.m_correlateFullPreamble
         << " correlationScale: " << m_correlationScale
         << " correlationThreshold: " << m_settings.m_correlationThreshold;

    int readIdx = m_sink->m_samplesPerFrame - 1;

    while (true)
    {
        int startIdx = readIdx;

        // Correlate received signal with expected preamble
        // chip+ indexes are 0, 2, 7, 9
        // correlating over first 6 bits gives a reduction in per-sample
        // processing, but more than doubles the number of false matches
        Real preambleCorrelationOnes = 0.0;
        Real preambleCorrelationZeros = 0.0;
        if (m_settings.m_correlateFullPreamble)
        {
            for (int i = 0; i < samplesPerChip; i++)
            {
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  0*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  1*samplesPerChip + i];

                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  2*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  3*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  4*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  5*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  6*samplesPerChip + i];
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  7*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  8*samplesPerChip + i];
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  9*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 10*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 11*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 12*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 13*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 14*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 15*samplesPerChip + i];
            }
        }
        else
        {
            for (int i = 0; i < samplesPerChip; i++)
            {
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  0*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  1*samplesPerChip + i];

                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  2*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  3*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  4*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  5*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  6*samplesPerChip + i];
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  7*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx +  8*samplesPerChip + i];
                preambleCorrelationOnes  += m_sink->m_sampleBuffer[readBuffer][startIdx +  9*samplesPerChip + i];

                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 10*samplesPerChip + i];
                preambleCorrelationZeros += m_sink->m_sampleBuffer[readBuffer][startIdx + 11*samplesPerChip + i];
            }
        }

        // Use the ratio of ones power over zeros power, as we don't care how powerful the signal
        // is, just whether there is a good correlation with the preamble. The absolute value varies
        // too much with different radios, AGC settings and and the noise floor is not constant
        // (E.g: it's quite possible to receive multiple frames simultaneously, so we don't
        // want a maximum threshold for the zeros, as a weaker signal may transmit 1s in
        // a stronger signals 0 chip position. Similarly a strong signal in an adjacent
        // channel may casue AGC to reduce gain, reducing the ampltiude of an otherwise
        // strong signal, as well as the noise floor)
        // The threshold accounts for the different number of zeros and ones in the preamble
        // If the sum of ones is exactly 0, it's probably no signal

        Real preambleCorrelation = preambleCorrelationOnes/preambleCorrelationZeros; // without one/zero ratio correction

        if ((preambleCorrelation > m_correlationThresholdLinear) && (preambleCorrelationOnes != 0.0f))
        {
            int firstIdx = startIdx;

            m_demodStats.m_correlatorMatches++;
            // Skip over preamble
            startIdx += samplesPerBit*ADS_B_PREAMBLE_BITS;

            // Demodulate waveform to bytes
            unsigned char data[ADS_B_ES_BYTES];
            int byteIdx = 0;
            int currentBit;
            unsigned char currentByte = 0;
            int df;

            for (int bit = 0; bit < ADS_B_ES_BITS; bit++)
            {
                // PPM (Pulse position modulation) - Each bit spreads to two chips, 1->10, 0->01
                // Determine if bit is 1 or 0, by seeing which chip has largest combined energy over the sampling period
                Real oneSum = 0.0f;
                Real zeroSum = 0.0f;
                for (int i = 0; i < samplesPerChip; i++)
                {
                    oneSum += m_sink->m_sampleBuffer[readBuffer][startIdx+i];
                    zeroSum += m_sink->m_sampleBuffer[readBuffer][startIdx+samplesPerChip+i];
                }
                currentBit = oneSum > zeroSum;
                startIdx += samplesPerBit;
                // Convert bit to bytes - MSB first
                currentByte |= currentBit << (7-(bit & 0x7));
                if ((bit & 0x7) == 0x7)
                {
                    data[byteIdx++] = currentByte;
                    currentByte = 0;
                    // Don't try to demodulate any further, if this isn't an ADS-B frame
                    // to help reduce processing overhead
                    if (!m_settings.m_demodModeS && (bit == 7))
                    {
                        df = ((data[0] >> 3) & ADS_B_DF_MASK);
                        if ((df != 17) && (df != 18))
                            break;
                    }
                }
            }

            // Is ADS-B?
            df = ((data[0] >> 3) & ADS_B_DF_MASK);
            if ((df == 17) || (df == 18))
            {
                m_crc.init();
                unsigned int parity = (data[11] << 16) | (data[12] << 8) | data[13]; // Parity / CRC

                m_crc.calculate(data, ADS_B_ES_BYTES-3);
                if (parity == m_crc.get())
                {
                    // Got a valid frame
                    m_demodStats.m_adsbFrames++;
                    // Don't try to re-demodulate the same frame
                    // We could possibly allow a partial overlap here
                    readIdx += (ADS_B_ES_BITS+ADS_B_PREAMBLE_BITS)*ADS_B_CHIPS_PER_BIT*samplesPerChip - 1;
                    // Pass to GUI
                    if (m_sink->getMessageQueueToGUI())
                    {
                        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(
                            QByteArray((char*)data, sizeof(data)),
                            preambleCorrelation * m_correlationScale,
                            preambleCorrelationOnes / samplesPerChip,
                            rxDateTime(firstIdx, readBuffer));
                        m_sink->getMessageQueueToGUI()->push(msg);
                    }
                    // Pass to worker to feed to other servers
                    if (m_sink->getMessageQueueToWorker())
                    {
                        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(
                            QByteArray((char*)data, sizeof(data)),
                            preambleCorrelation * m_correlationScale,
                            preambleCorrelationOnes / samplesPerChip,
                            rxDateTime(firstIdx, readBuffer));
                        m_sink->getMessageQueueToWorker()->push(msg);
                    }
                }
                else
                    m_demodStats.m_crcFails++;
            }
            else if (m_settings.m_demodModeS)
            {
                int bytes;

                m_crc.init();
                if ((df == 0) || (df == 4) || (df == 5) || (df == 11))
                    bytes = 56/8;
                else if ((df == 16) || (df == 20) || (df == 21) || (df >= 24))
                    bytes = 112/8;
                else
                    bytes = 0;
                if (bytes > 0)
                {
                    int parity = (data[bytes-3] << 16) | (data[bytes-2] << 8) | data[bytes-1];
                    m_crc.calculate(data, bytes-3);
                    int crc = m_crc.get();
                    // For DF11, the last 7 bits may have an address/interogration indentifier (II)
                    // XORed in, so we ignore those bits
                    if ((parity == crc) || ((df == 11) && (parity & 0xffff80) == (crc & 0xffff80)))
                    {
                        m_demodStats.m_modesFrames++;
                        // Pass to worker to feed to other servers
                        if (m_sink->getMessageQueueToWorker())
                        {
                            ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(
                                QByteArray((char*)data, sizeof(data)),
                                preambleCorrelation * m_correlationScale,
                                preambleCorrelationOnes / samplesPerChip,
                                rxDateTime(firstIdx, readBuffer));
                            m_sink->getMessageQueueToWorker()->push(msg);
                        }
                    }
                    else
                        m_demodStats.m_crcFails++;
                }
                else
                    m_demodStats.m_typeFails++;
            }
            else
                m_demodStats.m_typeFails++;
        }
        readIdx++;
        if (readIdx > m_sink->m_bufferSize - samplesPerFrame)
        {
            int nextBuffer = readBuffer+1;
            if (nextBuffer >= m_sink->m_buffers)
                nextBuffer = 0;

            // Update amount of time spent processing (don't include time spend in acquire)
            boost::chrono::duration<double> sec = boost::chrono::steady_clock::now() - startPoint;
            m_demodStats.m_demodTime += sec.count();
            m_demodStats.m_feedTime = m_sink->m_feedTime;

            // Send stats to GUI
            if (m_sink->getMessageQueueToGUI())
            {
                ADSBDemodReport::MsgReportDemodStats *msg = ADSBDemodReport::MsgReportDemodStats::create(m_demodStats);
                m_sink->getMessageQueueToGUI()->push(msg);
            }

            if (!isInterruptionRequested())
            {
                // Get next buffer
                m_sink->m_bufferRead[nextBuffer].acquire();

                // Check for updated settings
                handleInputMessages();

                // Resume timing how long we are processing
                startPoint = boost::chrono::steady_clock::now();

                int samplesRemaining = m_sink->m_bufferSize - readIdx;
                if (samplesRemaining > 0)
                {
                    // Copy remaining samples, to start of next buffer
                    memcpy(&m_sink->m_sampleBuffer[nextBuffer][samplesPerFrame - 1 - samplesRemaining], &m_sink->m_sampleBuffer[readBuffer][readIdx], samplesRemaining*sizeof(Real));
                    readIdx = samplesPerFrame - 1 - samplesRemaining;
                }
                else
                {
                    readIdx = samplesPerFrame - 1;
                }

                m_sink->m_bufferWrite[readBuffer].release();

                readBuffer = nextBuffer;
            }
            else
            {
                // Use a break to avoid testing a condition in the main loop
                break;
            }
        }
    }
}
void ADSBDemodSinkWorker::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (MsgConfigureADSBDemodSinkWorker::match(*message))
        {
            MsgConfigureADSBDemodSinkWorker* cfg = (MsgConfigureADSBDemodSinkWorker*)message;

            ADSBDemodSettings settings = cfg->getSettings();
            bool force = cfg->getForce();

            if (settings.m_correlateFullPreamble) {
                m_correlationScale = 3.0;
            } else {
                m_correlationScale = 2.0;
            }

            if ((m_settings.m_correlationThreshold != settings.m_correlationThreshold) || force)
            {
                m_correlationThresholdLinear = CalcDb::powerFromdB(settings.m_correlationThreshold);
                m_correlationThresholdLinear /= m_correlationScale;
                qDebug() << "m_correlationThresholdLinear: " << m_correlationThresholdLinear;
            }

            m_settings = settings;
            delete message;
        }
    }
}

QDateTime ADSBDemodSinkWorker::rxDateTime(int firstIdx, int readBuffer) const
{
    const qint64 samplesPerSecondMSec = ADS_B_BITS_PER_SECOND * m_settings.m_samplesPerBit / 1000;
    const qint64 offsetMSec = (firstIdx - m_sink->m_samplesPerFrame - 1) / samplesPerSecondMSec;
    return m_sink->m_bufferFirstSampleDateTime[readBuffer].addMSecs(offsetMSec);
}
