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

#include <QColor>
#include <QDataStream>
#include <QStandardPaths>

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"

#include "dvbdemodsettings.h"

QByteArray DVBDemodSettings::serializeSchedule(const QList<ScheduledRecording>& schedule)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << (quint32) 1 << (quint32) schedule.size();

    for (const auto& r : schedule) {
        out << (qint32) r.m_networkId << (qint32) r.m_serviceId << (qint32) r.m_eventId << r.m_start << (qint32) r.m_duration
            << r.m_title << r.m_serviceName;
    }

    return data;
}

QList<DVBDemodSettings::ScheduledRecording> DVBDemodSettings::deserializeSchedule(const QByteArray& data)
{
    QList<ScheduledRecording> schedule;

    if (data.isEmpty()) {
        return schedule;
    }

    QDataStream in(data);
    quint32 version, count;
    in >> version >> count;

    if (version != 1) {
        return schedule;
    }

    for (quint32 i = 0; (i < count) && (in.status() == QDataStream::Ok); i++)
    {
        ScheduledRecording r;
        qint32 networkId, serviceId, eventId, duration;
        in >> networkId >> serviceId >> eventId >> r.m_start >> duration >> r.m_title >> r.m_serviceName;
        r.m_networkId = networkId;
        r.m_serviceId = serviceId;
        r.m_eventId = eventId;
        r.m_duration = duration;

        if (in.status() == QDataStream::Ok) {
            schedule.append(r);
        }
    }

    return schedule;
}

QByteArray DVBDemodSettings::serializeSeries(const QList<SeriesRecording>& series)
{
    QByteArray data;
    QDataStream out(&data, QIODevice::WriteOnly);
    out << (quint32) 1 << (quint32) series.size();

    for (const auto& s : series) {
        out << (qint32) s.m_networkId << (qint32) s.m_serviceId << s.m_seriesCrid << s.m_title << s.m_serviceName << s.m_scheduled;
    }

    return data;
}

QList<DVBDemodSettings::SeriesRecording> DVBDemodSettings::deserializeSeries(const QByteArray& data)
{
    QList<SeriesRecording> series;

    if (data.isEmpty()) {
        return series;
    }

    QDataStream in(data);
    quint32 version, count;
    in >> version >> count;

    if (version != 1) {
        return series;
    }

    for (quint32 i = 0; (i < count) && (in.status() == QDataStream::Ok); i++)
    {
        SeriesRecording s;
        qint32 networkId, serviceId;
        in >> networkId >> serviceId >> s.m_seriesCrid >> s.m_title >> s.m_serviceName >> s.m_scheduled;
        s.m_networkId = networkId;
        s.m_serviceId = serviceId;

        if ((in.status() == QDataStream::Ok) && !s.m_seriesCrid.isEmpty()) {
            series.append(s);
        }
    }

    return series;
}

DVBDemodSettings::DVBDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DVBDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_bandwidth = 8000000;
    m_standard = StandardAutoTerrestrial;
    m_plpId = -1;
    m_lowPriority = false;
    m_symbolRate = 0; // Auto
    m_rollOff = 0;
    m_codeRate = -1;
    m_isi = -1;
    m_plScramblingCode = 0;
    m_equaliser = true;
    m_freesatRegion = 0;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 5004;
    m_videoEnabled = true;
    m_serviceId = 0;
    m_deinterlace = true;
    m_hardwareDecoding = true;
    m_audioLanguage = "";
    m_audioDescription = false;
    m_audioDescriptionMixed = false;
    m_cleanAudio = false;
    m_subtitlesHardOfHearing = false;
    m_subtitles = false;
    m_subtitleLanguage = "";
    m_avOffset = 0;
    m_audioMute = false;
    m_audioVolume = 50;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_recordDirectory = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    m_recordMode = 1;
    m_recordStartPadding = 120;
    m_recordEndPadding = 300;
    m_epgReport = 0;
    m_epgReportServiceId = 0;
    m_epgReportNetworkId = -1;
    m_scheduledRecordings.clear();
    m_seriesRecordings.clear();
    m_recordRetune = false;
    m_recordFollowRunning = true;
    m_rgbColor = QColor(65, 120, 200).rgb();
    m_title = "DVB Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_splitterSizes.clear();
    m_polarisationCommand = "";
    m_scanFrom = 0;
    m_scanTo = 0;
}

