///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Vasyl Samoilov <vasyl.samoilov@gmail.com>                  //
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

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMutexLocker>
#include <QStorageInfo>
#include <QUuid>

#include <algorithm>
#include <limits>

#include "dsp/dspengine.h"
#include "dsp/scopevis.h"
#include "channel/channelapi.h"
#include "util/messagequeue.h"
#include "maincore.h"

#include "radiopagingpocsag.h"
#include "radiopagingflex.h"
#include "radiopaginggsc.h"
#include "radiopagingpage.h"
#include "radiopagingkanji.h"
#include "radiopagingrecode.h"

#include "radiopagingvoicestore.h"
#include "radiopagingdemodsink.h"

namespace {

const int legFilterTaps = 65;
const int statusPeriodSamples = RadioPagingDemodSettings::m_channelSampleRate / 10;   // 100 ms
const double voiceCutoff = 3000.0;      // Hz, upper edge of the voice band
const int voiceWriteSamples = 4096;     // Voice samples written to the file at a time
const qint64 voiceMinFreeBytes = 500LL * 1024 * 1024;  // No recording on a disk with less free space
const Real dcBlockPole = 0.995f;        // About 30 Hz at 38400 Hz
const int audioBufferSize = 1 << 12;
const double huntCentreDecay = 0.967216;    // exp(-1 s / 30 s), per second
const double narrowResidual = 300.0;        // Hz: a leg is narrowed once a measurement is this close

enum Protocol {
    ProtocolPocsag,
    ProtocolFlex,
    ProtocolGsc
};

// The time zone as multimon-ng FLEX_NEXT writes it: +2h, -3h30m, with DST when the time is daylight saving time
QString flexZoneValue(const RadioPagingFlexDecoder::NetworkTime& t)
{
    const int offset = t.m_zoneMinutes;
    const int magnitude = std::abs(offset);
    QString value = (magnitude % 60 == 0) ? QString::asprintf("%+dh", offset / 60)
        : QString::asprintf("%+dh%02dm", offset / 60, magnitude % 60);

    return (t.m_dst == 0) ? value + " DST" : value;  // L0 = 0: daylight saving time (STD-43A 3.7.2)
}

// The network time of a FLEX message, in the form of the flextime object of multimon-ng FLEX_NEXT: the time of the
// frame of the message (the frame offset added to a time of frame 0), its precision, each component with its age in
// frames, and the time mode. Empty when the network sent none
QVariantMap flexTimeData(const RadioPagingFlexDecoder::NetworkTime& t)
{
    QVariantMap ft;

    if (!t.m_dateValid && !t.m_timeValid && !t.m_zoneValid) {
        return ft;
    }

    auto age = [](QVariantMap& component, int frames) {
        component.insert("age_frames", frames);
        component.insert("age_seconds", ((int) (frames * 187.5)) / 100.0);
    };

    if (t.m_dateValid && t.m_timeValid)
    {
        QString timestamp = QString::asprintf("%04d-%02u-%02u %02u:%02u:%05.2f", t.m_year, t.m_month, t.m_day,
            t.m_currentHour, t.m_currentMinute, t.m_currentSecond);

        if (t.m_zoneValid) {
            timestamp += " " + flexZoneValue(t);
        }

        ft.insert("timestamp", timestamp);
        // The extended second of the zone word refines the 7.5 s of the time word
        ft.insert("precision_seconds", t.m_zoneValid ? 0.9375 : 7.5);
    }
    if (t.m_dateValid)
    {
        QVariantMap date;
        date.insert("value", QString::asprintf("%04d-%02u-%02u", t.m_year, t.m_month, t.m_day));
        age(date, t.m_dateAge);
        ft.insert("date", date);
    }
    if (t.m_timeValid)
    {
        QVariantMap time;
        time.insert("value", QString::asprintf("%02u:%02u", t.m_hour, t.m_minute));
        time.insert("sec_coarse", t.m_second7_5);
        age(time, t.m_timeAge);
        ft.insert("time", time);
    }
    if (t.m_zoneValid)
    {
        QVariantMap tz;
        tz.insert("value", flexZoneValue(t));
        tz.insert("offset_min", t.m_zoneMinutes);
        tz.insert("dst", t.m_dst == 0);
        tz.insert("zone_code", t.m_zone);
        tz.insert("sec_ext", t.m_extendedSeconds);

        if (t.m_timeValid) {
            tz.insert("sec_combined", t.m_second7_5 * 7.5 + t.m_extendedSeconds * 0.9375);
        }

        age(tz, t.m_zoneAge);
        ft.insert("tz", tz);
    }

    // Standard: the time word is the time of frame 0 of the cycle, the frame offset is added; direct: the time of the
    // frame. Not confirmed: assumed standard, the time words of a cycle have not told it yet
    ft.insert("time_mode", (t.m_mode == 1) ? "direct" : "standard");
    ft.insert("time_mode_confirmed", t.m_modeConfirmed);
    return ft;
}

} // namespace

void RadioPagingDemodSink::Leg::create(double huntBandwidth, double trackBandwidth, double channelBandwidth, double deviation)
{
    m_huntBandwidth = std::min(huntBandwidth, channelBandwidth);
    m_trackBandwidth = std::min(trackBandwidth, channelBandwidth);
    m_deviation = deviation;
    m_maxShift = std::max(0.0, (channelBandwidth - trackBandwidth) / 2.0);
    m_filter.create(legFilterTaps, RadioPagingDemodSettings::m_channelSampleRate,
        (m_tracking ? m_trackBandwidth : m_huntBandwidth) / 2.0);

    // Outer deviation gives +/-1
    m_discri.setFMScaling(RadioPagingDemodSettings::m_channelSampleRate / (2.0 * deviation));
    m_discri.reset();
}

void RadioPagingDemodSink::Leg::setTracking(bool tracking)
{
    if (tracking != m_tracking)
    {
        m_tracking = tracking;
        m_filter.retune(legFilterTaps, RadioPagingDemodSettings::m_channelSampleRate,
            (m_tracking ? m_trackBandwidth : m_huntBandwidth) / 2.0);
    }
}

void RadioPagingDemodSink::Leg::setShift(double shift)
{
    shift = std::max(-m_maxShift, std::min(m_maxShift, shift));

    if (shift == m_shift) {
        return;
    }

    // The discriminator output moves by the change once the samples shifted before the
    // filter come out of it (its delay)
    m_pendingDelta += (float) (-(shift - m_shift) / m_deviation);
    m_pendingSamples = legFilterTaps / 2;
    m_shift = shift;
    m_nco.setFreq((Real) -m_shift, (Real) RadioPagingDemodSettings::m_channelSampleRate);
}

bool RadioPagingDemodSink::Leg::takeHandover(float& delta)
{
    if ((m_pendingSamples > 0) || (m_pendingDelta == 0.0f)) {
        return false;
    }

    delta = m_pendingDelta;
    m_pendingDelta = 0.0f;
    return true;
}

Real RadioPagingDemodSink::Leg::demod(const Complex& sample)
{
    double magsq;
    Real fmDev;
    Complex c = (m_shift != 0.0) ? sample * m_nco.nextIQ() : sample;

    if (m_pendingSamples > 0) {
        m_pendingSamples--;
    }

    return m_discri.phaseDiscriminatorDelta(m_filter.filter(c), magsq, fmDev);
}

// The residual offset measured by a receiver moves the leg onto the carrier and narrows it
void RadioPagingDemodSink::legMeasured(Leg& leg, int protocol, float dc)
{
    bool correction = (protocol == ProtocolPocsag) ? m_settings.m_pocsagDcCorrection
        : (protocol == ProtocolFlex) ? m_settings.m_flexDcCorrection : m_settings.m_gscDcCorrection;

    if (!correction) {
        return;
    }

    if (leg.m_fits == 0)
    {
        leg.m_messagesAtStart = m_status.m_messages[protocol];
        leg.m_framesAtStart = (protocol == ProtocolFlex) ? m_flex.getStatus().m_frames : 0;
    }

    // Near the noise threshold a signal cut by the filter on one side is measured too close
    // to the filter centre: the leg is narrowed only once the carrier is on its centre (a
    // small residual), before that it is only moved. GSC has too few known patterns for one
    // measurement to place the carrier closely at a low signal to noise ratio: its receiving
    // leg keeps the offset room and is narrowed at the first measurement
    double residual = dc * leg.m_deviation;

    leg.m_fits++;
    leg.setShift(leg.m_shift + residual);

    if ((protocol == ProtocolGsc) || (std::fabs(residual) < narrowResidual)) {
        leg.setTracking(true);
    }
}

