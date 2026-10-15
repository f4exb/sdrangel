///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2021-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include <algorithm>
#include <complex.h>
#include <cmath>

#include "dsp/scopevis.h"
#include "util/db.h"

#include "radioclock.h"
#include "pcsk225decoder.h"
#include "radioclocksink.h"

RadioClockSink::RadioClockSink() :
        m_scopeSink(nullptr),
        m_channel(nullptr),
        m_channelSampleRate(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsq(0.0),
        m_magsqSum(0.0),
        m_magsqPeak(0.0),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_data(0),
        m_prevData(0),
        m_sample(0),
        m_lowCount(0),
        m_highCount(0),
        m_periodCount(0),
        m_gotMinuteMarker(false),
        m_second(0),
        m_timeCode{},
        m_secondMarkers(0),
        m_threshold(0),
        m_dst(RadioClockSettings::UNKNOWN),
        m_timeCodeB{},
        m_zeroCount(0),
        m_bits{},
        m_sampleBufferIndex(0),
        m_gotMarker(false),
        m_rbuPrevSample(0.0f, 0.0f),
        m_rbuHavePrevSample(false),
        m_rbuHaveSymbolTiming(false),
        m_rbuSymbolSample(0),
        m_rbuCarrierLowCount(0),
        m_rbuTimingErrors(0),
        m_rbu100Real(0.0),
        m_rbu100Imag(0.0),
        m_rbu312Real(0.0),
        m_rbu312Imag(0.0),
        m_rbuCorrelationSamples(0),
        m_rbuRecentBits(0),
        m_rbuRecentBitCount(0),
        m_rbuBit(0),
        m_rbuSecondValid(true),
        m_rbuInvalidSeconds(0),
        m_pcskPrevSample(0.0f, 0.0f),
        m_pcskHavePrevSample(false),
        m_pcskUnwrappedPhase(0.0),
        m_pcskPhaseHistory{},
        m_pcskSampleCount(0),
        m_pcskSyncCandidate(false),
        m_pcskSyncQuietSamples(0),
        m_pcskSyncScore(0.0),
        m_pcskSyncEndSample(0),
        m_pcskSyncIntercept(0.0),
        m_pcskSyncSlope(0.0),
        m_pcskSyncSeparation(0.0),
        m_pcskCollectingFrame(false),
        m_pcskBit(0),
        m_pcskFrameStartSample(0),
        m_pcskNextSymbolSample(0),
        m_pcskPhaseIntercept(0.0),
        m_pcskPhaseSlope(0.0),
        m_pcskPhaseSeparation(0.0),
        m_pcskFrame{},
        m_pcskReferenceSample(0),
        m_pcskNextTimeReportSample(0),
        m_pcskLastValidFrameSample(0)
{
    m_phaseDiscri.setFMScaling(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / (2.0f * 20.0/M_PI));
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);

    for (int i = 0; i < RadioClockSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }
}

RadioClockSink::~RadioClockSink()
{
}

void RadioClockSink::setScopeSink(ScopeVis* scopeSink)
{
    m_scopeSink = scopeSink;
}