int DVBDemodSettings::occupiedBandwidth(double symbolRate) const
{
    if (isSatellite(m_standard)) {
        return (int) ((m_symbolRate > 0 ? m_symbolRate : symbolRate) * (1.0 + (m_rollOff > 0 ? m_rollOff : 35) / 100.0));
    }

    return (int) (m_bandwidth * 0.951); // 7.61 MHz in 8
}

QByteArray DVBDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_inputFrequencyOffset);
    s.writeS32(2, m_bandwidth);
    s.writeBool(3, m_udpEnabled);
    s.writeString(4, m_udpAddress);
    s.writeU32(5, m_udpPort);
    s.writeBool(6, m_videoEnabled);
    s.writeS32(7, m_serviceId);
    s.writeBool(8, m_audioMute);
    s.writeS32(9, m_audioVolume);
    s.writeString(10, m_audioDeviceName);
    s.writeBool(11, m_deinterlace);
    s.writeS32(12, m_standard);
    s.writeS32(13, m_plpId);
    s.writeS32(14, m_avOffset);
    s.writeBool(15, m_hardwareDecoding);
    s.writeString(16, m_recordDirectory);
    s.writeS32(17, m_recordMode);
    s.writeS32(18, m_recordStartPadding);
    s.writeS32(19, m_recordEndPadding);
    s.writeBlob(20, serializeSchedule(m_scheduledRecordings));
    s.writeU32(21, m_rgbColor);
    s.writeString(22, m_title);

    if (m_channelMarker) {
        s.writeBlob(23, m_channelMarker->serialize());
    }

    s.writeS32(24, m_streamIndex);
    s.writeBool(25, m_useReverseAPI);
    s.writeString(26, m_reverseAPIAddress);
    s.writeU32(27, m_reverseAPIPort);
    s.writeU32(28, m_reverseAPIDeviceIndex);
    s.writeU32(29, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(30, m_rollupState->serialize());
    }

    s.writeS32(32, m_workspaceIndex);
    s.writeBlob(33, m_geometryBytes);
    s.writeBool(34, m_hidden);

    s.writeString(40, m_audioLanguage);
    s.writeBool(41, m_audioDescription);
    s.writeBool(42, m_subtitles);
    s.writeString(43, m_subtitleLanguage);
    s.writeBool(44, m_lowPriority);
    s.writeS32(45, m_epgReport);
    s.writeS32(46, m_epgReportServiceId);
    s.writeS32(47, m_epgReportNetworkId);
    s.writeS32(48, m_symbolRate);
    s.writeS32(49, m_rollOff);
    s.writeS32(50, m_codeRate);
    s.writeS32(51, m_isi);
    s.writeS32(52, m_plScramblingCode);
    s.writeS32(53, m_freesatRegion);
    s.writeBool(54, m_equaliser);
    s.writeList(55, m_splitterSizes);
    s.writeString(56, m_polarisationCommand);
    s.writeS64(57, m_scanFrom);
    s.writeS64(58, m_scanTo);
    s.writeBlob(59, serializeSeries(m_seriesRecordings));
    s.writeBool(60, m_recordRetune);
    s.writeBool(61, m_recordFollowRunning);
    s.writeBool(62, m_audioDescriptionMixed);
    s.writeBool(63, m_cleanAudio);
    s.writeBool(64, m_subtitlesHardOfHearing);

    return s.final();
}