// The correction of a transmission that was really received (messages, or measured more than
// once, or a FLEX frame decoded) becomes the hunting centre; otherwise the previous one stays
void RadioPagingDemodSink::legReleased(Leg& leg, int protocol, bool confirmed)
{
    if (leg.m_fits == 0) {
        return;
    }

    confirmed = confirmed || (leg.m_fits >= 2) || (m_status.m_messages[protocol] > leg.m_messagesAtStart);

    if (confirmed) {
        leg.m_huntCentre = leg.m_shift;
    }
    leg.m_fits = 0;
    leg.setTracking(false);
    leg.setShift(leg.m_huntCentre);
}

// Every second: the hunting centres decay to 0 (time constant 30 s), so a wrong or old
// correction does not stay
void RadioPagingDemodSink::decayHuntCentres()
{
    for (Leg *leg : {&m_pocsagLeg, &m_flexLeg, &m_gscLeg})
    {
        // Only while hunting: a transmission being received (measured at least once) keeps
        // its correction
        if ((leg->m_fits > 0) || (leg->m_huntCentre == 0.0)) {
            continue;
        }

        leg->m_huntCentre *= huntCentreDecay;
        if (std::fabs(leg->m_huntCentre) < 10.0) {
            leg->m_huntCentre = 0.0;
        }
        leg->setShift(leg->m_huntCentre);
    }
}

RadioPagingDemodSink::RadioPagingDemodSink() :
    m_channel(nullptr),
    m_messageQueueToChannel(nullptr),
    m_channelSampleRate(RadioPagingDemodSettings::m_channelSampleRate),
    m_channelFrequencyOffset(0),
    m_interpolatorDistance(1.0f),
    m_interpolatorDistanceRemain(1.0f),
    m_magsq(0.0),
    m_magsqSum(0.0),
    m_magsqPeak(0.0),
    m_magsqCount(0),
    m_audioBufferFill(0),
    m_audioSampleRate(48000),
    m_audioDistance(1.0f),
    m_audioDistanceRemain(1.0f),
    m_audioOpen(false),
    m_dcIn(0.0f),
    m_dcOut(0.0f),
    m_statusCount(0)
{
    m_audioBuffer.resize(audioBufferSize);
    m_scopeBuffer.resize(m_scopeBufferSize);
    m_audioInterpolator.create(16, RadioPagingDemodSettings::m_channelSampleRate, voiceCutoff);

    for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++) {
        m_pocsag[i].create(RadioPagingDemodSettings::m_channelSampleRate, RadioPagingDemodSettings::m_pocsagSpeeds[i]);
    }
    m_flex.create(RadioPagingDemodSettings::m_channelSampleRate);
    m_gsc.create(RadioPagingDemodSettings::m_channelSampleRate);
    m_gsc.setNonBatterySaver(true);     // Every GSC transmission is decoded
    m_flex.setReportFragments(true);    // Each fragment is output, then the reassembled message

    m_status = Status();
    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

RadioPagingDemodSink::~RadioPagingDemodSink()
{
}

void RadioPagingDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
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

QString RadioPagingDemodSink::scopeSignalName(int signal)
{
    static const char *names[ScopeSignalCount] = {
        "I", "Q", "MagSq",
        "POCSAG FM", "FLEX FM", "GSC FM",
        "POCSAG offset", "FLEX offset", "GSC offset",
        "POCSAG sync", "FLEX sync", "GSC sync"
    };

    return ((signal >= 0) && (signal < ScopeSignalCount)) ? QString(names[signal]) : QString();
}

Real RadioPagingDemodSink::scopeValue(int signal, const Complex& ci, Real magsq, const Real fm[3], const bool sync[3]) const
{
    switch (signal)
    {
    case ScopeI: return ci.real() / SDR_RX_SCALEF;
    case ScopeQ: return ci.imag() / SDR_RX_SCALEF;
    case ScopeMagSq: return magsq;
    case ScopePocsagFm: return fm[ProtocolPocsag];
    case ScopeFlexFm: return fm[ProtocolFlex];
    case ScopeGscFm: return fm[ProtocolGsc];
    case ScopePocsagShift: return m_pocsagLeg.m_shift / 1000.0;
    case ScopeFlexShift: return m_flexLeg.m_shift / 1000.0;
    case ScopeGscShift: return m_gscLeg.m_shift / 1000.0;
    case ScopePocsagSync: return sync[ProtocolPocsag] ? 1.0f : 0.0f;
    case ScopeFlexSync: return sync[ProtocolFlex] ? 1.0f : 0.0f;
    case ScopeGscSync: return sync[ProtocolGsc] ? 1.0f : 0.0f;
    default: return 0.0f;
    }
}

void RadioPagingDemodSink::sampleToScope(const Complex& sample)
{
    m_scopeBuffer[m_scopeBufferIndex++] = sample;

    if (m_scopeBufferIndex == m_scopeBufferSize)
    {
        std::vector<ComplexVector::const_iterator> vbegin;
        vbegin.push_back(m_scopeBuffer.begin());
        m_scopeSink->feed(vbegin, m_scopeBufferSize);
        m_scopeBufferIndex = 0;
    }
}

void RadioPagingDemodSink::processOneSample(Complex& ci)
{
    Real magsq = (ci.real() * ci.real() + ci.imag() * ci.imag()) / (SDR_RX_SCALED * SDR_RX_SCALED);

    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    // A protocol that is on is decoded at every speed, mode and polarity. After each sample
    // the legs follow the carrier offsets the receivers measure (see Leg)
    float dc;
    float delta;
    Real fms[3] = {0.0f, 0.0f, 0.0f};
    bool syncs[3] = {false, false, false};

    // One carrier is one protocol at a time: while a FLEX frame is being received (its symbols those of its carrier),
    // the POCSAG and GSC decoders keep hunting, but a lock they find is dropped: it is in the FLEX data. Once the frame
    // ends, or its carrier is lost or replaced, they lock on whatever comes
    const bool flexReceiving = m_settings.m_flexEnabled && m_flex.isReceivingFrame();

    if (m_settings.m_pocsagEnabled)
    {
        Real fm = m_pocsagLeg.demod(ci);
        bool synced = false;
        fms[ProtocolPocsag] = fm;

        for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
        {
            m_pocsag[i].process(fm);

            if (flexReceiving && m_pocsag[i].isSynced())
            {
                m_pocsag[i].dropLock();
                m_status.m_locksDropped[ProtocolPocsag]++;
            }

            while (m_pocsag[i].hasMessage()) {
                sendPocsag(m_pocsag[i].takeMessage(), RadioPagingDemodSettings::m_pocsagSpeeds[i]);
            }
            if (m_pocsag[i].takeLevelsMeasured(dc) && !flexReceiving) {
                legMeasured(m_pocsagLeg, ProtocolPocsag, dc);
            }
            synced = synced || m_pocsag[i].isSynced();
        }

        if (!synced) {
            legReleased(m_pocsagLeg, ProtocolPocsag, false);
        }
        syncs[ProtocolPocsag] = synced;
        if (m_pocsagLeg.takeHandover(delta))
        {
            for (auto& pocsag : m_pocsag) {
                pocsag.offsetLevels(delta);
            }
        }
    }

    if (m_settings.m_flexEnabled)
    {
        fms[ProtocolFlex] = m_flexLeg.demod(ci);
        m_flex.process(fms[ProtocolFlex], magsq);
        syncs[ProtocolFlex] = !m_flex.isHunting();

        while (m_flex.hasMessage()) {
            sendFlex(m_flex.takeMessage());
        }
        if (m_flex.takeLevelsMeasured(dc)) {
            legMeasured(m_flexLeg, ProtocolFlex, dc);
        }
        if (m_flex.isHunting() && m_flexLeg.m_tracking) {
            legReleased(m_flexLeg, ProtocolFlex, m_flex.getStatus().m_frames > m_flexLeg.m_framesAtStart);
        }
        if (m_flexLeg.takeHandover(delta)) {
            m_flex.offsetLevels(delta);
        }
    }

    if (m_settings.m_gscEnabled)
    {
        Real fm = m_gscLeg.demod(ci);
        m_gsc.process(fm);
        fms[ProtocolGsc] = fm;

        if (flexReceiving && m_gsc.locked())
        {
            m_gsc.dropLock();
            m_status.m_locksDropped[ProtocolGsc]++;
        }

        syncs[ProtocolGsc] = m_gsc.locked();

        while (m_gsc.hasMessage()) {
            sendGsc(m_gsc.takeMessage());
        }
        if (m_gsc.takeLevelsMeasured(dc) && !flexReceiving) {
            legMeasured(m_gscLeg, ProtocolGsc, dc);
        }
        if (!m_gsc.locked()) {
            legReleased(m_gscLeg, ProtocolGsc, false);
        }
        if (m_gscLeg.takeHandover(delta)) {
            m_gsc.offsetLevels(delta);
        }

        if (m_settings.m_gscAudio || m_settings.m_gscVoiceRecord) {
            processAudio(fm);
        }
    }

    if (m_scopeSink && m_scopeEnabled.load(std::memory_order_relaxed))
    {
        sampleToScope(Complex(
            scopeValue(m_settings.m_scopeCh1, ci, magsq, fms, syncs),
            scopeValue(m_settings.m_scopeCh2, ci, magsq, fms, syncs)));
    }

    if (++m_statusCount >= statusPeriodSamples)
    {
        m_statusCount = 0;
        m_statusPeriods++;
        updateStatus();

        if (++m_afcCount >= 10)
        {
            m_afcCount = 0;
            decayHuntCentres();
        }
    }
}

void RadioPagingDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "RadioPagingDemodSink::applyChannelSettings:"
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
        m_interpolatorDistance = (Real) channelSampleRate / (Real) RadioPagingDemodSettings::m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
    applyReceiverFrequency();
}

void RadioPagingDemodSink::applyReceiverFrequency()
{
    qint64 frequency = m_centerFrequency + m_channelFrequencyOffset;

    for (auto& pocsag : m_pocsag) {
        pocsag.setFrequency(frequency);
    }

    m_flex.setFrequency(frequency);
    m_gsc.setFrequency(frequency);
}

void RadioPagingDemodSink::applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings, bool force)
{
    qDebug() << "RadioPagingDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) RadioPagingDemodSettings::m_channelSampleRate;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    bool legs = settingsKeys.contains("rfBandwidth")
        || settingsKeys.contains("pocsagDeviation") || settingsKeys.contains("flexDeviation")
        || settingsKeys.contains("gscDeviation") || settingsKeys.contains("pocsagDcCorrection")
        || settingsKeys.contains("flexDcCorrection") || settingsKeys.contains("gscDcCorrection") || force;

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    makeCustomCodepages(settingsKeys, force);

    if (legs) {
        applyLegs();
    }

    // A leg whose DC correction is switched off goes back to the channel centre
    const std::pair<const char*, Leg*> corrections[] = {
        {"pocsagDcCorrection", &m_pocsagLeg}, {"flexDcCorrection", &m_flexLeg}, {"gscDcCorrection", &m_gscLeg}
    };
    const bool enabled[] = {m_settings.m_pocsagDcCorrection, m_settings.m_flexDcCorrection, m_settings.m_gscDcCorrection};

    for (int i = 0; i < 3; i++)
    {
        if ((settingsKeys.contains(corrections[i].first) || force) && !enabled[i])
        {
            Leg& leg = *corrections[i].second;
            leg.m_fits = 0;
            leg.m_huntCentre = 0.0;
            leg.setTracking(false);
            leg.setShift(0.0);
        }
    }

    if (settingsKeys.contains("flexMaxMessageNumber") || force) {
        m_flex.setMaxMessageNumber(m_settings.m_flexMaxMessageNumber);
    }

    // A protocol switched off starts from scratch when it is switched on again
    if (settingsKeys.contains("pocsagEnabled") && !m_settings.m_pocsagEnabled) {
        resetProtocol(0);
    }
    if (settingsKeys.contains("flexEnabled") && !m_settings.m_flexEnabled) {
        resetProtocol(1);
    }
    if ((settingsKeys.contains("gscAudio") && !m_settings.m_gscAudio && !m_settings.m_gscVoiceRecord)
        || (settingsKeys.contains("gscVoiceRecord") && !m_settings.m_gscVoiceRecord && !m_settings.m_gscAudio)
        || (settingsKeys.contains("gscEnabled") && !m_settings.m_gscEnabled)) {
        closeAudio();
    }
    if (settingsKeys.contains("gscVoiceRecord") && !m_settings.m_gscVoiceRecord) {
        finishVoiceRecording();
    }
    if (settingsKeys.contains("gscVoiceRecord") && m_settings.m_gscVoiceRecord && m_audioOpen && !m_voiceRecorder) {
        startVoiceRecording();
    }
    if (settingsKeys.contains("gscEnabled") && !m_settings.m_gscEnabled) {
        resetProtocol(2);
    }
    if (settingsKeys.contains("gscVoiceTimeout") || force) {
        m_gsc.setVoiceTimeout(m_settings.m_gscVoiceTimeout);
    }
}

// The messages being received are released first: batches and frames decoded as far as they were received, partial
// messages marked incomplete
void RadioPagingDemodSink::flushProtocol(int protocol)
{
    if (((protocol == 0) || (protocol < 0)) && m_settings.m_pocsagEnabled)
    {
        for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
        {
            m_pocsag[i].flush();

            while (m_pocsag[i].hasMessage()) {
                sendPocsag(m_pocsag[i].takeMessage(), RadioPagingDemodSettings::m_pocsagSpeeds[i]);
            }
        }
    }
    if (((protocol == 1) || (protocol < 0)) && m_settings.m_flexEnabled)
    {
        m_flex.flush();

        while (m_flex.hasMessage()) {
            sendFlex(m_flex.takeMessage());
        }
    }
    if (((protocol == 2) || (protocol < 0)) && m_settings.m_gscEnabled)
    {
        m_gsc.flush();

        while (m_gsc.hasMessage()) {
            sendGsc(m_gsc.takeMessage());
        }
    }
}

void RadioPagingDemodSink::resetProtocol(int protocol)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();

    flushProtocol(protocol);

    // The polarities received are forgotten too: the status shows one again until another is received
    for (int p = 0; p < 3; p++)
    {
        if ((protocol >= 0) && (p != protocol)) {
            continue;
        }

        for (int pol = 0; pol < 2; pol++)
        {
            m_polLast[p][pol] = PolarityLast();
            QMutexLocker mutexLocker(&m_statusMutex);
            m_polMessageSeen[p][pol] = false;
        }
        if (p == 1)
        {
            for (int pol = 0; pol < 2; pol++)
            {
                m_flexLastMode[pol].clear();
                m_flexLastFrame[pol] = RadioPagingFlexDecoder::FrameReport();
                std::fill(m_flexLastPhaseBer[pol], m_flexLastPhaseBer[pol] + 4, RadioPagingBerWindow());
            }
        }
    }

    if ((protocol == 0) || (protocol < 0))
    {
        for (auto& pocsag : m_pocsag) {
            pocsag.reset();
        }
        m_resetTime[0] = now;
    }
    if ((protocol == 1) || (protocol < 0))
    {
        m_flex.reset();
        m_resetTime[1] = now;
    }
    if ((protocol == 2) || (protocol < 0))
    {
        m_gsc.reset();
        m_gscVoiceId[0].clear();
        m_gscVoiceId[1].clear();
        m_resetTime[2] = now;
    }
}

void RadioPagingDemodSink::applyAudioSampleRate(int sampleRate)
{
    qDebug("RadioPagingDemodSink::applyAudioSampleRate: %d", sampleRate);

    if (sampleRate <= 0) {
        return;
    }

    closeAudio();
    m_audioFifo.setSize(sampleRate);
    m_audioDistance = (Real) RadioPagingDemodSettings::m_channelSampleRate / (Real) sampleRate;
    m_audioDistanceRemain = m_audioDistance;
    m_audioSampleRate = sampleRate;
}

