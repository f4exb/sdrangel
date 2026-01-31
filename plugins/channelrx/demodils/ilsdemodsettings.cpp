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

#include "audio/audiodevicemanager.h"
#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "ilsdemodsettings.h"

ILSDemodSettings::ILSDemodSettings() :
    m_channelMarker(nullptr),
    m_spectrumGUI(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ILSDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 15000.0f; // 15k to support offset carrier
    m_mode = LOC;
    m_frequencyIndex = 0;
    m_squelch = -60.0;
    m_volume = 2.0;
    m_audioMute = false;
    m_average = false;
    m_ddmUnits = FULL_SCALE;
    m_identThreshold = 4.0f;
    m_ident = "";
    m_runway = "";
    m_trueBearing = 0.0f;
    m_slope = 0.0f;
    m_latitude = "";
    m_longitude = "";
    m_elevation = 0;
    m_glidePath = 3.0f;
    m_refHeight = 15.25;
    m_courseWidth = 4.0f;
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "ils_log.csv";
    m_logEnabled = false;
    m_scopeCh1 = 0;
    m_scopeCh2 = 1;

    m_rgbColor = QColor(0, 205, 200).rgb();
    m_title = "ILS Demodulator";
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ILSDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeS32(3, (int) m_mode);
    s.writeS32(4, m_frequencyIndex);
    s.writeS32(5, m_squelch);
    s.writeFloat(6, m_volume);
    s.writeBool(7, m_audioMute);
    s.writeBool(8, m_average);
    s.writeS32(9, (int) m_ddmUnits);
    s.writeFloat(10, m_identThreshold);
    s.writeString(11, m_ident);
    s.writeString(12, m_runway);
    s.writeFloat(13, m_trueBearing);
    s.writeFloat(14, m_slope);
    s.writeString(15, m_latitude);
    s.writeString(16, m_longitude);
    s.writeS32(17, m_elevation);
    s.writeFloat(18, m_glidePath);
    s.writeFloat(19, m_refHeight);
    s.writeFloat(20, m_courseWidth);
    s.writeBool(21, m_udpEnabled);
    s.writeString(22, m_udpAddress);
    s.writeU32(23, m_udpPort);
    s.writeString(24, m_logFilename);
    s.writeBool(25, m_logEnabled);
    s.writeS32(26, m_scopeCh1);
    s.writeS32(27, m_scopeCh2);

    s.writeU32(40, m_rgbColor);
    s.writeString(41, m_title);
    if (m_channelMarker) {
        s.writeBlob(42, m_channelMarker->serialize());
    }
    s.writeString(43, m_audioDeviceName);
    s.writeS32(44, m_streamIndex);
    s.writeBool(45, m_useReverseAPI);
    s.writeString(46, m_reverseAPIAddress);
    s.writeU32(47, m_reverseAPIPort);
    s.writeU32(48, m_reverseAPIDeviceIndex);
    s.writeU32(49, m_reverseAPIChannelIndex);
    if (m_spectrumGUI) {
        s.writeBlob(50, m_spectrumGUI->serialize());
    }
    if (m_scopeGUI) {
        s.writeBlob(51, m_scopeGUI->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(52, m_rollupState->serialize());
    }
    s.writeS32(53, m_workspaceIndex);
    s.writeBlob(54, m_geometryBytes);
    s.writeBool(55, m_hidden);

    return s.final();
}

bool ILSDemodSettings::deserialize(const QByteArray& data)
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
        uint32_t utmp;
        QString strtmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readFloat(2, &m_rfBandwidth, 15000.0f);
        d.readS32(3, (int *) &m_mode, (int) LOC);
        d.readS32(4, &m_frequencyIndex, 0);
        d.readS32(5, &m_squelch, -40);
        d.readFloat(6, &m_volume, 2.0f);
        d.readBool(7, &m_audioMute, false);
        d.readBool(8, &m_average, false);
        d.readS32(9, (int *) &m_ddmUnits, (int) FULL_SCALE);
        d.readFloat(10, &m_identThreshold, 4.0f);
        d.readString(11, &m_ident, "");
        d.readString(12, &m_runway, "");
        d.readFloat(13, &m_trueBearing, 0.0f);
        d.readFloat(14, &m_slope, 0.0f);
        d.readString(15, &m_latitude, "");
        d.readString(16, &m_longitude, "");
        d.readS32(17, &m_elevation, 0);
        d.readFloat(18, &m_glidePath, 30.f);
        d.readFloat(19, &m_refHeight, 15.25f);
        d.readFloat(20, &m_courseWidth, 4.0f);

        d.readBool(21, &m_udpEnabled);
        d.readString(22, &m_udpAddress);
        d.readU32(23, &utmp);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }
        d.readString(24, &m_logFilename, "ils_log.csv");
        d.readBool(25, &m_logEnabled, false);
        d.readS32(26, &m_scopeCh1, 0);
        d.readS32(27, &m_scopeCh2, 0);

        d.readU32(40, &m_rgbColor, QColor(0, 205, 200).rgb());
        d.readString(41, &m_title, "ILS Demodulator");
        if (m_channelMarker)
        {
            d.readBlob(42, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }
        d.readString(43, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
        d.readS32(44, &m_streamIndex, 0);
        d.readBool(45, &m_useReverseAPI, false);
        d.readString(46, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(47, &utmp, 0);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }
        d.readU32(48, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(49, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;
        if (m_spectrumGUI)
        {
            d.readBlob(50, &bytetmp);
            m_spectrumGUI->deserialize(bytetmp);
        }
        if (m_scopeGUI)
        {
            d.readBlob(51, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }
        if (m_rollupState)
        {
            d.readBlob(52, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }
        d.readS32(53, &m_workspaceIndex, 0);
        d.readBlob(54, &m_geometryBytes);
        d.readBool(55, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void ILSDemodSettings::applySettings(const QStringList& settingsKeys, const ILSDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("mode")) {
        m_mode = settings.m_mode;
    }
    if (settingsKeys.contains("frequencyIndex")) {
        m_frequencyIndex = settings.m_frequencyIndex;
    }
    if (settingsKeys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (settingsKeys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("average")) {
        m_average = settings.m_average;
    }
    if (settingsKeys.contains("ddmUnits")) {
        m_ddmUnits = settings.m_ddmUnits;
    }
    if (settingsKeys.contains("identThreshold")) {
        m_identThreshold = settings.m_identThreshold;
    }
    if (settingsKeys.contains("ident")) {
        m_ident = settings.m_ident;
    }
    if (settingsKeys.contains("runway")) {
        m_runway = settings.m_runway;
    }
    if (settingsKeys.contains("trueBearing")) {
        m_trueBearing = settings.m_trueBearing;
    }
    if (settingsKeys.contains("slope")) {
        m_slope = settings.m_slope;
    }
    if (settingsKeys.contains("latitude")) {
        m_latitude = settings.m_latitude;
    }
    if (settingsKeys.contains("longitude")) {
        m_longitude = settings.m_longitude;
    }
    if (settingsKeys.contains("elevation")) {
        m_elevation = settings.m_elevation;
    }
    if (settingsKeys.contains("glidePath")) {
        m_glidePath = settings.m_glidePath;
    }
    if (settingsKeys.contains("refHeight")) {
        m_refHeight = settings.m_refHeight;
    }
    if (settingsKeys.contains("courseWidth")) {
        m_courseWidth = settings.m_courseWidth;
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
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("scopeCh1")) {
        m_scopeCh1 = settings.m_scopeCh1;
    }
    if (settingsKeys.contains("scopeCh2")) {
        m_scopeCh2 = settings.m_scopeCh2;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
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
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString ILSDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("mode") || force) {
        ostr << " m_mode: " << m_mode;
    }
    if (settingsKeys.contains("frequencyIndex") || force) {
        ostr << " m_frequencyIndex: " << m_frequencyIndex;
    }
    if (settingsKeys.contains("squelch") || force) {
        ostr << " m_squelch: " << m_squelch;
    }
    if (settingsKeys.contains("volume") || force) {
        ostr << " m_volume: " << m_volume;
    }
    if (settingsKeys.contains("audioMute") || force) {
        ostr << " m_audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("average") || force) {
        ostr << " m_average: " << m_average;
    }
    if (settingsKeys.contains("ddmUnits") || force) {
        ostr << " m_ddmUnits: " << m_ddmUnits;
    }
    if (settingsKeys.contains("identThreshold") || force) {
        ostr << " m_identThreshold: " << m_identThreshold;
    }
    if (settingsKeys.contains("ident") || force) {
        ostr << " m_ident: " << m_ident.toStdString();
    }
    if (settingsKeys.contains("runway") || force) {
        ostr << " m_runway: " << m_runway.toStdString();
    }
    if (settingsKeys.contains("trueBearing") || force) {
        ostr << " m_trueBearing: " << m_trueBearing;
    }
    if (settingsKeys.contains("slope") || force) {
        ostr << " m_slope: " << m_slope;
    }
    if (settingsKeys.contains("latitude") || force) {
        ostr << " m_latitude: " << m_latitude.toStdString();
    }
    if (settingsKeys.contains("longitude") || force) {
        ostr << " m_longitude: " << m_longitude.toStdString();
    }
    if (settingsKeys.contains("elevation") || force) {
        ostr << " m_elevation: " << m_elevation;
    }
    if (settingsKeys.contains("glidePath") || force) {
        ostr << " m_glidePath: " << m_glidePath;
    }
    if (settingsKeys.contains("refHeight") || force) {
        ostr << " m_refHeight: " << m_refHeight;
    }
    if (settingsKeys.contains("courseWidth") || force) {
        ostr << " m_courseWidth: " << m_courseWidth;
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
    if (settingsKeys.contains("logFilename") || force) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("logEnabled") || force) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("scopeCh1") || force) {
        ostr << " m_scopeCh1: " << m_scopeCh1;
    }
    if (settingsKeys.contains("scopeCh2") || force) {
        ostr << " m_scopeCh2: " << m_scopeCh2;
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        ostr << " m_audioDeviceName: " << m_audioDeviceName.toStdString();
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
