///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2021-2026 Jon Beniston, M7RCE                                   //
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

#include "dsp/dspengine.h"
#include "dsp/datafifo.h"
#include "dsp/scopevis.h"
#include "util/db.h"
#include "maincore.h"

#include "acarsdemod.h"
#include "acarsdemodsink.h"

AcarsDemodSink::AcarsDemodSink(AcarsDemod *acarsDemod) :
        m_scopeSink(nullptr),
        m_acarsDemod(acarsDemod),
        m_channel(nullptr),
        m_channelSampleRate(ACARSDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_interpolatorDistance(1.0f),
        m_interpolatorDistanceRemain(0.0f),
        m_magsq(0.0),
        m_magsqSum(0.0f),
        m_magsqPeak(0.0f),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr),
        m_scopeBufferIndex(0),
        m_demodBufferFill(0),
        m_iridiumChannels(0),       // Configured on the first Iridium sample
        m_iridiumLogCount(0),
        m_iridiumCarrierCount(0),
        m_demodPipesPresent(false),
        m_demodPipeCheck(1)
{
    m_demodBuffer.resize(1<<12);
    m_scopeBuffer.resize(m_scopeBufferSize);
    m_scopeBegin.resize(1);

    applySettings(m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

AcarsDemodSink::~AcarsDemodSink()
{
}

void AcarsDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    if ((m_settings.m_mode == AcarsDemodSettings::Iridium)
     && (std::fabs(m_interpolatorDistance - 1.0f) < 1e-6f))
    {
        for (SampleVector::const_iterator it = begin; it != end; ++it)
        {
            Complex c(it->real(), it->imag());
            c *= m_nco.nextIQ();
            processOneSample(c);
        }

        return;
    }

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f)
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void AcarsDemodSink::sampleToScope(Real ch1, Real ch2)
{
    m_scopeBuffer[m_scopeBufferIndex++] = Sample(ch1 * SDR_RX_SCALEF, ch2 * SDR_RX_SCALEF);

    if (m_scopeBufferIndex == m_scopeBufferSize)
    {
        m_scopeBegin[0] = m_scopeBuffer.begin();
        m_scopeSink->feed(m_scopeBegin, m_scopeBufferSize);
        m_scopeBufferIndex = 0;
    }
}

Real AcarsDemodSink::scopeChannel(int selection, const Complex& ci, Real magsq) const
{
    bool vdl2 = m_settings.m_mode == AcarsDemodSettings::VDL2;
    bool hfdl = m_settings.m_mode == AcarsDemodSettings::HFDL;
    bool aero = m_settings.m_mode == AcarsDemodSettings::Aero;

    switch (selection)
    {
    case 0: return ci.real() / SDR_RX_SCALEF;
    case 1: return ci.imag() / SDR_RX_SCALEF;
    case 2: return magsq;
    case 3: return sqrt(magsq);
    case 4: return vdl2 ? (Real) std::min(m_vdl2Receiver.syncStatistic(), 10.0)
                 : hfdl ? (Real) m_hfdlReceiver.lastSyncError()
                 : aero ? (Real) m_aeroReceiver.lastSyncError()
                 : (Real) m_receiver.detectStatistic();
    case 5: return vdl2 ? (m_vdl2Receiver.synced() ? 1.0f : 0.0f)
                 : hfdl ? (m_hfdlReceiver.synced() ? 1.0f : 0.0f)
                 : aero ? (m_aeroReceiver.synced() ? 1.0f : 0.0f)
                 : (m_receiver.detected() ? 1.0f : 0.0f);
    default: return 0.0f;
    }
}

void AcarsDemodSink::processOneSample(Complex &ci)
{
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

    if (m_settings.m_mode == AcarsDemodSettings::VDL2)
    {
        if (m_vdl2Receiver.processSample(std::complex<double>(re, im)))
        {
            while (m_vdl2Receiver.hasFrame())
            {
                AcarsVdl2Receiver::Frame frame = m_vdl2Receiver.popFrame();

                if (!getMessageQueueToChannel()) {
                    continue;
                }

                if (frame.m_isAcars)
                {
                    // ACARS over AVLC carries the message from the mode character to the
                    // DEL, so prepending an SOH gives the exact VHF ACARS byte layout and
                    // the whole existing message path (GUI, UDP, log) works unchanged
                    QByteArray rxPacket;
                    rxPacket.append((char) 0x01);
                    rxPacket.append((const char *) frame.m_bytes.data() + frame.m_infoOffset + 3, frame.m_infoLength - 3);

                    // The AVLC header names the airframe by its ICAO address, where the
                    // block inside names it by registration. Both are worth having: the
                    // address collates this aircraft with its ADS-B, and the pair is the
                    // only evidence we ever receive that the two belong together
                    uint32_t acAddress = frame.m_fromAircraft ? frame.m_srcAddress : frame.m_dstAddress;
                    int acType = frame.m_fromAircraft ? frame.m_srcType : frame.m_dstType;
                    getMessageQueueToChannel()->push(AcarsDemod::MsgAcarsPacket::create(
                        m_acarsDemod, rxPacket, (acType == 1) ? acAddress : 0));
                }
                else
                {
                    getMessageQueueToChannel()->push(AcarsDemod::MsgVdl2Frame::create(frame));
                }
            }
        }
    }
    else if (m_settings.m_mode == AcarsDemodSettings::HFDL)
    {
        if (m_hfdlReceiver.processSample(std::complex<double>(re, im)))
        {
            while (m_hfdlReceiver.hasFrame())
            {
                AcarsHfdlReceiver::Frame frame = m_hfdlReceiver.popFrame();

                if (!getMessageQueueToChannel()) {
                    continue;
                }

                if (frame.m_isAcars)
                {
                    // The LPDU carries the ACARS message from its SOH onwards, so the
                    // bytes from the mode character with an SOH prepended give the
                    // exact VHF ACARS layout and the existing message path works
                    QByteArray rxPacket;
                    rxPacket.append((char) 0x01);
                    rxPacket.append((const char *) frame.m_bytes.data() + frame.m_acarsOffset,
                                    (int) (frame.m_bytes.size() - frame.m_acarsOffset));
                    getMessageQueueToChannel()->push(MainCore::MsgPacket::create(m_acarsDemod, rxPacket));
                }
                else
                {
                    getMessageQueueToChannel()->push(AcarsDemod::MsgHfdlFrame::create(frame));
                }
            }
        }
    }
    else if (m_settings.m_mode == AcarsDemodSettings::Aero)
    {
        if (m_aeroReceiver.processSample(std::complex<double>(re, im)))
        {
            while (m_aeroReceiver.hasFrame())
            {
                AcarsAeroReceiver::Frame frame = m_aeroReceiver.popFrame();

                if (!getMessageQueueToChannel()) {
                    continue;
                }

                if (frame.m_isAcars)
                {
                    // Unlike VDL-2 and HFDL, an Aero signal unit chain reassembles to a
                    // COMPLETE ARINC 618 block starting at its own SOH, so there is no
                    // SOH to prepend here. Parity is left on, as on the other protocols.
                    // Verified against AcarsMessage::decode on a real recording.
                    QByteArray rxPacket((const char *) frame.m_bytes.data(), (int) frame.m_bytes.size());
                    getMessageQueueToChannel()->push(MainCore::MsgPacket::create(m_acarsDemod, rxPacket));
                }
                else
                {
                    getMessageQueueToChannel()->push(AcarsDemod::MsgAeroFrame::create(frame));
                }
            }
        }
    }
    // Tested explicitly rather than left as a bare else: an unrecognised mode used to
    // fall through to the VHF MSK demodulator, so a mode added without touching this
    // line failed by quietly running the wrong receiver
    else if (m_settings.m_mode == AcarsDemodSettings::Iridium)
    {
        // Unlike the other modes this one scans: the channelizer in the scanner splits
        // the whole tuned width into Iridium channels and runs a receiver on each, so the
        // samples arriving here are the band rather than a channel
        const int channels = AcarsDemodSettings::iridiumScanChannels(m_settings.m_iridiumBandwidth);

        if (channels != m_iridiumChannels)
        {
            m_iridiumScanner.configure(channels);
            m_iridiumChannels = channels;
            m_iridiumCarrierCount = 0;
            m_iridiumLogCount = 0;
            m_iridiumLog = IridiumLog();
        }

        m_iridiumScanner.processSample(std::complex<double>(re, im));

        if (++m_iridiumLogCount >= (qint64) (m_iridiumLogSeconds * m_iridiumScanner.inputRate()))
        {
            m_iridiumLogCount = 0;
            logIridiumSummary();
        }

        while (m_iridiumScanner.hasDetection())
        {
            AcarsIridiumScanner::Detection detection = m_iridiumScanner.popDetection();

            if (!detection.m_message.isEmpty())
            {
                QByteArray payload;

                if (AcarsIridiumReassembler::sbdPayload(detection.m_message,
                                                        detection.m_frame.m_uplink, payload))
                {
                    m_iridiumCarrierCount++;

                    if (detection.m_acars.isEmpty())
                    {
                        // Both type bytes, because the first on its own does not say
                        // whether a message could carry ACARS: 0x7608 does and 0x7605
                        // does not, and they look the same from the leading byte
                        const quint8 type0 = (quint8) detection.m_message[0];
                        const quint8 type1 = (detection.m_message.size() > 1)
                                           ? (quint8) detection.m_message[1] : 0;

                        qInfo() << "Iridium short burst data message," << detection.m_message.size()
                                << "bytes, type" << QString("0x%1%2").arg(type0, 2, 16, QChar('0'))
                                                                     .arg(type1, 2, 16, QChar('0'))
                                << ":" << detection.m_message.toHex('.')
                                << "(payload is not ACARS)";
                    }
                }
            }

            if (detection.m_acars.isEmpty()) {
                continue;
            }

            if (getMessageQueueToChannel())
            {
                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_acarsDemod, detection.m_acars);
                getMessageQueueToChannel()->push(msg);
            }
        }
    }
    else if (m_settings.m_mode == AcarsDemodSettings::ACARS)
    {
        if (m_receiver.processSample(std::complex<double>(re, im)))
        {
            QByteArray rxPacket((const char *) m_receiver.message(), m_receiver.messageLength());

            if (getMessageQueueToChannel())
            {
                MainCore::MsgPacket *msg = MainCore::MsgPacket::create(m_acarsDemod, rxPacket);
                getMessageQueueToChannel()->push(msg);
            }
        }
    }

    if (m_scopeSink)
    {
        sampleToScope(scopeChannel(m_settings.m_scopeCh1, ci, magsq),
                      scopeChannel(m_settings.m_scopeCh2, ci, magsq));
    }

    // Demod Analyzer feature.
    //
    // Whether anything is listening is rechecked periodically rather than assumed, so
    // that the square root below is not paid for when nothing is. That mattered little
    // while every mode ran at 105 kS/s or less, but Iridium scans the band at up to
    // 8 MS/s, where this was eight million square roots a second and a pipe lookup two
    // thousand times a second for a feature that is usually not attached
    if (--m_demodPipeCheck <= 0)
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        m_demodPipesPresent = !dataPipes.isEmpty();
        m_demodPipeCheck = m_demodPipeCheckInterval;
    }

    if (!m_demodPipesPresent)
    {
        m_demodBufferFill = 0;
        return;
    }

    m_demodBuffer[m_demodBufferFill++] = (qint16)(sqrt(magsq) * std::numeric_limits<int16_t>::max());

    if (m_demodBufferFill >= m_demodBuffer.size())
    {
        QList<ObjectPipe*> dataPipes;
        MainCore::instance()->getDataPipes().getDataPipes(m_channel, "demod", dataPipes);

        for (const auto& pipe : dataPipes)
        {
            DataFifo *fifo = qobject_cast<DataFifo*>(pipe->m_element);

            if (fifo) {
                fifo->write((quint8*) &m_demodBuffer[0], m_demodBuffer.size() * sizeof(qint16), DataFifo::DataTypeI16);
            }
        }

        m_demodBufferFill = 0;
    }
}

