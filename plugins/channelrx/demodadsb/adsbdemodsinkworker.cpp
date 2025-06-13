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

#include <QDebug>

#include "util/db.h"
#include "util/profiler.h"
#include "util/units.h"
#include "util/osndb.h"
#include "util/popcount.h"

#include "adsbdemodreport.h"
#include "adsbdemodsink.h"
#include "adsbdemodsinkworker.h"
#include "adsbdemod.h"
#include "adsb.h"

MESSAGE_CLASS_DEFINITION(ADSBDemodSinkWorker::MsgConfigureADSBDemodSinkWorker, Message)

const Real ADSBDemodSinkWorker::m_correlationScale = 3.0f;

static int grayToBinary(int gray, int bits)
{
    int binary = 0;
    for (int i = bits - 1; i >= 0; i--) {
        binary = binary | ((((1 << (i+1)) & binary) >> 1) ^ ((1 << i) & gray));
    }
    return binary;
}

static int gillhamToFeet(int n)
{
    int c1 = (n >> 10) & 1;
    int a1 = (n >> 9) & 1;
    int c2 = (n >> 8) & 1;
    int a2 = (n >> 7) & 1;
    int c4 = (n >> 6) & 1;
    int a4 = (n >> 5) & 1;
    int b1 = (n >> 4) & 1;
    int b2 = (n >> 3) & 1;
    int d2 = (n >> 2) & 1;
    int b4 = (n >> 1) & 1;
    int d4 = n & 1;

    int n500 = grayToBinary((d2 << 7) | (d4 << 6) | (a1 << 5) | (a2 << 4) | (a4 << 3) | (b1 << 2) | (b2 << 1) | b4, 4);
    int n100 = grayToBinary((c1 << 2) | (c2 << 1) | c4, 3) - 1;

    if (n100 == 6) {
        n100 = 4;
    }
    if (n500 %2 != 0) {
        n100 = 4 - n100;
    }

    return -1200 + n500*500 + n100*100;
}

static int decodeModeSAltitude(const QByteArray& data)
{
    int altitude = 0;   // Altitude in feet
    int altitudeCode = ((data[2] & 0x1f) << 8) | (data[3] & 0xff);

    if (altitudeCode & 0x40) // M bit indicates metres
    {
        int altitudeMetres = ((altitudeCode & 0x1f80) >> 1) | (altitudeCode & 0x3f);
        altitude = Units::metresToFeet(altitudeMetres);
    }
    else
    {
        // Remove M and Q bits
        int altitudeFix = ((altitudeCode & 0x1f80) >> 2) | ((altitudeCode & 0x20) >> 1) | (altitudeCode & 0xf);

        // Convert to feet
        if (altitudeCode & 0x10) {
            altitude = altitudeFix * 25 - 1000;
        } else {
            altitude = gillhamToFeet(altitudeFix);
        }
    }
    return altitude;
}

void ADSBDemodSinkWorker::handleModeS(unsigned char *data, int bytes, unsigned icao, int df, int firstIndex, unsigned short preamble, Real preambleCorrelation, Real correlationOnes, const QDateTime& dateTime, unsigned crc)
{
    // Ignore downlink formats we can't decode / unlikely
    if ((df != 19) && (df != 22) && (df < 24))
    {
        QList<RXRecord> l;

        if (m_modeSOnlyIcaos.contains(icao))
        {
            l = m_modeSOnlyIcaos.value(icao);
            if (abs(l.last().m_firstIndex - firstIndex) < 4) {
                return; // Duplicate
            }
        }
        else
        {
            // Stop hash table from getting too big - clear every 10 seconds or so
            QDateTime currentDateTime = QDateTime::currentDateTime();
            if (m_lastClear.secsTo(currentDateTime) >= 10)
            {
                //qDebug() << "Clearing ModeS only hash. size=" << m_modeSOnlyIcaos.size();
                m_modeSOnlyIcaos.clear();
                m_lastClear = currentDateTime;
            }
        }

        RXRecord rx;
        rx.m_data = QByteArray((char*)data, bytes);
        rx.m_firstIndex = firstIndex;
        rx.m_preamble = preamble;
        rx.m_preambleCorrelation = preambleCorrelation;
        rx.m_correlationOnes = correlationOnes;
        rx.m_dateTime = dateTime;
        rx.m_crc = crc;
        l.append(rx);
        m_modeSOnlyIcaos.insert(icao, l);

        // Have we heard from the same address several times in the last 10 seconds?
        if (l.size() >= 5)
        {
            // Check all frames have consistent altitudes and identifiers
            bool idConsistent = true;
            bool altitudeConsistent = true;
            int altitude = -1;
            int id = -1;

            for (int i = 0; i < l.size(); i++)
            {
                int df2 = ((l[i].m_data[0] >> 3) & ADS_B_DF_MASK);

                if ((df2 == 0) || (df2 == 4) || (df2 == 16) || (df2 == 20))
                {
                    int curAltitude = decodeModeSAltitude(l[i].m_data);
                    if (altitude == -1)
                    {
                        altitude = curAltitude;
                    }
                    else
                    {
                        if (abs(curAltitude - altitude) > 1000) {
                            altitudeConsistent = false;
                        }
                    }
                }
                else if ((df2 == 5) || (df2 == 21))
                {
                    int curId = ((data[2] & 0x1f) << 8) | (data[3] & 0xff); // No decode - we just want to know if it changes

                    if (id == -1)
                    {
                        id = curId;
                    }
                    else
                    {
                        if (id != curId) {
                            idConsistent = false;
                        }
                    }
                }
            }

            // FIXME: We could optionally check to see if aircraft ICAO is in db, but the db isn't complete

            if (altitudeConsistent && idConsistent)
            {
                // Forward all frames
                for (int i = 0; i < l.size(); i++) {
                    forwardFrame((const unsigned char *) l[i].m_data.data(), l[i].m_data.size(), l[i].m_preambleCorrelation, l[i].m_correlationOnes, l[i].m_dateTime, l[i].m_crc);
                }

                m_icaos.insert(icao, l.back().m_dateTime.toMSecsSinceEpoch());
            }
            else
            {
                m_modeSOnlyIcaos.remove(icao);
            }
        }
    }
}