// The audio opens when the alert time of a voice page is over, as a pager opens its audio
// after the alert, and closes when the page ends: turn-off word, new transmission, signal
// lost or the voice timeout (the longest voice message)
void RadioPagingDemodSink::processAudio(Real fm)
{
    if (!m_gsc.inVoiceMessage())
    {
        closeAudio();
        return;
    }

    if (!m_audioOpen)
    {
        m_audioOpen = true;
        m_audioDistanceRemain = m_audioDistance;
        m_dcIn = fm;
        m_dcOut = 0.0f;

        if (m_settings.m_gscVoiceRecord) {
            startVoiceRecording();
        }
    }

    Real y = fm - m_dcIn + dcBlockPole * m_dcOut;
    m_dcIn = fm;
    m_dcOut = y;

    Complex c(y, 0.0f);
    Complex out;

    if (m_audioDistance < 1.0f)
    {
        while (!m_audioInterpolator.interpolate(&m_audioDistanceRemain, c, &out))
        {
            pushAudio(out.real());
            recordAudio(out.real());
            m_audioDistanceRemain += m_audioDistance;
        }
    }
    else if (m_audioInterpolator.decimate(&m_audioDistanceRemain, c, &out))
    {
        pushAudio(out.real());
        recordAudio(out.real());
        m_audioDistanceRemain += m_audioDistance;
    }
}

// A file per voice message, named by the time it started. The voice is not scaled: the
// discriminator gives +/-1 at the GSC deviation, as the transmitter takes the samples of a
// sound file. Without a directory to write to nothing is recorded
void RadioPagingDemodSink::startVoiceRecording()
{
    QString directory = m_settings.voiceDirectory();

    m_voiceRecorder.reset();
    m_voiceFile.clear();
    m_voiceFileDone.clear();
    m_voiceBuffer.clear();

    if (!QDir().mkpath(directory))
    {
        qWarning() << "RadioPagingDemodSink::startVoiceRecording: cannot create" << directory;
        return;
    }

    // A disk that fills up is not written to
    QStorageInfo storage(directory);

    if (storage.isValid() && (storage.bytesAvailable() < voiceMinFreeBytes))
    {
        qWarning() << "RadioPagingDemodSink::startVoiceRecording: not enough free space in" << directory;
        return;
    }

    std::unique_ptr<WavFileRecord> recorder(new WavFileRecord((quint32) m_audioSampleRate));
    recorder->setMono(true);
    // Named after the alias of the demodulator, by default its frequency
    recorder->setFileName(RadioPagingVoiceStore::filePrefix(directory,
        m_settings.channelName(), "rx"));

    if (!recorder->startRecording())
    {
        qWarning() << "RadioPagingDemodSink::startVoiceRecording: cannot write in" << directory;
        return;
    }

    m_voiceFile = recorder->getCurrentFileName();
    m_voiceBuffer.reserve(voiceWriteSamples);
    m_voiceRecorder = std::move(recorder);
}

void RadioPagingDemodSink::recordAudio(Real sample)
{
    if (!m_voiceRecorder) {
        return;
    }

    Real v = sample * std::numeric_limits<int16_t>::max();
    m_voiceBuffer.push_back((qint16) std::max<Real>(std::numeric_limits<int16_t>::min(),
        std::min<Real>(std::numeric_limits<int16_t>::max(), v)));

    // Written in blocks, not sample by sample
    if (m_voiceBuffer.size() >= voiceWriteSamples)
    {
        m_voiceRecorder->writeMono(m_voiceBuffer.data(), (int) m_voiceBuffer.size());
        m_voiceBuffer.clear();
    }
}

// The file of the voice message is kept for the record of its end, which can come after the
// audio closed (the record is sent by the receiver, the audio follows its state)
QString RadioPagingDemodSink::finishVoiceRecording()
{
    if (!m_voiceRecorder) {
        return QString();
    }

    if (!m_voiceBuffer.empty())
    {
        m_voiceRecorder->writeMono(m_voiceBuffer.data(), (int) m_voiceBuffer.size());
        m_voiceBuffer.clear();
    }

    m_voiceRecorder->stopRecording();
    m_voiceRecorder.reset();
    m_voiceFileDone = m_voiceFile;
    m_voiceFile.clear();

    // The old files go a few at a time, when a new one is complete, away from the DSP thread
    RadioPagingVoiceStore::cleanupInBackground(m_settings.voiceDirectory(), m_settings.m_gscVoiceKeepDays, m_settings.m_gscVoiceKeepMin,
        RadioPagingVoiceStore::m_autoCleanupBatch, m_voiceFileDone);

    return m_voiceFileDone;
}

void RadioPagingDemodSink::pushAudio(Real sample)
{
    if (!m_settings.m_gscAudio) {
        return;
    }

    // The discriminator gives +/-1 at the GSC deviation
    Real v = sample * m_settings.m_volume * std::numeric_limits<int16_t>::max();
    qint16 s = (qint16) std::max<Real>(std::numeric_limits<int16_t>::min(), std::min<Real>(std::numeric_limits<int16_t>::max(), v));

    m_audioBuffer[m_audioBufferFill].l = s;
    m_audioBuffer[m_audioBufferFill].r = s;

    if (++m_audioBufferFill >= m_audioBuffer.size())
    {
        m_audioFifo.write((const quint8*) &m_audioBuffer[0], m_audioBufferFill);
        m_audioBufferFill = 0;
    }
}

// The rest of the voice is output, then nothing until the next voice page
void RadioPagingDemodSink::closeAudio()
{
    if (!m_audioOpen) {
        return;
    }

    if (m_audioBufferFill > 0)
    {
        m_audioFifo.write((const quint8*) &m_audioBuffer[0], m_audioBufferFill);
        m_audioBufferFill = 0;
    }

    m_audioOpen = false;
    finishVoiceRecording();
}

// The filters are rebuilt when the bandwidth or a deviation changes. All POCSAG speeds
// share one leg sized to the fastest
void RadioPagingDemodSink::applyLegs()
{
    double channel = m_settings.m_rfBandwidth;

    // Following the carrier (DC correction), a leg hunts at the channel width; otherwise at
    // Carson and the offset room
    bool pocsagAfc = m_settings.m_pocsagDcCorrection;
    bool flexAfc = m_settings.m_flexDcCorrection;
    bool gscAfc = m_settings.m_gscDcCorrection;

    m_pocsagLeg.create(pocsagAfc ? channel : m_settings.pocsagBandwidth(), m_settings.pocsagCarson(), channel, m_settings.m_pocsagDeviation);
    m_flexLeg.create(flexAfc ? channel : m_settings.flexBandwidth(), m_settings.flexCarson(), channel, m_settings.m_flexDeviation);
    // GSC receives with the offset room: one measurement may place its carrier that far off
    m_gscLeg.create(gscAfc ? channel : m_settings.gscBandwidth(), gscAfc ? m_settings.gscBandwidth() : m_settings.gscCarson(),
        channel, m_settings.m_gscDeviation);

    qDebug() << "RadioPagingDemodSink::applyLegs: hunting / receiving"
        << "POCSAG" << m_pocsagLeg.m_huntBandwidth << m_pocsagLeg.m_trackBandwidth
        << "FLEX" << m_flexLeg.m_huntBandwidth << m_flexLeg.m_trackBandwidth
        << "GSC" << m_gscLeg.m_huntBandwidth << m_gscLeg.m_trackBandwidth;
}

RadioPagingDemodSink::Status RadioPagingDemodSink::getStatus() const
{
    QMutexLocker mutexLocker(&m_statusMutex);
    return m_status;
}

