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

#include <QColor>
#include <QDateTime>
#include <QTimeZone>

#include <algorithm>
#include <sstream>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "audio/audiodevicemanager.h"
#include "radiopagingcharset.h"
#include "radiopagingflex.h"
#include "radiopaginggsc.h"
#include "radiopagingvoicestore.h"
#include "radiopagingmodsettings.h"

QString RadioPagingModSettings::voiceDirectory() const
{
    return m_gscVoiceDirectory.isEmpty() ? RadioPagingVoiceStore::defaultDirectory("tx") : m_gscVoiceDirectory;
}

QString RadioPagingModSettings::channelName() const
{
    return m_title;
}

const RadioPagingCharset::Key& RadioPagingModSettings::charset(Protocol protocol) const
{
    return (protocol == Pocsag) ? m_pocsagCharset : (protocol == Flex) ? m_flexCharset : m_gscCharset;
}

RadioPagingCharset::Key RadioPagingModSettings::defaultCharset(const char *protocol)
{
    const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::defaultFor(protocol);
    return codepage ? codepage->key() : RadioPagingCharset::Key();
}

void RadioPagingModSettings::systemTimeZone(int& zoneMinutes, bool& daylightSaving)
{
    const QTimeZone zone = QTimeZone::systemTimeZone();
    const QDateTime now = QDateTime::currentDateTime();

    // The standard time offset of the system zone, UTC if it is not a zone of STD-43A Table 3.7.2-3
    zoneMinutes = zone.standardTimeOffset(now) / 60;
    daylightSaving = zone.isDaylightTime(now);

    if (RadioPagingFlex::timeZoneCode(zoneMinutes) < 0)
    {
        zoneMinutes = 0;
        daylightSaving = false;
    }
}

namespace {

// A codepage of the protocol (its key as in the registry), else its default
void validCharset(RadioPagingCharset::Key& key, const char *protocol)
{
    const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::find(protocol, key);
    key = codepage ? codepage->key() : RadioPagingModSettings::defaultCharset(protocol);
}

void writeCharset(SimpleSerializer& s, int tag, const RadioPagingCharset::Key& key)
{
    s.writeString(tag, QString::fromStdString(key.m_vendor));
    s.writeString(tag + 1, QString::fromStdString(key.m_model));
    s.writeString(tag + 2, QString::fromStdString(key.m_codepage));
}

RadioPagingCharset::Key readCharset(SimpleDeserializer& d, int tag, const char *protocol)
{
    const RadioPagingCharset::Key def = RadioPagingModSettings::defaultCharset(protocol);
    QString vendor, model, codepage;
    d.readString(tag, &vendor, QString::fromStdString(def.m_vendor));
    d.readString(tag + 1, &model, QString::fromStdString(def.m_model));
    d.readString(tag + 2, &codepage, QString::fromStdString(def.m_codepage));
    return RadioPagingCharset::Key{vendor.toStdString(), model.toStdString(), codepage.toStdString()};
}

} // namespace

const int RadioPagingModSettings::m_nbPocsagSpeeds;
const int RadioPagingModSettings::m_pocsagSpeeds[m_nbPocsagSpeeds] = { 512, 1200, 2400 };

