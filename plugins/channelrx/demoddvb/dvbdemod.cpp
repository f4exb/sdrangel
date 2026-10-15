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

#include <algorithm>
#include <functional>

#include <QBuffer>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSet>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGDVBDemodActions.h"
#include "SWGDVBDemodEPGEvent.h"
#include "SWGDVBDemodEPGService.h"

#include "channel/channelwebapiutils.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"

#include "dvbdemod.h"

MESSAGE_CLASS_DEFINITION(DVBDemod::MsgConfigureDVBDemod, Message)

typedef DVBDemodSettings S; // For the standards

const char * const DVBDemod::m_channelIdURI = "sdrangel.channel.demoddvb";
const char * const DVBDemod::m_channelId = "DVBDemod";

DVBDemod::DVBDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0),
        m_retuneSettled(false),
        m_manualRecording(-1)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("bbDVBDemod");

    m_basebandSink = new DVBDemodBaseband();
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, QStringList(), true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DVBDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &DVBDemod::handleIndexInDeviceSetChanged
    );

    // The guide from earlier, as a full one takes minutes to receive
    getEPG()->load(DVBDemodEPG::defaultCacheFileName());
    QObject::connect(&m_epgSaveTimer, &QTimer::timeout, this, &DVBDemod::saveEPG);
    m_epgSaveTimer.start(5 * 60 * 1000);

    // Programmes to record are started and stopped by the transport stream's time
    QObject::connect(&m_scheduleTimer, &QTimer::timeout, this, &DVBDemod::checkSchedule);
    m_scheduleTimer.start(1000);

    QObject::connect(&m_tuneTimer, &QTimer::timeout, this, &DVBDemod::checkTuning);
    m_tuneTimer.start(250);
}

// The service selected, or if automatic, the first TV service of the transport stream by channel number
int DVBDemod::currentServiceId() const
{
    if (m_settings.m_serviceId != 0) {
        return m_settings.m_serviceId;
    }

    // What the player chose (the first program with video, in its order), so a recording's of what's on screen
    if (m_playingServiceId != 0) {
        return m_playingServiceId;
    }

    int serviceId = 0;
    int lowest = 0x7FFFFFFF;

    for (const auto& service : m_basebandSink->getEPG()->getServices())
    {
        // EN 300 468 table 87's television services: SD and HD, MPEG-2, AVC (and its NVOD and frame compatible 3D)
        // and HEVC
        const bool tv = (service.m_type == 0x01) || (service.m_type == 0x11) || ((service.m_type >= 0x16) && (service.m_type <= 0x20));
        const int channel = service.m_channel >= 0 ? service.m_channel : 0x7FFFFFFE;

        if (service.m_actual && tv && (channel < lowest))
        {
            lowest = channel;
            serviceId = service.m_serviceId;
        }
    }

    return serviceId;
}

// A service on another multiplex is tuned to by moving the device, so the channel, keeping its offset from the
// device's centre (as for an LO offset), is on the multiplex's frequency from the NIT, or learned when it was received.
// If neither, the UHF channels are searched for it. The player then finds the service once the new transport stream
// arrives
bool DVBDemod::playService(int networkId, int serviceId, QString& error)
{
    // A recording waiting for the service played before isn't wanted now
    m_pendingRecording.m_active = false;

    // Played by the user: where the device's tuned is theirs, not returned from after a retune for recordings
    if (!m_retuningForRecording) {
        m_recordingRetune.m_active = false;
    }

    // Back to the multiplex searched from, so it isn't taken to be where the service is
    if (m_search.m_active) {
        cancelSearch();
    }

    if (m_scan.m_active) {
        cancelScan();
    }

    // The transport stream received's service with the ids, if it has one (an id can be used in more than one)
    DVBDemodEPG *epg = getEPG();
    DVBDemodEPG::Service target;

    if (!epg->getService(networkId, serviceId, target))
    {
        error = "Service not in the programme guide";
        return false;
    }

    if (target.m_removed)
    {
        error = "The service is no longer broadcast";
        return false;
    }

    m_search.m_text.clear();
    DVBDemodSettings settings = m_settings;
    QStringList settingsKeys = {"serviceId"};
    settings.m_serviceId = serviceId;

    if (!target.m_actual)
    {
        DVBDemodEPG::TransportStream ts;

        if (recordingInProgress())
        {
            error = "The service is on another multiplex: retuning would interrupt the recording";
            return false;
        }

        if (!epg->getTransportStream(networkId, target.m_transportStreamId, ts))
        {
            if (!target.m_multiplexKnown)
            {
                error = "The service's multiplex isn't in this area";
                return false;
            }

            if (DVBDemodSettings::isSatellite(m_settings.m_standard))
            {
                error = "The service's multiplex's frequency isn't known";
                return false;
            }

            DVBDemodSink::Status status;
            getStatus(status);

            if (!status.m_sampleRateOK)
            {
                error = "The sample rate is too low to search for the service's multiplex";
                return false;
            }

            startSearch(target);
            return true;
        }

        if (!retuneDevice(ts.m_frequency - m_settings.m_inputFrequencyOffset))
        {
            error = "The device can't be retuned";
            return false;
        }

        qDebug("DVBDemod::playService: service %d: tuning to %.3f MHz", serviceId, ts.m_frequency / 1e6);

        if (ts.m_satellite)
        {
            // The device's frequency is the downlink's, with its transverter delta set to the LNB's local oscillator. A
            // multiplex on the other polarisation needs the LNB switched: the GUI's done that first (see
            // polarisationChange()); the band isn't handled
            settings.m_standard = ts.m_s2 ? S::StandardDVBS2 : S::StandardDVBS;
            settingsKeys.append("standard");

            // Out of range (a NIT's, or a guide cache's, in error), automatic, not the transponder before's
            settings.m_symbolRate = DVBDemodSettings::isValidSymbolRate(ts.m_symbolRate) ? ts.m_symbolRate : 0;
            settingsKeys.append("symbolRate");

            // EN 300 468 table 35's inner code rates, as DVB-S's
            settings.m_codeRate = !ts.m_s2 && (ts.m_fecInner >= 1) && (ts.m_fecInner <= 5) ? ts.m_fecInner - 1 : -1;
            // The roll-off and input stream as received before (or the roll-off from the descriptor), else automatic
            settings.m_rollOff = !ts.m_s2 ? 0 : DVBDemodSettings::rollOffHundredths(ts.m_rollOff);
            settings.m_isi = ts.m_s2 ? ts.m_isi : -1;
            settingsKeys.append({"codeRate", "rollOff", "isi"});
            applyServiceSettings(settings, settingsKeys);
            m_guideSettings = true;
            m_guideCenterFrequency = ts.m_frequency - m_settings.m_inputFrequencyOffset;
            return true;
        }

        if (DVBDemodSettings::isValidBandwidth(ts.m_bandwidth))
        {
            settings.m_bandwidth = ts.m_bandwidth;
            settingsKeys.append("bandwidth");
        }

        // The UK describes DVB-T2 multiplexes with DVB-T's descriptor, so the standard is found, unless T2's says
        settings.m_standard = ts.m_t2 ? S::StandardDVBT2 : S::StandardAutoTerrestrial;
        settings.m_plpId = ts.m_t2 ? ts.m_plpId : -1;
        settings.m_lowPriority = !ts.m_t2 && ts.m_lowPriority;
        settingsKeys.append({"standard", "plpId", "lowPriority"});
    }

    applyServiceSettings(settings, settingsKeys);
    return true;
}

// The multiplex received says its own polarisation in the NIT; until it's known (as just after retuning), it's the one
// switched to
bool DVBDemod::polarisationChange(int networkId, int serviceId, int& needed, int& current)
{
    DVBDemodEPG *epg = getEPG();
    DVBDemodEPG::Service target;
    DVBDemodEPG::TransportStream ts, received;

    if (!epg->getService(networkId, serviceId, target) || target.m_actual
        || !epg->getTransportStream(networkId, target.m_transportStreamId, ts) || !ts.m_satellite)
    {
        return false;
    }

    if ((epg->getTransportStreamId() >= 0) && epg->getTransportStream(epg->getNetworkId(), epg->getTransportStreamId(), received)
        && received.m_satellite)
    {
        m_polarisation = received.m_polarisation;
    }

    needed = ts.m_polarisation;
    current = m_polarisation;
    return (current >= 0) && (needed != current);
}

void DVBDemod::applyServiceSettings(const DVBDemodSettings& settings, const QStringList& settingsKeys)
{
    applySettings(settings, settingsKeys);

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureDVBDemod::create(settings, settingsKeys, false));
    }
}

// The transport stream being received isn't known until its SI arrives from the new frequency
// A recording waiting for its service's multiplex: started once the transport stream received is its (the retune
// settled, so not the SI of the multiplex before); given up if not within 20 s (not counting a search)
void DVBDemod::checkPendingRecording()
{
    PendingRecording& p = m_pendingRecording;

    if (!p.m_active) {
        return;
    }

    DVBDemodEPG *epg = getEPG();

    if (m_search.m_active)
    {
        p.m_since.start();
    }
    else if (m_retuneSettled && (epg->getNetworkId() == p.m_networkId) && (epg->getTransportStreamId() == p.m_transportStreamId))
    {
        p.m_active = false;
        QString error;

        if (startRecording(error)) {
            qDebug("DVBDemod::checkPendingRecording: recording service %d", p.m_serviceId);
        } else {
            qWarning() << "DVBDemod::checkPendingRecording: can't record service" << p.m_serviceId << ":" << error;
        }
    }
    else if (p.m_since.elapsed() > m_pendingRecordingTimeoutMs)
    {
        p.m_active = false;
        qWarning("DVBDemod::checkPendingRecording: not recording service %d: its multiplex (%d/%d) wasn't received",
            p.m_serviceId, p.m_networkId, p.m_transportStreamId);
    }
}

void DVBDemod::retuned()
{
    m_playingServiceId = 0; // The player chooses again from the new multiplex's
    getEPG()->retuned();
    m_sinceRetune.start();
    m_retuneSettled = false;
}

bool DVBDemod::retuneDevice(qint64 frequency)
{
    if (!ChannelWebAPIUtils::setCenterFrequency(m_deviceAPI->getDeviceSetIndex(), frequency)) {
        return false;
    }

    m_retuneTarget = frequency;
    m_retuneRequested.start();
    retuned();
    return true;
}

// After the device or the channel is moved: the receivers would otherwise keep the old signal's state, and in Auto,
// the standard of the old multiplex for 5 s
void DVBDemod::resetReceivers()
{
    m_basebandSink->getInputMessageQueue()->push(DVBDemodBaseband::MsgResetReceivers::create());
}

void DVBDemod::checkTuning()
{
    DVBDemodEPG *epg = getEPG();

    // SI from the old frequency, still being decoded when retuned, may have set the transport stream: forget it. A
    // second after the device says it's where it was asked to go (a slow device's notification can come a second or
    // more after the retune: settled before, the search and scan took it to be retuned elsewhere), or once that's
    // given up on (it may round the frequency more than the tolerance)
    if (!m_retuneSettled && m_sinceRetune.isValid())
    {
        const bool there = (m_retuneTarget == 0) || (qAbs(m_centerFrequency - m_retuneTarget) <= m_retuneToleranceHz);
        const bool givenUp = !there && (m_retuneRequested.elapsed() >= m_retuneConfirmTimeoutMs);

        if (givenUp)
        {
            qWarning("DVBDemod::checkTuning: the device is at %.6f MHz, not %.6f MHz as asked", m_centerFrequency / 1e6,
                m_retuneTarget / 1e6);
        }

        if ((there && (m_sinceRetune.elapsed() >= m_retuneSettleMs)) || givenUp)
        {
            epg->retuned();
            m_retuneSettled = true;
            m_retuneTarget = 0;
        }
    }

    checkPendingRecording();

    if (m_scan.m_active)
    {
        checkScan();
        return;
    }

    if (m_search.m_active)
    {
        const qint64 tuned = m_centerFrequency + m_settings.m_inputFrequencyOffset;

        if (m_deviceAPI->state() != DeviceAPI::StRunning)
        {
            endSearch(false, "The search stopped, as the device stopped");
            return;
        }

        // Retuned elsewhere, as by the user: what's received isn't the channel being tried, and the device stays
        if (m_retuneSettled && (qAbs(tuned - m_search.m_tuned) > m_retuneToleranceHz))
        {
            qDebug("DVBDemod::checkTuning: retuned to %.3f MHz during the search", tuned / 1e6);
            m_search.m_active = false;
            m_search.m_text = "The search stopped, as the device was retuned";
            return;
        }

        if (m_retuneSettled && (epg->getTransportStreamId() >= 0))
        {
            // A multiplex. Whichever it is, it can be tuned to directly next time
            learnFrequency(tuned);

            if ((epg->getNetworkId() == m_search.m_networkId) && (epg->getTransportStreamId() == m_search.m_transportStreamId))
            {
                endSearch(true, QString());
            }
            else
            {
                m_search.m_index++;
                searchNext();
            }

            return;
        }

        // Nothing to synchronise to, or no transport stream identified in time
        DVBDemodSink::Status status;
        getStatus(status);
        const qint64 elapsed = m_search.m_dwell.elapsed();

        if (((elapsed >= m_searchNoSignalMs) && status.isSearching()) || (elapsed >= m_searchMaxDwellMs))
        {
            m_search.m_index++;
            searchNext();
        }

        return;
    }

    // Learn the frequency of the multiplex being received, if the NIT doesn't give it, once it's been received for a
    // while, so services on it can be tuned to from other multiplexes. A frequency learned before is replaced if this
    // is over 1 MHz from it (as when it was learned from another transmitter); the NIT's never are
    if (m_retuneSettled && (m_sinceRetune.elapsed() >= m_learnFrequencyMs) && (epg->getTransportStreamId() >= 0) && (m_centerFrequency > 0))
    {
        DVBDemodSink::Status status;
        getStatus(status);

        if (status.isLocked()) {
            learnFrequency(m_centerFrequency + m_settings.m_inputFrequencyOffset);
        }
    }
}

