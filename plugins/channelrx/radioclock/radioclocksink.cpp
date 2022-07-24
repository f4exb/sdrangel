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
#include "dsp/scopevis.h"
#include "util/db.h"
#include "maincore.h"

#include "radioclock.h"
#include "radioclocksink.h"

RadioClockSink::RadioClockSink(RadioClock *radioClock) :
        m_scopeSink(nullptr),
        m_radioClock(radioClock),
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
        m_dst(RadioClockSettings::UNKNOWN),
        m_zeroCount(0),
        m_sampleBufferIndex(0),
        m_gotMarker(false)
{
    m_phaseDiscri.setFMScaling(RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE / (2.0f * 20.0/M_PI));
    applySettings(m_settings, true);
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

    // Look for minute marker - two consequtive markers
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

void RadioClockSink::applySettings(const RadioClockSettings& settings, bool force)
{
    qDebug() << "RadioClockSink::applySettings:"
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_threshold: " << settings.m_threshold
            << " m_modulation: " << settings.m_modulation
            << " force: " << force;

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RadioClockSettings::RADIOCLOCK_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    if ((settings.m_threshold != m_settings.m_threshold) || force)
    {
        m_linearThreshold = CalcDb::powerFromdB(-settings.m_threshold);
    }

    if ((settings.m_modulation != m_settings.m_modulation) || force)
    {
        m_gotMinuteMarker = false;
        m_lowCount = 0;
        m_highCount = 0;
        m_zeroCount = 0;
        m_second = 0;
        m_dst = RadioClockSettings::UNKNOWN;
        if (getMessageQueueToChannel()) {
            getMessageQueueToChannel()->push(RadioClock::MsgStatus::create("Looking for minute marker"));
        }
    }

    m_settings = settings;
}
