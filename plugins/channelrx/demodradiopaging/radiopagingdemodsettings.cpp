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
#include <QDataStream>
#include <QDebug>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>
#include <sstream>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "audio/audiodevicemanager.h"
#include "radiopagingvoicestore.h"
#include "radiopagingcharset.h"
#include "radiopagingpocsag.h"
#include "radiopagingdemodsettings.h"

const int RadioPagingDemodSettings::m_nbPocsagSpeeds;
const int RadioPagingDemodSettings::m_pocsagSpeeds[m_nbPocsagSpeeds] = { 512, 1200, 2400 };
const int RadioPagingDemodSettings::m_channelSampleRate;
// Leg configuration, fixed (the sink test compares others): carrier offset room on each side
// of a leg
double RadioPagingDemodSettings::m_legOffsetAllowance = 2000.0;

namespace {

// Highest modulation rate of the legs (Carson bandwidth), bits per second
const int flexMaxRate = 3200;
const int gscRate = 600;


} // namespace

QString RadioPagingDemodSettings::voiceDirectory() const
{
    if (!m_gscVoiceDirectory.isEmpty()) {
        return m_gscVoiceDirectory;
    }

    return RadioPagingVoiceStore::defaultDirectory("rx");
}

QString RadioPagingDemodSettings::channelName() const
{
    return m_title;
}

RadioPagingCharset::Key RadioPagingDemodSettings::defaultCharset(const char *protocol)
{
    const RadioPagingCharset::Codepage *codepage = RadioPagingCharset::defaultFor(protocol);
    return codepage ? codepage->key() : RadioPagingCharset::Key();
}

std::vector<uint8_t> RadioPagingDemodSettings::pocsagFillCodes(const RadioPagingCharset::Codepage& codepage) const
{
    std::vector<uint8_t> codes;

    switch (m_pocsagFill)
    {
    case FillConservative:
        return RadioPagingPocsag::fillConservative();
    case FillCodepage:
        for (int code = 0; code < codepage.codes(); code++)
        {
            if (!codepage.glyph(code)) {
                codes.push_back((uint8_t) code);
            }
        }
        return codes;
    case FillCustom:
        parseFillCodes(m_pocsagFillCodes, codes);
        return codes;
    default:
        return RadioPagingPocsag::fillMinimal();
    }
}

bool RadioPagingDemodSettings::parseFillCodes(const QString& text, std::vector<uint8_t>& codes)
{
    codes.clear();

    for (const QString& token : text.split(QRegularExpression("[,;\\s]+"), Qt::SkipEmptyParts))
    {
        bool ok;
        QString hex = token;

        if (hex.startsWith("0x", Qt::CaseInsensitive)) {
            hex = hex.mid(2);
        }

        uint value = hex.toUInt(&ok, 16);

        if (!ok || (value > 0x7f) || (hex.size() > 2)) {
            return false;
        }

        codes.push_back((uint8_t) value);
    }

    return true;
}

namespace {

void writeCharset(SimpleSerializer& s, int tag, const RadioPagingCharset::Key& key)
{
    s.writeString(tag, QString::fromStdString(key.m_vendor));
    s.writeString(tag + 1, QString::fromStdString(key.m_model));
    s.writeString(tag + 2, QString::fromStdString(key.m_codepage));
}

RadioPagingCharset::Key readCharset(SimpleDeserializer& d, int tag, const char *protocol)
{
    const RadioPagingCharset::Key def = RadioPagingDemodSettings::defaultCharset(protocol);
    QString vendor, model, codepage;
    d.readString(tag, &vendor, QString::fromStdString(def.m_vendor));
    d.readString(tag + 1, &model, QString::fromStdString(def.m_model));
    d.readString(tag + 2, &codepage, QString::fromStdString(def.m_codepage));
    return RadioPagingCharset::Key{vendor.toStdString(), model.toStdString(), codepage.toStdString()};
}

std::string charsetText(const RadioPagingCharset::Key& key)
{
    return key.m_vendor + "/" + key.m_model + "/" + key.m_codepage;
}

} // namespace

RadioPagingDemodSettings::RadioPagingDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr),
    m_scopeGUI(nullptr)
{
    resetToDefaults();
}

void RadioPagingDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 20000.0f;           // FLEX 16 kHz and room for the oscillator offsets
    m_pocsagEnabled = true;
    m_pocsagDeviation = 4500.0f;
    m_flexEnabled = true;
    m_flexDeviation = 4800.0f;
    m_gscEnabled = true;
    m_gscDeviation = 4500.0f;
    m_gscVoiceTimeout = 50;
    m_gscAudio = false;
    m_gscVoiceRecord = true;
    m_gscVoiceDirectory = "";
    m_gscVoiceKeepDays = 3;
    m_gscVoiceKeepMin = 50;
    m_flexMaxMessageNumber = 63;
    m_volume = 1.0f;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_pocsagCharset = defaultCharset("pocsag");
    m_flexCharset = defaultCharset("flex");
    m_gscCharset = defaultCharset("gsc");
    m_pocsagFill = FillConservative;
    m_pocsagFillCodes = "00 03 04";
    m_pocsagCustomCodes.clear();
    m_flexCustomCodes.clear();
    m_gscCustomCodes.clear();
    m_pocsagDcCorrection = true;
    m_flexDcCorrection = true;
    m_gscDcCorrection = true;

    m_rgbColor = QColor(255, 170, 0).rgb();
    m_title = "Radio Paging Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
    m_messageColumns.clear();
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_udpFormat = UdpPagerDemod;
    m_logEnabled = false;
    m_logFilename = "radiopaging_log.csv";
    m_notificationSettings.clear();
    m_scopeCh1 = 3;                     // POCSAG FM and FLEX FM (RadioPagingDemodSink::ScopeSignal)
    m_scopeCh2 = 4;
}

// Tags 12 to 20 and 27 to 30 are the usual channel tags, 21 the audio device. New tags are only appended
QByteArray RadioPagingDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeBool(3, m_pocsagEnabled);
    s.writeFloat(4, m_pocsagDeviation);
    s.writeBool(5, m_flexEnabled);
    s.writeFloat(6, m_flexDeviation);
    s.writeBool(7, m_gscEnabled);
    s.writeFloat(8, m_gscDeviation);
    s.writeS32(9, m_gscVoiceTimeout);
    s.writeBool(10, m_gscAudio);
    s.writeFloat(11, m_volume);
    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);

    if (m_channelMarker) {
        s.writeBlob(14, m_channelMarker->serialize());
    }

    s.writeS32(15, m_streamIndex);
    s.writeBool(16, m_useReverseAPI);
    s.writeString(17, m_reverseAPIAddress);
    s.writeU32(18, m_reverseAPIPort);
    s.writeU32(19, m_reverseAPIDeviceIndex);
    s.writeU32(20, m_reverseAPIChannelIndex);
    s.writeString(21, m_audioDeviceName);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);
    s.writeBool(31, m_gscVoiceRecord);
    s.writeString(32, m_gscVoiceDirectory);
    s.writeS32(33, m_gscVoiceKeepDays);
    s.writeS32(34, m_gscVoiceKeepMin);
    s.writeS32(36, m_flexMaxMessageNumber);
    s.writeBlob(37, m_messageColumns);
    writeCharset(s, 38, m_pocsagCharset);
    writeCharset(s, 41, m_flexCharset);
    writeCharset(s, 44, m_gscCharset);
    s.writeS32(47, m_pocsagFill);
    s.writeString(48, m_pocsagFillCodes);
    s.writeString(63, m_pocsagCustomCodes);
    s.writeString(64, m_flexCustomCodes);
    s.writeString(65, m_gscCustomCodes);
    s.writeBool(49, m_pocsagDcCorrection);
    s.writeBool(50, m_flexDcCorrection);
    s.writeBool(51, m_gscDcCorrection);
    s.writeBool(52, m_udpEnabled);
    s.writeString(53, m_udpAddress);
    s.writeU32(54, m_udpPort);
    s.writeS32(55, m_udpFormat);
    s.writeBool(56, m_logEnabled);
    s.writeString(57, m_logFilename);
    s.writeList(58, m_notificationSettings);
    s.writeS32(59, m_scopeCh1);
    s.writeS32(60, m_scopeCh2);

    if (m_scopeGUI) {
        s.writeBlob(61, m_scopeGUI->serialize());
    }

    return s.final();
}