void RadioClockSink::sampleToScope(Complex sample)
{
    if (m_scopeSink)
    {
        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(m_magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(m_threshold, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(m_fmDemodMovingAverage.asDouble(), 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(m_data, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(m_sample, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(m_gotMinuteMarker, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(m_gotMarker, 0.0f);
        m_sampleBufferIndex++;

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < RadioClockSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void RadioClockSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

// Extract binary-coded decimal from time code - LSB first
int RadioClockSink::bcd(int firstBit, int lastBit)
{
    const int vals[] = {1, 2, 4, 8, 10, 20, 40, 80};
    int idx = 0;
    int val = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        if (m_timeCode[i]) {
            val += vals[idx];
        }
        idx++;
    }
    return val;
}

// Extract binary-coded decimal from time code - MSB first
int RadioClockSink::bcdMSB(int firstBit, int lastBit, int skipBit1, int skipBit2)
{
    const int vals[] = {1, 2, 4, 8, 10, 20, 40, 80, 100, 200};
    int idx = 0;
    int val = 0;
    for (int i = lastBit; i >= firstBit; i--)
    {
        if ((i != skipBit1) && (i != skipBit2)) {
            if (m_timeCode[i]) {
                val += vals[idx];
            }
            idx++;
        }
    }
    return val;
}

// XOR bits together for parity check
int RadioClockSink::xorBits(int firstBit, int lastBit)
{
    int x = 0;
    for (int i = firstBit; i <= lastBit; i++)
    {
        x ^= m_timeCode[i];
    }
    return x;
}

bool RadioClockSink::evenParity(int firstBit, int lastBit, int parityBit)
{
    return xorBits(firstBit, lastBit) == parityBit;
}

bool RadioClockSink::oddParity(int firstBit, int lastBit, int parityBit)
{
    return xorBits(firstBit, lastBit) != parityBit;
}

// German DCF77
// https://en.wikipedia.org/wiki/DCF77
void RadioClockSink::dcf77()
{
    // DCF77 reduces carrier by -16.5dB
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - 59th second carrier is held high
    if ((m_data == 0) && (m_prevData == 1))
    {
        if (   (m_highCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 2)
            && (m_highCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 1.6)
            && (m_lowCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.3)
            && (m_lowCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.1)
           )
        {
            qDebug() << "RadioClockSink::dcf77 - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 0;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }

        m_lowCount = 0;
    }
    else if ((m_data == 1) && (m_prevData == 0))
    {
        m_highCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 50)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 0;
            // If we see too many 1s instead of 0s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::dcf77 - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 150)
        {
            // Get data for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to a time and date
                int minute = bcd(21, 27);
                int hour = bcd(29, 34);
                int day = bcd(36, 41);
                int month = bcd(45, 49);
                int year = 2000 + bcd(50, 57);

                QString parityError;
                if (!evenParity(21, 27, m_timeCode[28])) {
                    parityError = "Minute parity error";
                }
                if (!evenParity(29, 34, m_timeCode[35])) {
                    parityError = "Hour parity error";
                }
                if (!evenParity(36, 57, m_timeCode[58])) {
                    parityError= "Data parity error";
                }

                // Daylight savings
                if (m_timeCode[17] && m_timeCode[16]) {
                    m_dst = RadioClockSettings::ENDING;
                } else if (m_timeCode[17]) {
                    m_dst = RadioClockSettings::IN_EFFECT;
                } else if (m_timeCode[18] && m_timeCode[16]) {
                    m_dst = RadioClockSettings::STARTING;
                } else if (m_timeCode[18]) {
                    m_dst = RadioClockSettings::NOT_IN_EFFECT;
                } else {
                    m_dst = RadioClockSettings::UNKNOWN;
                }

                if (parityError.isEmpty())
                {
                    // Bit 17 indicates CEST rather than CET
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCode[17] ? 2*3600 : 3600);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime, m_dst);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }
    m_prevData = m_data;
}

// French TDF 162kHz
// https://en.wikipedia.org/wiki/TDF_time_signal
// Uses phase modulation, rather than OOK
void RadioClockSink::tdf(Complex &ci)
{
    // FM demodulation
    double magsqRaw;
    Real deviation;
    Real fmDemod = m_phaseDiscri.phaseDiscriminatorDelta(ci, magsqRaw, deviation);

    // Filter
    m_fmDemodMovingAverage(fmDemod);

    // Ternary encoding
    Real avg = m_fmDemodMovingAverage.asDouble();
    if (avg >= 0.5) {
        m_data = 1;
    } else if (avg <= -0.5) {
        m_data = -1;
    } else {
        m_data = 0;
    }

    // Look for minute marker - 59th second is not phase modulated
    if ((m_data == 1) && (m_prevData == 0))
    {
        if (   (m_zeroCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 2)
            && (m_zeroCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 1)
           )
        {
            qDebug() << "RadioClockSink::tdf - Minute marker: (zero " << m_zeroCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 0;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }
    }
    else if ((m_data == 0) && (m_prevData != 0))
    {
        m_zeroCount = 0;
    }
    else if (m_data == 0)
    {
        m_zeroCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 12)
        {
            m_bits[0] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+50)
        {
            m_bits[1] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+100)
        {
            m_bits[2] = m_data;
            m_sample = true;
        }
        else if (m_periodCount == 12+150)
        {
            m_bits[3] = m_data;
            m_sample = true;

            // Check we got second marker
            m_secondMarkers += ((m_bits[0] == 1) && (m_bits[1] == -1));
            // If too many second markers are missing, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::tdf - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }

            // No phase modulation from 50ms to 150ms is 0, pos then neg is 1
            if ((m_bits[2] == 0) && (m_bits[3] == 0)) {
                m_timeCode[m_second] = 0;
            } else if ((m_bits[2] == 1) && (m_bits[3] == -1)) {
                m_timeCode[m_second] = 1;
            } else {
                //qDebug() << "Unexpected modulation " << m_second;
            }
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to time and date
                int minute = bcd(21, 27);
                int hour = bcd(29, 34);
                int day = bcd(36, 41);
                int month = bcd(45, 49);
                int year = 2000 + bcd(50, 57);

                // Daylight savings
                if (m_timeCode[17] && m_timeCode[16]) {
                    m_dst = RadioClockSettings::ENDING;
                } else if (m_timeCode[17]) {
                    m_dst = RadioClockSettings::IN_EFFECT;
                } else if (m_timeCode[18] && m_timeCode[16]) {
                    m_dst = RadioClockSettings::STARTING;
                } else if (m_timeCode[18]) {
                    m_dst = RadioClockSettings::NOT_IN_EFFECT;
                } else {
                    m_dst = RadioClockSettings::UNKNOWN;
                }

                QString parityError;
                if (!evenParity(21, 27, m_timeCode[28])) {
                    parityError = "Minute parity error";
                }
                if (!evenParity(29, 34, m_timeCode[35])) {
                    parityError = "Hour parity error";
                }
                if (!evenParity(36, 57, m_timeCode[58])) {
                    parityError= "Data parity error";
                }

                if (parityError.isEmpty())
                {
                    // Bit 17 indicates CEST rather than CET
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCode[17] ? 2*3600 : 3600);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime, m_dst);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }
    m_prevData = m_data;
}

// UK MSF 60kHz
// https://www.npl.co.uk/products-services/time-frequency/msf-radio-time-signal/msf_time_date_code
void RadioClockSink::msf60()
{
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - 500ms low, then 500ms high
    if ((m_data == 0) && (m_prevData == 1))
    {
        if (   (m_highCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.6)
            && (m_highCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.4)
            && (m_lowCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.6)
            && (m_lowCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.4)
           )
        {
            qDebug() << "RadioClockSink::msf60 - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
            if (getMessageQueueToChannel() && !m_gotMinuteMarker) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
            m_periodCount = 0;
            m_second = 1;
            m_gotMinuteMarker = true;
            m_secondMarkers = 1;
        }
        m_lowCount = 0;
    }
    else if ((m_data == 1) && (m_prevData == 0))
    {
        m_highCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 50)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 0;
            // If we see too many 1s instead of 0s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::msf60 - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 150)
        {
            // Get data bit A for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 250)
        {
            // Get data bit B for timecode
            m_timeCodeB[m_second] = !m_data;
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Decode timecode to time and date
                int minute = bcdMSB(45, 51);
                int hour = bcdMSB(39, 44);
                int day = bcdMSB(30, 35);
                //int dayOfWeek = bcdMSB(36, 38);
                int month = bcdMSB(25, 29);
                int year = 2000 + bcdMSB(17, 24);

                // Daylight savings
                if (m_timeCodeB[58] && m_timeCodeB[53]) {
                    m_dst = RadioClockSettings::ENDING;
                } else if (m_timeCodeB[58]) {
                    m_dst = RadioClockSettings::IN_EFFECT;
                } else if (m_timeCodeB[53]) {
                    m_dst = RadioClockSettings::STARTING;
                } else {
                    m_dst = RadioClockSettings::NOT_IN_EFFECT;
                }

                QString parityError;
                if (!oddParity(39, 51, m_timeCodeB[57])) {
                    parityError = "Hour/minute parity error";
                }
                if (!oddParity(25, 35, m_timeCodeB[55])) {
                    parityError= "Day/month parity error";
                }
                if (!oddParity(17, 24, m_timeCodeB[54])) {
                    parityError = "Hour/minute parity error";
                }

                if (parityError.isEmpty())
                {
                    // Bit 58B indicates BST rather than GMT
                    m_dateTime = QDateTime(QDate(year, month, day), QTime(hour, minute), Qt::OffsetFromUTC, m_timeCodeB[58] ? 1*3600 : 0);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                    }
                }
                else
                {
                    m_dateTime = m_dateTime.addSecs(1);
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(parityError));
                    }
                }
                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime, m_dst);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }

    m_prevData = m_data;
}

// USA WWVB 60kHz
// https://en.wikipedia.org/wiki/WWVB
void RadioClockSink::wwvb()
{
    // WWVB reduces carrier by -17dB
    // 0.2s reduction is zero bit, 0.5s reduction is one bit
    // 0.8s reduction is a marker. Seven markers per minute (0, 9, 19, 29, 39, 49, and 59s) and for leap second
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - two consecutive markers
    if ((m_data == 0) && (m_prevData == 1))
    {
        if (   (m_highCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.3)
            && (m_lowCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.7)
           )
        {
            if (m_gotMarker && !m_gotMinuteMarker)
            {
                qDebug() << "RadioClockSink::wwvb - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
                m_gotMinuteMarker = true;
                m_second = 1;
                m_secondMarkers = 1;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
                }
            }
            else
            {
                qDebug() << "RadioClockSink::wwvb - Marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount << " second " << m_second;
            }
            m_gotMarker = true;
            m_periodCount = 0;
        }
        else
        {
            m_gotMarker = false;
        }
        m_lowCount = 0;
    }
    else if ((m_data == 1) && (m_prevData == 0))
    {
        m_highCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 100)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 0;
            // If we see too many 1s instead of 0s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::wwvb - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 350)
        {
            // Get data bit A for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Check markers are decoded as 1s
                const QList<int> markerBits = {9, 19, 29, 39, 49, 59};
                int missingMarkers = 0;
                for (int i = 0; i < markerBits.size(); i++)
                {
                    if (m_timeCode[markerBits[i]] != 1)
                    {
                        missingMarkers++;
                        qDebug() << "RadioClockSink::wwvb - Missing marker at bit " << markerBits[i];
                    }
                }
                if (missingMarkers >= 3)
                {
                    m_gotMinuteMarker = false;
                    qDebug() << "RadioClockSink::wwvb - Lost lock: Missing markers: " << missingMarkers;
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                    }
                }

                // Check 0s where expected
                const QList<int> zeroBits = {4, 10, 11, 14, 20, 21, 24, 34, 35, 44, 54};
                for (int i = 0; i < zeroBits.size(); i++)
                {
                    if (m_timeCode[zeroBits[i]] != 0) {
                        qDebug() << "RadioClockSink::wwvb - Unexpected 1 at bit " << zeroBits[i];
                    }
                }

                // Decode timecode to time and date
                int minute = bcdMSB(1, 8, 4);
                int hour = bcdMSB(12, 18, 14);
                int dayOfYear = bcdMSB(22, 33, 24, 29);
                int year = 2000 + bcdMSB(45, 53, 49);

                // Daylight savings
                int dst = (m_timeCode[57] << 1) | m_timeCode[58];
                switch (dst)
                {
                case 0:
                    m_dst = RadioClockSettings::NOT_IN_EFFECT;
                    break;
                case 1:
                    m_dst = RadioClockSettings::ENDING;
                    break;
                case 2:
                    m_dst = RadioClockSettings::STARTING;
                    break;
                case 3:
                    m_dst = RadioClockSettings::IN_EFFECT;
                    break;
                }

                // Time is UTC
                QDate date(year, 1, 1);
                date = date.addDays(dayOfYear - 1);
                m_dateTime = QDateTime(date, QTime(hour, minute), Qt::OffsetFromUTC, 0);
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                }

                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime, m_dst);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }

    m_prevData = m_data;
}

