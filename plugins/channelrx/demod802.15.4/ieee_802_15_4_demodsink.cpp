///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
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

#include "dsp/scopevis.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"

#include "ieee_802_15_4_demod.h"
#include "ieee_802_15_4_demodsink.h"

IEEE_802_15_4_DemodSink::IEEE_802_15_4_DemodSink(IEEE_802_15_4_Demod *demod) :
        m_scopeSink(nullptr),
        m_demod(demod),
        m_channel(nullptr),
        m_channelSampleRate(8000000),
        m_channelFrequencyOffset(0),
        m_sampleRate(8000000),
        m_interpolatorDistance(1.0f),
        m_interpolatorDistanceRemain(1.0f),
        m_magsq(0.0),
        m_magsqSum(0.0),
        m_magsqPeak(0.0),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_sampleBufferIndex(0)
{
    for (int i = 0; i < IEEE_802_15_4_DemodSettings::m_scopeStreams; i++) {
        m_sampleBuffer[i].resize(m_sampleBufferSize);
    }

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

IEEE_802_15_4_DemodSink::~IEEE_802_15_4_DemodSink()
{
}

void IEEE_802_15_4_DemodSink::sampleToScope(Complex sample, Real magsq)
{
    if (m_scopeSink)
    {
        const IEEE_802_15_4_DemodReceiver::Scope& scope = m_receiver.getScope();

        m_sampleBuffer[0][m_sampleBufferIndex] = sample;
        m_sampleBuffer[1][m_sampleBufferIndex] = Complex(magsq, 0.0f);
        m_sampleBuffer[2][m_sampleBufferIndex] = Complex(scope.m_detectMetric, 0.0f);
        m_sampleBuffer[3][m_sampleBufferIndex] = Complex(scope.m_correlation, 0.0f);
        m_sampleBuffer[4][m_sampleBufferIndex] = Complex(scope.m_chip, 0.0f);
        m_sampleBuffer[5][m_sampleBufferIndex] = Complex(scope.m_phase, 0.0f);
        m_sampleBuffer[6][m_sampleBufferIndex] = Complex(scope.m_state, 0.0f);
        m_sampleBuffer[7][m_sampleBufferIndex] = Complex(scope.m_crc, 0.0f);
        m_sampleBufferIndex++;

        if (m_sampleBufferIndex == m_sampleBufferSize)
        {
            std::vector<ComplexVector::const_iterator> vbegin;

            for (int i = 0; i < IEEE_802_15_4_DemodSettings::m_scopeStreams; i++) {
                vbegin.push_back(m_sampleBuffer[i].begin());
            }

            m_scopeSink->feed(vbegin, m_sampleBufferSize);
            m_sampleBufferIndex = 0;
        }
    }
}

void IEEE_802_15_4_DemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void IEEE_802_15_4_DemodSink::processOneSample(Complex &ci)
{
    Complex x = ci / SDR_RX_SCALEF;
    Real magsq = std::norm(x);

    // Calculate average and peak levels for level meter
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    m_receiver.process(IEEE_802_15_4_DemodReceiver::Cf(x.real(), x.imag()));

    while (m_receiver.frameAvailable()) {
        sendFrame(m_receiver.popFrame());
    }

    sampleToScope(x, magsq);
}

void IEEE_802_15_4_DemodSink::sendFrame(const IEEE_802_15_4_DemodReceiver::Frame& frame)
{
    if (!getMessageQueueToChannel()) {
        return;
    }

    QDateTime dateTime = QDateTime::currentDateTime();

    if (m_settings.m_useFileTime)
    {
        QString hardwareId = m_demod->getDeviceAPI()->getHardwareId();

        if ((hardwareId == "FileInput") || (hardwareId == "SigMFFileInput"))
        {
            QString dateTimeStr;
            int deviceIdx = m_demod->getDeviceSetIndex();

            if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr))
            {
                QDateTime fileTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);

                if (fileTime.isValid()) {
                    dateTime = fileTime;
                }
            }
        }
    }

    QByteArray psdu((const char *) frame.m_psdu.data(), (int) frame.m_psdu.size());
    float powerDB = frame.m_power > 0.0f ? 10.0f * std::log10(frame.m_power) : -150.0f;

    IEEE_802_15_4_Demod::MsgFrame *msg = IEEE_802_15_4_Demod::MsgFrame::create(
        psdu, dateTime, frame.m_fcsOK, frame.m_coherent, frame.m_frequencyOffset, frame.m_lqi, powerDB, frame.m_snr);
    getMessageQueueToChannel()->push(msg);
}

void IEEE_802_15_4_DemodSink::configureResampler()
{
    m_sampleRate = m_settings.getChannelSampleRate();
    // Filter to the RF bandwidth, but keep the cutoff below the Nyquist frequency of the
    // lower of the input and output sample rates, so nothing aliases on to the signal
    double cutoff = std::min(m_settings.m_rfBandwidth / 2.0, 0.45 * std::min(m_channelSampleRate, m_sampleRate));
    m_interpolator.create(16, m_channelSampleRate, cutoff);
    m_interpolatorDistance = (Real) m_channelSampleRate / (Real) m_sampleRate;
    m_interpolatorDistanceRemain = m_interpolatorDistance;
}

void IEEE_802_15_4_DemodSink::configureReceiver()
{
    m_receiver.configure(m_settings.getPHY(), m_settings.m_samplesPerChip, m_settings.m_maxFrequencyOffset, m_settings.m_threshold);
}

void IEEE_802_15_4_DemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "IEEE_802_15_4_DemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_channelSampleRate = channelSampleRate;
        configureResampler();
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void IEEE_802_15_4_DemodSink::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings, bool force)
{
    qDebug() << "IEEE_802_15_4_DemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    bool resampler = (settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth))
        || (settingsKeys.contains("phy") && (settings.m_phy != m_settings.m_phy))
        || (settingsKeys.contains("samplesPerChip") && (settings.m_samplesPerChip != m_settings.m_samplesPerChip))
        || force;
    bool receiver = (settingsKeys.contains("phy") && (settings.m_phy != m_settings.m_phy))
        || (settingsKeys.contains("samplesPerChip") && (settings.m_samplesPerChip != m_settings.m_samplesPerChip))
        || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (resampler) {
        configureResampler();
    }

    if (receiver)
    {
        configureReceiver();
    }
    else
    {
        if (settingsKeys.contains("maxFrequencyOffset")) {
            m_receiver.setMaxFrequencyOffset(m_settings.m_maxFrequencyOffset);
        }
        if (settingsKeys.contains("threshold")) {
            m_receiver.setThreshold(m_settings.m_threshold);
        }
    }
}