void DVBDemod::learnFrequency(qint64 frequency)
{
    DVBDemodEPG *epg = getEPG();

    if (DVBDemodSettings::isSatellite(m_settings.m_standard))
    {
        DVBDemodSink::Status status;
        getStatus(status);

        if ((status.m_standard == S::StandardDVBS) || (status.m_standard == S::StandardDVBS2)) {
            const int symbolRate = m_settings.m_symbolRate > 0 ? m_settings.m_symbolRate : (int) std::max(0.0, status.m_autoSymbolRate);

            if (symbolRate > 0)
            {
                const bool s2 = status.m_standard == S::StandardDVBS2;
                epg->setSatelliteFrequency(frequency, symbolRate, s2, s2 ? status.m_s2.m_matypeRollOff : 0,
                    s2 && (status.m_s2.m_isiCount > 0) ? status.m_s2.m_isi : -1);
            }
        }
    }
    else
    {
        epg->setFrequency(frequency, m_settings.m_bandwidth);
    }
}

// The UHF channels (8 MHz raster, 21 to 48 first, as used since the 700 MHz band was cleared, then 49 to 69), except
// those of multiplexes already known
void DVBDemod::startSearch(const DVBDemodEPG::Service& target)
{
    DVBDemodEPG *epg = getEPG();

    // So the multiplex being left can be returned to from the guide, if it hasn't been received for long
    if (m_retuneSettled && (epg->getTransportStreamId() >= 0) && (m_centerFrequency > 0)) {
        learnFrequency(m_centerFrequency + m_settings.m_inputFrequencyOffset);
    }
    std::vector<qint64> known;
    known.push_back(m_centerFrequency + m_settings.m_inputFrequencyOffset);

    for (const auto& ts : epg->getTransportStreams())
    {
        if (ts.m_frequency > 0) {
            known.push_back(ts.m_frequency);
        }
    }

    m_search = Search();
    m_search.m_active = true;
    m_search.m_networkId = target.m_networkId;
    m_search.m_transportStreamId = target.m_transportStreamId;
    m_search.m_serviceId = target.m_serviceId;
    m_search.m_name = target.m_name.isEmpty() ? QString("service %1").arg(target.m_serviceId) : target.m_name;
    m_search.m_centerFrequency = m_centerFrequency;
    m_search.m_settings = m_settings;

    for (int channel = 21; channel <= 69; channel++)
    {
        const qint64 frequency = 474000000LL + 8000000LL * (channel - 21);
        bool skip = false;

        for (qint64 f : known) {
            skip = skip || (qAbs(f - frequency) < 1000000);
        }

        if (!skip) {
            m_search.m_frequencies.push_back(frequency);
        }
    }

    qDebug("DVBDemod::startSearch: %s (%d/%d): %d channels", qPrintable(m_search.m_name), target.m_networkId,
        target.m_transportStreamId, (int) m_search.m_frequencies.size());

    // Either standard, and any PLP, as the multiplex's aren't known
    DVBDemodSettings settings = m_settings;
    settings.m_standard = S::StandardAutoTerrestrial;
    settings.m_plpId = -1;
    settings.m_lowPriority = false;
    applyServiceSettings(settings, {"standard", "plpId", "lowPriority"});
    searchNext();
}

void DVBDemod::searchNext()
{
    if (m_search.m_index >= (int) m_search.m_frequencies.size())
    {
        endSearch(false, QString("The multiplex of %1 wasn't found").arg(m_search.m_name));
        return;
    }

    const qint64 frequency = m_search.m_frequencies[m_search.m_index];

    if (!retuneDevice(frequency - m_settings.m_inputFrequencyOffset))
    {
        endSearch(false, "The device can't be retuned");
        return;
    }

    m_search.m_tuned = frequency;
    m_search.m_dwell.start();
    m_search.m_text = QString("Searching for %1: %2 MHz (%3 of %4)").arg(m_search.m_name).arg(frequency / 1e6, 0, 'f', 0)
        .arg(m_search.m_index + 1).arg(m_search.m_frequencies.size());
}

void DVBDemod::endSearch(bool found, const QString& text)
{
    m_search.m_active = false;
    m_search.m_text = text;
    DVBDemodSettings settings = m_search.m_settings;

    if (found)
    {
        qDebug("DVBDemod::endSearch: found %s at %.3f MHz", qPrintable(m_search.m_name),
            m_search.m_frequencies[m_search.m_index] / 1e6);
        settings = m_settings;
        settings.m_serviceId = m_search.m_serviceId;
        applyServiceSettings(settings, {"serviceId"});
        m_pendingRecording.m_since.start(); // Its recording, if one's waiting, from now
    }
    else
    {
        qDebug("DVBDemod::endSearch: %s", qPrintable(text));

        if (m_pendingRecording.m_active)
        {
            qWarning() << "DVBDemod::endSearch: not recording" << m_search.m_name << ": its multiplex wasn't found";
            m_pendingRecording.m_active = false;
        }

        retuneDevice(m_search.m_centerFrequency);
        applyServiceSettings(settings, {"serviceId", "standard", "plpId", "lowPriority"});
    }
}

void DVBDemod::cancelSearch()
{
    if (m_search.m_active) {
        endSearch(false, QString());
    }
}

// Kept by the baseband, and set again as the device starts, so the sink isn't left with a scan's band (or the sweep's
// spectrum only, with no symbol rate searched for) if the device stops meanwhile
void DVBDemod::setRateSearchBand(double frequency, double width, bool spectrumOnly)
{
    m_basebandSink->setRateSearchBand(frequency, width, spectrumOnly, ++m_bandGeneration);
}

void DVBDemod::setSatelliteAutomatic(DVBDemodSettings& settings)
{
    settings.m_standard = S::StandardAutoSatellite;
    settings.m_symbolRate = 0;
    settings.m_codeRate = -1;
    settings.m_rollOff = 0;
    settings.m_isi = -1;
}

bool DVBDemod::startScan(qint64 from, qint64 to, QString& error)
{
    if (recordingInProgress())
    {
        error = "Not while recording";
        return false;
    }

    const int rate = getChannelSampleRate();

    if ((m_deviceAPI->state() != DeviceAPI::StRunning) || (rate <= 0) || (m_centerFrequency <= 0))
    {
        error = "The device isn't running";
        return false;
    }

    cancelSearch();
    cancelScan();

    // Steps of most of the device's band (its edges are filtered), its spectrum taken at each, and the signals found in
    // all of it at once
    const qint64 usable = (qint64) (0.8 * rate);
    m_scan = Scan();

    if ((from > 0) && (to > from))
    {
        for (qint64 centre = from + usable / 2; centre - usable / 2 < to; centre += usable) {
            m_scan.m_centres.push_back(centre);
        }

        m_scan.m_from = from;
        m_scan.m_to = to;
    }
    else
    {
        m_scan.m_centres.push_back(m_centerFrequency);
        m_scan.m_from = m_centerFrequency - usable / 2;
        m_scan.m_to = m_centerFrequency + usable / 2;
    }

    m_scan.m_active = true;
    m_scan.m_centerFrequency = m_centerFrequency;
    m_scan.m_settings = m_settings;
    qInfo("DVBDemod::startScan: %.3f to %.3f MHz, %d steps", m_scan.m_from / 1e6, m_scan.m_to / 1e6, (int) m_scan.m_centres.size());

    // Either satellite standard, any symbol rate, MODCOD and stream
    DVBDemodSettings settings = m_settings;
    setSatelliteAutomatic(settings);
    settings.m_inputFrequencyOffset = 0;
    applyServiceSettings(settings, {"standard", "symbolRate", "codeRate", "rollOff", "isi", "inputFrequencyOffset"});
    scanCentre();
    return true;
}

void DVBDemod::cancelScan()
{
    if (m_scan.m_active) {
        endScan("The scan was stopped");
    }
}

// The sweep: the device to the next step, for its spectrum. Then the signals in all of it
void DVBDemod::scanCentre()
{
    if (m_scan.m_centre >= (int) m_scan.m_centres.size())
    {
        findScanSignals();
        return;
    }

    const qint64 centre = m_scan.m_centres[m_scan.m_centre];

    if ((centre != m_centerFrequency) && !retuneDevice(centre))
    {
        endScan("The device can't be retuned");
        return;
    }

    m_scan.m_tuned = centre;

    if (m_settings.m_inputFrequencyOffset != 0)
    {
        DVBDemodSettings settings = m_settings;
        settings.m_inputFrequencyOffset = 0;
        applyServiceSettings(settings, {"inputFrequencyOffset"});
    }

    retuned();
    m_scan.m_surveying = true;
    m_scan.m_bandSent = false;
    m_scan.m_dwell.start();
    m_search.m_text = QString("Scanning: sweeping %1 MHz (%2 of %3)")
        .arg(centre / 1e6, 0, 'f', 1).arg(m_scan.m_centre + 1).arg(m_scan.m_centres.size());
}

// A step's spectrum into the sweep's, each bin at its downlink frequency
void DVBDemod::addScanSpectrum(const DVBDemodSink::Status& status)
{
    const int n = (int) status.m_spectrum.size();
    const double binHz = status.m_spectrumBinHz;

    if ((n == 0) || (binHz <= 0.0)) {
        return;
    }

    if (m_scan.m_power.empty())
    {
        const int bins = (int) ((m_scan.m_to - m_scan.m_from) / binHz) + 1;
        m_scan.m_binHz = binHz;
        m_scan.m_minWidth = status.m_minSignalWidth;
        m_scan.m_power.assign(bins, 0.0);
        m_scan.m_seen.assign(bins, 0);
    }

    for (int i = 0; i < n; i++)
    {
        const double offset = (i - n / 2) * binHz;

        if (std::fabs(offset) > m_sweepHalfWidth * n * binHz) {
            continue;
        }

        const int k = (int) std::lround((m_centerFrequency + offset - m_scan.m_from) / m_scan.m_binHz);

        if ((k >= 0) && (k < (int) m_scan.m_power.size()))
        {
            m_scan.m_power[k] += status.m_spectrum[i];
            m_scan.m_seen[k]++;
        }
    }
}

// The signals in the sweep's spectrum, over the noise floor near each, then tried in turn
void DVBDemod::findScanSignals()
{
    const int bins = (int) m_scan.m_power.size();
    std::vector<double> power(bins, 0.0);
    std::vector<bool> seen(bins, false);

    for (int k = 0; k < bins; k++)
    {
        seen[k] = m_scan.m_seen[k] > 0;
        power[k] = seen[k] ? m_scan.m_power[k] / m_scan.m_seen[k] : 0.0;
    }

    // The floor every m_sweepFloorStep, interpolated between: the 10th percentile of the bins seen near
    const int span = std::max(1, (int) (m_sweepFloorSpan / std::max(1.0, m_scan.m_binHz)));
    const int step = std::max(1, (int) (m_sweepFloorStep / std::max(1.0, m_scan.m_binHz)));
    std::vector<int> floorBins;
    std::vector<double> floors, near;

    for (int k = 0; k < bins; k += step)
    {
        near.clear();

        for (int j = std::max(0, k - span); j <= std::min(bins - 1, k + span); j++)
        {
            if (seen[j]) {
                near.push_back(power[j]);
            }
        }

        if (!near.empty())
        {
            std::nth_element(near.begin(), near.begin() + near.size() / 10, near.end());
            floorBins.push_back(k);
            floors.push_back(near[near.size() / 10]);
        }
    }

    // Over its floor (bins not seen at it), so it's 1
    std::vector<double> overFloor(bins, 1.0);
    size_t f = 0;

    for (int k = 0; (k < bins) && !floors.empty(); k++)
    {
        while ((f + 1 < floorBins.size()) && (floorBins[f + 1] <= k)) {
            f++;
        }

        double floor = floors[f];

        if ((f + 1 < floorBins.size()) && (k > floorBins[f]))
        {
            const double t = (double) (k - floorBins[f]) / (floorBins[f + 1] - floorBins[f]);
            floor = (1.0 - t) * floors[f] + t * floors[f + 1];
        }

        if (seen[k] && (floor > 0.0)) {
            overFloor[k] = power[k] / floor;
        }
    }

    const auto candidates = dvbs::SymbolRateSearch::findSignals(overFloor, 1.0, m_scan.m_binHz, (double) m_scan.m_from,
        m_scan.m_minWidth, m_sweepThreshold);
    m_scan.m_signals.clear();
    m_scan.m_signal = 0;
    QStringList list;

    // Those overlapping as one, the strongest: on a broad weak signal, each small peak over the threshold takes the
    // signal's half power points (below the threshold), so they come out at much the same frequency, out of order
    std::vector<ScanSignal> sorted;

    for (const auto& s : candidates) {
        sorted.push_back({(qint64) std::llround(s.m_frequency), s.m_width, s.m_levelDb});
    }

    std::sort(sorted.begin(), sorted.end(), [](const ScanSignal& a, const ScanSignal& b) {
        return a.m_frequency < b.m_frequency;
    });

    for (const ScanSignal& signal : sorted)
    {
        ScanSignal *last = m_scan.m_signals.empty() ? nullptr : &m_scan.m_signals.back();

        if (last && (qAbs(signal.m_frequency - last->m_frequency) < 0.5 * std::max(signal.m_width, last->m_width)))
        {
            if (signal.m_levelDb > last->m_levelDb) {
                *last = signal;
            }

            continue;
        }

        m_scan.m_signals.push_back(signal);
    }

    for (const ScanSignal& s : m_scan.m_signals)
    {
        list.append(QString("%1 MHz (%2 MHz wide, S/N %3 dB)").arg(s.m_frequency / 1e6, 0, 'f', 3).arg(s.m_width / 1e6, 0, 'f', 1)
            .arg(s.m_levelDb, 0, 'f', 1));
    }

    m_scan.m_power.clear();
    m_scan.m_seen.clear();
    qInfo("DVBDemod::findScanSignals: %.3f to %.3f MHz swept: %d signals to try: %s", m_scan.m_from / 1e6, m_scan.m_to / 1e6,
        (int) m_scan.m_signals.size(), qPrintable(list.join(", ")));
    scanSignal();
}