RadioPagingModSettings::RadioPagingModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RadioPagingModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_protocol = Pocsag;
    m_mode = OneShot;
    m_deviation = 4500.0f;
    m_gain = 0.0f;
    m_channelMute = false;
    m_rampUpMs = 5.0f;
    m_rampDownMs = 5.0f;
    m_rampRange = 60;
    m_pocsagSpeed = 1200;
    m_pocsagInverted = false;
    m_flexMode = (int) RadioPagingFlex::Mode1600_2;
    m_flexInverted = false;
    m_flexCollapse = 0;
    m_flexRoaming = false;
    m_ssidLocalId = 0;
    m_ssidCoverageZone = 0;
    m_ssidCountryCode = 0;
    m_ssidTrafficFlags = 0xf;
    // FLEX time on by default: date, time and zone, the zone and daylight saving of the system
    m_flexTime = true;
    m_flexTimeFrameZero = true;
    systemTimeZone(m_flexTimeZoneMinutes, m_flexTimeDaylightSaving);
    m_flexTimeSendZone = true;
    m_flexTimeOffsetSeconds = 0;
    m_flexTimeEquivalentYear = true;
    m_flexTimeExtendedSeconds = 0;
    m_maxQueued = 10000;
    m_flexNumbering = true;
    m_flexMaxMessageNumber = 63;
    m_gscMode = (int) RadioPagingGsc::Auto;
    m_gscInverted = false;
    m_gscVoiceSeconds = 30;
    m_audioInputDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_gscVoiceDirectory = "";
    m_gscVoiceKeepDays = 3;
    m_gscVoiceKeepMin = 50;
    m_flexMixPocsag = false;
    m_pocsagDeviation = 4500.0f;
    m_mixFramesPerMinute = 0;
    m_mixShared = false;
    m_mixSwitchMs[0] = 400;
    m_mixSwitchMs[1] = 200;
    m_mixSwitchMs[2] = 100;

    m_rgbColor = QColor(255, 170, 0).rgb();
    m_title = "Radio Paging Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_messageColumns.clear();
    m_voicePlaybackDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_pocsagCharset = defaultCharset("pocsag");
    m_flexCharset = defaultCharset("flex");
    m_gscCharset = defaultCharset("gsc");
}

void RadioPagingModSettings::validate()
{
    validCharset(m_pocsagCharset, "pocsag");
    validCharset(m_flexCharset, "flex");
    validCharset(m_gscCharset, "gsc");
    // The ranges of the GUI
    m_rampUpMs = std::max(0.0f, std::min(100.0f, m_rampUpMs));
    m_rampDownMs = std::max(0.0f, std::min(100.0f, m_rampDownMs));
    m_rampRange = std::max(0, std::min(120, m_rampRange));
    m_deviation = std::max(500.0f, std::min(10000.0f, m_deviation));
    m_gain = std::max(-60.0f, std::min(0.0f, m_gain));

    if (!isPocsagSpeed(m_pocsagSpeed)) {
        m_pocsagSpeed = 1200;
    }
    if ((m_flexMode < (int) RadioPagingFlex::Mode1600_2) || (m_flexMode > (int) RadioPagingFlex::Mode6400_4)) {
        m_flexMode = (int) RadioPagingFlex::Mode1600_2;
    }

    m_flexCollapse = std::max(0, std::min(RadioPagingFlex::m_maxCollapse, m_flexCollapse));
    if ((m_gscMode < (int) RadioPagingGsc::Auto) || (m_gscMode > (int) RadioPagingGsc::NonBatterySaver)) {
        m_gscMode = (int) RadioPagingGsc::Auto;
    }

    m_ssidLocalId = std::max(0, std::min(511, m_ssidLocalId));
    m_ssidCoverageZone = std::max(0, std::min(31, m_ssidCoverageZone));
    m_ssidCountryCode = std::max(0, std::min(1023, m_ssidCountryCode));
    m_ssidTrafficFlags = std::max(0, std::min(15, m_ssidTrafficFlags));

    // A zone of Table 3.7.2-3, else UTC
    if (RadioPagingFlex::timeZoneCode(m_flexTimeZoneMinutes) < 0) {
        m_flexTimeZoneMinutes = 0;
    }
    m_flexTimeExtendedSeconds = std::max(0, std::min(7, m_flexTimeExtendedSeconds));
    m_maxQueued = std::max(1, std::min((int) m_maxQueuedLimit, m_maxQueued));

    m_flexMaxMessageNumber = std::max(0, std::min(63, m_flexMaxMessageNumber));
    m_gscVoiceSeconds = std::max(1, std::min(50, m_gscVoiceSeconds));
    m_pocsagDeviation = std::max(500.0f, std::min(10000.0f, m_pocsagDeviation));
    m_mixFramesPerMinute = std::max(0, std::min(m_mixShared ? 128 : 32, m_mixFramesPerMinute));

    for (int i = 0; i < 3; i++) {
        m_mixSwitchMs[i] = std::max(0, std::min(1000, m_mixSwitchMs[i]));
    }
}

float RadioPagingModSettings::defaultDeviation(Protocol protocol)
{
    return (protocol == Flex) ? 4800.0f : 4500.0f;
}

