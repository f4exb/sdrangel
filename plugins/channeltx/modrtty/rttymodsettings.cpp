///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
#include <QDebug>

#include "util/baudot.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "rttymodsettings.h"

RttyModSettings::RttyModSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void RttyModSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_baud = 45.45;
    m_rfBandwidth = 340;
    m_frequencyShift = 170;
    m_gain = 0.0f;
    m_channelMute = false;
    m_repeat = false;
    m_repeatCount = 10;
    m_lpfTaps = 301;
    m_rfNoise = false;
    m_text = "CQ CQ CQ DE SDRangel CQ";
    m_characterSet = Baudot::ITA2;
    m_unshiftOnSpace = false;
    m_msbFirst = false;
    m_spaceHigh = false;
    m_prefixCRLF = true;
    m_postfixCRLF = true;
    m_predefinedTexts = QStringList({
        "CQ CQ CQ DE ${callsign} ${callsign} CQ",
        "DE ${callsign} ${callsign} ${callsign}",
        "UR 599 QTH IS ${location}",
        "TU DE ${callsign} CQ"
    });
    m_rgbColor = QColor(180, 205, 130).rgb();
    m_title = "RTTY Modulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_pulseShaping = false;
    m_beta = 1.0f;
    m_symbolSpan = 6;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9998;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QString RttyModSettings::getMode() const
{
    return QString("%1/%2").arg(m_baud).arg(m_frequencyShift);
}

QByteArray RttyModSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeReal(2, m_baud);
    s.writeS32(3, m_rfBandwidth);
    s.writeS32(4, m_frequencyShift);
    s.writeReal(5, m_gain);
    s.writeBool(6, m_channelMute);
    s.writeBool(7, m_repeat);
    s.writeS32(9, m_repeatCount);
    s.writeS32(23, m_lpfTaps);
    s.writeBool(25, m_rfNoise);
    s.writeString(30, m_text);

    s.writeS32(60, (int)m_characterSet);
    s.writeBool(61, m_unshiftOnSpace);
    s.writeBool(62, m_msbFirst);
    s.writeBool(63, m_spaceHigh);
    s.writeBool(64, m_prefixCRLF);
    s.writeBool(65, m_postfixCRLF);
    s.writeList(66, m_predefinedTexts);

    s.writeU32(31, m_rgbColor);
    s.writeString(32, m_title);

    if (m_channelMarker) {
        s.writeBlob(33, m_channelMarker->serialize());
    }

    s.writeS32(34, m_streamIndex);
    s.writeBool(35, m_useReverseAPI);
    s.writeString(36, m_reverseAPIAddress);
    s.writeU32(37, m_reverseAPIPort);
    s.writeU32(38, m_reverseAPIDeviceIndex);
    s.writeU32(39, m_reverseAPIChannelIndex);
    s.writeBool(46, m_pulseShaping);
    s.writeReal(47, m_beta);
    s.writeS32(48, m_symbolSpan);
    s.writeBool(51, m_udpEnabled);
    s.writeString(52, m_udpAddress);
    s.writeU32(53, m_udpPort);

    if (m_rollupState) {
        s.writeBlob(54, m_rollupState->serialize());
    }

    s.writeS32(55, m_workspaceIndex);
    s.writeBlob(56, m_geometryBytes);
    s.writeBool(57, m_hidden);


    return s.final();
}