// The device centred on the next signal found by the sweep (a wide transponder only fits in its band centred), its
// symbol rate searched for in the signal's band only, once the samples are from there
void DVBDemod::scanSignal()
{
    // Not one in a transponder found (split in two, as by a dip in its spectrum), nor within one tried
    while (m_scan.m_signal < (int) m_scan.m_signals.size())
    {
        const qint64 frequency = m_scan.m_signals[m_scan.m_signal].m_frequency;
        bool found = false, tried = false;

        for (const ScanFound& f : m_scan.m_found) {
            found = found || (qAbs(f.m_frequency - frequency) < 0.6 * f.m_symbolRate);
        }

        for (const ScanSignal& t : m_scan.m_tried) {
            tried = tried || (qAbs(t.m_frequency - frequency) < 0.5 * t.m_width);
        }

        if (!found && !tried) {
            break;
        }

        qInfo("DVBDemod::scanSignal: %.3f MHz: in a transponder %s", frequency / 1e6, found ? "found" : "tried");
        m_scan.m_signal++;
    }

    if (m_scan.m_signal >= (int) m_scan.m_signals.size())
    {
        endScan(QString());
        return;
    }

    const ScanSignal& signal = m_scan.m_signals[m_scan.m_signal];

    if ((signal.m_frequency != m_centerFrequency) && !retuneDevice(signal.m_frequency))
    {
        endScan("The device can't be retuned");
        return;
    }

    m_scan.m_tuned = signal.m_frequency;
    retuned();
    m_scan.m_surveying = false;
    m_scan.m_bandSent = false;
    m_scan.m_dwell.start();
    m_search.m_text = QString("Scanning: trying %1 MHz (signal %2 of %3), %4 found")
        .arg(signal.m_frequency / 1e6, 0, 'f', 3).arg(m_scan.m_signal + 1).arg(m_scan.m_signals.size())
        .arg(m_scan.m_found.size());
}

void DVBDemod::checkScan()
{
    if (m_deviceAPI->state() != DeviceAPI::StRunning)
    {
        endScan("The scan stopped, as the device stopped");
        return;
    }

    // Retuned elsewhere (by the user): the scan stops there
    if (m_retuneSettled && (qAbs(m_centerFrequency - m_scan.m_tuned) > m_retuneToleranceHz))
    {
        m_scan.m_active = false;
        setRateSearchBand(0.0, 0.0);
        m_search.m_text = QString("The scan stopped, as the device was retuned: %1 found").arg(m_scan.m_found.size());
        return;
    }

    DVBDemodSink::Status status;
    const qint64 elapsed = m_scan.m_dwell.elapsed();

    if (m_scan.m_surveying)
    {
        // The step's spectrum, from a rate search of the whole band, on samples since the retune: the band's set once
        // the retune's come (the receivers' reset with it restarts the search, after the samples from before), not
        // waiting the second the guide's given to settle, as nothing's decoded. The spectrum only, no rate, so the
        // receivers aren't run: their searches, started again at each step, took the CPU the device needed
        if (!m_scan.m_bandSent)
        {
            if (qAbs(m_centerFrequency - m_scan.m_tuned) > m_retuneToleranceHz)
            {
                if (elapsed >= m_scanRetuneTimeoutMs)
                {
                    m_scan.m_active = false;
                    setRateSearchBand(0.0, 0.0);
                    m_search.m_text = QString("The scan stopped, as the device didn't retune to %1 MHz (or was retuned elsewhere)")
                        .arg(m_scan.m_tuned / 1e6, 0, 'f', 3);
                }

                return;
            }

            setRateSearchBand(0.0, 0.0, true);
            m_scan.m_bandGeneration = m_bandGeneration;
            m_scan.m_bandSent = true;
            m_scan.m_dwell.start();
            return;
        }

        getStatus(status);

        if ((status.m_bandGeneration == m_scan.m_bandGeneration) && (status.m_rateSearches > 0))
        {
            addScanSpectrum(status);
            m_scan.m_centre++;
            scanCentre();
        }
        else if (elapsed >= m_scanSpectrumTimeoutMs)
        {
            m_scan.m_centre++;  // Nothing from this step: its part of the spectrum's left out
            scanCentre();
        }

        return;
    }

    if (!m_retuneSettled) {
        return;
    }

    getStatus(status);

    const ScanSignal& signal = m_scan.m_signals[m_scan.m_signal];

    if (!m_scan.m_bandSent)
    {
        setRateSearchBand(0.0, 1.3 * signal.m_width);
        m_scan.m_bandGeneration = m_bandGeneration;
        m_scan.m_bandSent = true;
        m_scan.m_dwell.start();
        return;
    }

    if (status.m_bandGeneration != m_scan.m_bandGeneration)
    {
        // The band's not applied yet. It's set again if the device restarted meanwhile, so it's not waited for forever
        if (elapsed >= m_scanBandTimeoutMs)
        {
            qWarning("DVBDemod::checkScan: %.3f MHz: the symbol rate search's band wasn't applied", signal.m_frequency / 1e6);
            m_scan.m_tried.push_back(signal);
            m_scan.m_signal++;
            scanSignal();
        }

        return;
    }

    DVBDemodEPG *epg = getEPG();
    const bool generic = (status.m_standard == S::StandardDVBS2) && DVBDemodSink::Status::isGeneric(status.m_s2.m_bb);

    if ((status.isLocked() && (epg->getTransportStreamId() >= 0) && (m_sinceRetune.elapsed() >= m_scanSettleMs)) || (generic && (elapsed >= m_scanSettleMs)))
    {
        // Found: what it is, and (with its SI) into the guide, with where it is
        // Where it is from the carrier offset measured: the survey's estimate is rough for a signal cut at the band's edge
        ScanFound found;
        const bool s2 = status.m_standard == S::StandardDVBS2;
        const double offset = s2 ? status.m_s2.m_frequencyOffset : status.m_s.m_frequencyOffset;
        found.m_frequency = m_centerFrequency + (qint64) std::llround(offset);
        found.m_symbolRate = (int) std::max(0.0, status.m_autoSymbolRate);
        found.m_text = QString("%1 MHz %2 kS/s %3").arg(found.m_frequency / 1e6, 0, 'f', 3).arg(found.m_symbolRate / 1e3, 0, 'f', 0)
            .arg(!s2 ? QString("DVB-S %1").arg(status.m_s.m_fec.m_codeRateKnown ? dvbt::toString(status.m_s.m_fec.m_codeRate) : QString("?"))
                : QString("DVB-S2 %1").arg(status.m_s2.m_pl.m_vlsnr >= 0 ? QString("VL-SNR %1").arg(dvbs2::vlsnrModcod(status.m_s2.m_pl.m_vlsnr).m_name)
                    : status.m_s2.m_pl.m_modcod >= 0 ? QString(dvbs2::modcodString(status.m_s2.m_pl.m_modcod)) : QString("?")));

        if (generic) {
            found.m_text += " (generic stream)";
        } else {
            found.m_text += QString(" network %1 TS %2").arg(epg->getNetworkId()).arg(epg->getTransportStreamId());
        }

        // The same as one found before, from a candidate beside it, isn't listed again
        bool again = false;

        for (const ScanFound& f : m_scan.m_found) {
            again = again || (qAbs(f.m_frequency - found.m_frequency) < 0.5 * std::max(f.m_symbolRate, found.m_symbolRate));
        }

        qInfo("DVBDemod::checkScan: found %s%s", qPrintable(found.m_text), again ? " (again)" : "");

        if (!generic) {
            learnFrequency(found.m_frequency);
        }

        if (!again) {
            m_scan.m_found.push_back(found);
        }

        m_scan.m_signal++;
        scanSignal();
        return;
    }

    // No symbol rate found by two searches (each a fraction of a second, found by the first if there's a signal), or in
    // time, or no lock (or SI) after it was: DVB-S and S2 lock, and the PAT comes, within a few seconds (a scan of
    // Astra 2's band, its 22 transponders all within 2.5 s of the retune)
    const bool noRate = (status.m_autoSymbolRate < 0.0) && ((status.m_rateSearches >= 2) || (elapsed >= m_scanNoRateMs));

    if (noRate || (elapsed >= m_scanNoLockMs))
    {
        // A signal wider at half power than the highest rate the sample rate allows can't be measured or received
        const double maxRate = getChannelSampleRate() / 1.2;
        const QString why = !noRate ? QString("no lock")
            : signal.m_width > 0.95 * maxRate
                ? QString("no symbol rate (%1 MHz wide: more than the %2 MS/s this sample rate allows)")
                    .arg(signal.m_width / 1e6, 0, 'f', 1).arg(maxRate / 1e6, 0, 'f', 2)
                : QString("no symbol rate");
        qInfo("DVBDemod::checkScan: %.3f MHz (S/N %.1f dB): %s", signal.m_frequency / 1e6, signal.m_levelDb, qPrintable(why));
        m_scan.m_tried.push_back(signal);
        m_scan.m_signal++;
        scanSignal();
    }
}

void DVBDemod::endScan(const QString& text)
{
    m_scan.m_active = false;
    setRateSearchBand(0.0, 0.0);
    QStringList found;

    for (const ScanFound& f : m_scan.m_found) {
        found.append(f.m_text);
    }

    m_search.m_text = (text.isEmpty() ? QString("Scan done") : text)
        + QString(": %1 transponder%2 found%3%4").arg(m_scan.m_found.size()).arg(m_scan.m_found.size() == 1 ? "" : "s")
            .arg(found.isEmpty() ? "" : ": ").arg(found.join("; "));
    qInfo("DVBDemod::endScan: %s", qPrintable(m_search.m_text));

    // Back to where it was
    retuneDevice(m_scan.m_centerFrequency);
    applyServiceSettings(m_scan.m_settings, {"standard", "symbolRate", "codeRate", "rollOff", "isi", "inputFrequencyOffset"});
    retuned();
}

QString DVBDemod::serviceName(int serviceId) const
{
    for (const auto& service : m_basebandSink->getEPG()->getServices())
    {
        if (service.m_actual && (service.m_serviceId == serviceId) && !service.m_name.isEmpty()) {
            return service.m_name;
        }
    }

    return QString("Service %1").arg(serviceId);
}

// In the recording directory: the name and local start time, without characters file systems don't allow
QString DVBDemod::recordingFileName(const QString& name, qint64 utc) const
{
    QString base = QString("%1 %2").arg(name).arg(QDateTime::fromSecsSinceEpoch(utc).toLocalTime().toString("yyyy-MM-dd HHmm"));

    for (QChar& c : base)
    {
        if (QString("<>:\"/\\|?*").contains(c) || (c.unicode() < 0x20)) {
            c = '_';
        }
    }

    QDir dir(m_settings.m_recordDirectory.isEmpty() ? QDir::homePath() : m_settings.m_recordDirectory);
    QString fileName = dir.filePath(base + ".ts");

    for (int i = 2; QFile::exists(fileName); i++) {
        fileName = dir.filePath(QString("%1 (%2).ts").arg(base).arg(i));
    }

    return fileName;
}

bool DVBDemod::startRecording(QString& error)
{
    if (m_manualRecording >= 0) {
        return true;
    }

    const qint64 t = getEPG()->getTime() > 0 ? getEPG()->getTime() : QDateTime::currentSecsSinceEpoch();
    const bool service = m_settings.m_recordMode == 1;
    const int serviceId = service ? currentServiceId() : 0;

    if (service && (serviceId == 0))
    {
        error = "No service to record yet";
        return false;
    }

    const QString name = service ? serviceName(serviceId)
        : QString("DVB %1 MHz").arg((m_centerFrequency + m_settings.m_inputFrequencyOffset) / 1e6, 0, 'f', 3);
    const int id = m_basebandSink->startRecording(recordingFileName(name, t),
        service ? DVBDemodRecorder::Service : DVBDemodRecorder::Multiplex, serviceId, error);

    if (id < 0) {
        return false;
    }

    m_manualRecording = id;
    return true;
}