// Japan JJY 40kHz
// https://en.wikipedia.org/wiki/JJY
void RadioClockSink::jjy()
{
    // JJY reduces carrier by -10dB
    // Full power, then reduced power, which is the opposite of WWVB
    // 0.8s full power is is zero bit, 0.5s full power is one bit
    // 0.2s full power is a marker. Seven markers per minute (0, 9, 19, 29, 39, 49, and 59s) and for leap second
    m_threshold = m_thresholdMovingAverage.asDouble() * m_linearThreshold; // xdB below average
    m_data = m_magsq > m_threshold;

    // Look for minute marker - two consecutive markers
    if ((m_data == 1) && (m_prevData == 0))
    {
        if (   (m_highCount <= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.3)
            && (m_lowCount >= RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE * 0.7)
           )
        {
            if (m_gotMarker && !m_gotMinuteMarker)
            {
                qDebug() << "RadioClockSink::jjy - Minute marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount;
                m_gotMinuteMarker = true;
                m_second = 1;
                m_secondMarkers = 1;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
                }
            }
            else
            {
                qDebug() << "RadioClockSink::jjy - Marker: (low " << m_lowCount << " high " << m_highCount << ") prev period " << m_periodCount << " second " << m_second;
            }
            m_gotMarker = true;
            m_periodCount = 0;
        }
        else
        {
            m_gotMarker = false;
        }
        m_highCount = 0;
    }
    else if ((m_data == 0) && (m_prevData == 1))
    {
        m_lowCount = 0;
    }
    else if (m_data == 1)
    {
        m_highCount++;
    }
    else if (m_data == 0)
    {
        m_lowCount++;
    }

    m_sample = false;
    if (m_gotMinuteMarker)
    {
        m_periodCount++;
        if (m_periodCount == 100)
        {
            // Check we get second marker
            m_secondMarkers += m_data == 1;
            // If we see too many 0s instead of 1s, assume we've lost the signal
            if ((m_second > 10) && (m_secondMarkers / m_second < 0.7))
            {
                qDebug() << "RadioClockSink::jjy - Lost lock: " << m_secondMarkers << m_second;
                m_gotMinuteMarker = false;
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                }
            }
            m_sample = true;
        }
        else if (m_periodCount == 650)
        {
            // Get data bit A for timecode
            m_timeCode[m_second] = !m_data; // No carrier = 1, carrier = 0
            m_sample = true;
        }
        else if (m_periodCount == 950)
        {
            if (m_second == 59)
            {
                // Check markers are decoded as 1s
                const QList<int> markerBits = {9, 19, 29, 39, 49, 59};
                int missingMarkers = 0;
                for (int i = 0; i < markerBits.size(); i++)
                {
                    if (m_timeCode[markerBits[i]] != 1)
                    {
                        missingMarkers++;
                        qDebug() << "RadioClockSink::jjy - Missing marker at bit " << markerBits[i];
                    }
                }
                if (missingMarkers >= 3)
                {
                    m_gotMinuteMarker = false;
                    qDebug() << "RadioClockSink::jjy - Lost lock: Missing markers: " << missingMarkers;
                    if (getMessageQueueToChannel()) {
                        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
                    }
                }

                // Check 0s where expected
                const QList<int> zeroBits = {4, 10, 11, 14, 20, 21, 24, 34, 35, 44, 55, 56, 57, 58};
                for (int i = 0; i < zeroBits.size(); i++)
                {
                    if (m_timeCode[zeroBits[i]] != 0) {
                        qDebug() << "RadioClockSink::jjy - Unexpected 1 at bit " << zeroBits[i];
                    }
                }

                // Decode timecode to time and date
                int minute = bcdMSB(1, 8, 4);
                int hour = bcdMSB(12, 18, 14);
                int dayOfYear = bcdMSB(22, 33, 24, 29);
                int year = 2000 + bcdMSB(41, 48);

                // Japan doesn't have daylight savings
                m_dst = RadioClockSettings::NOT_IN_EFFECT;

                // Time is UTC
                QDate date(year, 1, 1);
                date = date.addDays(dayOfYear - 1);
                m_dateTime = QDateTime(date, QTime(hour, minute), Qt::OffsetFromUTC, 0);
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                }

                m_second = 0;
            }
            else
            {
                m_second++;
                m_dateTime = m_dateTime.addSecs(1);
            }

            if (getMessageQueueToChannel())
            {
                RadioClock::MsgDateTime *msg = RadioClock::MsgDateTime::create(m_dateTime, m_dst);
                getMessageQueueToChannel()->push(msg);
            }
        }
        else if (m_periodCount == 1000)
        {
            m_periodCount = 0;
        }
    }

    m_prevData = m_data;
}

