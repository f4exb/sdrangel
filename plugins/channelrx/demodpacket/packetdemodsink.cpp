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

#include <algorithm>
#include <complex.h>
#include <vector>

#include "dsp/datafifo.h"
#include "device/deviceapi.h"
#include "channel/channelwebapiutils.h"
#include "maincore.h"

#include "util/ax25.h"
#include "util/popcount.h"

#include "packetdemod.h"
#include "packetdemodsink.h"

PacketDemodSink::PacketDemodSink(PacketDemod *packetDemod) :
        m_packetDemod(packetDemod),
        m_channelSampleRate(PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr)
{
    // The framer emits frames; what a decode MEANS - deduplication, the burst's live
    // decode count, and the tone pair learned from replayed frames - stays here.
    // The discriminator path frames its own bits; the core frames the MLSE chains'.
    m_framer.setFrameHandler([this](const QByteArray& packet, bool viaChase) -> bool {
        (void) viaChase;
        return sendPacket(packet, m_core.sampleCount());
    });

    // What a decode MEANS - deduplication and the reporting timestamp - stays here; the
    // core decides what a decode IS.
    m_core.setPacketHandler([this](const QByteArray& packet, quint64 stamp) -> bool {
        return sendPacket(packet, stamp);
    });

    m_magsq = 0.0;

    m_demodBuffer.resize(1<<12);
    m_demodBufferFill = 0;

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

PacketDemodSink::~PacketDemodSink()
{
}

void PacketDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

void PacketDemodSink::processOneSample(Complex &ci)
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

    if (m_settings.m_mlse)
    {
        // Detect on the complex baseband. The discriminator output is still produced
        // above; the symbol rate estimator decodes its tone transitions to keep the
        // chains on the transmitter's real clock.
        m_core.processSample(ci, fmDemod, magsq);
    }
    else
    {
        Complex corrF0;
        Complex corrF1;

        if (m_correlator.push(fmDemod, corrF0, corrF1))
        {
            // Low pass filter, to minimize changes above the baud rate
            Real f0Filt = m_lowpassF0.filter(std::abs(corrF0));
            Real f1Filt = m_lowpassF1.filter(std::abs(corrF1));

            // Determine which is the closest match and then quantise to 1 or -1
            // FIXME: We should try to account for the fact that higher frequencies can have preemphasis
            float diff = f1Filt - f0Filt;
            int sample = diff >= 0.0f ? 1 : 0;

            // Look for edge
            if (sample != m_samplePrev)
            {
                m_syncCount = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/m_settings.getBaudRate()/2;
            }
            else
            {
                m_syncCount--;
                if (m_syncCount <= 0)
                {
                    m_framer.process(m_deframer, sample, diff, true);
                    m_syncCount = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/m_settings.getBaudRate();
                }
            }
            m_samplePrev = sample;
        }
    }

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

bool PacketDemodSink::sendPacket(const QByteArray& packet, quint64 stamp)
{
    // The MLSE runs many detectors over the same signal and several of them will decode the
    // same transmission, a symbol period or so apart. Report it once. Genuine retransmissions
    // are seconds to minutes apart, and a digipeated repeat has the via path marked, so it
    // does not compare equal.
    if (m_settings.m_mlse)
    {
        const quint64 rate = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;

        // Timestamp a frame by when it was TRANSMITTED, not when it was decoded: a replay
        // reports a burst's frames seconds after the live chains would have, and the two
        // must still compare equal. Entries are kept long enough for a replay to catch up.

        while (!m_recent.empty()
            && (stamp - m_recent.front().second > (quint64) 10 * rate)) {
            m_recent.pop_front();
        }

        for (const auto& r : m_recent)
        {
            quint64 dt = (stamp > r.second) ? (stamp - r.second) : (r.second - stamp);

            if ((r.first == packet) && (dt < rate)) {
                return false;       // a duplicate of one already reported
            }
        }

        m_recent.push_back(std::make_pair(packet, stamp));
    }

    qDebug() << "RX: " << packet.toHex();

    if (!getMessageQueueToChannel()) {
        return false;
    }

    QDateTime dateTime = QDateTime::currentDateTime();

    if (m_settings.m_useFileTime)
    {
        QString hwType = m_packetDemod->getDeviceAPI()->getHardwareId();

        if ((hwType == "FileInput") || (hwType == "SigMFFileInput"))
        {
            QString dateTimeStr;
            int deviceIdx = m_packetDemod->getDeviceSetIndex();

            if (ChannelWebAPIUtils::getDeviceReportValue(deviceIdx, "absoluteTime", dateTimeStr)) {
                dateTime = QDateTime::fromString(dateTimeStr, Qt::ISODateWithMs);
            }
        }
    }

    MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_packetDemod, packet, dateTime);
    getMessageQueueToChannel()->push(msg);
    return true;
}

void PacketDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "PacketDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

static PacketDemodCore::Config coreConfig(const PacketDemodSettings& settings)
{
    PacketDemodCore::Config cfg;
    cfg.m_chase = settings.m_chase;
    cfg.m_mlse = settings.m_mlse;
    cfg.m_baudRate = settings.getBaudRate();

    return cfg;
}

void PacketDemodSink::applySettings(const QStringList& settingsKeys, const PacketDemodSettings& settings, bool force)
{
    qDebug() << "PacketDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }
    // Both paths frame their own bits, so both need the depth - and the MLSE path is the
    // default, which is where a Chase setting that only reached the discriminator would
    // have looked like it did nothing at all
    if (settingsKeys.contains("chase") || force)
    {
        m_framer.setChaseDepth(settings.m_chase);
        m_core.setChaseDepth(settings.m_chase);
    }

    if (settingsKeys.contains("mlse") || force) {
        m_framer.setRequirePlausible(settings.m_mlse);
    }

    if (force)
    {
        // Deviation is a constant now, so the discriminator scaling is set once
        m_phaseDiscri.setFMScaling(PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE
            / (2.0f * PacketDemodSettings::PACKETDEMOD_FM_DEVIATION));

        m_correlationLength = PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE/settings.getBaudRate();
        m_correlator.create(m_correlationLength,
                            PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE,
                            2200.0, 1200.0);

        m_lowpassF1.create(PACKETDEMOD_LOWPASS_TAPS, PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE, settings.getBaudRate() * 1.1f);
        m_lowpassF0.create(PACKETDEMOD_LOWPASS_TAPS, PacketDemodSettings::PACKETDEMOD_CHANNEL_SAMPLE_RATE, settings.getBaudRate() * 1.1f);
        m_deframer.reset();
        m_samplePrev = 0;
        m_syncCount = 0;
        m_settings = settings;
        m_core.applyConfig(coreConfig(settings));
    }
    else
    {
        bool rebuild = settingsKeys.contains("mlse")
            || settingsKeys.contains("mode");

        m_settings.applySettings(settingsKeys, settings);

        if (rebuild) {
            m_core.applyConfig(coreConfig(m_settings));
        }
    }
}