void DVBDemod::stopRecording()
{
    // One waiting for the service's multiplex too (the retune or search goes on, to play it)
    m_pendingRecording.m_active = false;

    if (m_manualRecording >= 0)
    {
        m_basebandSink->stopRecording(m_manualRecording);
        m_manualRecording = -1;
    }
}

bool DVBDemod::isScheduled(int networkId, int serviceId, int eventId) const
{
    for (const auto& r : m_settings.m_scheduledRecordings)
    {
        if ((r.m_networkId == networkId) && (r.m_serviceId == serviceId) && (r.m_eventId == eventId)) {
            return true;
        }
    }

    return false;
}

bool DVBDemod::scheduleRecording(int networkId, int serviceId, int eventId)
{
    if (isScheduled(networkId, serviceId, eventId)) {
        return false;
    }

    for (const auto& event : getEPG()->getEvents(networkId, serviceId))
    {
        if (event.m_eventId == eventId)
        {
            DVBDemodSettings::ScheduledRecording r;
            r.m_networkId = networkId;
            r.m_serviceId = serviceId;
            r.m_eventId = eventId;
            r.m_start = event.m_start;
            r.m_duration = event.m_duration;
            r.m_title = event.m_title;
            r.m_serviceName = serviceName(serviceId);
            QList<DVBDemodSettings::ScheduledRecording> schedule = m_settings.m_scheduledRecordings;
            schedule.append(r);
            setSchedule(schedule);
            checkSchedule(); // It may have started
            return true;
        }
    }

    return false;
}

bool DVBDemod::scheduleSeries(int networkId, int serviceId, int eventId)
{
    DVBDemodEPG::Event event;

    if (!getEPG()->getEvent(networkId, serviceId, eventId, event) || event.m_seriesCrid.isEmpty()
        || isSeriesScheduled(networkId, serviceId, eventId)) {
        return false;
    }

    DVBDemodSettings::SeriesRecording s;
    s.m_networkId = networkId;
    s.m_serviceId = serviceId;
    s.m_seriesCrid = event.m_seriesCrid;
    s.m_title = event.m_title;
    s.m_serviceName = serviceName(serviceId);
    QList<DVBDemodSettings::SeriesRecording> series = m_settings.m_seriesRecordings;
    series.append(s);
    setSeries(series);
    m_seriesRevision = 0;   // Its programmes scheduled now
    checkSchedule();
    return true;
}

void DVBDemod::cancelSeries(int networkId, int serviceId, int eventId)
{
    DVBDemodEPG::Event event;

    if (getEPG()->getEvent(networkId, serviceId, eventId, event) && !event.m_seriesCrid.isEmpty()) {
        cancelSeries(networkId, serviceId, event.m_seriesCrid);
    }
}

void DVBDemod::cancelSeries(int networkId, int serviceId, const QString& seriesCrid)
{
    QList<DVBDemodSettings::SeriesRecording> series = m_settings.m_seriesRecordings;

    for (int i = 0; i < series.size(); i++)
    {
        if ((series[i].m_networkId == networkId) && (series[i].m_serviceId == serviceId) && (series[i].m_seriesCrid == seriesCrid))
        {
            series.removeAt(i);
            setSeries(series);
            return;
        }
    }
}

bool DVBDemod::isSeriesScheduled(int networkId, int serviceId, int eventId) const
{
    DVBDemodEPG::Event event;

    if (!m_basebandSink->getEPG()->getEvent(networkId, serviceId, eventId, event) || event.m_seriesCrid.isEmpty()) {
        return false;
    }

    for (const auto& s : m_settings.m_seriesRecordings)
    {
        if ((s.m_networkId == networkId) && (s.m_serviceId == serviceId) && (s.m_seriesCrid == event.m_seriesCrid)) {
            return true;
        }
    }

    return false;
}

void DVBDemod::setSeries(const QList<DVBDemodSettings::SeriesRecording>& series)
{
    DVBDemodSettings settings = m_settings;
    settings.m_seriesRecordings = series;
    applySettings(settings, {"seriesRecordings"});

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureDVBDemod::create(settings, {"seriesRecordings"}, false));
    }
}

// Each series' programmes in the guide not yet scheduled, nor scheduled before (a repeat has its first showing's
// programme CRID: not recorded again), from now on. Only when the guide's changed
bool DVBDemod::checkSeries(QList<DVBDemodSettings::ScheduledRecording>& schedule)
{
    DVBDemodEPG *epg = getEPG();

    if (m_settings.m_seriesRecordings.isEmpty() || (epg->getRevision() == m_seriesRevision)) {
        return false;
    }

    m_seriesRevision = epg->getRevision();
    const qint64 t = epg->getTime() > 0 ? epg->getTime() : QDateTime::currentSecsSinceEpoch();
    QList<DVBDemodSettings::SeriesRecording> series = m_settings.m_seriesRecordings;
    bool added = false;
    static const int maxRemembered = 500;   // Of a series' programmes scheduled, the latest

    for (auto& s : series)
    {
        for (const auto& event : epg->getEvents(s.m_networkId, s.m_serviceId))
        {
            if ((event.m_seriesCrid != s.m_seriesCrid) || (event.end() <= t)) {
                continue;
            }

            const QString id = event.m_programmeCrid.isEmpty() ? QString("event:%1").arg(event.m_eventId) : event.m_programmeCrid;
            const bool scheduled = std::any_of(schedule.begin(), schedule.end(), [&](const DVBDemodSettings::ScheduledRecording& r) {
                return (r.m_networkId == s.m_networkId) && (r.m_serviceId == s.m_serviceId) && (r.m_eventId == event.m_eventId);
            });

            if (scheduled || s.m_scheduled.contains(id)) {
                continue;
            }

            DVBDemodSettings::ScheduledRecording r;
            r.m_networkId = s.m_networkId;
            r.m_serviceId = s.m_serviceId;
            r.m_eventId = event.m_eventId;
            r.m_start = event.m_start;
            r.m_duration = event.m_duration;
            r.m_title = event.m_title;
            r.m_serviceName = s.m_serviceName;
            schedule.append(r);
            s.m_scheduled.append(id);

            while (s.m_scheduled.size() > maxRemembered) {
                s.m_scheduled.removeFirst();
            }

            qDebug() << "DVBDemod::checkSeries: scheduled" << r.m_title << "of" << s.m_title << "at" << QDateTime::fromSecsSinceEpoch(r.m_start);
            added = true;
        }
    }

    if (added) {
        setSeries(series);
    }

    return added;
}

bool DVBDemod::retuneForRecording(const DVBDemodSettings::ScheduledRecording& r)
{
    // Not while anything else is being recorded (its multiplex would be lost), nor in a search, scan or retune for
    // another, nor to a satellite multiplex on another polarisation (the LNB isn't switched from here)
    int needed, current;

    if (m_recordingRetune.m_active || recordingInProgress() || !m_scheduledRecordingIds.empty() || m_search.m_active
        || m_scan.m_active || m_pendingRecording.m_active || polarisationChange(r.m_networkId, r.m_serviceId, needed, current)) {
        return false;
    }

    const qint64 returnFrequency = m_centerFrequency;
    const DVBDemodSettings returnSettings = m_settings;
    QString error;
    m_retuningForRecording = true;
    const bool ok = playService(r.m_networkId, r.m_serviceId, error);
    m_retuningForRecording = false;

    if (!ok)
    {
        qWarning() << "DVBDemod::retuneForRecording: can't retune for" << r.m_title << ":" << error;
        return false;
    }

    qInfo() << "DVBDemod::retuneForRecording: retuned for" << r.m_title << "on" << r.m_serviceName;
    m_recordingRetune = RecordingRetune();
    m_recordingRetune.m_active = true;
    m_recordingRetune.m_returnFrequency = returnFrequency;
    m_recordingRetune.m_returnSettings = returnSettings;
    m_recordingRetune.m_since.start();
    return true;
}

void DVBDemod::checkRecordingReturn(const QList<DVBDemodSettings::ScheduledRecording>& schedule, qint64 t)
{
    RecordingRetune& rr = m_recordingRetune;

    if (!rr.m_active || m_search.m_active) {
        return;
    }

    // The user's retuned the device meanwhile: where it is now is theirs
    if (rr.m_recorded && (m_centerFrequency != rr.m_tunedFrequency))
    {
        rr.m_active = false;
        return;
    }

    // Still recording, or one due (or about to be) here: stay. Given up if no recording started within a minute
    const bool due = std::any_of(schedule.begin(), schedule.end(), [&](const DVBDemodSettings::ScheduledRecording& r) {
        return (t >= r.m_start - m_settings.m_recordStartPadding - m_recordRetuneLeadSeconds) && (t < r.end() + m_settings.m_recordEndPadding);
    });

    if (!m_scheduledRecordingIds.empty() || (rr.m_recorded && due) || (!rr.m_recorded && (rr.m_since.elapsed() < m_recordRetuneGiveUpMs))) {
        return;
    }

    qInfo("DVBDemod::checkRecordingReturn: back to %.3f MHz", rr.m_returnFrequency / 1e6);
    rr.m_active = false;

    if (retuneDevice(rr.m_returnFrequency))
    {
        applyServiceSettings(rr.m_returnSettings, {"serviceId", "standard", "bandwidth", "plpId", "lowPriority", "symbolRate",
            "codeRate", "rollOff", "isi"});
    }
}

void DVBDemod::cancelScheduledRecording(int networkId, int serviceId, int eventId)
{
    QList<DVBDemodSettings::ScheduledRecording> schedule = m_settings.m_scheduledRecordings;

    for (int i = 0; i < schedule.size(); i++)
    {
        const auto& r = schedule[i];

        if ((r.m_networkId == networkId) && (r.m_serviceId == serviceId) && (r.m_eventId == eventId))
        {
            auto it = m_scheduledRecordingIds.find(scheduleKey(networkId, serviceId, eventId));

            if (it != m_scheduledRecordingIds.end())
            {
                m_basebandSink->stopRecording(it->second);
                m_scheduledRecordingIds.erase(it);
            }

            schedule.removeAt(i);
            setSchedule(schedule);
            return;
        }
    }
}

// The schedule changes in the settings, here and in the GUI, which saves them
void DVBDemod::setSchedule(const QList<DVBDemodSettings::ScheduledRecording>& schedule)
{
    DVBDemodSettings settings = m_settings;
    settings.m_scheduledRecordings = schedule;
    applySettings(settings, {"scheduledRecordings"});

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgConfigureDVBDemod::create(settings, {"scheduledRecordings"}, false));
    }
}

// Start and stop scheduled recordings, by the time in the transport stream (or the computer's, without it), and
// follow changes to the programmes' times in the guide. A recording that's missed is dropped
void DVBDemod::checkSchedule()
{
    if (m_settings.m_scheduledRecordings.isEmpty() && m_scheduledRecordingIds.empty() && m_settings.m_seriesRecordings.isEmpty()
        && !m_recordingRetune.m_active) {
        return;
    }

    DVBDemodEPG *epg = getEPG();
    const qint64 t = epg->getTime() > 0 ? epg->getTime() : QDateTime::currentSecsSinceEpoch();
    // Not by a time that's jumped, until it's believed: recordings ended and dropped from the schedule by it are lost
    const bool timeOK = scheduleTimeConfirmed(t);
    QList<DVBDemodSettings::ScheduledRecording> schedule = m_settings.m_scheduledRecordings;
    bool changed = timeOK && checkSeries(schedule);

    for (int i = 0; i < schedule.size(); )
    {
        DVBDemodSettings::ScheduledRecording& r = schedule[i];

        DVBDemodEPG::Event event;

        if (epg->getEvent(r.m_networkId, r.m_serviceId, r.m_eventId, event) && ((event.m_start != r.m_start) || (event.m_duration != r.m_duration)))
        {
            r.m_start = event.m_start;
            r.m_duration = event.m_duration;
            changed = true;
        }

        const qint64 key = scheduleKey(r.m_networkId, r.m_serviceId, r.m_eventId);
        auto active = m_scheduledRecordingIds.find(key);
        qint64 from = r.m_start - m_settings.m_recordStartPadding;
        qint64 to = r.end() + m_settings.m_recordEndPadding;

        // The programme's running status, where the broadcaster sends it (in its multiplex's present and following
        // table): it's recorded from when it starts (or is about to), even if early, and while it's running or
        // paused, even past its end (as a match that goes to extra time), for up to 3 hours more
        const int running = m_settings.m_recordFollowRunning ? epg->getRunningStatus(r.m_networkId, r.m_serviceId, r.m_eventId) : -1;
        static const qint64 maxOverrun = 3 * 3600;

        if ((running >= 2) && (running <= 4)) {
            from = std::min(from, t);
        }

        if (((running == 3) || (running == 4)) && (t < r.end() + maxOverrun)) {
            to = std::max(to, t + 60);
        }

        // Recorded only from the multiplex the service is on: another may not have the service, or have another with
        // its id (the recorder records by service id). Not retuned to, which would interrupt what's being watched
        // (Where the guide doesn't know which multiplex it's on, it's tried on this one, as before)
        DVBDemodEPG::Service service;
        const bool known = epg->getService(r.m_networkId, r.m_serviceId, service) && (service.m_transportStreamId >= 0);
        const bool here = !known || service.m_actual;
        const bool elsewhere = !here && (epg->getTransportStreamId() >= 0);   // Not while the multiplex isn't known yet

        if ((active != m_scheduledRecordingIds.end()) && elsewhere && (t < to))
        {
            // The device was retuned to another multiplex while recording: stopped (started again if it's tuned back)
            qWarning() << "DVBDemod::checkSchedule: stopped recording" << r.m_title << ": the multiplex received changed";
            m_basebandSink->stopRecording(active->second);
            m_scheduledRecordingIds.erase(active);
            i++;
            continue;
        }

        if (!timeOK)
        {
            // Carries on as it is
        }
        else if ((active == m_scheduledRecordingIds.end()) && (t >= from) && (t < to) && !here)
        {
            // Retuned to, if allowed and nothing else is being recorded (or done with the device)
            if (elsewhere && !(m_settings.m_recordRetune && retuneForRecording(r)) && m_scheduledRecordingFailed.insert(key).second) {
                qWarning() << "DVBDemod::checkSchedule: can't record" << r.m_title << ": its service isn't on the multiplex received";
            }
        }
        else if ((active == m_scheduledRecordingIds.end()) && (t >= from) && (t < to))
        {
            QString error;
            const int id = m_basebandSink->startRecording(recordingFileName(QString("%1 - %2").arg(r.m_serviceName).arg(r.m_title), r.m_start),
                DVBDemodRecorder::Service, r.m_serviceId, error);

            if (id >= 0)
            {
                m_scheduledRecordingIds[key] = id;
                m_scheduledRecordingFailed.erase(key);

                if (m_recordingRetune.m_active && !m_recordingRetune.m_recorded)
                {
                    m_recordingRetune.m_recorded = true;
                    m_recordingRetune.m_tunedFrequency = m_centerFrequency;
                }
            }
            else if (m_scheduledRecordingFailed.insert(key).second)
            {
                qWarning() << "DVBDemod::checkSchedule: can't record" << r.m_title << ":" << error;
            }
        }
        else if (t >= to)
        {
            if (active != m_scheduledRecordingIds.end())
            {
                m_basebandSink->stopRecording(active->second);
                m_scheduledRecordingIds.erase(active);
            }

            m_scheduledRecordingFailed.erase(key);
            schedule.removeAt(i);
            changed = true;
            continue;
        }

        i++;
    }

    // Recordings no longer scheduled, as when settings are loaded or the schedule is replaced
    for (auto it = m_scheduledRecordingIds.begin(); it != m_scheduledRecordingIds.end(); )
    {
        const bool scheduled = std::any_of(schedule.begin(), schedule.end(), [&it](const DVBDemodSettings::ScheduledRecording& r) {
            return scheduleKey(r.m_networkId, r.m_serviceId, r.m_eventId) == it->first;
        });

        if (scheduled)
        {
            ++it;
        }
        else
        {
            m_basebandSink->stopRecording(it->second);
            it = m_scheduledRecordingIds.erase(it);
        }
    }

    if (timeOK) {
        checkRecordingReturn(schedule, t);
    }

    if (changed) {
        setSchedule(schedule);
    }
}