// Russian RBU 66.666...kHz
// GOST 8.515-2016 and ITU-R TF.2487-0
// Each 100ms symbol contains 80ms of phase modulation at 100Hz (0) or
// 312.5Hz (1), followed by a 5ms carrier interruption used for timing.
void RadioClockSink::rbu(Complex& ci, Real magsq)
{
    const int symbolSamples = RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 10;
    const int correlationStart = RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 100;
    const int correlationEnd = 9 * RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 100;
    const double twoPi = 2.0 * M_PI;

    Real averagePower = m_thresholdMovingAverage.instantAverage();
    m_threshold = averagePower * m_linearThreshold;
    bool carrierPresent = magsq > m_threshold;

    Real phaseDelta = 0.0f;
    if (m_rbuHavePrevSample) {
        phaseDelta = std::arg(ci * std::conj(m_rbuPrevSample));
    }
    m_rbuPrevSample = ci;
    m_rbuHavePrevSample = true;
    m_fmDemodMovingAverage(phaseDelta);

    // Accept a carrier recovery after an interruption as a symbol edge. Once
    // timing is acquired the clock free-runs at 10Hz, with valid interruptions
    // correcting its phase. This avoids losing a whole frame on a single faded
    // or noise-filled 5ms notch.
    if (!carrierPresent)
    {
        m_rbuCarrierLowCount++;
    }
    else if (m_rbuCarrierLowCount > 0)
    {
        bool validDrop = (m_rbuCarrierLowCount >= 2) && (m_rbuCarrierLowCount <= 30);
        bool expectedEdge = !m_rbuHaveSymbolTiming
            || (m_rbuSymbolSample >= symbolSamples - 15)
            || (m_rbuSymbolSample <= 15);

        if (validDrop && expectedEdge)
        {
            m_rbuHaveSymbolTiming = true;
            m_rbuTimingErrors = 0;
            m_rbuSymbolSample = 0;
            m_rbu100Real = 0.0;
            m_rbu100Imag = 0.0;
            m_rbu312Real = 0.0;
            m_rbu312Imag = 0.0;
            m_rbuCorrelationSamples = 0;
        }
        else if (validDrop)
        {
            m_rbuTimingErrors++;
        }

        m_rbuCarrierLowCount = 0;
    }

    m_sample = false;

    if (!m_rbuHaveSymbolTiming) {
        return;
    }

    if ((m_rbuSymbolSample >= correlationStart) && (m_rbuSymbolSample < correlationEnd))
    {
        int n = m_rbuSymbolSample - correlationStart;
        double phase100 = twoPi * 100.0 * n / RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        double phase312 = twoPi * 312.5 * n / RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_rbu100Real += phaseDelta * std::cos(phase100);
        m_rbu100Imag -= phaseDelta * std::sin(phase100);
        m_rbu312Real += phaseDelta * std::cos(phase312);
        m_rbu312Imag -= phaseDelta * std::sin(phase312);
        m_rbuCorrelationSamples++;
    }
    else if (m_rbuSymbolSample == correlationEnd)
    {
        if (m_rbuCorrelationSamples == correlationEnd - correlationStart)
        {
            double power100 = m_rbu100Real * m_rbu100Real + m_rbu100Imag * m_rbu100Imag;
            double power312 = m_rbu312Real * m_rbu312Real + m_rbu312Imag * m_rbu312Imag;
            m_data = power312 > power100 ? 1 : 0;
            m_sample = true;
            rbuProcessBit(m_data);
        }
    }

    m_rbuSymbolSample++;

    if (m_rbuTimingErrors >= 3)
    {
        qDebug() << "RadioClockSink::rbu - Repeated symbol timing errors";
        rbuReset(true);
    }
    else if (m_rbuSymbolSample >= symbolSamples)
    {
        m_rbuSymbolSample = 0;
        m_rbu100Real = 0.0;
        m_rbu100Imag = 0.0;
        m_rbu312Real = 0.0;
        m_rbu312Imag = 0.0;
        m_rbuCorrelationSamples = 0;
    }
}