int RadioPagingModSettings::flexModeOf(const QString& name)
{
    for (int mode = (int) RadioPagingFlex::Mode1600_2; mode <= (int) RadioPagingFlex::Mode6400_4; mode++)
    {
        if (name == RadioPagingFlex::modeName((RadioPagingFlex::Mode) mode)) {
            return mode;
        }
    }

    return -1;
}

bool RadioPagingModSettings::isPocsagSpeed(int baud)
{
    for (int i = 0; i < m_nbPocsagSpeeds; i++)
    {
        if (m_pocsagSpeeds[i] == baud) {
            return true;
        }
    }

    return false;
}

QString RadioPagingModSettings::protocolName(Protocol protocol)
{
    switch (protocol)
    {
    case Pocsag:
        return "POCSAG";
    case Flex:
        return "FLEX";
    case Gsc:
        return "GSC";
    }

    return "";
}

// Tags 1 to 14 and 33 to 60 are the modulator settings, 20 to 32 the usual channel tags. New tags are only appended
QByteArray RadioPagingModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_inputFrequencyOffset);
    s.writeS32(2, (int) m_protocol);
    s.writeS32(3, (int) m_mode);
    s.writeFloat(4, m_deviation);
    s.writeFloat(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeFloat(7, m_rampUpMs);
    s.writeFloat(8, m_rampDownMs);
    s.writeS32(9, m_rampRange);
    s.writeS32(10, m_pocsagSpeed);
    s.writeBool(11, m_pocsagInverted);
    s.writeS32(12, m_flexMode);
    s.writeBool(13, m_flexInverted);
    s.writeS32(14, m_flexCollapse);

    s.writeU32(20, m_rgbColor);
    s.writeString(21, m_title);

    if (m_channelMarker) {
        s.writeBlob(22, m_channelMarker->serialize());
    }

    s.writeS32(23, m_streamIndex);
    s.writeBool(24, m_useReverseAPI);
    s.writeString(25, m_reverseAPIAddress);
    s.writeU32(26, m_reverseAPIPort);
    s.writeU32(27, m_reverseAPIDeviceIndex);
    s.writeU32(28, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(29, m_rollupState->serialize());
    }

    s.writeS32(30, m_workspaceIndex);
    s.writeBlob(31, m_geometryBytes);
    s.writeBool(32, m_hidden);
    s.writeBool(33, m_flexMixPocsag);
    s.writeFloat(34, m_pocsagDeviation);
    s.writeS32(35, m_mixFramesPerMinute);
    s.writeBool(36, m_mixShared);
    s.writeS32(37, m_mixSwitchMs[0]);
    s.writeS32(38, m_mixSwitchMs[1]);
    s.writeS32(39, m_mixSwitchMs[2]);
    s.writeS32(40, m_gscMode);
    s.writeBool(41, m_gscInverted);
    s.writeS32(42, m_gscVoiceSeconds);
    s.writeString(43, m_audioInputDeviceName);
    s.writeString(45, m_gscVoiceDirectory);
    s.writeS32(46, m_gscVoiceKeepDays);
    s.writeS32(47, m_gscVoiceKeepMin);
    s.writeBool(48, m_flexRoaming);
    s.writeS32(49, m_ssidLocalId);
    s.writeS32(50, m_ssidCoverageZone);
    s.writeS32(51, m_ssidCountryCode);
    s.writeS32(52, m_ssidTrafficFlags);
    s.writeBool(53, m_flexTime);
    s.writeBool(54, m_flexTimeFrameZero);
    s.writeS32(55, m_flexTimeZoneMinutes);
    s.writeBool(56, m_flexTimeSendZone);
    s.writeBool(57, m_flexTimeDaylightSaving);
    s.writeS64(58, m_flexTimeOffsetSeconds);
    s.writeBool(59, m_flexNumbering);
    s.writeS32(60, m_flexMaxMessageNumber);
    s.writeBlob(61, m_messageColumns);
    s.writeString(62, m_voicePlaybackDeviceName);
    writeCharset(s, 63, m_pocsagCharset);
    writeCharset(s, 66, m_flexCharset);
    writeCharset(s, 69, m_gscCharset);
    s.writeBool(72, m_flexTimeEquivalentYear);
    s.writeS32(73, m_flexTimeExtendedSeconds);
    s.writeS32(74, m_maxQueued);

    return s.final();
}