bool DVBDemod::scheduleTimeConfirmed(qint64 t)
{
    ScheduleClock& c = m_scheduleClock;

    if (!c.m_clock.isValid())
    {
        c.m_clock.start();
        c.m_time = t;
        c.m_at = 0;
        return true;
    }

    const qint64 now = c.m_clock.elapsed();
    const qint64 expected = c.m_time + (now - c.m_at) / 1000;

    // As expected (to within the TDT's resolution, the checks' timing, and drift): followed from here
    if (qAbs(t - expected) <= m_scheduleJumpSeconds)
    {
        if (c.m_jumped) {
            qInfo("DVBDemod::scheduleTimeConfirmed: the time's back to %s", qPrintable(QDateTime::fromSecsSinceEpoch(t).toString()));
        }

        c.m_time = t;
        c.m_at = now;
        c.m_jumped = false;
        return true;
    }

    // Jumped: to another time than the last jump's, it starts again
    const qint64 jumpExpected = c.m_jumpTime + (now - c.m_jumpAt) / 1000;

    if (!c.m_jumped || (qAbs(t - jumpExpected) > m_scheduleJumpSeconds))
    {
        qWarning("DVBDemod::scheduleTimeConfirmed: the time's jumped %lld s, to %s: not believed yet", (long long) (t - expected),
            qPrintable(QDateTime::fromSecsSinceEpoch(t).toString()));
        c.m_jumped = true;
        c.m_jumpTime = t;
        c.m_jumpAt = now;
        c.m_jumpChecks = 0;
        return false;
    }

    // Kept to for long enough: believed
    if ((++c.m_jumpChecks >= m_scheduleJumpChecks) && (now - c.m_jumpAt >= m_scheduleJumpConfirmMs))
    {
        qInfo("DVBDemod::scheduleTimeConfirmed: the time's now %s", qPrintable(QDateTime::fromSecsSinceEpoch(t).toString()));
        c.m_time = t;
        c.m_at = now;
        c.m_jumped = false;
        return true;
    }

    return false;
}

void DVBDemod::saveEPG()
{
    if (getEPG()->isModified()) {
        getEPG()->save(DVBDemodEPG::defaultCacheFileName());
    }
}