bool RadioPagingDemodSettings::deserialize(const QByteArray& data)
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

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readFloat(2, &m_rfBandwidth, 20000.0f);
        d.readBool(3, &m_pocsagEnabled, true);
        d.readFloat(4, &m_pocsagDeviation, 4500.0f);
        d.readBool(5, &m_flexEnabled, true);
        d.readFloat(6, &m_flexDeviation, 4800.0f);
        d.readBool(7, &m_gscEnabled, true);
        d.readFloat(8, &m_gscDeviation, 4500.0f);
        d.readS32(9, &m_gscVoiceTimeout, 50);
        d.readBool(10, &m_gscAudio, false);
        d.readFloat(11, &m_volume, 1.0f);
        d.readU32(12, &m_rgbColor, QColor(255, 170, 0).rgb());
        d.readString(13, &m_title, "Radio Paging Demodulator");

        if (m_channelMarker)
        {
            d.readBlob(14, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(15, &m_streamIndex, 0);
        d.readBool(16, &m_useReverseAPI, false);
        d.readString(17, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(18, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(19, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(20, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readString(21, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);
        d.readBool(31, &m_gscVoiceRecord, true);
        d.readString(32, &m_gscVoiceDirectory, "");
        d.readS32(33, &m_gscVoiceKeepDays, 3);
        d.readS32(34, &m_gscVoiceKeepMin, 50);
        d.readS32(36, &m_flexMaxMessageNumber, 63);
        m_flexMaxMessageNumber = std::max(0, std::min(63, m_flexMaxMessageNumber));
        d.readBlob(37, &m_messageColumns);
        m_pocsagCharset = readCharset(d, 38, "pocsag");
        m_flexCharset = readCharset(d, 41, "flex");
        m_gscCharset = readCharset(d, 44, "gsc");
        d.readS32(47, &m_pocsagFill, FillConservative);
        m_pocsagFill = std::max((int) FillMinimal, std::min((int) FillCustom, m_pocsagFill));
        d.readString(48, &m_pocsagFillCodes, "00 03 04");
        d.readString(63, &m_pocsagCustomCodes, "");
        d.readString(64, &m_flexCustomCodes, "");
        d.readString(65, &m_gscCustomCodes, "");
        d.readBool(49, &m_pocsagDcCorrection, true);
        d.readBool(50, &m_flexDcCorrection, true);
        d.readBool(51, &m_gscDcCorrection, true);
        d.readBool(52, &m_udpEnabled, false);
        d.readString(53, &m_udpAddress, "127.0.0.1");
        d.readU32(54, &utmp, 9999);

        if ((utmp > 0) && (utmp < 65536)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        d.readS32(55, &m_udpFormat, UdpPagerDemod);
        m_udpFormat = std::max((int) UdpPagerDemod, std::min((int) UdpJson, m_udpFormat));
        d.readBool(56, &m_logEnabled, false);
        d.readString(57, &m_logFilename, "radiopaging_log.csv");
        d.readList(58, &m_notificationSettings);
        d.readS32(59, &m_scopeCh1, 3);
        d.readS32(60, &m_scopeCh2, 4);

        if (m_scopeGUI)
        {
            d.readBlob(61, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        m_gscVoiceKeepDays = std::max(0, std::min(3650, m_gscVoiceKeepDays));
        m_gscVoiceKeepMin = std::max(0, std::min(100000, m_gscVoiceKeepMin));

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RadioPagingDemodSettings::applySettings(const QStringList& settingsKeys, const RadioPagingDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("pocsagEnabled")) {
        m_pocsagEnabled = settings.m_pocsagEnabled;
    }
    if (settingsKeys.contains("pocsagDeviation")) {
        m_pocsagDeviation = settings.m_pocsagDeviation;
    }
    if (settingsKeys.contains("flexEnabled")) {
        m_flexEnabled = settings.m_flexEnabled;
    }
    if (settingsKeys.contains("flexDeviation")) {
        m_flexDeviation = settings.m_flexDeviation;
    }
    if (settingsKeys.contains("gscEnabled")) {
        m_gscEnabled = settings.m_gscEnabled;
    }
    if (settingsKeys.contains("gscDeviation")) {
        m_gscDeviation = settings.m_gscDeviation;
    }
    if (settingsKeys.contains("gscVoiceTimeout")) {
        m_gscVoiceTimeout = settings.m_gscVoiceTimeout;
    }
    if (settingsKeys.contains("gscAudio")) {
        m_gscAudio = settings.m_gscAudio;
    }
    if (settingsKeys.contains("gscVoiceRecord")) {
        m_gscVoiceRecord = settings.m_gscVoiceRecord;
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
    if (settingsKeys.contains("flexMaxMessageNumber")) {
        m_flexMaxMessageNumber = settings.m_flexMaxMessageNumber;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("pocsagCharset")) {
        m_pocsagCharset = settings.m_pocsagCharset;
    }
    if (settingsKeys.contains("pocsagDcCorrection")) {
        m_pocsagDcCorrection = settings.m_pocsagDcCorrection;
    }
    if (settingsKeys.contains("flexDcCorrection")) {
        m_flexDcCorrection = settings.m_flexDcCorrection;
    }
    if (settingsKeys.contains("gscDcCorrection")) {
        m_gscDcCorrection = settings.m_gscDcCorrection;
    }
    if (settingsKeys.contains("flexCharset")) {
        m_flexCharset = settings.m_flexCharset;
    }
    if (settingsKeys.contains("gscCharset")) {
        m_gscCharset = settings.m_gscCharset;
    }
    if (settingsKeys.contains("pocsagFill")) {
        m_pocsagFill = settings.m_pocsagFill;
    }
    if (settingsKeys.contains("pocsagFillCodes")) {
        m_pocsagFillCodes = settings.m_pocsagFillCodes;
    }
    if (settingsKeys.contains("pocsagCustomCodes")) {
        m_pocsagCustomCodes = settings.m_pocsagCustomCodes;
    }
    if (settingsKeys.contains("flexCustomCodes")) {
        m_flexCustomCodes = settings.m_flexCustomCodes;
    }
    if (settingsKeys.contains("gscCustomCodes")) {
        m_gscCustomCodes = settings.m_gscCustomCodes;
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
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("udpFormat")) {
        m_udpFormat = settings.m_udpFormat;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("notificationSettings")) {
        m_notificationSettings = settings.m_notificationSettings;
    }
    if (settingsKeys.contains("scopeCh1")) {
        m_scopeCh1 = settings.m_scopeCh1;
    }
    if (settingsKeys.contains("scopeCh2")) {
        m_scopeCh2 = settings.m_scopeCh2;
    }
}

QString RadioPagingDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("pocsagEnabled") || force) {
        ostr << " m_pocsagEnabled: " << m_pocsagEnabled;
    }
    if (settingsKeys.contains("pocsagDeviation") || force) {
        ostr << " m_pocsagDeviation: " << m_pocsagDeviation;
    }
    if (settingsKeys.contains("flexEnabled") || force) {
        ostr << " m_flexEnabled: " << m_flexEnabled;
    }
    if (settingsKeys.contains("flexDeviation") || force) {
        ostr << " m_flexDeviation: " << m_flexDeviation;
    }
    if (settingsKeys.contains("gscEnabled") || force) {
        ostr << " m_gscEnabled: " << m_gscEnabled;
    }
    if (settingsKeys.contains("gscDeviation") || force) {
        ostr << " m_gscDeviation: " << m_gscDeviation;
    }
    if (settingsKeys.contains("gscVoiceTimeout") || force) {
        ostr << " m_gscVoiceTimeout: " << m_gscVoiceTimeout;
    }
    if (settingsKeys.contains("gscAudio") || force) {
        ostr << " m_gscAudio: " << m_gscAudio;
    }
    if (settingsKeys.contains("gscVoiceRecord") || force) {
        ostr << " m_gscVoiceRecord: " << m_gscVoiceRecord;
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
    if (settingsKeys.contains("flexMaxMessageNumber") || force) {
        ostr << " m_flexMaxMessageNumber: " << m_flexMaxMessageNumber;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("pocsagCharset") || force) {
        ostr << " m_pocsagCharset: " << charsetText(m_pocsagCharset);
    }
    if (settingsKeys.contains("pocsagDcCorrection") || force) {
        ostr << " m_pocsagDcCorrection: " << m_pocsagDcCorrection;
    }
    if (settingsKeys.contains("flexDcCorrection") || force) {
        ostr << " m_flexDcCorrection: " << m_flexDcCorrection;
    }
    if (settingsKeys.contains("gscDcCorrection") || force) {
        ostr << " m_gscDcCorrection: " << m_gscDcCorrection;
    }
    if (settingsKeys.contains("flexCharset") || force) {
        ostr << " m_flexCharset: " << charsetText(m_flexCharset);
    }
    if (settingsKeys.contains("gscCharset") || force) {
        ostr << " m_gscCharset: " << charsetText(m_gscCharset);
    }
    if (settingsKeys.contains("pocsagFill") || force) {
        ostr << " m_pocsagFill: " << m_pocsagFill;
    }
    if (settingsKeys.contains("pocsagFillCodes") || force) {
        ostr << " m_pocsagFillCodes: " << m_pocsagFillCodes.toStdString();
    }
    if (settingsKeys.contains("pocsagCustomCodes") || force) {
        ostr << " m_pocsagCustomCodes: " << m_pocsagCustomCodes.toStdString();
    }
    if (settingsKeys.contains("flexCustomCodes") || force) {
        ostr << " m_flexCustomCodes: " << m_flexCustomCodes.toStdString();
    }
    if (settingsKeys.contains("gscCustomCodes") || force) {
        ostr << " m_gscCustomCodes: " << m_gscCustomCodes.toStdString();
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
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("udpFormat") || force) {
        ostr << " m_udpFormat: " << m_udpFormat;
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("notificationSettings") || force) {
        ostr << " m_notificationSettings: " << m_notificationSettings.size();
    }
    if (settingsKeys.contains("scopeCh1") || force) {
        ostr << " m_scopeCh1: " << m_scopeCh1;
    }
    if (settingsKeys.contains("scopeCh2") || force) {
        ostr << " m_scopeCh2: " << m_scopeCh2;
    }

    return QString(ostr.str().c_str());
}

double RadioPagingDemodSettings::pocsagCarson() const
{
    return 2.0 * (m_pocsagDeviation + m_pocsagSpeeds[m_nbPocsagSpeeds - 1]);
}

double RadioPagingDemodSettings::flexCarson() const
{
    return 2.0 * (m_flexDeviation + flexMaxRate);
}

double RadioPagingDemodSettings::gscCarson() const
{
    return 2.0 * (m_gscDeviation + gscRate);
}

// Hunting, the legs pass the Carson bandwidth and the carrier offset on each side: a signal
// off centre is not cut on one side, which would bias the measured DC offset and lose symbols
double RadioPagingDemodSettings::pocsagBandwidth() const
{
    // All speeds share the leg: sized to the fastest
    return 2.0 * (m_pocsagDeviation + m_pocsagSpeeds[m_nbPocsagSpeeds - 1] + m_legOffsetAllowance);
}

double RadioPagingDemodSettings::flexBandwidth() const
{
    return 2.0 * (m_flexDeviation + flexMaxRate + m_legOffsetAllowance);
}

double RadioPagingDemodSettings::gscBandwidth() const
{
    return 2.0 * (m_gscDeviation + gscRate + m_legOffsetAllowance);
}

RadioPagingDemodSettings::NotificationSettings::NotificationSettings() :
    m_matchField(MatchAddress),
    m_highlight(false),
    m_highlightColor(QColor(Qt::red).rgba()),
    m_plotOnMap(false)
{
}

void RadioPagingDemodSettings::NotificationSettings::updateRegularExpression()
{
    m_regularExpression.setPattern(m_regExp);
    m_regularExpression.optimize();

    if (!m_regularExpression.isValid()) {
        qDebug() << "RadioPagingDemodSettings::NotificationSettings: regular expression not valid:" << m_regExp;
    }
}

QByteArray RadioPagingDemodSettings::NotificationSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_matchField);
    s.writeString(2, m_regExp);
    s.writeString(3, m_speech);
    s.writeString(4, m_command);
    s.writeBool(5, m_highlight);
    s.writeS32(6, m_highlightColor);
    s.writeBool(7, m_plotOnMap);

    return s.final();
}

bool RadioPagingDemodSettings::NotificationSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid() || (d.getVersion() != 1)) {
        return false;
    }

    d.readS32(1, &m_matchField, MatchAddress);
    m_matchField = std::max((int) MatchAddress, std::min((int) MatchAny, m_matchField));
    d.readString(2, &m_regExp);
    d.readString(3, &m_speech);
    d.readString(4, &m_command);
    d.readBool(5, &m_highlight, false);
    d.readS32(6, &m_highlightColor, QColor(Qt::red).rgba());
    d.readBool(7, &m_plotOnMap, false);
    updateRegularExpression();

    return true;
}

QString RadioPagingDemodSettings::NotificationSettings::matchFieldName(int field)
{
    switch (field)
    {
    case MatchAddress:
        return "Address";
    case MatchMessage:
        return "Message";
    case MatchProtocol:
        return "Protocol";
    case MatchType:
        return "Type";
    case MatchChannel:
        return "Channel";
    case MatchAny:
        return "Any";
    default:
        return "";
    }
}

QDataStream& operator<<(QDataStream& out, const QSharedPointer<RadioPagingDemodSettings::NotificationSettings>& settings)
{
    out << settings->serialize();
    return out;
}

QDataStream& operator>>(QDataStream& in, QSharedPointer<RadioPagingDemodSettings::NotificationSettings>& settings)
{
    settings = QSharedPointer<RadioPagingDemodSettings::NotificationSettings>::create();
    QByteArray data;
    in >> data;
    settings->deserialize(data);
    return in;
}