bool RadioPagingModSettings::deserialize(const QByteArray& data)
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
        int itmp;

        d.readS64(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &itmp, (int) Pocsag);
        m_protocol = ((itmp >= (int) Pocsag) && (itmp <= (int) Gsc)) ? (Protocol) itmp : Pocsag;
        d.readS32(3, &itmp, (int) OneShot);
        m_mode = ((itmp >= (int) OneShot) && (itmp <= (int) Network)) ? (Mode) itmp : OneShot;
        d.readFloat(4, &m_deviation, 4500.0f);
        d.readFloat(5, &m_gain, 0.0f);
        d.readBool(6, &m_channelMute, false);
        d.readFloat(7, &m_rampUpMs, 5.0f);
        d.readFloat(8, &m_rampDownMs, 5.0f);
        d.readS32(9, &m_rampRange, 60);
        d.readS32(10, &m_pocsagSpeed, 1200);

        if (!isPocsagSpeed(m_pocsagSpeed)) {
            m_pocsagSpeed = 1200;
        }

        d.readBool(11, &m_pocsagInverted, false);
        d.readS32(12, &m_flexMode, (int) RadioPagingFlex::Mode1600_2);
        d.readBool(13, &m_flexInverted, false);
        d.readS32(14, &m_flexCollapse, 0);

        d.readU32(20, &m_rgbColor, QColor(255, 170, 0).rgb());
        d.readString(21, &m_title, "Radio Paging Modulator");

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(23, &m_streamIndex, 0);
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(28, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(29, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(30, &m_workspaceIndex, 0);
        d.readBlob(31, &m_geometryBytes);
        d.readBool(32, &m_hidden, false);
        d.readBool(33, &m_flexMixPocsag, false);
        d.readFloat(34, &m_pocsagDeviation, 4500.0f);
        d.readS32(35, &m_mixFramesPerMinute, 0);
        d.readBool(36, &m_mixShared, false);
        d.readS32(37, &m_mixSwitchMs[0], 400);
        d.readS32(38, &m_mixSwitchMs[1], 200);
        d.readS32(39, &m_mixSwitchMs[2], 100);
        d.readS32(40, &m_gscMode, (int) RadioPagingGsc::Auto);
        d.readBool(41, &m_gscInverted, false);
        d.readS32(42, &m_gscVoiceSeconds, 30);
        d.readString(43, &m_audioInputDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readString(45, &m_gscVoiceDirectory, "");
        d.readS32(46, &m_gscVoiceKeepDays, 3);
        d.readS32(47, &m_gscVoiceKeepMin, 50);
        d.readBool(48, &m_flexRoaming, false);
        d.readS32(49, &m_ssidLocalId, 0);
        d.readS32(50, &m_ssidCoverageZone, 0);
        d.readS32(51, &m_ssidCountryCode, 0);
        d.readS32(52, &m_ssidTrafficFlags, 0xf);
        d.readBool(53, &m_flexTime, false);
        d.readBool(54, &m_flexTimeFrameZero, true);
        d.readS32(55, &m_flexTimeZoneMinutes, 0);
        d.readBool(56, &m_flexTimeSendZone, true);
        d.readBool(57, &m_flexTimeDaylightSaving, false);
        d.readS64(58, &m_flexTimeOffsetSeconds, 0);
        d.readBool(59, &m_flexNumbering, true);
        d.readS32(60, &m_flexMaxMessageNumber, 63);
        d.readBlob(61, &m_messageColumns);
        d.readString(62, &m_voicePlaybackDeviceName, AudioDeviceManager::m_defaultDeviceName);
        m_pocsagCharset = readCharset(d, 63, "pocsag");
        m_flexCharset = readCharset(d, 66, "flex");
        m_gscCharset = readCharset(d, 69, "gsc");
        d.readBool(72, &m_flexTimeEquivalentYear, true);
        d.readS32(73, &m_flexTimeExtendedSeconds, 0);
        d.readS32(74, &m_maxQueued, 10000);
        m_gscVoiceKeepDays = std::max(0, std::min(3650, m_gscVoiceKeepDays));
        m_gscVoiceKeepMin = std::max(0, std::min(100000, m_gscVoiceKeepMin));

        validate();

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RadioPagingModSettings::applySettings(const QStringList& settingsKeys, const RadioPagingModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("protocol")) {
        m_protocol = settings.m_protocol;
    }
    if (settingsKeys.contains("mode")) {
        m_mode = settings.m_mode;
    }
    if (settingsKeys.contains("deviation")) {
        m_deviation = settings.m_deviation;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("rampUpMs")) {
        m_rampUpMs = settings.m_rampUpMs;
    }
    if (settingsKeys.contains("rampDownMs")) {
        m_rampDownMs = settings.m_rampDownMs;
    }
    if (settingsKeys.contains("rampRange")) {
        m_rampRange = settings.m_rampRange;
    }
    if (settingsKeys.contains("pocsagSpeed")) {
        m_pocsagSpeed = settings.m_pocsagSpeed;
    }
    if (settingsKeys.contains("pocsagInverted")) {
        m_pocsagInverted = settings.m_pocsagInverted;
    }
    if (settingsKeys.contains("flexMode")) {
        m_flexMode = settings.m_flexMode;
    }
    if (settingsKeys.contains("flexInverted")) {
        m_flexInverted = settings.m_flexInverted;
    }
    if (settingsKeys.contains("flexCollapse")) {
        m_flexCollapse = settings.m_flexCollapse;
    }
    if (settingsKeys.contains("flexRoaming")) {
        m_flexRoaming = settings.m_flexRoaming;
    }
    if (settingsKeys.contains("ssidLocalId")) {
        m_ssidLocalId = settings.m_ssidLocalId;
    }
    if (settingsKeys.contains("ssidCoverageZone")) {
        m_ssidCoverageZone = settings.m_ssidCoverageZone;
    }
    if (settingsKeys.contains("ssidCountryCode")) {
        m_ssidCountryCode = settings.m_ssidCountryCode;
    }
    if (settingsKeys.contains("ssidTrafficFlags")) {
        m_ssidTrafficFlags = settings.m_ssidTrafficFlags;
    }
    if (settingsKeys.contains("flexTime")) {
        m_flexTime = settings.m_flexTime;
    }
    if (settingsKeys.contains("flexTimeFrameZero")) {
        m_flexTimeFrameZero = settings.m_flexTimeFrameZero;
    }
    if (settingsKeys.contains("flexTimeZoneMinutes")) {
        m_flexTimeZoneMinutes = settings.m_flexTimeZoneMinutes;
    }
    if (settingsKeys.contains("flexTimeSendZone")) {
        m_flexTimeSendZone = settings.m_flexTimeSendZone;
    }
    if (settingsKeys.contains("flexTimeDaylightSaving")) {
        m_flexTimeDaylightSaving = settings.m_flexTimeDaylightSaving;
    }
    if (settingsKeys.contains("flexTimeOffsetSeconds")) {
        m_flexTimeOffsetSeconds = settings.m_flexTimeOffsetSeconds;
    }
    if (settingsKeys.contains("flexTimeEquivalentYear")) {
        m_flexTimeEquivalentYear = settings.m_flexTimeEquivalentYear;
    }
    if (settingsKeys.contains("flexTimeExtendedSeconds")) {
        m_flexTimeExtendedSeconds = settings.m_flexTimeExtendedSeconds;
    }
    if (settingsKeys.contains("maxQueued")) {
        m_maxQueued = settings.m_maxQueued;
    }
    if (settingsKeys.contains("flexNumbering")) {
        m_flexNumbering = settings.m_flexNumbering;
    }
    if (settingsKeys.contains("flexMaxMessageNumber")) {
        m_flexMaxMessageNumber = settings.m_flexMaxMessageNumber;
    }
    if (settingsKeys.contains("gscMode")) {
        m_gscMode = settings.m_gscMode;
    }
    if (settingsKeys.contains("gscInverted")) {
        m_gscInverted = settings.m_gscInverted;
    }
    if (settingsKeys.contains("gscVoiceSeconds")) {
        m_gscVoiceSeconds = settings.m_gscVoiceSeconds;
    }
    if (settingsKeys.contains("audioInputDeviceName")) {
        m_audioInputDeviceName = settings.m_audioInputDeviceName;
    }
    if (settingsKeys.contains("gscVoiceDirectory")) {
        m_gscVoiceDirectory = settings.m_gscVoiceDirectory;
    }
    if (settingsKeys.contains("gscVoiceKeepDays")) {
        m_gscVoiceKeepDays = settings.m_gscVoiceKeepDays;
    }
    if (settingsKeys.contains("gscVoiceKeepMin")) {
        m_gscVoiceKeepMin = settings.m_gscVoiceKeepMin;
    }
    if (settingsKeys.contains("mixPocsag")) {
        m_flexMixPocsag = settings.m_flexMixPocsag;
    }
    if (settingsKeys.contains("pocsagDeviation")) {
        m_pocsagDeviation = settings.m_pocsagDeviation;
    }
    if (settingsKeys.contains("mixFramesPerMinute")) {
        m_mixFramesPerMinute = settings.m_mixFramesPerMinute;
    }
    if (settingsKeys.contains("mixShared")) {
        m_mixShared = settings.m_mixShared;
    }
    if (settingsKeys.contains("mixSwitchMs512")) {
        m_mixSwitchMs[0] = settings.m_mixSwitchMs[0];
    }
    if (settingsKeys.contains("mixSwitchMs1200")) {
        m_mixSwitchMs[1] = settings.m_mixSwitchMs[1];
    }
    if (settingsKeys.contains("mixSwitchMs2400")) {
        m_mixSwitchMs[2] = settings.m_mixSwitchMs[2];
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
    if (settingsKeys.contains("messageColumns")) {
        m_messageColumns = settings.m_messageColumns;
    }
    if (settingsKeys.contains("voicePlaybackDeviceName")) {
        m_voicePlaybackDeviceName = settings.m_voicePlaybackDeviceName;
    }
    if (settingsKeys.contains("pocsagCharset")) {
        m_pocsagCharset = settings.m_pocsagCharset;
    }
    if (settingsKeys.contains("flexCharset")) {
        m_flexCharset = settings.m_flexCharset;
    }
    if (settingsKeys.contains("gscCharset")) {
        m_gscCharset = settings.m_gscCharset;
    }
}

QString RadioPagingModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("protocol") || force) {
        ostr << " m_protocol: " << (int) m_protocol;
    }
    if (settingsKeys.contains("mode") || force) {
        ostr << " m_mode: " << (int) m_mode;
    }
    if (settingsKeys.contains("deviation") || force) {
        ostr << " m_deviation: " << m_deviation;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("rampUpMs") || force) {
        ostr << " m_rampUpMs: " << m_rampUpMs;
    }
    if (settingsKeys.contains("rampDownMs") || force) {
        ostr << " m_rampDownMs: " << m_rampDownMs;
    }
    if (settingsKeys.contains("rampRange") || force) {
        ostr << " m_rampRange: " << m_rampRange;
    }
    if (settingsKeys.contains("pocsagSpeed") || force) {
        ostr << " m_pocsagSpeed: " << m_pocsagSpeed;
    }
    if (settingsKeys.contains("pocsagInverted") || force) {
        ostr << " m_pocsagInverted: " << m_pocsagInverted;
    }
    if (settingsKeys.contains("flexMode") || force) {
        ostr << " m_flexMode: " << m_flexMode;
    }
    if (settingsKeys.contains("flexInverted") || force) {
        ostr << " m_flexInverted: " << m_flexInverted;
    }
    if (settingsKeys.contains("flexCollapse") || force) {
        ostr << " m_flexCollapse: " << m_flexCollapse;
    }
    if (settingsKeys.contains("flexRoaming") || force) {
        ostr << " m_flexRoaming: " << m_flexRoaming;
    }
    if (settingsKeys.contains("ssidLocalId") || force) {
        ostr << " m_ssidLocalId: " << m_ssidLocalId;
    }
    if (settingsKeys.contains("ssidCoverageZone") || force) {
        ostr << " m_ssidCoverageZone: " << m_ssidCoverageZone;
    }
    if (settingsKeys.contains("ssidCountryCode") || force) {
        ostr << " m_ssidCountryCode: " << m_ssidCountryCode;
    }
    if (settingsKeys.contains("ssidTrafficFlags") || force) {
        ostr << " m_ssidTrafficFlags: " << m_ssidTrafficFlags;
    }
    if (settingsKeys.contains("flexTime") || force) {
        ostr << " m_flexTime: " << m_flexTime;
    }
    if (settingsKeys.contains("flexTimeFrameZero") || force) {
        ostr << " m_flexTimeFrameZero: " << m_flexTimeFrameZero;
    }
    if (settingsKeys.contains("flexTimeZoneMinutes") || force) {
        ostr << " m_flexTimeZoneMinutes: " << m_flexTimeZoneMinutes;
    }
    if (settingsKeys.contains("flexTimeSendZone") || force) {
        ostr << " m_flexTimeSendZone: " << m_flexTimeSendZone;
    }
    if (settingsKeys.contains("flexTimeDaylightSaving") || force) {
        ostr << " m_flexTimeDaylightSaving: " << m_flexTimeDaylightSaving;
    }
    if (settingsKeys.contains("flexTimeOffsetSeconds") || force) {
        ostr << " m_flexTimeOffsetSeconds: " << m_flexTimeOffsetSeconds;
    }
    if (settingsKeys.contains("flexTimeEquivalentYear") || force) {
        ostr << " m_flexTimeEquivalentYear: " << m_flexTimeEquivalentYear;
    }
    if (settingsKeys.contains("flexTimeExtendedSeconds") || force) {
        ostr << " m_flexTimeExtendedSeconds: " << m_flexTimeExtendedSeconds;
    }
    if (settingsKeys.contains("maxQueued") || force) {
        ostr << " m_maxQueued: " << m_maxQueued;
    }
    if (settingsKeys.contains("flexNumbering") || force) {
        ostr << " m_flexNumbering: " << m_flexNumbering;
    }
    if (settingsKeys.contains("flexMaxMessageNumber") || force) {
        ostr << " m_flexMaxMessageNumber: " << m_flexMaxMessageNumber;
    }
    if (settingsKeys.contains("gscMode") || force) {
        ostr << " m_gscMode: " << m_gscMode;
    }
    if (settingsKeys.contains("gscInverted") || force) {
        ostr << " m_gscInverted: " << m_gscInverted;
    }
    if (settingsKeys.contains("gscVoiceSeconds") || force) {
        ostr << " m_gscVoiceSeconds: " << m_gscVoiceSeconds;
    }
    if (settingsKeys.contains("audioInputDeviceName") || force) {
        ostr << " m_audioInputDeviceName: " << m_audioInputDeviceName.toStdString();
    }
    if (settingsKeys.contains("gscVoiceDirectory") || force) {
        ostr << " m_gscVoiceDirectory: " << m_gscVoiceDirectory.toStdString();
    }
    if (settingsKeys.contains("gscVoiceKeepDays") || force) {
        ostr << " m_gscVoiceKeepDays: " << m_gscVoiceKeepDays;
    }
    if (settingsKeys.contains("gscVoiceKeepMin") || force) {
        ostr << " m_gscVoiceKeepMin: " << m_gscVoiceKeepMin;
    }
    if (settingsKeys.contains("mixPocsag") || force) {
        ostr << " m_flexMixPocsag: " << m_flexMixPocsag;
    }
    if (settingsKeys.contains("pocsagDeviation") || force) {
        ostr << " m_pocsagDeviation: " << m_pocsagDeviation;
    }
    if (settingsKeys.contains("mixFramesPerMinute") || force) {
        ostr << " m_mixFramesPerMinute: " << m_mixFramesPerMinute;
    }
    if (settingsKeys.contains("mixShared") || force) {
        ostr << " m_mixShared: " << m_mixShared;
    }
    if (settingsKeys.contains("mixSwitchMs512") || force) {
        ostr << " m_mixSwitchMs512: " << m_mixSwitchMs[0];
    }
    if (settingsKeys.contains("mixSwitchMs1200") || force) {
        ostr << " m_mixSwitchMs1200: " << m_mixSwitchMs[1];
    }
    if (settingsKeys.contains("mixSwitchMs2400") || force) {
        ostr << " m_mixSwitchMs2400: " << m_mixSwitchMs[2];
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        ostr << " m_reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        ostr << " m_reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        ostr << " m_reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        ostr << " m_reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