DVBDemod::~DVBDemod()
{
    qDebug("DVBDemod::~DVBDemod");
    // Just stopped: not retuned back (the device set may be being deleted too), nor the GUI told (it's deleted first)
    m_search.m_active = false;
    m_scan.m_active = false;
    m_pendingRecording.m_active = false;
    m_epgSaveTimer.stop();
    m_scheduleTimer.stop();
    m_tuneTimer.stop();
    saveEPG();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DVBDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void DVBDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t DVBDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

int DVBDemod::getChannelSampleRate() const
{
    return m_basebandSink->getChannelSampleRate();
}

void DVBDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DVBDemod::start()
{
    qDebug("DVBDemod::start");

    // In the device's DSP thread: what this object's thread changes is copied with the mutex
    int basebandSampleRate;
    qint64 centerFrequency;
    DVBDemodSettings settings;

    {
        QMutexLocker locker(&m_settingsMutex);
        basebandSampleRate = m_basebandSampleRate;
        centerFrequency = m_centerFrequency;
        settings = m_settings;
    }

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(basebandSampleRate, centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    DVBDemodBaseband::MsgConfigureDVBDemodBaseband *msg = DVBDemodBaseband::MsgConfigureDVBDemodBaseband::create(settings, QStringList(), true);
    m_basebandSink->getInputMessageQueue()->push(msg);
    // The symbol rate search's band, if one was set: its message may have been cleared by reset(), sent while stopped
    m_basebandSink->reapplyRateSearchBand();
}

void DVBDemod::stop()
{
    qDebug("DVBDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool DVBDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDVBDemod::match(cmd))
    {
        MsgConfigureDVBDemod& cfg = (MsgConfigureDVBDemod&) cmd;
        qDebug() << "DVBDemod::handleMessage: MsgConfigureDVBDemod";
        applySettings(cfg.getSettings(), cfg.getSettingsKeys(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        {
            QMutexLocker locker(&m_settingsMutex);
            m_basebandSampleRate = notif.getSampleRate();
        }

        const bool retune = notif.getCenterFrequency() != m_centerFrequency;

        if (retune) {
            retuned();
        }

        // Retuned elsewhere than the guide did (by the user: further than any fine tuning of the same transponder): the
        // satellite settings the guide set were for its transponder, so they're automatic again. Left, a transponder at another symbol rate, or the other standard,
        // was never locked to (in Sync, until the DVB Demod was opened again)
        if (retune && m_guideSettings && (qAbs(notif.getCenterFrequency() - m_guideCenterFrequency) > m_guideRetuneTolerance))
        {
            m_guideSettings = false;
            DVBDemodSettings settings = m_settings;
            setSatelliteAutomatic(settings);
            qDebug("DVBDemod::handleMessage: retuned elsewhere than the guide: the satellite settings are automatic again");
            applyServiceSettings(settings, {"standard", "symbolRate", "codeRate", "rollOff", "isi"});
        }

        {
            QMutexLocker locker(&m_settingsMutex);
            m_centerFrequency = notif.getCenterFrequency();
        }

        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "DVBDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

        if (retune) {
            resetReceivers();
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else
    {
        return false;
    }
}

void DVBDemod::setCenterFrequency(qint64 frequency)
{
    DVBDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, {"inputFrequencyOffset"}, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDVBDemod *msgToGUI = MsgConfigureDVBDemod::create(settings, {"inputFrequencyOffset"}, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void DVBDemod::applySettings(const DVBDemodSettings& settings, const QStringList& settingsKeys, bool force)
{
    qDebug() << "DVBDemod::applySettings:"
             << settings.getDebugString(settingsKeys, force)
             << " force: " << force;

    // Set by the user (or the guide again, after this): the user's to keep
    if (settingsKeys.contains("standard") || settingsKeys.contains("symbolRate") || force) {
        m_guideSettings = false;
    }

    if (settingsKeys.contains("freesatRegion") || force) {
        getEPG()->setFreesatRegion(settings.m_freesatRegion);
    }

    if (settingsKeys.contains("streamIndex"))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);

            {
                QMutexLocker locker(&m_settingsMutex);
                m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            }

            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    DVBDemodBaseband::MsgConfigureDVBDemodBaseband *msg = DVBDemodBaseband::MsgConfigureDVBDemodBaseband::create(settings, settingsKeys, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    // Another frequency, or another transport stream from the same (a PLP, or hierarchical stream)
    const bool offsetChanged = settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset);

    if (offsetChanged && !force) {
        resetReceivers(); // After the settings, as the channel moves with them
    }

    if (offsetChanged
        || (settingsKeys.contains("plpId") && (settings.m_plpId != m_settings.m_plpId))
        || (settingsKeys.contains("lowPriority") && (settings.m_lowPriority != m_settings.m_lowPriority))
        || (settingsKeys.contains("isi") && (settings.m_isi != m_settings.m_isi)) // Another input stream, another multiplex
        || force)
    {
        retuned();
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI) ||
            settingsKeys.contains("reverseAPIAddress") ||
            settingsKeys.contains("reverseAPIPort") ||
            settingsKeys.contains("reverseAPIDeviceIndex") ||
            settingsKeys.contains("reverseAPIChannelIndex");
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    QMutexLocker locker(&m_settingsMutex);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray DVBDemod::serialize() const
{
    return m_settings.serialize();
}

bool DVBDemod::deserialize(const QByteArray& data)
{
    bool ok;
    DVBDemodSettings settings;

    {
        QMutexLocker locker(&m_settingsMutex);
        ok = m_settings.deserialize(data);

        if (!ok) {
            m_settings.resetToDefaults();
        }

        settings = m_settings;
    }

    // Not holding the mutex: from this thread, the message is handled now, and applySettings() takes it
    MsgConfigureDVBDemod *msg = MsgConfigureDVBDemod::create(settings, QStringList(), true);
    m_inputMessageQueue.push(msg);
    return ok;
}

void DVBDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}

// Run f in object's thread, waiting for it. Only from a thread that thread never waits for (see dvbdemod.h)
int DVBDemod::inObjectThread(QObject *object, const std::function<int()>& f)
{
    if (QThread::currentThread() == object->thread()) {
        return f();
    }

    int result = 500;
    QMetaObject::invokeMethod(object, [&]() {
        result = f();
    }, Qt::BlockingQueuedConnection);
    return result;
}

int DVBDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;

    // The settings, which this object's thread changes
    if (QThread::currentThread() != thread()) {
        return inObjectThread(this, [&]() { return webapiSettingsGet(response, errorMessage); });
    }

    response.setDvbDemodSettings(new SWGSDRangel::SWGDVBDemodSettings());
    response.getDvbDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DVBDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    if (QThread::currentThread() != thread()) {
        return inObjectThread(this, [&]() { return webapiWorkspaceGet(response, errorMessage); });
    }

    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DVBDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    // From the settings, which this object's thread changes
    if (QThread::currentThread() != thread()) {
        return inObjectThread(this, [&]() { return webapiSettingsPutPatch(force, channelSettingsKeys, response, errorMessage); });
    }

    DVBDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    // Others would divide by zero, or leave the receivers and the report disagreeing on the standard
    if (channelSettingsKeys.contains("bandwidth") && !DVBDemodSettings::isValidBandwidth(settings.m_bandwidth))
    {
        errorMessage = QString("bandwidth %1 Hz is not 8000000, 7000000 or 6000000").arg(settings.m_bandwidth);
        return 400;
    }

    if (channelSettingsKeys.contains("standard") && !DVBDemodSettings::isValidStandard(settings.m_standard))
    {
        errorMessage = QString("standard %1 is not 0 (automatic, DVB-T or T2), 1 (DVB-T), 2 (DVB-T2), 3 (DVB-T2 T2-Lite), 4 (automatic, DVB-S or S2), 5 (DVB-S) or 6 (DVB-S2)")
            .arg(settings.m_standard);
        return 400;
    }

    if (channelSettingsKeys.contains("symbolRate") && !DVBDemodSettings::isValidSymbolRate(settings.m_symbolRate))
    {
        errorMessage = QString("symbolRate %1 is not 0 (automatic) or from 10000 to 50000000").arg(settings.m_symbolRate);
        return 400;
    }

    if (channelSettingsKeys.contains("rollOff") && !DVBDemodSettings::isValidRollOff(settings.m_rollOff))
    {
        errorMessage = QString("rollOff %1 is not 0 (automatic), 35, 25, 20, 15, 10 or 5").arg(settings.m_rollOff);
        return 400;
    }

    if (channelSettingsKeys.contains("codeRate") && !DVBDemodSettings::isValidCodeRate(settings.m_codeRate))
    {
        errorMessage = QString("codeRate %1 is not -1 (automatic) or 0 to 4 (1/2 to 7/8)").arg(settings.m_codeRate);
        return 400;
    }

    if (channelSettingsKeys.contains("freesatRegion") && !DVBDemodSettings::isValidFreesatRegion(settings.m_freesatRegion))
    {
        errorMessage = QString("freesatRegion %1 is not from 0 to 65535").arg(settings.m_freesatRegion);
        return 400;
    }

    if (channelSettingsKeys.contains("isi") && !DVBDemodSettings::isValidISI(settings.m_isi))
    {
        errorMessage = QString("isi %1 is not -1 (automatic) or from 0 to 255").arg(settings.m_isi);
        return 400;
    }

    if (channelSettingsKeys.contains("plScramblingCode") && !DVBDemodSettings::isValidScramblingCode(settings.m_plScramblingCode))
    {
        errorMessage = QString("plScramblingCode %1 is not from 0 to 262141").arg(settings.m_plScramblingCode);
        return 400;
    }

    if (channelSettingsKeys.contains("epgReport") && !DVBDemodSettings::isValidEPGReport(settings.m_epgReport))
    {
        errorMessage = QString("epgReport %1 is not 0, 1 or 2").arg(settings.m_epgReport);
        return 400;
    }

    MsgConfigureDVBDemod *msg = MsgConfigureDVBDemod::create(settings, channelSettingsKeys, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDVBDemod *msgToGUI = MsgConfigureDVBDemod::create(settings, channelSettingsKeys, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int DVBDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    // The web API's thread: the report reads the settings and the scheduled recordings, which this object's changes
    if (QThread::currentThread() != thread()) {
        return inObjectThread(this, [&]() { return webapiReportGet(response, errorMessage); });
    }

    response.setDvbDemodReport(new SWGSDRangel::SWGDVBDemodReport());
    response.getDvbDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DVBDemod::webapiUpdateChannelSettings(
        DVBDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    SWGSDRangel::SWGDVBDemodSettings *swgSettings = response.getDvbDemodSettings();

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = swgSettings->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidth")) {
        settings.m_bandwidth = swgSettings->getBandwidth();
    }
    if (channelSettingsKeys.contains("standard")) {
        settings.m_standard = swgSettings->getStandard();
    }
    if (channelSettingsKeys.contains("plpId")) {
        settings.m_plpId = swgSettings->getPlpId();
    }
    if (channelSettingsKeys.contains("lowPriority")) {
        settings.m_lowPriority = swgSettings->getLowPriority() != 0;
    }
    if (channelSettingsKeys.contains("symbolRate")) {
        settings.m_symbolRate = swgSettings->getSymbolRate();
    }
    if (channelSettingsKeys.contains("rollOff")) {
        settings.m_rollOff = swgSettings->getRollOff();
    }
    if (channelSettingsKeys.contains("codeRate")) {
        settings.m_codeRate = swgSettings->getCodeRate();
    }
    if (channelSettingsKeys.contains("isi")) {
        settings.m_isi = swgSettings->getIsi();
    }
    if (channelSettingsKeys.contains("plScramblingCode")) {
        settings.m_plScramblingCode = swgSettings->getPlScramblingCode();
    }
    if (channelSettingsKeys.contains("recordRetune")) {
        settings.m_recordRetune = swgSettings->getRecordRetune() != 0;
    }
    if (channelSettingsKeys.contains("recordFollowRunning")) {
        settings.m_recordFollowRunning = swgSettings->getRecordFollowRunning() != 0;
    }
    if (channelSettingsKeys.contains("equaliser")) {
        settings.m_equaliser = swgSettings->getEqualiser() != 0;
    }
    if (channelSettingsKeys.contains("freesatRegion")) {
        settings.m_freesatRegion = swgSettings->getFreesatRegion();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = swgSettings->getUdpEnabled() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *swgSettings->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort") && (swgSettings->getUdpPort() > 0) && (swgSettings->getUdpPort() <= 65535)) {
        settings.m_udpPort = swgSettings->getUdpPort();
    }
    if (channelSettingsKeys.contains("videoEnabled")) {
        settings.m_videoEnabled = swgSettings->getVideoEnabled() != 0;
    }
    if (channelSettingsKeys.contains("serviceId")) {
        settings.m_serviceId = swgSettings->getServiceId();
    }
    if (channelSettingsKeys.contains("deinterlace")) {
        settings.m_deinterlace = swgSettings->getDeinterlace() != 0;
    }
    if (channelSettingsKeys.contains("avOffset")) {
        settings.m_avOffset = swgSettings->getAvOffset();
    }
    if (channelSettingsKeys.contains("recordDirectory")) {
        settings.m_recordDirectory = *swgSettings->getRecordDirectory();
    }
    if (channelSettingsKeys.contains("recordMode")) {
        settings.m_recordMode = swgSettings->getRecordMode();
    }
    if (channelSettingsKeys.contains("recordStartPadding")) {
        settings.m_recordStartPadding = swgSettings->getRecordStartPadding();
    }
    if (channelSettingsKeys.contains("recordEndPadding")) {
        settings.m_recordEndPadding = swgSettings->getRecordEndPadding();
    }
    if (channelSettingsKeys.contains("epgReport")) {
        settings.m_epgReport = swgSettings->getEpgReport();
    }
    if (channelSettingsKeys.contains("epgReportServiceId")) {
        settings.m_epgReportServiceId = swgSettings->getEpgReportServiceId();
    }
    if (channelSettingsKeys.contains("epgReportNetworkId")) {
        settings.m_epgReportNetworkId = swgSettings->getEpgReportNetworkId();
    }
    if (channelSettingsKeys.contains("hardwareDecoding")) {
        settings.m_hardwareDecoding = swgSettings->getHardwareDecoding() != 0;
    }
    if (channelSettingsKeys.contains("audioLanguage")) {
        settings.m_audioLanguage = *swgSettings->getAudioLanguage();
    }
    if (channelSettingsKeys.contains("audioDescription")) {
        settings.m_audioDescription = swgSettings->getAudioDescription() != 0;
    }
    if (channelSettingsKeys.contains("audioDescriptionMixed")) {
        settings.m_audioDescriptionMixed = swgSettings->getAudioDescriptionMixed() != 0;
    }
    if (channelSettingsKeys.contains("cleanAudio")) {
        settings.m_cleanAudio = swgSettings->getCleanAudio() != 0;
    }
    if (channelSettingsKeys.contains("subtitlesHardOfHearing")) {
        settings.m_subtitlesHardOfHearing = swgSettings->getSubtitlesHardOfHearing() != 0;
    }
    if (channelSettingsKeys.contains("subtitles")) {
        settings.m_subtitles = swgSettings->getSubtitles() != 0;
    }
    if (channelSettingsKeys.contains("subtitleLanguage")) {
        settings.m_subtitleLanguage = *swgSettings->getSubtitleLanguage();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = swgSettings->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("audioVolume")) {
        settings.m_audioVolume = swgSettings->getAudioVolume();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *swgSettings->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = swgSettings->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *swgSettings->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = swgSettings->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = swgSettings->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *swgSettings->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = swgSettings->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = swgSettings->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = swgSettings->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, swgSettings->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, swgSettings->getRollupState());
    }
}

// Set a string in the SWG settings, reusing the QString if there is one
void DVBDemod::setString(QString *existing, std::function<void(QString*)> setter, const QString& value)
{
    if (existing) {
        *existing = value;
    } else {
        setter(new QString(value));
    }
}

void DVBDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DVBDemodSettings& settings)
{
    SWGSDRangel::SWGDVBDemodSettings *swgSettings = response.getDvbDemodSettings();

    swgSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    swgSettings->setBandwidth(settings.m_bandwidth);
    swgSettings->setStandard(settings.m_standard);
    swgSettings->setPlpId(settings.m_plpId);
    swgSettings->setLowPriority(settings.m_lowPriority ? 1 : 0);
    swgSettings->setSymbolRate(settings.m_symbolRate);
    swgSettings->setRollOff(settings.m_rollOff);
    swgSettings->setCodeRate(settings.m_codeRate);
    swgSettings->setIsi(settings.m_isi);
    swgSettings->setPlScramblingCode(settings.m_plScramblingCode);
    swgSettings->setEqualiser(settings.m_equaliser ? 1 : 0);
    swgSettings->setRecordRetune(settings.m_recordRetune ? 1 : 0);
    swgSettings->setRecordFollowRunning(settings.m_recordFollowRunning ? 1 : 0);
    swgSettings->setFreesatRegion(settings.m_freesatRegion);
    swgSettings->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    setString(swgSettings->getUdpAddress(), [=](QString *s) { swgSettings->setUdpAddress(s); }, settings.m_udpAddress);
    swgSettings->setUdpPort(settings.m_udpPort);
    swgSettings->setVideoEnabled(settings.m_videoEnabled ? 1 : 0);
    swgSettings->setServiceId(settings.m_serviceId);
    swgSettings->setDeinterlace(settings.m_deinterlace ? 1 : 0);
    swgSettings->setAvOffset(settings.m_avOffset);
    swgSettings->setHardwareDecoding(settings.m_hardwareDecoding ? 1 : 0);
    setString(swgSettings->getAudioLanguage(), [=](QString *s) { swgSettings->setAudioLanguage(s); }, settings.m_audioLanguage);
    swgSettings->setAudioDescription(settings.m_audioDescription ? 1 : 0);
    swgSettings->setAudioDescriptionMixed(settings.m_audioDescriptionMixed ? 1 : 0);
    swgSettings->setCleanAudio(settings.m_cleanAudio ? 1 : 0);
    swgSettings->setSubtitlesHardOfHearing(settings.m_subtitlesHardOfHearing ? 1 : 0);
    swgSettings->setSubtitles(settings.m_subtitles ? 1 : 0);
    setString(swgSettings->getSubtitleLanguage(), [=](QString *s) { swgSettings->setSubtitleLanguage(s); }, settings.m_subtitleLanguage);
    setString(swgSettings->getRecordDirectory(), [=](QString *s) { swgSettings->setRecordDirectory(s); }, settings.m_recordDirectory);
    swgSettings->setRecordMode(settings.m_recordMode);
    swgSettings->setRecordStartPadding(settings.m_recordStartPadding);
    swgSettings->setRecordEndPadding(settings.m_recordEndPadding);
    swgSettings->setEpgReport(settings.m_epgReport);
    swgSettings->setEpgReportServiceId(settings.m_epgReportServiceId);
    swgSettings->setEpgReportNetworkId(settings.m_epgReportNetworkId);
    swgSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    swgSettings->setAudioVolume(settings.m_audioVolume);
    setString(swgSettings->getAudioDeviceName(), [=](QString *s) { swgSettings->setAudioDeviceName(s); }, settings.m_audioDeviceName);
    swgSettings->setRgbColor(settings.m_rgbColor);
    setString(swgSettings->getTitle(), [=](QString *s) { swgSettings->setTitle(s); }, settings.m_title);
    swgSettings->setStreamIndex(settings.m_streamIndex);
    swgSettings->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    setString(swgSettings->getReverseApiAddress(), [=](QString *s) { swgSettings->setReverseApiAddress(s); }, settings.m_reverseAPIAddress);
    swgSettings->setReverseApiPort(settings.m_reverseAPIPort);
    swgSettings->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    swgSettings->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (swgSettings->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(swgSettings->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            swgSettings->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (swgSettings->getRollupState())
        {
            settings.m_rollupState->formatTo(swgSettings->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            swgSettings->setRollupState(swgRollupState);
        }
    }
}

// The programme guide: its size, and what's on now and next on the service selected, or the first TV service
void DVBDemod::webapiFormatEPGReport(SWGSDRangel::SWGDVBDemodReport *report)
{
    DVBDemodEPG *epg = getEPG();
    const std::vector<DVBDemodEPG::Service> services = epg->getServices();
    report->setEpgServices((int) services.size());
    report->setEpgEvents(epg->getEventCount());
    const int serviceId = currentServiceId();

    const std::vector<DVBDemodSink::Recording> recordings = getRecordings();
    qint64 recorded = 0;

    for (const auto& recording : recordings) {
        recorded += recording.m_status.m_bytes;
    }

    report->setRecording((int) recordings.size());
    *report->getRecordingFile() = recordings.empty() ? QString() : recordings[0].m_status.m_fileName;
    report->setRecordingSize(recorded);
    report->setScheduledRecordings(m_settings.m_scheduledRecordings.size());

    report->setEpgServiceId(serviceId);
    auto iso = [](qint64 utc) { return QDateTime::fromSecsSinceEpoch(utc, Qt::UTC).toString(Qt::ISODate); };

    if (m_settings.m_epgReport >= 1) {
        webapiFormatEPGLists(report, services, serviceId);
    }

    DVBDemodEPG::Event now, next;

    if ((serviceId == 0) || !epg->getNowNext(serviceId, now, next)) {
        return;
    }

    if (now.isValid())
    {
        *report->getEpgNowTitle() = now.m_title;
        *report->getEpgNowStart() = iso(now.m_start);
        report->setEpgNowDuration(now.m_duration);
        *report->getEpgNowDescription() = now.m_text;
    }

    if (next.isValid())
    {
        *report->getEpgNextTitle() = next.m_title;
        *report->getEpgNextStart() = iso(next.m_start);
        report->setEpgNextDuration(next.m_duration);
    }
}

// What service_type is, for the web API
QString DVBDemod::serviceKind(int type)
{
    switch (type)
    {
    case 0x01: case 0x16: case 0x17: case 0x18: return "TV";
    case 0x11: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1F: return "HD TV";
    case 0x02: case 0x07: case 0x0A: return "Radio";
    case 0x0C: return "Data";
    default: return "Other";
    }
}

// An event of the guide, for the web API: just its id, title and time, unless full
SWGSDRangel::SWGDVBDemodEPGEvent *DVBDemod::webapiEPGEvent(const DVBDemodEPG::Event& event, int networkId, bool full, bool scheduled)
{
    SWGSDRangel::SWGDVBDemodEPGEvent *swgEvent = new SWGSDRangel::SWGDVBDemodEPGEvent();
    swgEvent->init();
    swgEvent->setEventId(event.m_eventId);
    *swgEvent->getTitle() = event.m_title;
    *swgEvent->getStart() = QDateTime::fromSecsSinceEpoch(event.m_start, Qt::UTC).toString(Qt::ISODate);
    swgEvent->setDuration(event.m_duration);

    if (scheduled) {
        swgEvent->setScheduled(1);
    }

    if (!full) {
        return swgEvent;
    }

    *swgEvent->getDescription() = event.m_text;

    if (event.m_genre >= 0) {
        *swgEvent->getGenre() = DVBDemodEPG::genreText(event.m_genre, networkId);
    }

    if (event.m_minimumAge > 0) {
        swgEvent->setMinimumAge(event.m_minimumAge);
    }

    *swgEvent->getGuidance() = event.m_guidance;
    swgEvent->setHd(event.m_hd ? 1 : 0);
    swgEvent->setSubtitles(event.m_subtitles ? 1 : 0);
    swgEvent->setAudioDescription(event.m_audioDescription ? 1 : 0);
    return swgEvent;
}

// Every service of the guide with what's on now and next, in channel order (those without a number last), and with
// epgReport 2, a service's schedule
void DVBDemod::webapiFormatEPGLists(SWGSDRangel::SWGDVBDemodReport *report, std::vector<DVBDemodEPG::Service> services, int serviceId)
{
    DVBDemodEPG *epg = getEPG();
    QSet<QString> scheduled;

    for (const auto& recording : m_settings.m_scheduledRecordings) {
        scheduled.insert(QString("%1/%2/%3").arg(recording.m_networkId).arg(recording.m_serviceId).arg(recording.m_eventId));
    }

    auto isScheduled = [&scheduled](int networkId, int serviceId, int eventId) {
        return scheduled.contains(QString("%1/%2/%3").arg(networkId).arg(serviceId).arg(eventId));
    };

    std::stable_sort(services.begin(), services.end(), [](const DVBDemodEPG::Service& a, const DVBDemodEPG::Service& b) {
        const unsigned int ca = (unsigned int) a.m_channel, cb = (unsigned int) b.m_channel; // -1 last
        return ca != cb ? ca < cb : (a.m_networkId != b.m_networkId ? a.m_networkId < b.m_networkId : a.m_serviceId < b.m_serviceId);
    });

    QList<SWGSDRangel::SWGDVBDemodEPGService*> *list = new QList<SWGSDRangel::SWGDVBDemodEPGService*>();

    for (const auto& service : services)
    {
        SWGSDRangel::SWGDVBDemodEPGService *swgService = new SWGSDRangel::SWGDVBDemodEPGService();
        swgService->init();
        swgService->setNetworkId(service.m_networkId);
        swgService->setTransportStreamId(service.m_transportStreamId);
        swgService->setServiceId(service.m_serviceId);
        *swgService->getName() = service.m_name;
        *swgService->getProvider() = service.m_provider;
        swgService->setType(service.m_type);
        *swgService->getKind() = serviceKind(service.m_type);
        swgService->setChannel(service.m_channel);
        swgService->setVisible(service.m_visible ? 1 : 0);
        swgService->setOnMultiplex(service.m_actual ? 1 : 0);
        swgService->setFrequency(service.m_frequency);
        swgService->setEvents(service.m_events);
        DVBDemodEPG::Event now, next;

        if (epg->getNowNext(service.m_networkId, service.m_serviceId, now, next))
        {
            if (now.isValid())
            {
                delete swgService->getNow();
                swgService->setNow(webapiEPGEvent(now, service.m_networkId, false, isScheduled(service.m_networkId, service.m_serviceId, now.m_eventId)));
            }

            if (next.isValid())
            {
                delete swgService->getNext();
                swgService->setNext(webapiEPGEvent(next, service.m_networkId, false, isScheduled(service.m_networkId, service.m_serviceId, next.m_eventId)));
            }
        }

        list->append(swgService);
    }

    delete report->getEpgServiceList();
    report->setEpgServiceList(list);

    if (m_settings.m_epgReport < 2) {
        return;
    }

    // The service asked for, of the network given, else that received, else the first with the id
    const int scheduleServiceId = m_settings.m_epgReportServiceId != 0 ? m_settings.m_epgReportServiceId : serviceId;
    int networkId = m_settings.m_epgReportNetworkId;
    bool actual = false;

    if (networkId < 0)
    {
        for (const auto& service : services)
        {
            if ((service.m_serviceId == scheduleServiceId) && !actual && (service.m_actual || (networkId < 0)))
            {
                networkId = service.m_networkId;
                actual = service.m_actual;
            }
        }
    }

    if ((scheduleServiceId == 0) || (networkId < 0)) {
        return;
    }

    report->setEpgScheduleServiceId(scheduleServiceId);
    report->setEpgScheduleNetworkId(networkId);
    QList<SWGSDRangel::SWGDVBDemodEPGEvent*> *schedule = new QList<SWGSDRangel::SWGDVBDemodEPGEvent*>();

    for (const auto& event : epg->getEvents(networkId, scheduleServiceId)) {
        schedule->append(webapiEPGEvent(event, networkId, true, isScheduled(networkId, scheduleServiceId, event.m_eventId)));
    }

    delete report->getEpgSchedule();
    report->setEpgSchedule(schedule);
}

int DVBDemod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    // The web API's thread: the actions use the search and the schedule, as the timers in this object's thread do
    if (QThread::currentThread() != thread()) {
        return inObjectThread(this, [&]() { return webapiActionsPost(channelActionsKeys, query, errorMessage); });
    }

    SWGSDRangel::SWGDVBDemodActions *actions = query.getDvbDemodActions();

    if (!actions)
    {
        errorMessage = "Missing DVBDemodActions in query";
        return 400;
    }

    if (channelActionsKeys.contains("scan"))
    {
        if (actions->getScan() == 0)
        {
            cancelScan();
        }
        else if (!startScan(channelActionsKeys.contains("scanFrom") ? actions->getScanFrom() : 0,
            channelActionsKeys.contains("scanTo") ? actions->getScanTo() : 0, errorMessage))
        {
            return 400;
        }
    }

    // Playing first, so a recording of the service selected is of the one asked for
    int playedNetworkId = -1;

    if (channelActionsKeys.contains("play") && (actions->getPlay() != 0))
    {
        if (!channelActionsKeys.contains("serviceId"))
        {
            errorMessage = "play needs serviceId";
            return 400;
        }

        const int networkId = serviceNetworkId(actions->getServiceId(), channelActionsKeys.contains("networkId") ? actions->getNetworkId() : -1);

        if (networkId < 0)
        {
            errorMessage = "Service not in the programme guide";
            return 404;
        }

        if (!playService(networkId, actions->getServiceId(), errorMessage)) {
            return 400;
        }

        playedNetworkId = networkId;
    }

    // Playing a service on another multiplex (retuning, or searching for it): it isn't received yet, and the multiplex
    // before may have a service with the same id, so it's recorded once its multiplex is (or not at all, if it isn't)
    DVBDemodEPG::Service played;
    const bool elsewhere = channelActionsKeys.contains("play") && (actions->getPlay() != 0)
        && getEPG()->getService(playedNetworkId, actions->getServiceId(), played) && !played.m_actual;

    if (channelActionsKeys.contains("record"))
    {
        if ((actions->getRecord() != 0) && elsewhere)
        {
            m_pendingRecording.m_active = true;
            m_pendingRecording.m_networkId = played.m_networkId;
            m_pendingRecording.m_transportStreamId = played.m_transportStreamId;
            m_pendingRecording.m_serviceId = played.m_serviceId;
            m_pendingRecording.m_since.start();
        }
        else if (actions->getRecord() != 0)
        {
            if (!startRecording(errorMessage)) {
                return 400;
            }
        }
        else
        {
            stopRecording();
        }
    }

    if (channelActionsKeys.contains("recordEvent"))
    {
        if (!channelActionsKeys.contains("serviceId") || !channelActionsKeys.contains("eventId"))
        {
            errorMessage = "recordEvent needs serviceId and eventId";
            return 400;
        }

        const int networkId = serviceNetworkId(actions->getServiceId(), channelActionsKeys.contains("networkId") ? actions->getNetworkId() : -1);

        if (networkId < 0)
        {
            errorMessage = "Service not in the programme guide";
            return 404;
        }

        if (actions->getRecordEvent() != 0)
        {
            if (!scheduleRecording(networkId, actions->getServiceId(), actions->getEventId()) && !isScheduled(networkId, actions->getServiceId(), actions->getEventId()))
            {
                errorMessage = "Programme not in the programme guide";
                return 404;
            }
        }
        else
        {
            cancelScheduledRecording(networkId, actions->getServiceId(), actions->getEventId());
        }
    }

    if (channelActionsKeys.contains("recordSeries"))
    {
        if (!channelActionsKeys.contains("serviceId") || !channelActionsKeys.contains("eventId"))
        {
            errorMessage = "recordSeries needs serviceId and eventId";
            return 400;
        }

        DVBDemodEPG::Service service;
        const int networkId = channelActionsKeys.contains("networkId") ? actions->getNetworkId() : getEPG()->getNetworkId();
        DVBDemodEPG::Event event;

        // The network given, else the one received, else the first with the service id
        int network = networkId;

        if (!getEPG()->getService(network, actions->getServiceId(), service))
        {
            network = -1;

            for (const auto& s : getEPG()->getServices())
            {
                if (s.m_serviceId == actions->getServiceId())
                {
                    network = s.m_networkId;
                    break;
                }
            }
        }

        if ((network < 0) || !getEPG()->getEvent(network, actions->getServiceId(), actions->getEventId(), event))
        {
            errorMessage = "Programme not in the programme guide";
            return 404;
        }

        if (actions->getRecordSeries() != 0)
        {
            if (event.m_seriesCrid.isEmpty())
            {
                errorMessage = "The programme isn't part of a series (it has no series CRID)";
                return 400;
            }

            scheduleSeries(network, actions->getServiceId(), actions->getEventId());
        }
        else
        {
            cancelSeries(network, actions->getServiceId(), actions->getEventId());
        }
    }

    return 202;
}

// Given, only it; else the first received (actual), else the first
int DVBDemod::serviceNetworkId(int serviceId, int networkId)
{
    int found = -1;
    bool actual = false;

    for (const auto& service : getEPG()->getServices())
    {
        if (service.m_serviceId != serviceId) {
            continue;
        }

        if (networkId >= 0)
        {
            if (service.m_networkId == networkId) {
                return networkId;
            }
        }
        else if (!actual && (service.m_actual || (found < 0)))
        {
            found = service.m_networkId;
            actual = service.m_actual;
        }
    }

    return found;
}

void DVBDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    DVBDemodSink::Status status;
    getStatus(status);
    const dvbt::OFDMDemod::Status& ofdm = status.m_receiver.m_ofdm;
    SWGSDRangel::SWGDVBDemodReport *report = response.getDvbDemodReport();

    report->setChannelPowerDb(status.m_power);
    report->setChannelSampleRate(status.m_channelSampleRate);
    *report->getState() = status.stateText(m_settings.m_bandwidth);
    report->setTsBitRate(status.m_tsBitRate);
    report->setUdpDatagrams(status.m_udpDatagrams);
    *report->getSearchText() = getSearchText();
    report->setScanTransponders(getScanTransponders());
    webapiFormatEPGReport(report);

    if (status.m_standard == S::StandardDVBT2)
    {
        webapiFormatT2Report(status, report);
        return;
    }

    if (status.isSatellite())
    {
        webapiFormatSatelliteReport(status, report);
        return;
    }

    *report->getStandard() = status.m_standard == S::StandardDVBT ? "DVB-T" : "";

    if (ofdm.m_state != dvbt::OFDMDemod::Acquire)
    {
        *report->getMode() = dvbt::toString(ofdm.m_mode);
        *report->getGuardInterval() = dvbt::toString(ofdm.m_guard);
        report->setFrequencyOffset(ofdm.m_frequencyOffset);
        report->setClockOffset(ofdm.m_clockOffset);
    }

    if (ofdm.m_tpsValid)
    {
        *report->getConstellation() = dvbt::toString(ofdm.m_tps.m_constellation);
        *report->getCodeRate() = dvbt::toString(ofdm.m_tps.m_hpCodeRate);
        *report->getHierarchy() = dvbt::toString(ofdm.m_tps.m_hierarchy);
        report->setMer(status.m_receiver.m_mer);
        report->setSnr(ofdm.m_snr);
        report->setPreViterbiBer(status.m_receiver.m_preViterbiBER);
        report->setPostViterbiBer(status.m_receiver.m_postViterbiBER);
    }

    report->setCellId(ofdm.m_cellId);
    report->setTsSync(status.m_receiver.m_tsSync ? 1 : 0);
    report->setTsPackets(status.m_receiver.m_outer.m_packets);
    report->setRsCorrected(status.m_receiver.m_outer.m_rsCorrected);
    report->setRsFailed(status.m_receiver.m_outer.m_rsFailed);
}

void DVBDemod::webapiFormatSatelliteReport(const DVBDemodSink::Status& status, SWGSDRangel::SWGDVBDemodReport *report)
{
    // Set, or found when automatic (0 while searching)
    report->setSymbolRate(m_settings.m_symbolRate > 0 ? m_settings.m_symbolRate : (int) std::max(0.0, status.m_autoSymbolRate));

    if (status.m_standard == S::StandardDVBS)
    {
        const dvbs::Receiver::Status& s = status.m_s;
        *report->getStandard() = "DVB-S";
        report->setMeasuredSymbolRate(s.m_frontEnd.m_measuredSymbolRate);
        report->setRollOff(0.35f);

        if (!s.m_frontEnd.m_estimating)
        {
            report->setFrequencyOffset(s.m_frequencyOffset);
            report->setClockOffset(s.m_clockOffset);
        }

        *report->getConstellation() = "QPSK";

        if (s.m_fec.m_codeRateKnown) {
            *report->getCodeRate() = dvbt::toString(s.m_fec.m_codeRate);
        }

        if (s.m_carrierLocked)
        {
            report->setMer(s.m_mer);
            report->setSnr(s.m_snr);
        }

        report->setPreViterbiBer(s.m_fec.m_preViterbiBER);
        report->setPostViterbiBer(s.m_fec.m_postViterbiBER);
        report->setTsSync(s.m_fec.m_tsSync ? 1 : 0);
        report->setTsPackets(s.m_fec.m_outer.m_packets);
        report->setReacquisitions(s.m_restarts);
        report->setRsCorrected(s.m_fec.m_outer.m_rsCorrected);
        report->setRsFailed(s.m_fec.m_outer.m_rsFailed);
        return;
    }

    if (status.m_standard == S::StandardDVBS2)
    {
        const dvbs2::Receiver::Status& s = status.m_s2;
        *report->getStandard() = "DVB-S2";
        report->setMeasuredSymbolRate(s.m_frontEnd.m_measuredSymbolRate);
        report->setRollOff(s.m_rollOff);

        if (!s.m_frontEnd.m_estimating)
        {
            report->setFrequencyOffset(s.m_frequencyOffset);
            report->setClockOffset(s.m_clockOffset);
        }

        if (s.m_pl.m_vlsnr >= 0)
        {
            // A VL-SNR frame's MODCOD, by its canonical name (its rate the name's, after its modulation)
            const dvbs2::VLSNRModcod& vl = dvbs2::vlsnrModcod(s.m_pl.m_vlsnr);
            const QString name(vl.m_name);
            *report->getModcod() = QString("VL-SNR %1").arg(name);
            *report->getConstellation() = dvbs2::toString(vl.m_modulation);
            *report->getCodeRate() = name.section(' ', -1);
            report->setPilots(1);
            *report->getFrameSize() = vl.m_type == dvbt2::FECType::Short ? "short" : vl.m_type == dvbt2::FECType::Medium ? "medium" : "normal";
            *report->getFecType() = vl.m_type == dvbt2::FECType::Short ? "16K" : vl.m_type == dvbt2::FECType::Medium ? "32K" : "64K";
        }
        else if (s.m_pl.m_modcod > 0)
        {
            dvbs2::Modulation modulation;
            dvbt2::CodeRate rate;

            if (dvbs2::modcodInfo(s.m_pl.m_modcod, modulation, rate))
            {
                *report->getModcod() = dvbs2::modcodString(s.m_pl.m_modcod);
                *report->getConstellation() = dvbs2::toString(modulation);
                *report->getCodeRate() = dvbt2::toString(rate);
            }

            report->setPilots((s.m_pl.m_type & 1) ? 1 : 0);
            *report->getFrameSize() = (s.m_pl.m_type & 2) ? "short" : "normal";
            *report->getFecType() = (s.m_pl.m_type & 2) ? "16K" : "64K";
        }

        if (s.m_pl.m_state == dvbs2::PLDecoder::State::Locked)
        {
            report->setMer(s.m_pl.m_mer);
            report->setEsN0(s.m_pl.m_esn0);
            report->setSnr(s.m_pl.m_esn0); // As Es/N0, from the known symbols
        }

        report->setIsi(s.m_isi);
        report->setIsiCount(s.m_isiCount);
        report->setPlFrames(s.m_pl.m_frames);
        report->setDummyFrames(s.m_pl.m_dummyFrames);
        report->setPlHeaderMisses(s.m_pl.m_headerMisses);
        report->setReacquisitions(s.m_restarts);
        report->setLdpcIterations(s.m_ldpcIterations);
        report->setFecBlocks(s.m_fecFrames);
        report->setBchFailures(s.m_bchFailures);
        report->setTsSync(s.m_streamPackets > 0 ? 1 : 0);    // Of the input stream selected
        report->setTsPackets(s.m_bb.m_packets);

        if (s.m_bb.m_streamType >= 0) {
            *report->getStreamType() = dvbt2::BBFrameDecoder::streamTypeName(s.m_bb.m_streamType);
        }

        return;
    }

    *report->getStandard() = "";
}

void DVBDemod::webapiFormatT2Report(const DVBDemodSink::Status& status, SWGSDRangel::SWGDVBDemodReport *report)
{
    const dvbt2::Receiver::Status& t2 = status.m_t2;
    *report->getStandard() = t2.m_ofdm.m_p1.isT2Lite() ? "DVB-T2-Lite" : "DVB-T2";

    if (t2.m_ofdm.m_state != dvbt2::OFDMDemod::SearchP1)
    {
        *report->getMode() = QString("%1%2").arg(dvbt2::fftSizeString(t2.m_ofdm.m_fftSize)).arg(t2.m_l1Valid && t2.m_pre.m_bwtExt ? "E" : "");
        *report->getGuardInterval() = dvbt2::toString(t2.m_ofdm.m_guard);
        report->setFrequencyOffset(t2.m_ofdm.m_frequencyOffset);
        report->setClockOffset(t2.m_ofdm.m_clockOffset);
    }

    if (t2.m_l1Valid)
    {
        *report->getPilotPattern() = dvbt2::toString(t2.m_pre.m_pilotPattern);
        report->setExtendedCarriers(t2.m_pre.m_bwtExt ? 1 : 0);
        report->setCellId(t2.m_pre.m_cellId);
        report->setPlpCount((int) t2.m_post.m_plps.size());

        for (const dvbt2::PLP& plp : t2.m_post.m_plps)
        {
            if (plp.m_id == t2.m_plpId)
            {
                *report->getConstellation() = dvbt2::toString(plp.m_constellation);
                *report->getCodeRate() = dvbt2::toString(plp.m_codeRate);
                report->setRotation(plp.m_rotation ? 1 : 0);
                *report->getFecType() = plp.m_fecType == dvbt2::FECType::Normal ? "64K" : "16K";
            }
        }
    }

    report->setPlpId(t2.m_plpId);
    report->setMer(t2.m_mer);
    report->setSnr(t2.m_ofdm.m_snr);
    report->setLdpcIterations(t2.m_ldpcIterations);
    report->setFecBlocks(t2.m_fecBlocks);
    report->setBchFailures(t2.m_bchFailures);
    report->setL1Errors(t2.m_l1Errors);
    report->setTsSync(t2.m_bb.m_packets > 0 ? 1 : 0);
    report->setTsPackets(t2.m_bb.m_packets);

    if (t2.m_bb.m_streamType >= 0) {
        *report->getStreamType() = dvbt2::BBFrameDecoder::streamTypeName(t2.m_bb.m_streamType);
    }
}

void DVBDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DVBDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void DVBDemod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DVBDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());

    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = m_channelId;
    } else {
        swgChannelSettings->setChannelType(new QString(m_channelId));
    }

    swgChannelSettings->setDvbDemodSettings(new SWGSDRangel::SWGDVBDemodSettings());
    SWGSDRangel::SWGDVBDemodSettings *swgSettings = swgChannelSettings->getDvbDemodSettings();

    // Transfer data that has been modified. When force is on transfer all data except reverse API data
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidth") || force) {
        swgSettings->setBandwidth(settings.m_bandwidth);
    }
    if (channelSettingsKeys.contains("standard") || force) {
        swgSettings->setStandard(settings.m_standard);
    }
    if (channelSettingsKeys.contains("plpId") || force) {
        swgSettings->setPlpId(settings.m_plpId);
    }
    if (channelSettingsKeys.contains("lowPriority") || force) {
        swgSettings->setLowPriority(settings.m_lowPriority ? 1 : 0);
    }
    if (channelSettingsKeys.contains("symbolRate") || force) {
        swgSettings->setSymbolRate(settings.m_symbolRate);
    }
    if (channelSettingsKeys.contains("rollOff") || force) {
        swgSettings->setRollOff(settings.m_rollOff);
    }
    if (channelSettingsKeys.contains("codeRate") || force) {
        swgSettings->setCodeRate(settings.m_codeRate);
    }
    if (channelSettingsKeys.contains("isi") || force) {
        swgSettings->setIsi(settings.m_isi);
    }
    if (channelSettingsKeys.contains("plScramblingCode") || force) {
        swgSettings->setPlScramblingCode(settings.m_plScramblingCode);
    }
    if (channelSettingsKeys.contains("recordRetune") || force) {
        swgSettings->setRecordRetune(settings.m_recordRetune ? 1 : 0);
    }
    if (channelSettingsKeys.contains("recordFollowRunning") || force) {
        swgSettings->setRecordFollowRunning(settings.m_recordFollowRunning ? 1 : 0);
    }
    if (channelSettingsKeys.contains("equaliser") || force) {
        swgSettings->setEqualiser(settings.m_equaliser ? 1 : 0);
    }
    if (channelSettingsKeys.contains("freesatRegion") || force) {
        swgSettings->setFreesatRegion(settings.m_freesatRegion);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgSettings->setUdpEnabled(settings.m_udpEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("videoEnabled") || force) {
        swgSettings->setVideoEnabled(settings.m_videoEnabled ? 1 : 0);
    }
    if (channelSettingsKeys.contains("serviceId") || force) {
        swgSettings->setServiceId(settings.m_serviceId);
    }
    if (channelSettingsKeys.contains("deinterlace") || force) {
        swgSettings->setDeinterlace(settings.m_deinterlace ? 1 : 0);
    }
    if (channelSettingsKeys.contains("avOffset") || force) {
        swgSettings->setAvOffset(settings.m_avOffset);
    }
    if (channelSettingsKeys.contains("hardwareDecoding") || force) {
        swgSettings->setHardwareDecoding(settings.m_hardwareDecoding ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioLanguage") || force) {
        swgSettings->setAudioLanguage(new QString(settings.m_audioLanguage));
    }
    if (channelSettingsKeys.contains("audioDescription") || force) {
        swgSettings->setAudioDescription(settings.m_audioDescription ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioDescriptionMixed") || force) {
        swgSettings->setAudioDescriptionMixed(settings.m_audioDescriptionMixed ? 1 : 0);
    }
    if (channelSettingsKeys.contains("cleanAudio") || force) {
        swgSettings->setCleanAudio(settings.m_cleanAudio ? 1 : 0);
    }
    if (channelSettingsKeys.contains("subtitlesHardOfHearing") || force) {
        swgSettings->setSubtitlesHardOfHearing(settings.m_subtitlesHardOfHearing ? 1 : 0);
    }
    if (channelSettingsKeys.contains("subtitles") || force) {
        swgSettings->setSubtitles(settings.m_subtitles ? 1 : 0);
    }
    if (channelSettingsKeys.contains("subtitleLanguage") || force) {
        swgSettings->setSubtitleLanguage(new QString(settings.m_subtitleLanguage));
    }
    if (channelSettingsKeys.contains("recordDirectory") || force) {
        swgSettings->setRecordDirectory(new QString(settings.m_recordDirectory));
    }
    if (channelSettingsKeys.contains("recordMode") || force) {
        swgSettings->setRecordMode(settings.m_recordMode);
    }
    if (channelSettingsKeys.contains("recordStartPadding") || force) {
        swgSettings->setRecordStartPadding(settings.m_recordStartPadding);
    }
    if (channelSettingsKeys.contains("recordEndPadding") || force) {
        swgSettings->setRecordEndPadding(settings.m_recordEndPadding);
    }
    if (channelSettingsKeys.contains("epgReport") || force) {
        swgSettings->setEpgReport(settings.m_epgReport);
    }
    if (channelSettingsKeys.contains("epgReportServiceId") || force) {
        swgSettings->setEpgReportServiceId(settings.m_epgReportServiceId);
    }
    if (channelSettingsKeys.contains("epgReportNetworkId") || force) {
        swgSettings->setEpgReportNetworkId(settings.m_epgReportNetworkId);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioVolume") || force) {
        swgSettings->setAudioVolume(settings.m_audioVolume);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgSettings->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgSettings->setRollupState(swgRollupState);
    }
}

void DVBDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DVBDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DVBDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