// Check if a Mode S frame has reserved bits set or reserved field values
// Refer: Annex 10 Volume IV
bool ADSBDemodSinkWorker::modeSValid(unsigned char *data, unsigned df)
{
    bool invalid = false;

    if (df == 0)
    {
        invalid = ((data[0] & 0x1) | (data[1] & 0x18) | (data[2] & 0x60)) != 0;
    }
    else if ((df == 4) || (df == 20))
    {
        unsigned fs = data[0] & 0x3;
        unsigned dr = (data[1] >> 3) & 0x1f;

        invalid = (fs == 6) || (fs == 7) || ((dr >= 8) && (dr <= 15));
    }
    else if ((df == 5) || (df == 21))
    {
        unsigned fs = data[0] & 0x3;
        unsigned dr = (data[1] >> 3) & 0x1f;

        invalid = (fs == 6) || (fs == 7) || ((dr >= 8) && (dr <= 15)) || ((data[3] & 0x40) != 0);
    }
    else if (df == 11)
    {
        unsigned ca = data[0] & 0x7;

        invalid = ((ca >= 1) && (ca <= 3));
    }
    else if (df == 16)
    {
        invalid = ((data[0] & 0x3) | (data[1] & 0x18) | (data[2] & 0x60)) != 0;
    }

    return invalid;
}

// Check if valid ICAO address - i.e. not a reserved address - see Table 9-1 in Annex X Volume III
bool ADSBDemodSinkWorker::icaoValid(unsigned icao)
{
    unsigned msn = (icao >> 20) & 0xf;

    return (icao != 0) && (msn != 0xf) && (msn != 0xb) && (msn != 0xd);
}

// Is it less than a minute since the last received frame for this ICAO
bool ADSBDemodSinkWorker::icaoHeardRecently(unsigned icao, const QDateTime &dateTime)
{
    const int timeLimitMSec = 60*1000;

    if (m_icaos.contains(icao))
    {
        if ((dateTime.toMSecsSinceEpoch() - m_icaos.value(icao)) < timeLimitMSec) {
            return true;
        } else {
            m_icaos.remove(icao);
        }
    }

    return false;
}

void ADSBDemodSinkWorker::forwardFrame(const unsigned char *data, int size, float preambleCorrelation, float correlationOnes, const QDateTime& dateTime, unsigned crc)
{
    // Pass to GUI
    if (m_sink->getMessageQueueToGUI())
    {
        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(
            QByteArray((char*)data, size),
            preambleCorrelation,
            correlationOnes,
            dateTime,
            crc);
        m_sink->getMessageQueueToGUI()->push(msg);
    }
    // Pass to worker to feed to other servers
    if (m_sink->getMessageQueueToWorker())
    {
        ADSBDemodReport::MsgReportADSB *msg = ADSBDemodReport::MsgReportADSB::create(
            QByteArray((char*)data, size),
            preambleCorrelation,
            correlationOnes,
            dateTime,
            crc);
        m_sink->getMessageQueueToWorker()->push(msg);
    }
}