bool DVBDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;

        d.readS64(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_bandwidth, 8000000);

        if (!isValidBandwidth(m_bandwidth)) {
            m_bandwidth = 8000000;
        }

        d.readBool(3, &m_udpEnabled, false);
        d.readString(4, &m_udpAddress, "127.0.0.1");
        d.readU32(5, &utmp, 5004);
        m_udpPort = utmp > 65535 ? 5004 : utmp;
        d.readBool(6, &m_videoEnabled, true);
        d.readS32(7, &m_serviceId, 0);
        d.readBool(8, &m_audioMute, false);
        d.readS32(9, &m_audioVolume, 50);
        d.readString(10, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readBool(11, &m_deinterlace, true);
        d.readS32(12, &m_standard, StandardAutoTerrestrial);

        if (!isValidStandard(m_standard)) {
            m_standard = StandardAutoTerrestrial;
        }

        d.readS32(13, &m_plpId, -1);
        d.readS32(14, &m_avOffset, 0);
        d.readBool(15, &m_hardwareDecoding, true);
        d.readString(16, &m_recordDirectory, QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
        d.readS32(17, &m_recordMode, 1);
        d.readS32(18, &m_recordStartPadding, 120);
        d.readS32(19, &m_recordEndPadding, 300);
        QByteArray schedule;
        d.readBlob(20, &schedule);
        m_scheduledRecordings = deserializeSchedule(schedule);
        d.readU32(21, &m_rgbColor, QColor(65, 120, 200).rgb());
        d.readString(22, &m_title, "DVB Demodulator");

        if (m_channelMarker)
        {
            d.readBlob(23, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(24, &m_streamIndex, 0);
        d.readBool(25, &m_useReverseAPI, false);
        d.readString(26, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(27, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(28, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(29, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(30, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(32, &m_workspaceIndex, 0);
        d.readBlob(33, &m_geometryBytes);
        d.readBool(34, &m_hidden, false);

        d.readString(40, &m_audioLanguage, "");
        d.readBool(41, &m_audioDescription, false);
        d.readBool(42, &m_subtitles, false);
        d.readString(43, &m_subtitleLanguage, "");
        d.readBool(44, &m_lowPriority, false);
        d.readS32(45, &m_epgReport, 0);

        if (!isValidEPGReport(m_epgReport)) {
            m_epgReport = 0;
        }

        d.readS32(46, &m_epgReportServiceId, 0);
        d.readS32(47, &m_epgReportNetworkId, -1);
        d.readS32(48, &m_symbolRate, 0);

        if (!isValidSymbolRate(m_symbolRate)) {
            m_symbolRate = 0;
        }

        d.readS32(49, &m_rollOff, 0);

        if (!isValidRollOff(m_rollOff)) {
            m_rollOff = 0;
        }

        d.readS32(50, &m_codeRate, -1);

        if (!isValidCodeRate(m_codeRate)) {
            m_codeRate = -1;
        }

        d.readS32(51, &m_isi, -1);

        if (!isValidISI(m_isi)) {
            m_isi = -1;
        }

        d.readS32(52, &m_plScramblingCode, 0);

        if (!isValidScramblingCode(m_plScramblingCode)) {
            m_plScramblingCode = 0;
        }

        d.readS32(53, &m_freesatRegion, 0);
        d.readBool(54, &m_equaliser, true);
        d.readList(55, &m_splitterSizes);
        d.readString(56, &m_polarisationCommand, "");
        d.readS64(57, &m_scanFrom, 0);
        d.readS64(58, &m_scanTo, 0);
        QByteArray series;
        d.readBlob(59, &series);
        m_seriesRecordings = deserializeSeries(series);
        d.readBool(60, &m_recordRetune, false);
        d.readBool(61, &m_recordFollowRunning, true);
        d.readBool(62, &m_audioDescriptionMixed, false);
        d.readBool(63, &m_cleanAudio, false);
        d.readBool(64, &m_subtitlesHardOfHearing, false);

        if (!isValidFreesatRegion(m_freesatRegion)) {
            m_freesatRegion = 0;
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void DVBDemodSettings::applySettings(const QStringList& settingsKeys, const DVBDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("bandwidth")) {
        m_bandwidth = settings.m_bandwidth;
    }
    if (settingsKeys.contains("standard")) {
        m_standard = settings.m_standard;
    }
    if (settingsKeys.contains("symbolRate")) {
        m_symbolRate = settings.m_symbolRate;
    }
    if (settingsKeys.contains("rollOff")) {
        m_rollOff = settings.m_rollOff;
    }
    if (settingsKeys.contains("codeRate")) {
        m_codeRate = settings.m_codeRate;
    }
    if (settingsKeys.contains("isi")) {
        m_isi = settings.m_isi;
    }
    if (settingsKeys.contains("plScramblingCode")) {
        m_plScramblingCode = settings.m_plScramblingCode;
    }
    if (settingsKeys.contains("equaliser")) {
        m_equaliser = settings.m_equaliser;
    }
    if (settingsKeys.contains("freesatRegion")) {
        m_freesatRegion = settings.m_freesatRegion;
    }
    if (settingsKeys.contains("plpId")) {
        m_plpId = settings.m_plpId;
    }
    if (settingsKeys.contains("lowPriority")) {
        m_lowPriority = settings.m_lowPriority;
    }
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("videoEnabled")) {
        m_videoEnabled = settings.m_videoEnabled;
    }
    if (settingsKeys.contains("serviceId")) {
        m_serviceId = settings.m_serviceId;
    }
    if (settingsKeys.contains("deinterlace")) {
        m_deinterlace = settings.m_deinterlace;
    }
    if (settingsKeys.contains("avOffset")) {
        m_avOffset = settings.m_avOffset;
    }
    if (settingsKeys.contains("hardwareDecoding")) {
        m_hardwareDecoding = settings.m_hardwareDecoding;
    }
    if (settingsKeys.contains("audioLanguage")) {
        m_audioLanguage = settings.m_audioLanguage;
    }
    if (settingsKeys.contains("audioDescription")) {
        m_audioDescription = settings.m_audioDescription;
    }
    if (settingsKeys.contains("audioDescriptionMixed")) {
        m_audioDescriptionMixed = settings.m_audioDescriptionMixed;
    }
    if (settingsKeys.contains("cleanAudio")) {
        m_cleanAudio = settings.m_cleanAudio;
    }
    if (settingsKeys.contains("subtitlesHardOfHearing")) {
        m_subtitlesHardOfHearing = settings.m_subtitlesHardOfHearing;
    }
    if (settingsKeys.contains("subtitles")) {
        m_subtitles = settings.m_subtitles;
    }
    if (settingsKeys.contains("subtitleLanguage")) {
        m_subtitleLanguage = settings.m_subtitleLanguage;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("audioVolume")) {
        m_audioVolume = settings.m_audioVolume;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("recordDirectory")) {
        m_recordDirectory = settings.m_recordDirectory;
    }
    if (settingsKeys.contains("recordMode")) {
        m_recordMode = settings.m_recordMode;
    }
    if (settingsKeys.contains("recordStartPadding")) {
        m_recordStartPadding = settings.m_recordStartPadding;
    }
    if (settingsKeys.contains("recordEndPadding")) {
        m_recordEndPadding = settings.m_recordEndPadding;
    }
    if (settingsKeys.contains("epgReport")) {
        m_epgReport = settings.m_epgReport;
    }
    if (settingsKeys.contains("epgReportServiceId")) {
        m_epgReportServiceId = settings.m_epgReportServiceId;
    }
    if (settingsKeys.contains("epgReportNetworkId")) {
        m_epgReportNetworkId = settings.m_epgReportNetworkId;
    }
    if (settingsKeys.contains("scheduledRecordings")) {
        m_scheduledRecordings = settings.m_scheduledRecordings;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("splitterSizes")) {
        m_splitterSizes = settings.m_splitterSizes;
    }
    if (settingsKeys.contains("polarisationCommand")) {
        m_polarisationCommand = settings.m_polarisationCommand;
    }
    if (settingsKeys.contains("scanFrom")) {
        m_scanFrom = settings.m_scanFrom;
    }
    if (settingsKeys.contains("scanTo")) {
        m_scanTo = settings.m_scanTo;
    }
    if (settingsKeys.contains("seriesRecordings")) {
        m_seriesRecordings = settings.m_seriesRecordings;
    }
    if (settingsKeys.contains("recordRetune")) {
        m_recordRetune = settings.m_recordRetune;
    }
    if (settingsKeys.contains("recordFollowRunning")) {
        m_recordFollowRunning = settings.m_recordFollowRunning;
    }
}

QString DVBDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    QStringList s;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        s.append(QString("m_inputFrequencyOffset: %1").arg(m_inputFrequencyOffset));
    }
    if (settingsKeys.contains("bandwidth") || force) {
        s.append(QString("m_bandwidth: %1").arg(m_bandwidth));
    }
    if (settingsKeys.contains("standard") || force) {
        s.append(QString("m_standard: %1").arg(m_standard));
    }
    if (settingsKeys.contains("symbolRate") || force) {
        s.append(QString("m_symbolRate: %1").arg(m_symbolRate));
    }
    if (settingsKeys.contains("rollOff") || force) {
        s.append(QString("m_rollOff: %1").arg(m_rollOff));
    }
    if (settingsKeys.contains("codeRate") || force) {
        s.append(QString("m_codeRate: %1").arg(m_codeRate));
    }
    if (settingsKeys.contains("isi") || force) {
        s.append(QString("m_isi: %1").arg(m_isi));
    }
    if (settingsKeys.contains("plScramblingCode") || force) {
        s.append(QString("m_plScramblingCode: %1").arg(m_plScramblingCode));
    }
    if (settingsKeys.contains("equaliser") || force) {
        s.append(QString("m_equaliser: %1").arg(m_equaliser));
    }
    if (settingsKeys.contains("freesatRegion") || force) {
        s.append(QString("m_freesatRegion: %1").arg(m_freesatRegion));
    }
    if (settingsKeys.contains("plpId") || force) {
        s.append(QString("m_plpId: %1").arg(m_plpId));
    }
    if (settingsKeys.contains("lowPriority") || force) {
        s.append(QString("m_lowPriority: %1").arg(m_lowPriority));
    }
    if (settingsKeys.contains("udpEnabled") || force) {
        s.append(QString("m_udpEnabled: %1").arg(m_udpEnabled));
    }
    if (settingsKeys.contains("udpAddress") || force) {
        s.append(QString("m_udpAddress: %1").arg(m_udpAddress));
    }
    if (settingsKeys.contains("udpPort") || force) {
        s.append(QString("m_udpPort: %1").arg(m_udpPort));
    }
    if (settingsKeys.contains("videoEnabled") || force) {
        s.append(QString("m_videoEnabled: %1").arg(m_videoEnabled));
    }
    if (settingsKeys.contains("serviceId") || force) {
        s.append(QString("m_serviceId: %1").arg(m_serviceId));
    }
    if (settingsKeys.contains("deinterlace") || force) {
        s.append(QString("m_deinterlace: %1").arg(m_deinterlace));
    }
    if (settingsKeys.contains("avOffset") || force) {
        s.append(QString("m_avOffset: %1").arg(m_avOffset));
    }
    if (settingsKeys.contains("hardwareDecoding") || force) {
        s.append(QString("m_hardwareDecoding: %1").arg(m_hardwareDecoding));
    }
    if (settingsKeys.contains("audioLanguage") || force) {
        s.append(QString("m_audioLanguage: %1").arg(m_audioLanguage));
    }
    if (settingsKeys.contains("audioDescription") || force) {
        s.append(QString("m_audioDescription: %1").arg(m_audioDescription));
    }
    if (settingsKeys.contains("audioDescriptionMixed") || force) {
        s.append(QString("m_audioDescriptionMixed: %1").arg(m_audioDescriptionMixed));
    }
    if (settingsKeys.contains("cleanAudio") || force) {
        s.append(QString("m_cleanAudio: %1").arg(m_cleanAudio));
    }
    if (settingsKeys.contains("subtitlesHardOfHearing") || force) {
        s.append(QString("m_subtitlesHardOfHearing: %1").arg(m_subtitlesHardOfHearing));
    }
    if (settingsKeys.contains("subtitles") || force) {
        s.append(QString("m_subtitles: %1").arg(m_subtitles));
    }
    if (settingsKeys.contains("subtitleLanguage") || force) {
        s.append(QString("m_subtitleLanguage: %1").arg(m_subtitleLanguage));
    }
    if (settingsKeys.contains("audioMute") || force) {
        s.append(QString("m_audioMute: %1").arg(m_audioMute));
    }
    if (settingsKeys.contains("audioVolume") || force) {
        s.append(QString("m_audioVolume: %1").arg(m_audioVolume));
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        s.append(QString("m_audioDeviceName: %1").arg(m_audioDeviceName));
    }
    if (settingsKeys.contains("recordDirectory") || force) {
        s.append(QString("m_recordDirectory: %1").arg(m_recordDirectory));
    }
    if (settingsKeys.contains("recordMode") || force) {
        s.append(QString("m_recordMode: %1").arg(m_recordMode));
    }
    if (settingsKeys.contains("recordStartPadding") || force) {
        s.append(QString("m_recordStartPadding: %1").arg(m_recordStartPadding));
    }
    if (settingsKeys.contains("recordEndPadding") || force) {
        s.append(QString("m_recordEndPadding: %1").arg(m_recordEndPadding));
    }
    if (settingsKeys.contains("epgReport") || force) {
        s.append(QString("m_epgReport: %1").arg(m_epgReport));
    }
    if (settingsKeys.contains("epgReportServiceId") || force) {
        s.append(QString("m_epgReportServiceId: %1").arg(m_epgReportServiceId));
    }
    if (settingsKeys.contains("epgReportNetworkId") || force) {
        s.append(QString("m_epgReportNetworkId: %1").arg(m_epgReportNetworkId));
    }
    if (settingsKeys.contains("scheduledRecordings") || force) {
        s.append(QString("m_scheduledRecordings: %1").arg(m_scheduledRecordings.size()));
    }
    if (settingsKeys.contains("polarisationCommand") || force) {
        s.append(QString("m_polarisationCommand: %1").arg(m_polarisationCommand));
    }
    if (settingsKeys.contains("scanFrom") || force) {
        s.append(QString("m_scanFrom: %1").arg(m_scanFrom));
    }
    if (settingsKeys.contains("scanTo") || force) {
        s.append(QString("m_scanTo: %1").arg(m_scanTo));
    }
    if (settingsKeys.contains("seriesRecordings") || force) {
        s.append(QString("m_seriesRecordings: %1").arg(m_seriesRecordings.size()));
    }
    if (settingsKeys.contains("recordRetune") || force) {
        s.append(QString("m_recordRetune: %1").arg(m_recordRetune));
    }
    if (settingsKeys.contains("recordFollowRunning") || force) {
        s.append(QString("m_recordFollowRunning: %1").arg(m_recordFollowRunning));
    }
    if (settingsKeys.contains("title") || force) {
        s.append(QString("m_title: %1").arg(m_title));
    }
    if (settingsKeys.contains("streamIndex") || force) {
        s.append(QString("m_streamIndex: %1").arg(m_streamIndex));
    }

    return s.join(" ");
}