void RadioPagingDemodSink::updateStatus()
{
    Status status;

    for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
    {
        RadioPagingPocsagReceiver::Status s = m_pocsag[i].getStatus();
        bool normal = s.m_normal.m_state != RadioPagingPocsagDecoder::Hunting;
        bool inverted = s.m_inverted.m_state != RadioPagingPocsagDecoder::Hunting;

        status.m_pocsagSync[i] = normal || inverted;
        status.m_pocsagInverted[i] = inverted && !normal;

        // The BER of a synchronised decoder is the live one; the last one is kept after
        if (normal || inverted)
        {
            const RadioPagingBerWindow& ber = inverted && !normal ? s.m_inverted.m_ber : s.m_normal.m_ber;
            if (ber.valid()) {
                m_lastBer[0] = ber;
            }
            // The carrier offset: the correction of the leg and what is left at the
            // discriminator (+/-1 at the deviation)
            m_lastOffset[0] = m_pocsagLeg.m_shift + s.m_dc * m_settings.m_pocsagDeviation;
            m_lastOffsetValid[0] = true;
            m_lastPocsagBatches = inverted && !normal ? s.m_inverted.m_batches : s.m_normal.m_batches;
        }
    }

    bool pocsagSync = status.m_pocsagSync[0] || status.m_pocsagSync[1] || status.m_pocsagSync[2];

    RadioPagingFlexReceiver::Status flex = m_flex.getStatus();
    status.m_flexSync = flex.m_state != RadioPagingFlexReceiver::Hunting;
    status.m_flexMode = RadioPagingFlex::modeName(flex.m_mode);
    status.m_flexInverted = flex.m_inverted;
    status.m_flexQuality = flex.m_quality;
    status.m_flexErs = flex.m_ers;
    status.m_flexErsSeconds = flex.m_ersSeconds;
    status.m_flexErsCount = flex.m_ersDetections;
    if (status.m_flexSync && flex.m_ber.valid()) {
        m_lastBer[1] = flex.m_ber;
    }
    if (status.m_flexSync)
    {
        m_lastOffset[1] = m_flexLeg.m_shift + flex.m_dc * m_settings.m_flexDeviation;
        m_lastOffsetValid[1] = true;
    }
    status.m_flexFrame = flex.m_frameReport;
    status.m_flexTime = flex.m_networkTime;

    for (int p = 0; p < 4; p++) {
        status.m_flexPhaseBer[p] = flex.m_phaseBer[p];
    }
    status.m_flexGroups = flex.m_groups;
    status.m_flexReassemblies = flex.m_reassemblies;
    status.m_pocsagBatches = m_lastPocsagBatches;
    status.m_gscPreamble = m_lastGscPreamble;
    status.m_gscBatch = m_lastGscBatch;
    status.m_gscNbs = m_lastGscNbs;
    status.m_gscSlot = m_lastGscSlot;

    RadioPagingGscReceiver::Status gsc = m_gsc.getStatus();
    bool gscNormal = gsc.m_normal.m_state != RadioPagingGscDecoder::Hunting;
    bool gscInverted = gsc.m_inverted.m_state != RadioPagingGscDecoder::Hunting;
    status.m_gscSync = gscNormal || gscInverted;
    if (status.m_gscSync)
    {
        const RadioPagingBerWindow& ber = gscInverted && !gscNormal ? gsc.m_inverted.m_ber : gsc.m_normal.m_ber;
        if (ber.valid()) {
            m_lastBer[2] = ber;
        }
        m_lastOffset[2] = m_gscLeg.m_shift + gsc.m_dc * m_settings.m_gscDeviation;
        m_lastOffsetValid[2] = true;
    }

    bool sync[3] = { pocsagSync, status.m_flexSync, status.m_gscSync };

    for (int p = 0; p < 3; p++)
    {
        status.m_ber[p] = m_lastBer[p];
        status.m_berLive[p] = sync[p] && m_lastBer[p].valid();
        status.m_offset[p] = m_lastOffset[p];
        status.m_offsetValid[p] = m_lastOffsetValid[p];
    }
    status.m_gscVoice = (gsc.m_normal.m_state == RadioPagingGscDecoder::Voice) || (gsc.m_inverted.m_state == RadioPagingGscDecoder::Voice);
    status.m_gscInverted = gscInverted && !gscNormal;
    status.m_gscAudio = m_audioOpen;

    status.m_enabled[0] = m_settings.m_pocsagEnabled;
    status.m_enabled[1] = m_settings.m_flexEnabled;
    status.m_enabled[2] = m_settings.m_gscEnabled;

    int pocsagRemembered = 0;
    for (const auto& pocsag : m_pocsag) {
        pocsagRemembered += pocsag.remembered();
    }
    status.m_remembered[0] = pocsagRemembered;
    status.m_remembered[1] = m_flex.remembered();
    status.m_remembered[2] = m_gsc.remembered();
    std::copy(m_resetTime, m_resetTime + 3, status.m_resetTime);

    updatePolarityStatus(status, flex, gsc);

    QMutexLocker mutexLocker(&m_statusMutex);
    std::copy(m_status.m_messages, m_status.m_messages + 3, status.m_messages);

    for (int p = 0; p < 3; p++)
    {
        for (int pol = 0; pol < 2; pol++)
        {
            status.m_pol[p][pol].m_messages = m_polMessages[p][pol];
            status.m_pol[p][pol].m_seen = status.m_pol[p][pol].m_seen || m_polMessageSeen[p][pol];
        }
    }

    // Every sample counts, signal or noise: the time of the signal processed
    status.m_signalMs = (qint64) (m_statusPeriods * statusPeriodSamples * 1000 / RadioPagingDemodSettings::m_channelSampleRate);
    m_status = status;
}

// The status of each polarity of each protocol: a polarity is shown once it was received (synchronised or a message)
void RadioPagingDemodSink::updatePolarityStatus(Status& status, const RadioPagingFlexReceiver::Status& flex,
    const RadioPagingGscReceiver::Status& gsc)
{
    bool sync[3][2] = {{false, false}, {false, false}, {false, false}};

    for (int pol = 0; pol < 2; pol++)
    {
        for (int i = 0; i < RadioPagingDemodSettings::m_nbPocsagSpeeds; i++)
        {
            RadioPagingPocsagReceiver::Status s = m_pocsag[i].getStatus();
            const RadioPagingPocsagDecoder::Status& d = (pol == 1) ? s.m_inverted : s.m_normal;
            // A real signal only: a sync codeword alone is found in noise and in the other protocols, it counts once
            // it is confirmed or a batch was decoded after it
            bool speedSync = (d.m_state != RadioPagingPocsagDecoder::Hunting) && (d.m_confirmed || (d.m_batches > 0));
            status.m_pocsagSpeedSync[pol][i] = speedSync;

            if (speedSync)
            {
                sync[0][pol] = true;
                if (d.m_ber.valid()) {
                    m_polLast[0][pol].m_ber = d.m_ber;
                }
                m_polLast[0][pol].m_offset = m_pocsagLeg.m_shift + s.m_dc * m_settings.m_pocsagDeviation;
                m_polLast[0][pol].m_offsetValid = true;
            }
        }

        const RadioPagingGscDecoder::Status& g = (pol == 1) ? gsc.m_inverted : gsc.m_normal;
        // A real signal only: a preamble lock alone is found in noise and in the other protocols, it counts once the
        // start code or an address follows
        sync[2][pol] = (g.m_state == RadioPagingGscDecoder::InTransmission) || (g.m_state == RadioPagingGscDecoder::Voice);
        status.m_gscVoicePol[pol] = g.m_state == RadioPagingGscDecoder::Voice;

        if (sync[2][pol])
        {
            if (g.m_ber.valid()) {
                m_polLast[2][pol].m_ber = g.m_ber;
            }
            m_polLast[2][pol].m_offset = m_gscLeg.m_shift + gsc.m_dc * m_settings.m_gscDeviation;
            m_polLast[2][pol].m_offsetValid = true;
        }

        // FLEX: the networks of both polarities are kept by the decoder
        status.m_flexGroupsPol[pol] = m_flex.groups(pol == 1);
        status.m_flexGroupStatesPol[pol] = m_flex.groupStates(pol == 1);
        status.m_flexReassembliesPol[pol] = m_flex.reassemblies(pol == 1);
        status.m_flexTimePol[pol] = m_flex.networkTime(pol == 1);
    }

    // FLEX, the frame being received: its polarity, mode and carrier offset
    if (status.m_flexSync)
    {
        int pol = flex.m_inverted ? 1 : 0;
        sync[1][pol] = true;
        m_polLast[1][pol].m_offset = m_flexLeg.m_shift + flex.m_dc * m_settings.m_flexDeviation;
        m_polLast[1][pol].m_offsetValid = true;
        m_flexLastMode[pol] = RadioPagingFlex::modeName(flex.m_mode);
    }

    // FLEX, the last frame decoded of each polarity and its bit error rates, kept by the decoder for each network:
    // the frames of the two polarities may come in turn, the frame being received is not the last one decoded
    for (int pol = 0; pol < 2; pol++)
    {
        if (flex.m_berPol[pol].valid()) {
            m_polLast[1][pol].m_ber = flex.m_berPol[pol];
        }
        if (flex.m_frameReportPol[pol].m_fiwValid)
        {
            m_flexLastFrame[pol] = flex.m_frameReportPol[pol];
            std::copy(flex.m_phaseBerPol[pol], flex.m_phaseBerPol[pol] + 4, m_flexLastPhaseBer[pol]);
        }
    }

    for (int pol = 0; pol < 2; pol++)
    {
        status.m_flexModePol[pol] = m_flexLastMode[pol];
        status.m_flexFramePol[pol] = m_flexLastFrame[pol];
        std::copy(m_flexLastPhaseBer[pol], m_flexLastPhaseBer[pol] + 4, status.m_flexPhaseBerPol[pol]);
    }

    for (int p = 0; p < 3; p++)
    {
        for (int pol = 0; pol < 2; pol++)
        {
            PolarityLast& last = m_polLast[p][pol];
            Status::Polarity& s = status.m_pol[p][pol];
            last.m_seen = last.m_seen || sync[p][pol];
            s.m_seen = last.m_seen;
            s.m_sync = sync[p][pol];
            s.m_ber = last.m_ber;
            s.m_berLive = sync[p][pol] && last.m_ber.valid();
            s.m_offset = last.m_offset;
            s.m_offsetValid = last.m_offsetValid;
        }
    }

    int pocsagRemembered[2] = {0, 0};
    for (const auto& pocsag : m_pocsag)
    {
        pocsagRemembered[0] += pocsag.remembered(0);
        pocsagRemembered[1] += pocsag.remembered(1);
    }
    for (int pol = 0; pol < 2; pol++)
    {
        status.m_pol[0][pol].m_remembered = pocsagRemembered[pol];
        status.m_pol[1][pol].m_remembered = m_flex.remembered(pol);
        status.m_pol[2][pol].m_remembered = m_gsc.remembered(pol);
    }
}