bool RttyModSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        QByteArray bytetmp;
        qint32 tmp;
        uint32_t utmp;

        d.readS32(1, &tmp, 0);
        m_inputFrequencyOffset = tmp;
        d.readReal(2, &m_baud, 45.45f);
        d.readS32(3, &m_rfBandwidth, 340);
        d.readS32(4, &m_frequencyShift, 170);
        d.readReal(5, &m_gain, 0.0f);
        d.readBool(6, &m_channelMute, false);
        d.readBool(7, &m_repeat, false);
        d.readS32(9, &m_repeatCount, -1);
        d.readS32(23, &m_lpfTaps, 301);
        d.readBool(25, &m_rfNoise, false);
        d.readString(30, &m_text, "CQ CQ CQ anyone using SDRangel");

        d.readS32(60, (int*)&m_characterSet, (int)Baudot::ITA2);
        d.readBool(61, &m_unshiftOnSpace, false);
        d.readBool(62, &m_msbFirst, false);
        d.readBool(63, &m_spaceHigh, false);
        d.readBool(64, &m_prefixCRLF, true);
        d.readBool(65, &m_postfixCRLF, true);
        d.readList(66, &m_predefinedTexts);

        d.readU32(31, &m_rgbColor);
        d.readString(32, &m_title, "RTTY Modulator");

        if (m_channelMarker)
        {
            d.readBlob(33, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(34, &m_streamIndex, 0);
        d.readBool(35, &m_useReverseAPI, false);
        d.readString(36, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(37, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(38, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(39, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        d.readBool(46, &m_pulseShaping, false);
        d.readReal(47, &m_beta, 1.0f);
        d.readS32(48, &m_symbolSpan, 6);
        d.readBool(51, &m_udpEnabled);
        d.readString(52, &m_udpAddress, "127.0.0.1");
        d.readU32(53, &utmp);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9998;
        }

        if (m_rollupState)
        {
            d.readBlob(54, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(55, &m_workspaceIndex, 0);
        d.readBlob(56, &m_geometryBytes);
        d.readBool(57, &m_hidden, false);

        return true;
    }
    else
    {
        qDebug() << "RttyModSettings::deserialize: ERROR";
        resetToDefaults();
        return false;
    }
}

void RttyModSettings::applySettings(const QStringList& settingsKeys, const RttyModSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("baud")) {
        m_baud = settings.m_baud;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("frequencyShift")) {
        m_frequencyShift = settings.m_frequencyShift;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("channelMute")) {
        m_channelMute = settings.m_channelMute;
    }
    if (settingsKeys.contains("repeat")) {
        m_repeat = settings.m_repeat;
    }
    if (settingsKeys.contains("repeatCount")) {
        m_repeatCount = settings.m_repeatCount;
    }
    if (settingsKeys.contains("lpfTaps")) {
        m_lpfTaps = settings.m_lpfTaps;
    }
    if (settingsKeys.contains("rfNoise")) {
        m_rfNoise = settings.m_rfNoise;
    }
    if (settingsKeys.contains("writeToFile")) {
        m_writeToFile = settings.m_writeToFile;
    }
    if (settingsKeys.contains("text")) {
        m_text = settings.m_text;
    }
    if (settingsKeys.contains("pulseShaping")) {
        m_pulseShaping = settings.m_pulseShaping;
    }
    if (settingsKeys.contains("beta")) {
        m_beta = settings.m_beta;
    }
    if (settingsKeys.contains("symbolSpan")) {
        m_symbolSpan = settings.m_symbolSpan;
    }
    if (settingsKeys.contains("characterSet")) {
        m_characterSet = settings.m_characterSet;
    }
    if (settingsKeys.contains("unshiftOnSpace")) {
        m_unshiftOnSpace = settings.m_unshiftOnSpace;
    }
    if (settingsKeys.contains("msbFirst")) {
        m_msbFirst = settings.m_msbFirst;
    }
    if (settingsKeys.contains("spaceHigh")) {
        m_spaceHigh = settings.m_spaceHigh;
    }
    if (settingsKeys.contains("prefixCRLF")) {
        m_prefixCRLF = settings.m_prefixCRLF;
    }
    if (settingsKeys.contains("postfixCRLF")) {
        m_postfixCRLF = settings.m_postfixCRLF;
    }
    if (settingsKeys.contains("predefinedTexts")) {
        m_predefinedTexts = settings.m_predefinedTexts;
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
    if (settingsKeys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (settingsKeys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString RttyModSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("baud") || force) {
        ostr << " m_baud: " << m_baud;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("frequencyShift") || force) {
        ostr << " m_frequencyShift: " << m_frequencyShift;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("channelMute") || force) {
        ostr << " m_channelMute: " << m_channelMute;
    }
    if (settingsKeys.contains("repeat") || force) {
        ostr << " m_repeat: " << m_repeat;
    }
    if (settingsKeys.contains("repeatCount") || force) {
        ostr << " m_repeatCount: " << m_repeatCount;
    }
    if (settingsKeys.contains("lpfTaps") || force) {
        ostr << " m_lpfTaps: " << m_lpfTaps;
    }
    if (settingsKeys.contains("rfNoise") || force) {
        ostr << " m_rfNoise: " << m_rfNoise;
    }
    if (settingsKeys.contains("writeToFile") || force) {
        ostr << " m_writeToFile: " << m_writeToFile;
    }
    if (settingsKeys.contains("text") || force) {
        ostr << " m_text: " << m_text.toStdString();
    }
    if (settingsKeys.contains("pulseShaping") || force) {
        ostr << " m_pulseShaping: " << m_pulseShaping;
    }
    if (settingsKeys.contains("beta") || force) {
        ostr << " m_beta: " << m_beta;
    }
    if (settingsKeys.contains("symbolSpan") || force) {
        ostr << " m_symbolSpan: " << m_symbolSpan;
    }
    if (settingsKeys.contains("characterSet") || force) {
        ostr << " m_characterSet: " << (int)m_characterSet;
    }
    if (settingsKeys.contains("unshiftOnSpace") || force) {
        ostr << " m_unshiftOnSpace: " << m_unshiftOnSpace;
    }
    if (settingsKeys.contains("msbFirst") || force) {
        ostr << " m_msbFirst: " << m_msbFirst;
    }
    if (settingsKeys.contains("spaceHigh") || force) {
        ostr << " m_spaceHigh: " << m_spaceHigh;
    }
    if (settingsKeys.contains("prefixCRLF") || force) {
        ostr << " m_prefixCRLF: " << m_prefixCRLF;
    }
    if (settingsKeys.contains("postfixCRLF") || force) {
        ostr << " m_postfixCRLF: " << m_postfixCRLF;
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
    if (settingsKeys.contains("udpEnabled") || force) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress") || force) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort") || force) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