void RadioClockSink::rbuProcessBit(int bit)
{
    m_rbuRecentBits = ((m_rbuRecentBits << 1) | (bit & 1)) & 0x3ff;
    if (m_rbuRecentBitCount < 10) {
        m_rbuRecentBitCount++;
    }

    if (!m_gotMinuteMarker)
    {
        // Second 59 contains five fixed zero symbols followed by the three
        // marker ones; the first two symbols of second 00 are also ones.
        // Matching the full 0000011111 sequence avoids false locks on noise.
        if ((m_rbuRecentBitCount == 10) && (m_rbuRecentBits == 0x1f))
        {
            qDebug() << "RadioClockSink::rbu - Minute marker";
            m_gotMinuteMarker = true;
            m_second = 0;
            m_rbuBit = 2; // Bits 0 and 1 of second 00 formed the marker.
            m_timeCode[0] = 1;
            m_timeCodeB[0] = 1;
            m_rbuSecondValid = true;
            m_rbuInvalidSeconds = 0;
            m_dst = RadioClockSettings::UNKNOWN;
            if (getMessageQueueToChannel()) {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got minute marker"));
            }
        }
        return;
    }

    if (m_rbuBit == 0)
    {
        m_timeCode[m_second] = bit;
        if ((m_second == 0) && (bit != 1)) {
            m_rbuSecondValid = false;
        }
    }
    else if (m_rbuBit == 1)
    {
        m_timeCodeB[m_second] = bit;
        if ((m_second == 0) && (bit != 1)) {
            m_rbuSecondValid = false;
        }
    }
    else if ((m_rbuBit >= 2) && (m_rbuBit <= 6))
    {
        if (bit != 0) {
            m_rbuSecondValid = false;
        }
    }
    else if ((m_rbuBit == 7) || (m_rbuBit == 8))
    {
        int expected = m_second == 59 ? 1 : 0;
        if (bit != expected) {
            m_rbuSecondValid = false;
        }
    }
    else if ((m_rbuBit == 9) && (bit != 1))
    {
        m_rbuSecondValid = false;
    }

    if (m_rbuBit == 9)
    {
        if (m_rbuSecondValid) {
            m_rbuInvalidSeconds = 0;
        } else {
            m_rbuInvalidSeconds++;
        }

        if (m_rbuInvalidSeconds >= 3)
        {
            qDebug() << "RadioClockSink::rbu - Lost lock: invalid second structure";
            rbuReset(false);
            return;
        }

        if (m_second == 59)
        {
            QString error;
            if (rbuDecodeTimeCode(error))
            {
                if (getMessageQueueToChannel()) {
                    getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("OK"));
                }
            }
            else if (getMessageQueueToChannel())
            {
                getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(error));
            }
            m_second = 0;
        }
        else
        {
            m_second++;
            if (m_dateTime.isValid()) {
                m_dateTime = m_dateTime.addSecs(1);
            }
        }

        if (getMessageQueueToChannel() && m_dateTime.isValid()) {
            getMessageQueueToChannel()->push(RadioClock::MsgDateTime::create(m_dateTime, m_dst));
        }

        m_rbuBit = 0;
        m_rbuSecondValid = true;
    }
    else
    {
        m_rbuBit++;
    }
}