void ADSBDemodSinkWorker::run()
{
    int readBuffer = 0;
    m_lastClear = QDateTime::currentDateTime();

    // Acquire first buffer
    m_sink->m_bufferRead[readBuffer].acquire();

    if (isInterruptionRequested()) {
        return;
    }

    // Start recording how much time is spent processing in this method
    PROFILER_START();

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
         << " correlationThreshold: " << m_settings.m_correlationThreshold;

    int readIdx = m_sink->m_samplesPerFrame - 1;

    while (true)
    {
        int startIdx = readIdx;

        // Correlate received signal with expected preamble
        // chip+ indexes are 0, 2, 7, 9
        Real preambleCorrelationOnes = 0.0;
        Real preambleCorrelationZeros = 0.0;
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

        // Use the ratio of ones power over zeros power, as we don't care how powerful the signal
        // is, just whether there is a good correlation with the preamble. The absolute value varies
        // too much with different radios, AGC settings and and the noise floor is not constant
        // (E.g: it's quite possible to receive multiple frames simultaneously, so we don't
        // want a maximum threshold for the zeros, as a weaker signal may transmit 1s in
        // a stronger signals 0 chip position. Similarly a strong signal in an adjacent
        // channel may cause AGC to reduce gain, reducing the ampltiude of an otherwise
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
            int firstIndex = 0;

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
                    if (byteIdx == 0) {
                        firstIndex = startIdx;
                    }
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

            Real preambleCorrelationScaled = preambleCorrelation * m_correlationScale;
            Real correlationOnes = preambleCorrelationOnes / samplesPerChip;
            QDateTime dateTime = rxDateTime(firstIdx, readBuffer);

            // Is ADS-B?
            df = ((data[0] >> 3) & ADS_B_DF_MASK);
            if ((df == 17) || (df == 18))
            {
                m_crc.init();
                unsigned int parity = (data[11] << 16) | (data[12] << 8) | data[13]; // Parity / CRC

                m_crc.calculate(data, ADS_B_ES_BYTES-3);
                if (parity == m_crc.get())
                {
                    // Get 24-bit ICAO
                    unsigned icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff);

                    if (icaoValid(icao))
                    {
                        // Got a valid frame
                        m_demodStats.m_adsbFrames++;
                        // Save in hash of ICAOs that have been seen
                        m_icaos.insert(icao, dateTime.toMSecsSinceEpoch());
                        // Don't try to re-demodulate the same frame
                        // We could possibly allow a partial overlap here
                        readIdx += (ADS_B_ES_BITS+ADS_B_PREAMBLE_BITS)*ADS_B_CHIPS_PER_BIT*samplesPerChip - 1;
                        forwardFrame(data, sizeof(data), preambleCorrelationScaled, correlationOnes, dateTime, m_crc.get());
                    }
                    else
                    {
                        m_demodStats.m_icaoFails++;
                    }
                }
                else
                {
                    m_demodStats.m_crcFails++;
                }
            }
            else if (m_settings.m_demodModeS)
            {
                // Decode premable, as correlation alone results in too many false positives for Mode S frames
                startIdx = readIdx;
                unsigned short preamble = 0;
                QVector<float> preambleChips(16);

                for (int bit = 0; bit < ADS_B_PREAMBLE_BITS; bit++)
                {
                    preambleChips[bit*2] = 0.0f;
                    preambleChips[bit*2+1] = 0.0f;
                    for (int i = 0; i < samplesPerChip; i++)
                    {
                        preambleChips[bit*2] += m_sink->m_sampleBuffer[readBuffer][startIdx+i];
                        preambleChips[bit*2+1] += m_sink->m_sampleBuffer[readBuffer][startIdx+samplesPerChip+i];
                    }

                    startIdx += samplesPerBit;
                }
                startIdx = firstIdx;

                float onesAvg = (preambleChips[0] + preambleChips[2] + preambleChips[7] + preambleChips[9]) / 4.0f;
                float zerosAvg = (preambleChips[1] + preambleChips[3] + preambleChips[4] + preambleChips[5] + preambleChips[6] + preambleChips[8]
                    + preambleChips[10] +  + preambleChips[11] + preambleChips[12] + preambleChips[13] + preambleChips[14] + preambleChips[15]) / 12.0f;
                float midPoint = zerosAvg + (onesAvg - zerosAvg) / 2.0f;
                for (int i = 0; i < 16; i++)
                {
                    unsigned chip = preambleChips[i] > midPoint;
                    preamble |= chip << (15-i);
                }

                //    qDebug() << "Preamble" << preambleChips << "zerosAvg" << zerosAvg << "onesAvg" << onesAvg << "midPoint" << midPoint << "chips" << Qt::hex << preamble;

                const unsigned short expectedPreamble = 0xa140;
                int preambleDifferences = popcount(expectedPreamble ^ preamble);

                if (preambleDifferences <= m_settings.m_chipsThreshold)
                {
                    int bytes;

                    // Determine number of bytes in frame depending on downlink format
                    if ((df == 0) || (df == 4) || (df == 5) || (df == 11)) {
                        bytes = 56/8;
                    } else if ((df == 16) || (df == 19) || (df == 20) || (df == 21) || (df == 22) || ((df >= 24) && (df <= 27))) {
                        bytes = 112/8;
                    } else {
                        bytes = 0;
                    }

                    if (bytes > 0)
                    {
                        bool invalid = modeSValid(data, df);

                        if (!invalid)
                        {
                            // Extract received parity
                            int parity = (data[bytes-3] << 16) | (data[bytes-2] << 8) | data[bytes-1];
                            // Calculate CRC on received frame
                            m_crc.init();
                            m_crc.calculate(data, bytes-3);
                            int crc = m_crc.get();
                            bool forward = false;

                            // ICAO address XORed in to parity, apart from DF11
                            // Extract ICAO from parity and see if it matches an aircraft we've already
                            // received an ADS-B or Mode S frame from
                            // For DF11, the last 7 bits may have an iterogration code (II/SI)
                            // XORed in, so we ignore those bits. This does sometimes lead to false-positives
                            if (df != 11)
                            {
                                unsigned icao = (parity ^ crc) & 0xffffff;

                                if (icaoValid(icao))
                                {
                                    if (icaoHeardRecently(icao, dateTime)) {
                                        forward = true;
                                    } else {
                                        handleModeS(data, bytes, icao, df, firstIndex, preamble, preambleCorrelationScaled, correlationOnes, dateTime, m_crc.get());
                                    }
                                }
                                else
                                {
                                    m_demodStats.m_icaoFails++;
                                }
                            }
                            else
                            {
                                // Ignore IC bits
                                parity &= 0xffff80;
                                crc &= 0xffff80;
                                if (parity == crc)
                                {
                                    // Get 24-bit ICAO
                                    unsigned icao = ((data[1] & 0xff) << 16) | ((data[2] & 0xff) << 8) | (data[3] & 0xff);

                                    if (icaoValid(icao))
                                    {
                                        if (icaoHeardRecently(icao, dateTime)) {
                                            forward = true;
                                        } else {
                                            handleModeS(data, bytes, icao, df, firstIndex, preamble, preambleCorrelationScaled, correlationOnes, dateTime, m_crc.get());
                                        }
                                    }
                                    else
                                    {
                                        m_demodStats.m_icaoFails++;
                                    }
                                }
                                else
                                {
                                    m_demodStats.m_crcFails++;
                                }
                            }
                            if (forward)
                            {
                                m_demodStats.m_modesFrames++;
                                // Don't try to re-demodulate the same frame
                                // We could possibly allow a partial overlap here
                                readIdx += ((bytes*8)+ADS_B_PREAMBLE_BITS)*ADS_B_CHIPS_PER_BIT*samplesPerChip - 1;
                                forwardFrame(data, bytes, preambleCorrelationScaled, correlationOnes, dateTime, m_crc.get());
                            }
                            else
                            {
                                m_demodStats.m_crcFails++;
                            }
                        }
                        else
                        {
                            m_demodStats.m_invalidFails++;
                        }
                    }
                    else
                    {
                        m_demodStats.m_typeFails++;
                    }
                }
                else
                {
                    m_demodStats.m_preambleFails++;
                }
            }
            else
            {
                m_demodStats.m_typeFails++;
            }
        }

        readIdx++;
        if (readIdx > m_sink->m_bufferSize - samplesPerFrame)
        {
            int nextBuffer = readBuffer+1;
            if (nextBuffer >= m_sink->m_buffers)
                nextBuffer = 0;

            // Update amount of time spent processing (don't include time spend in acquire)
            PROFILER_STOP("ADS-B demod");

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
                PROFILER_RESTART();

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
            QStringList settingsKeys = cfg->getSettingsKeys();
            bool force = cfg->getForce();

            if ((settingsKeys.contains("correlationThreshold") && (m_settings.m_correlationThreshold != settings.m_correlationThreshold)) || force)
            {
                m_correlationThresholdLinear = CalcDb::powerFromdB(settings.m_correlationThreshold);
                m_correlationThresholdLinear /= m_correlationScale;
                qDebug() << "m_correlationThresholdLinear: " << m_correlationThresholdLinear;
            }

            if (force) {
                m_settings = settings;
            } else {
                m_settings.applySettings(settingsKeys, settings);
            }
            delete message;
        }
        else if (ADSBDemod::MsgResetStats::match(*message))
        {
            m_demodStats.reset();
        }
    }
}

QDateTime ADSBDemodSinkWorker::rxDateTime(int firstIdx, int readBuffer) const
{
    const qint64 samplesPerSecondMSec = ADS_B_BITS_PER_SECOND * m_settings.m_samplesPerBit / 1000;
    const qint64 offsetMSec = (firstIdx - m_sink->m_samplesPerFrame - 1) / samplesPerSecondMSec;
    return m_sink->m_bufferFirstSampleDateTime[readBuffer].addMSecs(offsetMSec);
}