//! Taps per phase for the input resampler. The sharp filter the other modes need is to
//! keep the root raised cosine skirt intact right up to the channel edge. Iridium does not
//! want a channel filter at all - it passes the whole scanned width and lets its own
//! channelizer do the filtering - so the cheap setting is enough, and at several MS/s the
//! difference between 4.5 and 18 taps per phase is a large part of the whole cost
static Real inputFilterTaps(const AcarsDemodSettings& settings)
{
    return ((settings.m_mode == AcarsDemodSettings::ACARS)
         || (settings.m_mode == AcarsDemodSettings::Iridium)) ? 4.5 : 18.0;
}

static Real effectiveRfBandwidth(const AcarsDemodSettings& settings)
{
    if (settings.m_mode == AcarsDemodSettings::Iridium) {
        return (Real) AcarsDemodSink::channelSampleRate(settings);
    }

    return settings.m_rfBandwidth;
}

void AcarsDemodSink::logIridiumSummary()
{
    const int bursts = m_iridiumScanner.burstCount();

    if (bursts == m_iridiumLog.m_bursts) {
        return;
    }

    qInfo() << "Iridium:" << (bursts - m_iridiumLog.m_bursts) << "bursts in the last"
            << m_iridiumLogSeconds << "s on" << m_iridiumChannels << "channels,"
            << (m_iridiumScanner.duplicateCount() - m_iridiumLog.m_duplicates) << "duplicated,"
            << (m_iridiumScanner.frameCount() - m_iridiumLog.m_frames) << "frames decoded of which"
            << (m_iridiumScanner.idaCount() - m_iridiumLog.m_ida) << "short burst data,"
            << (m_iridiumScanner.messageCount() - m_iridiumLog.m_messages) << "messages reassembled of which"
            << (m_iridiumCarrierCount - m_iridiumLog.m_carriers) << "carry a payload,"
            << (m_iridiumScanner.acarsCount() - m_iridiumLog.m_acars) << "ACARS";

    m_iridiumLog.m_bursts = bursts;
    m_iridiumLog.m_duplicates = m_iridiumScanner.duplicateCount();
    m_iridiumLog.m_frames = m_iridiumScanner.frameCount();
    m_iridiumLog.m_ida = m_iridiumScanner.idaCount();
    m_iridiumLog.m_messages = m_iridiumScanner.messageCount();
    m_iridiumLog.m_carriers = m_iridiumCarrierCount;
    m_iridiumLog.m_acars = m_iridiumScanner.acarsCount();
}

void AcarsDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "AcarsDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        // The published HFDL channel frequency is the (suppressed) SSB carrier; the PSK
        // subcarrier sits 1440 Hz above it, so tuning to the published frequency needs
        // an extra shift down onto the subcarrier
        int subcarrier = m_settings.m_mode == AcarsDemodSettings::HFDL ? ACARSHFDL_SUBCARRIER_HZ : 0;
        m_nco.setFreq(-(channelFrequencyOffset + subcarrier), channelSampleRate);

        // Retuning moves the signal, so everything a receiver has worked out about where
        // it is - the carrier it is tracking, the symbol clock phase it has locked, the
        // instant it expects the next frame at - describes the old position and not the
        // new one. Only a mode or rate change used to reset them, which is why changing
        // one was the way to get a demodulator going again after nudging the offset.
        //
        // Not on a sample rate change alone: that path already rebuilds the interpolator
        // below and the receivers cope with it, and a reset there would throw away a
        // transmission in progress for nothing.
        if (m_channelFrequencyOffset != channelFrequencyOffset)
        {
            m_vdl2Receiver.reset();
            m_hfdlReceiver.reset();
            m_aeroReceiver.reset();
        }
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        // VDL-2, HFDL and Aero need a sharper filter: the root raised cosine skirt sits
        // close to the channel edge and the default 4.5 taps per phase dents it enough
        // to cause symbol errors
        m_interpolator.create(16, channelSampleRate, effectiveRfBandwidth(m_settings) / 2.2f,
                              inputFilterTaps(m_settings));
        m_interpolatorDistance = (Real) channelSampleRate / (Real) AcarsDemodSink::channelSampleRate(m_settings);
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void AcarsDemodSink::applySettings(const AcarsDemodSettings& settings, bool force)
{
    qDebug() << "AcarsDemodSink::applySettings: force: " << force;

    // An Aero submode or an Iridium scan width moves the rate just as a mode change does
    bool modeChanged = AcarsDemodSettings::rateChanged(settings, m_settings);

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || modeChanged || force)
    {
        m_interpolator.create(16, m_channelSampleRate, effectiveRfBandwidth(settings) / 2.2f,
                              inputFilterTaps(settings));
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) AcarsDemodSink::channelSampleRate(settings);
        m_interpolatorDistanceRemain = m_interpolatorDistance;
        int subcarrier = settings.m_mode == AcarsDemodSettings::HFDL ? ACARSHFDL_SUBCARRIER_HZ : 0;
        m_nco.setFreq(-(m_channelFrequencyOffset + subcarrier), m_channelSampleRate);
    }

    // Reconfiguring reallocates and resets the receiver, which throws away a transmission in
    // progress, so only do it when something it cares about has actually moved
    if ((settings.m_correlationThreshold != m_settings.m_correlationThreshold) || force)
    {
        AcarsOqpskReceiver::Config config = m_receiver.config();

        config.m_detectThreshold = settings.m_correlationThreshold;
        m_receiver.configure(config);
    }

    if (modeChanged || force)
    {
        // Stamp the protocol into the SAME queue the packets go to, so the worker applies
        // it in stream order with them rather than from its own separately delivered
        // settings - see AcarsDemod::MsgProtocolChange
        if (m_messageQueueToChannel) {
            m_messageQueueToChannel->push(AcarsDemod::MsgProtocolChange::create(settings.m_mode));
        }
        m_vdl2Receiver.reset();
        m_hfdlReceiver.reset();
        // setMode reconfigures the matched filter and buffers for the new rate, and
        // resets, so it has to be called rather than reset() alone
        m_aeroReceiver.setMode(AcarsAeroReceiver::submodeRate(settings.m_aeroChannel),
                               AcarsAeroReceiver::submodeChannel(settings.m_aeroChannel));
    }

    m_settings = settings;
}