bool RadioClockSink::rbuDecodeTimeCode(QString& error)
{
    auto weightedValue = [](const int *code, int firstBit, const int *weights, int count) {
        int value = 0;
        for (int i = 0; i < count; i++) {
            value += code[firstBit + i] ? weights[i] : 0;
        }
        return value;
    };
    auto parity = [](const int *code, int firstBit, int lastBit, int parityBit) {
        int value = parityBit;
        for (int i = firstBit; i <= lastBit; i++) {
            value ^= code[i];
        }
        return value == 0;
    };

    bool parityOK = parity(m_timeCodeB, 18, 25, m_timeCodeB[49])
        && parity(m_timeCodeB, 26, 33, m_timeCodeB[50])
        && parity(m_timeCode, 18, 23, m_timeCodeB[53])
        && parity(m_timeCode, 25, 32, m_timeCodeB[54])
        && parity(m_timeCode, 33, 40, m_timeCodeB[55])
        && parity(m_timeCode, 41, 46, m_timeCodeB[56])
        && parity(m_timeCode, 47, 52, m_timeCodeB[57])
        && parity(m_timeCode, 53, 59, m_timeCodeB[58]);

    if (!parityOK)
    {
        error = "Parity error";
        return false;
    }

    static const int yearWeights[] = {80, 40, 20, 10, 8, 4, 2, 1};
    static const int monthWeights[] = {10, 8, 4, 2, 1};
    static const int weekdayWeights[] = {4, 2, 1};
    static const int dayWeights[] = {20, 10, 8, 4, 2, 1};
    static const int hourWeights[] = {20, 10, 8, 4, 2, 1};
    static const int minuteWeights[] = {40, 20, 10, 8, 4, 2, 1};
    static const int offsetWeights[] = {10, 8, 4, 2, 1};
    static const int tjdWeights[] = {8000, 4000, 2000, 1000, 800, 400, 200, 100, 80, 40, 20, 10, 8, 4, 2, 1};

    int year = 2000 + weightedValue(m_timeCode, 25, yearWeights, 8);
    int month = weightedValue(m_timeCode, 33, monthWeights, 5);
    int weekday = weightedValue(m_timeCode, 38, weekdayWeights, 3);
    int day = weightedValue(m_timeCode, 41, dayWeights, 6);
    int hour = weightedValue(m_timeCode, 47, hourWeights, 6);
    int minute = weightedValue(m_timeCode, 53, minuteWeights, 7);
    int offsetHours = weightedValue(m_timeCode, 19, offsetWeights, 5);
    int tjd = weightedValue(m_timeCodeB, 18, tjdWeights, 16);
    if (m_timeCode[18]) {
        offsetHours = -offsetHours;
    }

    QDate date(year, month, day);
    QTime time(hour, minute);
    int expectedTJD = date.isValid() ? static_cast<int>((date.toJulianDay() - 2400001) % 10000) : -1;

    if (!date.isValid() || !time.isValid() || (weekday != date.dayOfWeek()) || (tjd != expectedTJD)
        || (offsetHours < -12) || (offsetHours > 14))
    {
        qDebug() << "RadioClockSink::rbu - Invalid timecode:"
                 << year << month << day << weekday << hour << minute
                 << "offset" << offsetHours << "TJD" << tjd << "expected" << expectedTJD;
        error = "Invalid timecode";
        return false;
    }

    // The frame carries Moscow civil time for the minute beginning immediately
    // after this frame and explicitly supplies its offset from UTC.
    m_dateTime = QDateTime(date, time, Qt::OffsetFromUTC, offsetHours * 3600);
    m_dst = RadioClockSettings::UNKNOWN;
    error.clear();
    return true;
}

void RadioClockSink::rbuReset(bool resetTiming)
{
    bool hadMinuteMarker = m_gotMinuteMarker;
    m_gotMinuteMarker = false;
    m_rbuRecentBits = 0;
    m_rbuRecentBitCount = 0;
    m_rbuBit = 0;
    m_rbuSecondValid = true;
    m_rbuInvalidSeconds = 0;
    m_second = 0;
    m_dst = RadioClockSettings::UNKNOWN;

    if (resetTiming)
    {
        m_rbuHaveSymbolTiming = false;
        m_rbuSymbolSample = 0;
        m_rbuCarrierLowCount = 0;
        m_rbuTimingErrors = 0;
        m_rbuHavePrevSample = false;
    }

    if (hadMinuteMarker && getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
    }
}

// Polish PCSK-225 time signal on the 225kHz carrier of Polish Radio.
// Frames use 50bit/s NRZ phase states separated by approximately 36 degrees.
// The fixed 0x55 0x55 0x60 header supplies both symbol timing and per-frame
// carrier phase/frequency estimates, so no wall-clock timing is required.
void RadioClockSink::pcsk225(Complex& ci, Real magsq)
{
    constexpr int samplesPerBit = RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 50;
    const qint64 currentSample = m_pcskSampleCount++;

    Real averagePower = m_thresholdMovingAverage.instantAverage();
    m_threshold = averagePower * 0.01f; // Reject only very deep carrier fades (-20dB).

    Real phaseDelta = 0.0f;
    if (m_pcskHavePrevSample) {
        phaseDelta = std::arg(ci * std::conj(m_pcskPrevSample));
    }
    m_pcskPrevSample = ci;
    m_pcskHavePrevSample = true;
    m_pcskUnwrappedPhase += phaseDelta;
    m_pcskPhaseHistory[currentSample % m_pcskPhaseHistorySize] = m_pcskUnwrappedPhase;
    m_fmDemodMovingAverage(phaseDelta);
    m_sample = false;

    if (m_pcskCollectingFrame && (currentSample >= m_pcskNextSymbolSample))
    {
        double phase0 = m_pcskPhaseIntercept + m_pcskPhaseSlope * m_pcskBit;
        double phase1 = phase0 + m_pcskPhaseSeparation;
        double phase = m_pcskUnwrappedPhase;
        int decodedBit = std::abs(phase - phase1) < std::abs(phase - phase0) ? 1 : 0;
        int byteIndex = m_pcskBit / 8;
        int bitIndex = 7 - m_pcskBit % 8;

        if (decodedBit) {
            m_pcskFrame[byteIndex] |= static_cast<quint8>(1U << bitIndex);
        } else {
            m_pcskFrame[byteIndex] &= static_cast<quint8>(~(1U << bitIndex));
        }

        m_data = decodedBit;
        m_sample = true;
        m_pcskBit++;
        m_pcskNextSymbolSample += samplesPerBit;

        if (m_pcskBit == 96) {
            pcskCompleteFrame(currentSample);
        }
    }
    else if (!m_pcskCollectingFrame)
    {
        if (magsq > m_threshold) {
            pcskTryFrameSync(currentSample);
        } else {
            m_pcskSyncCandidate = false;
            m_pcskSyncQuietSamples = 0;
        }
    }

    if (m_pcskReferenceDateTime.isValid() && (currentSample >= m_pcskNextTimeReportSample))
    {
        qint64 elapsedMSecs = (currentSample - m_pcskReferenceSample) * 1000
            / RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_dateTime = m_pcskReferenceDateTime.addMSecs(elapsedMSecs);

        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgDateTime::create(m_dateTime, m_dst));
        }

        do {
            m_pcskNextTimeReportSample += RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        } while (m_pcskNextTimeReportSample <= currentSample);
    }

    if (m_gotMinuteMarker && (m_pcskLastValidFrameSample > 0)
        && (currentSample - m_pcskLastValidFrameSample > 125LL * RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE))
    {
        m_gotMinuteMarker = false;
        m_pcskReferenceDateTime = QDateTime();
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for PCSK-225 frame"));
        }
    }
}