void RadioPagingDemodSink::sendRecord(MessageRecord& record, int protocol)
{
    record.m_direction = MessageRecord::DirectionRX;
    record.m_kind = MessageRecord::KindMessage;
    if (record.m_id.isNull()) {
        record.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    record.m_timestamp = QDateTime::currentMSecsSinceEpoch();
    record.m_protocolData.insert("interpretedBy", "decoder");
    record.m_quality = RadioPagingPage::receivedQuality(record);

    {
        QMutexLocker mutexLocker(&m_statusMutex);
        m_status.m_messages[protocol]++;
        int pol = (record.m_polarity == MessageRecord::PolarityInverted) ? 1 : 0;
        m_polMessages[protocol][pol]++;
        m_polMessageSeen[protocol][pol] = true;
    }

    if (m_messageQueueToChannel) {
        m_messageQueueToChannel->push(MainCore::MsgMessageRecord::create(m_channel, record));
    }
}

void RadioPagingDemodSink::sendPocsag(const RadioPagingPocsagDecoder::Message& message, int baud)
{
    MessageRecord record;
    const RadioPagingCharset::Codepage *charset = codepage(m_settings.m_pocsagCharset, "pocsag");
    const std::vector<uint8_t> fill = m_settings.pocsagFillCodes(*charset);
    RadioPagingPocsag::Interpretation interpretation = RadioPagingPocsag::interpret(message.m_rawData, message.m_rawBits, charset, fill);

    record.m_protocol = "pocsag";
    record.m_mode = QString::number(baud);
    record.m_speed = baud;
    // RIC and function (sub-address A-D): the address of the pager
    record.m_address = QString::fromStdString(RadioPagingPocsag::addressText(message.m_address, message.m_function));
    record.m_polarity = polarity(message.m_polarity == RadioPagingPocsagDecoder::Inverted);
    record.m_msgType = QString::fromStdString(interpretation.m_type);
    if (message.m_rawBits > 0)
    {
        // The characters of the codewords that could not be corrected are '?'; char_status colours the others
        QString charStatus;
        record.m_text = RadioPagingRecode::pocsagText(message.m_rawData, message.m_rawBits, interpretation.m_type, charset, fill,
            message.m_wordStatus, charStatus);
        record.m_protocolData.insert("word_status", QString::fromStdString(message.m_wordStatus));

        if (charset->m_rtl && (interpretation.m_type == "alphanumeric")) {
            RadioPagingRecode::reverseText(record.m_text, charStatus);
        }

        insertCustomCodes(record, charset, 0);

        if (!charStatus.isEmpty()) {
            record.m_protocolData.insert("char_status", charStatus);
        }
    }
    record.m_rawData = toByteArray(message.m_rawData);
    record.m_rawBits = message.m_rawBits;
    record.m_errorsCorrected = message.m_errorsCorrected;
    record.m_errorsUncorrectable = message.m_errorsUncorrectable;
    record.m_protocolData.insert("ric", (qulonglong) message.m_address);
    record.m_protocolData.insert("function", message.m_function);
    record.m_protocolData.insert("batch", message.m_batch);
    // How it ended: idle, address, batch (filled the last batch, no idle codeword after it) or cut
    record.m_protocolData.insert("end", RadioPagingPocsagDecoder::endName(message.m_end));
    record.m_protocolData.insert("score", interpretation.m_score);
    insertAlternative(record, interpretation.m_altType, interpretation.m_altText, interpretation.m_altScore);
    // The codepage of the alphanumeric text (the type or the other candidate) and its fill codes, to decode it again
    if (message.m_rawBits > 0)
    {
        QVariantList fillCodes;

        for (uint8_t code : fill) {
            fillCodes.append((int) code);
        }

        record.m_protocolData.insert("charset", RadioPagingPage::charsetData(charset->key()));
        record.m_protocolData.insert("fill_codes", fillCodes);
    }
    record.m_duplicate = message.m_duplicate ? MessageRecord::DuplicateDup : MessageRecord::DuplicateNone;

    sendRecord(record, ProtocolPocsag);
}

void RadioPagingDemodSink::sendFlex(const RadioPagingFlexDecoder::Message& message)
{
    MessageRecord record;

    record.m_protocol = "flex";
    record.m_mode = RadioPagingFlex::modeName(message.m_mode);
    record.m_speed = message.m_bitRate;
    record.m_address = QString::fromStdString(message.m_address);
    record.m_polarity = polarity(message.m_inverted);
    record.m_msgType = QString::fromStdString(message.m_typeName);
    // A short message with every digit a space is the tone only payload: no text. The characters of an alphanumeric
    // message (secure type 00 too) are the codes of the codepage
    const bool characters = (message.m_type == RadioPagingFlexDecoder::TypeAlphanumeric)
        || ((message.m_type == RadioPagingFlexDecoder::TypeSecure) && (message.m_secureType == 0));
    const RadioPagingCharset::Codepage *flexCharset = codepage(m_settings.m_flexCharset, "flex");
    // Japanese Text Service (STD-43A 1.3.1): the Kanji codepage shows the HEX/binary messages of 16 or 8-bit
    // characters as Shift JIS text; its alphanumeric messages keep the default codepage
    const int blocking = (message.m_blocking == 0) ? 16 : message.m_blocking;
    const bool kanji = isKanji(flexCharset) && (message.m_type == RadioPagingFlexDecoder::TypeBinary)
        && ((blocking == 16) || (blocking == 8));
    const RadioPagingCharset::Codepage *charset = kanji ? flexCharset
        : !characters ? nullptr
        : isKanji(flexCharset) ? RadioPagingCharset::defaultFor("flex")
        : flexCharset;

    if (kanji)
    {
        if (!message.m_text.empty()) {
            record.m_text = escapeText(RadioPagingKanji::decode(message.m_text, blocking, message.m_displayRtl));
        }

        record.m_protocolData.insert("charset", RadioPagingPage::charsetData(charset->key()));
    }
    else
    {
        if (!message.m_text.empty() && !message.m_toneOnly)
        {
            // Lost characters are '?', a lost fragment of unknown length is <...>; char_status colours them
            QString charStatus;

            if (!charset && (message.m_charStatus.size() == message.m_text.size()))
            {
                // Digits (numeric, HEX/binary, secure binary): the text as it is, a status for each digit; a lost
                // fragment of hex ('|', G) is coloured as lost
                record.m_text = RadioPagingRecode::escapeText(message.m_text);
                charStatus = QString::fromStdString(message.m_charStatus).replace('G', 'X');

                if (charStatus.size() != record.m_text.size()) {
                    charStatus.clear();
                }
            }
            else
            {
                record.m_text = RadioPagingRecode::decodeWithStatus(message.m_text, message.m_charStatus, charset, charStatus);
            }

            if (charset && charset->m_rtl) {
                RadioPagingRecode::reverseText(record.m_text, charStatus);
            }

            if (!charStatus.isEmpty()) {
                record.m_protocolData.insert("char_status", charStatus);
            }
        }
        if (charset) {
            record.m_protocolData.insert("charset", RadioPagingPage::charsetData(charset->key()));
            // The 7-bit characters and their status, to decode them again with another codepage
            record.m_protocolData.insert("alpha_codes", QString::fromLatin1(QByteArray(message.m_text.data(), (int) message.m_text.size()).toHex()));
            record.m_protocolData.insert("code_status", QString::fromStdString(message.m_charStatus));
            insertCustomCodes(record, charset, 1);
        }
    }
    record.m_rawData = toByteArray(message.m_rawData);
    record.m_rawBits = message.m_rawBits;
    record.m_errorsCorrected = message.m_bitErrors;
    record.m_errorsUncorrectable = message.m_wordsUncorrectable;
    record.m_protocolData.insert("capcode", (qulonglong) message.m_capcode);
    record.m_protocolData.insert("long_address", message.m_longAddress);
    record.m_protocolData.insert("priority", message.m_priority);
    record.m_protocolData.insert("cycle", message.m_cycle);
    record.m_protocolData.insert("frame", message.m_frame);
    record.m_protocolData.insert("phase", QString(QChar(message.m_phase)));

    // The network time as of the frame of the message (the network of its polarity)
    const QVariantMap flexTime = flexTimeData(m_flex.networkTime(message.m_inverted));

    if (!flexTime.isEmpty()) {
        record.m_protocolData.insert("flextime", flexTime);
    }

    // Multiple transmission (STD-43A 3.4.2): a subframe of the frame, its copies combined
    if (message.m_transmissions > 1)
    {
        record.m_protocolData.insert("transmissions", message.m_transmissions);
        record.m_protocolData.insert("subframe_copies", message.m_copies);
    }
    record.m_protocolData.insert("message_number", message.m_messageNumber);
    record.m_protocolData.insert("fragmented", message.m_fragmented);
    record.m_protocolData.insert("fragment_count", message.m_fragmentCount);
    // Kind of the last fragment (complete for a message in one piece), its F field, and the frame of the first
    record.m_protocolData.insert("fragment_kind", RadioPagingFlexDecoder::fragmentKindName(message.m_fragment));
    record.m_protocolData.insert("fragment_number", message.m_fragmentNumber);
    record.m_protocolData.insert("first_cycle", message.m_firstCycle);
    record.m_protocolData.insert("first_frame", message.m_firstFrame);
    record.m_protocolData.insert("complete", message.m_complete);
    record.m_protocolData.insert("word_map", QString::fromStdString(message.m_wordMap));
    record.m_protocolData.insert("words_corrected", message.m_wordsCorrected);
    record.m_protocolData.insert("parity_corrections", message.m_parityCorrections);
    record.m_protocolData.insert("quality", message.m_quality);
    record.m_protocolData.insert("addr_type", RadioPagingFlexDecoder::addressTypeName(message.m_addressType));

    // Operator messaging address (STD-43A 3.8.2.4): what its address names and the first message word
    if (!message.m_operatorSubtype.empty())
    {
        record.m_protocolData.insert("operator_subtype", QString::fromStdString(message.m_operatorSubtype));
        record.m_protocolData.insert("operator_category", QString::fromStdString(message.m_operatorCategory));
        record.m_protocolData.insert("operator_payload", (qlonglong) message.m_operatorPayload);
    }
    record.m_protocolData.insert("maildrop", message.m_mailDrop);
    record.m_protocolData.insert("retrieval", message.m_retrieval);

    // Numbers of the sequence of the address skipped before this message (R = 1): missed messages
    if (!message.m_missedNumbers.empty())
    {
        QVariantList missed;

        for (int n : message.m_missedNumbers) {
            missed.append(n);
        }

        record.m_protocolData.insert("missed_numbers", missed);
    }
    record.m_protocolData.insert("special", message.m_special);
    record.m_protocolData.insert("checksum", checkName(message.m_checksum));
    record.m_protocolData.insert("signature", checkName(message.m_signature));
    record.m_protocolData.insert("is_fragment", message.m_isFragment);
    record.m_protocolData.insert("fragment_gap", message.m_fragmentGap);
    record.m_protocolData.insert("payload_unknown", message.m_payloadUnknown);

    // A vector of a message of no words: no header, no data; the raw data is the vector
    if (message.m_noWords) {
        record.m_protocolData.insert("no_words", true);
    }

    // Alphanumeric fragment control bits U0 V0 (3.10.1.3): "00" enhanced fragmentation not supported, "01" reserved,
    // "10" default character mode, "11" alternative character mode. Fragments after the first only
    if (message.m_fragmentControl >= 0)
    {
        static const char *const control[4] = {"00", "01", "10", "11"};

        record.m_protocolData.insert("fragment_control", control[message.m_fragmentControl & 3]);
        record.m_protocolData.insert("nul_error", message.m_nulError);
    }

    // HEX/binary header flags (STD-43A 3.10.1.2): blocking length (bits per character, 16 when 0), display
    // direction, header message, status information enabler, reserved bits
    if (message.m_type == RadioPagingFlexDecoder::TypeBinary)
    {
        record.m_protocolData.insert("blocking", message.m_blocking == 0 ? 16 : message.m_blocking);
        record.m_protocolData.insert("display_direction", message.m_displayRtl ? "rtl" : "ltr");
        record.m_protocolData.insert("header_message", message.m_headerMessage);
        record.m_protocolData.insert("status_info", message.m_statusInfo);
        record.m_protocolData.insert("reserved_bits", message.m_reservedBits);

        // The digits of characters of 8 or 16 bits, to decode them again as Shift JIS text (or back to hex)
        if ((blocking == 16) || (blocking == 8)) {
            record.m_protocolData.insert("hex", QString::fromStdString(message.m_text));
        }
    }

    // Secure message type t1t0 (00 alphanumeric, 10 binary, 01 defined separately, 11 reserved)
    if (message.m_type == RadioPagingFlexDecoder::TypeSecure)
    {
        record.m_protocolData.insert("secure_type", message.m_secureType);
        record.m_protocolData.insert("registration_ack", message.m_registrationAck);
    }

    // The text is hex: digit size, order of the bits in a digit and the number of data bits. fill_state: how the
    // end of the data was found (consistent, inconsistent with the termination rules, unknown)
    if (message.m_hexNibbleBits > 0)
    {
        static const char *const fill[3] = {"consistent", "inconsistent", "unknown"};

        record.m_protocolData.insert("fill_state", fill[std::max(0, std::min(2, message.m_fillState))]);
        record.m_protocolData.insert("hex_nibble_bits", message.m_hexNibbleBits);
        record.m_protocolData.insert("hex_bit_order", message.m_hexLsbFirst ? "lsb_first" : "msb_first");
        record.m_protocolData.insert("hex_bits", message.m_dataBits);
    }

    if (message.m_nidSystemMessage)
    {
        record.m_protocolData.insert("nid_system_message", true);
        record.m_protocolData.insert("nid_change", message.m_nidChange);
    }

    if (message.m_tempSlot >= 0)
    {
        QVariantList members;

        for (uint64_t capcode : message.m_groupMembers) {
            members.append(QString::fromStdString(RadioPagingFlex::addressText(capcode)));
        }

        record.m_protocolData.insert("group_slot", message.m_tempSlot);
        record.m_protocolData.insert("group_tracked", message.m_groupTracked);
        record.m_protocolData.insert("group_capcodes", members);
    }

    if (message.m_type == RadioPagingFlexDecoder::TypeShortMessage)
    {
        record.m_protocolData.insert("short_type", message.m_shortType);
        record.m_protocolData.insert("tone_only", message.m_toneOnly);
        record.m_protocolData.insert("short_source", message.m_shortSource);

        if (message.m_networkId)
        {
            record.m_protocolData.insert("nid_service_area", message.m_nidServiceArea);
            record.m_protocolData.insert("nid_multiplier", message.m_nidMultiplier);
            record.m_protocolData.insert("nid_traffic_flags", message.m_nidTrafficFlags);
        }
    }

    if (message.m_type == RadioPagingFlexDecoder::TypeShortInstruction)
    {
        record.m_protocolData.insert("instruction_type", message.m_instructionType);
        record.m_protocolData.insert("instruction_data", message.m_instructionData);
        record.m_protocolData.insert("instruction_slot", message.m_instructionSlot);
        record.m_protocolData.insert("instruction_frame", message.m_instructionFrame);

        // System event (type 1): the event flags of the 14 data bits
        if (message.m_instructionType == 1) {
            record.m_protocolData.insert("event_flags", message.m_eventFlags);
        }
    }

    record.m_duplicate = !message.m_duplicate ? MessageRecord::DuplicateNone
        : message.m_recovered ? MessageRecord::DuplicateDupPlus : MessageRecord::DuplicateDup;

    sendRecord(record, ProtocolFlex);
}

void RadioPagingDemodSink::sendGsc(const RadioPagingGscDecoder::Message& message)
{
    MessageRecord record;
    int pol = (message.m_polarity == RadioPagingGscDecoder::Inverted) ? 1 : 0;

    // Address and address number 1-4 (function A-D, 0-3 as in POCSAG); the page type is in the message type
    record.m_protocol = "gsc";
    record.m_mode = "600";
    record.m_speed = 600;
    record.m_address = QString::fromStdString(RadioPagingGsc::addressText(message.m_address, message.m_addressNumber));
    record.m_polarity = polarity(pol == 1);
    record.m_rawData = toByteArray(message.m_rawData);
    record.m_rawBits = message.m_rawBits;
    record.m_errorsCorrected = message.m_errorsCorrected;
    record.m_errorsUncorrectable = message.m_errorsUncorrectable;
    record.m_protocolData.insert("address_number", message.m_addressNumber);
    record.m_protocolData.insert("function", message.m_addressNumber - 1);
    record.m_protocolData.insert("preamble", message.m_preamble);
    // Batch mode (the batch of the transmission), not the batch index of POCSAG
    record.m_protocolData.insert("batch_mode", message.m_batch);
    record.m_protocolData.insert("slot", message.m_slot);
    record.m_protocolData.insert("non_battery_saver", message.m_nonBatterySaver);
    // How it ended: complete, or cut (the transmission ended before the message did)
    record.m_protocolData.insert("end", message.m_truncated ? "cut" : "complete");

    m_lastGscPreamble = message.m_preamble;
    m_lastGscBatch = message.m_batch;
    m_lastGscNbs = message.m_nonBatterySaver;
    m_lastGscSlot = message.m_slot;
    record.m_duplicate = message.m_duplicate ? MessageRecord::DuplicateDup : MessageRecord::DuplicateNone;

    if (message.m_type == RadioPagingGsc::Data)
    {
        const RadioPagingCharset::Codepage *charset = codepage(m_settings.m_gscCharset, "gsc");
        RadioPagingGsc::Interpretation interpretation = RadioPagingGsc::interpret(message.m_rawData, message.m_rawBits, charset,
            &message.m_rawLost, &message.m_rawCorrected);

        record.m_msgType = QString::fromStdString(interpretation.m_type);
        record.m_text = escapeText(interpretation.m_text);
        // The status of each character colours it (corrected, lost); none when escaping changed the length
        QString charStatus = QString::fromStdString(interpretation.m_status);

        if (charStatus.size() != record.m_text.size()) {
            charStatus.clear();
        }
        if (charset->m_rtl && (interpretation.m_type == "alphanumeric")) {
            RadioPagingRecode::reverseText(record.m_text, charStatus);
        }
        if (!charStatus.isEmpty()) {
            record.m_protocolData.insert("char_status", charStatus);
        }

        insertCustomCodes(record, charset, 2);
        // The bits of the codewords that could not be corrected, their characters '?': kept for a new decoding
        if (std::any_of(message.m_rawLost.begin(), message.m_rawLost.end(), [](uint8_t b) { return b != 0; })) {
            record.m_protocolData.insert("lost_bits", QString(toByteArray(message.m_rawLost).toHex()));
        }
        record.m_protocolData.insert("charset", RadioPagingPage::charsetData(charset->key()));
        record.m_protocolData.insert("checksum_errors", message.m_checksumErrors);
        record.m_protocolData.insert("checksum_passed", message.m_checksumPassed);
        record.m_protocolData.insert("score", interpretation.m_score);
        insertAlternative(record, interpretation.m_altType, interpretation.m_altText, interpretation.m_altScore);
    }
    else if (message.m_type == RadioPagingGsc::Voice)
    {
        static const char *ends[] = { "", "turn_off", "new_transmission", "timeout", "signal_lost" };

        record.m_msgType = "voice";

        if (message.m_update)
        {
            // The end of the voice message: an update of the voice page record
            record.m_id = m_gscVoiceId[pol];
            m_gscVoiceId[pol].clear();
            record.m_protocolData.insert("voice_end", ends[(int) message.m_voiceEnd]);
            record.m_protocolData.insert("voice_seconds", message.m_voiceSeconds);

            // The file is closed before the record goes out, the message log plays it from there
            finishVoiceRecording();

            if (!m_voiceFileDone.isEmpty())
            {
                record.m_protocolData.insert("voice_file", m_voiceFileDone);
                m_voiceFileDone.clear();
            }
        }
        else
        {
            // An update follows when the voice message ends
            record.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            record.m_partial = true;
            m_gscVoiceId[pol] = record.m_id;
        }
    }
    else
    {
        record.m_msgType = "tone";
        record.m_protocolData.insert("interleaved", message.m_interleaved);
    }

    sendRecord(record, ProtocolGsc);
}

// The other reading of a message. A long numeric one is not given: a text read as numbers is mostly digits and
// separators of no meaning
void RadioPagingDemodSink::insertAlternative(MessageRecord& record, const std::string& type, const std::string& text, int score)
{
    QString altType = QString::fromStdString(type);
    QString altText = escapeText(text);

    if (!RadioPagingRecode::alternativeGiven(altType, altText))
    {
        altType.clear();
        altText.clear();
    }

    record.m_protocolData.insert("alt_type", altType);
    record.m_protocolData.insert("alt_text", altText);
    record.m_protocolData.insert("alt_score", score);
}

QString RadioPagingDemodSink::checkName(int check)
{
    switch (check)
    {
    case RadioPagingFlexDecoder::CheckOk: return "ok";
    case RadioPagingFlexDecoder::CheckFailed: return "failed";
    case RadioPagingFlexDecoder::CheckIncomplete: return "incomplete";
    default: return "";
    }
}

MessageRecord::Polarity RadioPagingDemodSink::polarity(bool inverted)
{
    return inverted ? MessageRecord::PolarityInverted : MessageRecord::PolarityNormal;
}

const RadioPagingCharset::Codepage *RadioPagingDemodSink::codepage(const RadioPagingCharset::Key& key, const char *protocol) const
{
    if (RadioPagingCustomCodepage::isCustom(key))
    {
        const int p = (std::string(protocol) == "pocsag") ? 0 : (std::string(protocol) == "flex") ? 1 : 2;

        if (m_customCodepage[p]) {
            return &m_customCodepage[p]->codepage();
        }
    }

    const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::find(protocol, key);
    return codepage ? codepage : RadioPagingCharset::defaultFor(protocol);
}

void RadioPagingDemodSink::makeCustomCodepages(const QStringList& settingsKeys, bool force)
{
    const char *keys[3] = {"pocsagCustomCodes", "flexCustomCodes", "gscCustomCodes"};
    const QString *codes[3] = {&m_settings.m_pocsagCustomCodes, &m_settings.m_flexCustomCodes, &m_settings.m_gscCustomCodes};
    const char *protocols[3] = {"pocsag", "flex", "gsc"};

    for (int p = 0; p < 3; p++)
    {
        if (force || settingsKeys.contains(keys[p]) || !m_customCodepage[p]) {
            m_customCodepage[p].reset(new RadioPagingCustomCodepage(protocols[p], codes[p]->toStdString()));
        }
    }
}

void RadioPagingDemodSink::insertCustomCodes(MessageRecord& record, const RadioPagingCharset::Codepage *charset, int protocol) const
{
    const QString *codes[3] = {&m_settings.m_pocsagCustomCodes, &m_settings.m_flexCustomCodes, &m_settings.m_gscCustomCodes};

    if (charset && m_customCodepage[protocol] && (charset == &m_customCodepage[protocol]->codepage())) {
        record.m_protocolData.insert("custom_codes", *codes[protocol]);
    }
}

bool RadioPagingDemodSink::isKanji(const RadioPagingCharset::Codepage *codepage)
{
    return codepage && codepage->m_transcoder && codepage->m_transcoder->m_binary;
}

QString RadioPagingDemodSink::escapeText(const std::string& text)
{
    return RadioPagingRecode::escapeText(text);
}

QByteArray RadioPagingDemodSink::toByteArray(const std::vector<uint8_t>& data)
{
    return QByteArray((const char *) data.data(), (int) data.size());
}