bool RadioClockSink::pcskTryFrameSync(qint64 currentSample)
{
    constexpr int headerBits = 24;
    constexpr int samplesPerBit = RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 50;
    constexpr quint32 header = 0x555560;

    if (currentSample < (headerBits - 1) * samplesPerBit) {
        return false;
    }

    // Least-squares fit: phase(bit) = intercept + slope*bit + separation*knownBit.
    // This removes residual carrier offset while measuring the two NRZ states.
    double matrix[3][4]{};

    for (int i = 0; i < headerBits; i++)
    {
        qint64 sample = currentSample - (headerBits - 1 - i) * samplesPerBit;
        double phase = m_pcskPhaseHistory[sample % m_pcskPhaseHistorySize];
        double knownBit = (header >> (headerBits - 1 - i)) & 1;
        double x[3] = {1.0, static_cast<double>(i), knownBit};

        for (int row = 0; row < 3; row++)
        {
            for (int column = 0; column < 3; column++) {
                matrix[row][column] += x[row] * x[column];
            }
            matrix[row][3] += x[row] * phase;
        }
    }

    for (int pivot = 0; pivot < 3; pivot++)
    {
        int bestRow = pivot;
        for (int row = pivot + 1; row < 3; row++) {
            if (std::abs(matrix[row][pivot]) > std::abs(matrix[bestRow][pivot])) {
                bestRow = row;
            }
        }

        if (std::abs(matrix[bestRow][pivot]) < 1.0e-9) {
            return false;
        }

        if (bestRow != pivot) {
            for (int column = pivot; column < 4; column++) {
                std::swap(matrix[pivot][column], matrix[bestRow][column]);
            }
        }

        double divisor = matrix[pivot][pivot];
        for (int column = pivot; column < 4; column++) {
            matrix[pivot][column] /= divisor;
        }

        for (int row = 0; row < 3; row++)
        {
            if (row == pivot) {
                continue;
            }
            double multiplier = matrix[row][pivot];
            for (int column = pivot; column < 4; column++) {
                matrix[row][column] -= multiplier * matrix[pivot][column];
            }
        }
    }

    double intercept = matrix[0][3];
    double slope = matrix[1][3];
    double separation = matrix[2][3];
    double squaredError = 0.0;

    for (int i = 0; i < headerBits; i++)
    {
        qint64 sample = currentSample - (headerBits - 1 - i) * samplesPerBit;
        double phase = m_pcskPhaseHistory[sample % m_pcskPhaseHistorySize];
        double knownBit = (header >> (headerBits - 1 - i)) & 1;
        double error = phase - (intercept + slope * i + separation * knownBit);
        squaredError += error * error;
    }

    double rmsError = std::sqrt(squaredError / headerBits);
    double phaseDepth = std::abs(separation);

    const bool validCandidate = (phaseDepth >= 0.4) && (phaseDepth <= 0.85)
        && (rmsError <= 0.08) && (phaseDepth / std::max(rmsError, 1.0e-6) >= 6.0);

    if (validCandidate)
    {
        double score = rmsError / phaseDepth;

        if (!m_pcskSyncCandidate || (score < m_pcskSyncScore))
        {
            m_pcskSyncCandidate = true;
            m_pcskSyncQuietSamples = 0;
            m_pcskSyncScore = score;
            m_pcskSyncEndSample = currentSample;
            m_pcskSyncIntercept = intercept;
            m_pcskSyncSlope = slope;
            m_pcskSyncSeparation = separation;
        }
        else {
            m_pcskSyncQuietSamples++;
        }
    }
    else if (m_pcskSyncCandidate) {
        m_pcskSyncQuietSamples++;
    }

    // A threshold crossing can occur almost one bit before the optimum symbol
    // centre. Wait for ten samples without a better fit and use the local best
    // candidate. This still completes before the first payload bit is due.
    if (!m_pcskSyncCandidate || (m_pcskSyncQuietSamples < samplesPerBit / 2)) {
        return false;
    }

    m_pcskCollectingFrame = true;
    m_pcskSyncCandidate = false;
    m_pcskBit = headerBits;
    m_pcskFrameStartSample = m_pcskSyncEndSample - (headerBits - 1) * samplesPerBit;
    m_pcskNextSymbolSample = m_pcskSyncEndSample + samplesPerBit;
    m_pcskPhaseIntercept = m_pcskSyncIntercept;
    m_pcskPhaseSlope = m_pcskSyncSlope;
    m_pcskPhaseSeparation = m_pcskSyncSeparation;
    m_pcskFrame.fill(0);
    m_pcskFrame[0] = 0x55;
    m_pcskFrame[1] = 0x55;
    m_pcskFrame[2] = 0x60;
    m_data = 0;
    m_sample = true;

    qDebug() << "RadioClockSink::pcsk225 - Frame sync, phase depth"
             << std::abs(m_pcskPhaseSeparation) * 180.0 / M_PI
             << "degrees, normalized RMS" << m_pcskSyncScore;

    if (!m_gotMinuteMarker && getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Got PCSK-225 frame sync"));
    }

    return true;
}

void RadioClockSink::pcskCompleteFrame(qint64 currentSample)
{
    constexpr int samplesPerBit = RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / 50;
    PCSK225Decoder::Result result{};
    QString error;
    m_pcskCollectingFrame = false;

    if (!PCSK225Decoder::decode(m_pcskFrame, result, error))
    {
        qDebug() << "RadioClockSink::pcsk225 -" << error;
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(error));
        }
        return;
    }

    // The encoded time applies at the documented timing point 25 bits after
    // the first symbol centre. Keep that sample as the reference so reports
    // account for the 1.4 seconds spent receiving the rest of the frame.
    m_pcskReferenceSample = m_pcskFrameStartSample + 25 * samplesPerBit;
    m_pcskReferenceDateTime = result.m_dateTimeUtc.toOffsetFromUtc(result.m_utcOffsetHours * 3600);
    m_pcskNextTimeReportSample = currentSample;
    m_pcskLastValidFrameSample = currentSample;
    m_gotMinuteMarker = true;

    if (result.m_timeChangePending && (result.m_utcOffsetHours == 1)) {
        m_dst = RadioClockSettings::STARTING;
    } else if (result.m_timeChangePending && (result.m_utcOffsetHours == 2)) {
        m_dst = RadioClockSettings::ENDING;
    } else if (result.m_utcOffsetHours == 2) {
        m_dst = RadioClockSettings::IN_EFFECT;
    } else if (result.m_utcOffsetHours == 1) {
        m_dst = RadioClockSettings::NOT_IN_EFFECT;
    } else {
        m_dst = RadioClockSettings::UNKNOWN;
    }

    QString status = "OK";
    if (result.m_correctedSymbols > 0) {
        status += QString(" (%1 RS symbol%2 corrected")
            .arg(result.m_correctedSymbols)
            .arg(result.m_correctedSymbols == 1 ? "" : "s");
        status += ")";
    }
    if (result.m_leapSecondPending) {
        status += result.m_leapSecondDelete ? "; negative leap second pending" : "; leap second pending";
    }
    if (result.m_transmitterStatus != 0) {
        status += QString("; transmitter status %1").arg(result.m_transmitterStatus);
    }

    qDebug() << "RadioClockSink::pcsk225 -" << status
             << result.m_dateTimeUtc << "UTC offset" << result.m_utcOffsetHours;

    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(RadioClock::MsgStatus::create(status));
    }
}

void RadioClockSink::pcskReset()
{
    m_pcskPrevSample = Complex(0.0f, 0.0f);
    m_pcskHavePrevSample = false;
    m_pcskUnwrappedPhase = 0.0;
    m_pcskPhaseHistory.fill(0.0);
    m_pcskSampleCount = 0;
    m_pcskSyncCandidate = false;
    m_pcskSyncQuietSamples = 0;
    m_pcskSyncScore = 0.0;
    m_pcskSyncEndSample = 0;
    m_pcskSyncIntercept = 0.0;
    m_pcskSyncSlope = 0.0;
    m_pcskSyncSeparation = 0.0;
    m_pcskCollectingFrame = false;
    m_pcskBit = 0;
    m_pcskFrameStartSample = 0;
    m_pcskNextSymbolSample = 0;
    m_pcskPhaseIntercept = 0.0;
    m_pcskPhaseSlope = 0.0;
    m_pcskPhaseSeparation = 0.0;
    m_pcskFrame.fill(0);
    m_pcskReferenceDateTime = QDateTime();
    m_pcskReferenceSample = 0;
    m_pcskNextTimeReportSample = 0;
    m_pcskLastValidFrameSample = 0;
}

void RadioClockSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    Real re = ci.real() / SDR_RX_SCALEF;
    Real im = ci.imag() / SDR_RX_SCALEF;
    Real magsq = re*re + im*im;
    m_movingAverage(magsq);
    m_thresholdMovingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak)
    {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // Demodulate
    if (m_settings.m_modulation == RadioClockSettings::DCF77) {
        dcf77();
    } else if (m_settings.m_modulation == RadioClockSettings::TDF) {
        tdf(ci);
    } else if (m_settings.m_modulation == RadioClockSettings::WWVB) {
        wwvb();
    } else if (m_settings.m_modulation == RadioClockSettings::JJY) {
        jjy();
    } else if (m_settings.m_modulation == RadioClockSettings::RBU) {
        rbu(ci, magsq);
    } else if (m_settings.m_modulation == RadioClockSettings::PCSK225) {
        pcsk225(ci, magsq);
    } else {
        msf60();
    }

    // Feed signals to scope
    sampleToScope(Complex(re, im));
}

void RadioClockSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RadioClockSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void RadioClockSink::applySettings(const QStringList& settingsKeys, const RadioClockSettings& settings, bool force)
{
    qDebug() << "RadioClockSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settingsKeys.contains("threshold") && (settings.m_threshold != m_settings.m_threshold)) || force)
    {
        m_linearThreshold = CalcDb::powerFromdB(-settings.m_threshold);
    }

    if ((settingsKeys.contains("modulation") && (settings.m_modulation != m_settings.m_modulation)) || force)
    {
        m_gotMinuteMarker = false;
        m_lowCount = 0;
        m_highCount = 0;
        m_zeroCount = 0;
        m_second = 0;
        m_dst = RadioClockSettings::UNKNOWN;
        rbuReset(true);
        pcskReset();
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
        }
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}
