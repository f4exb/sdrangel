///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2021-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
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

#include <algorithm>
#include <cmath>

#include "audio/audiodevicemanager.h"
#include "settings/serializable.h"
#include "util/simpleserializer.h"

#include "selcaldemodsettings.h"
#include "selcaldecoder.h"

SELCALDemodSettings::SELCALDemodSettings()
{
    resetToDefaults();
}

void SELCALDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 3000.0f;
    m_modulation = 0;
    m_toneFrequencyOffset = 0.0f;
    m_autoToneFrequencyOffset = true;
    m_threshold = SELCALDecoder::m_defaultThreshold;
    m_volume = 1.0f;
    m_audioMute = false;
    m_squelch = -80;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_filter = QString();
    m_udpEnabled = false;
    m_udpAddress = QStringLiteral("127.0.0.1");
    m_udpPort = 9999;
    m_logFilename = QStringLiteral("selcal_log.csv");
    m_logEnabled = false;
    m_rgbColor = QColor(181, 230, 29).rgb();
    m_title = QStringLiteral("SELCAL Demodulator");
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = QStringLiteral("127.0.0.1");
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_geometryBytes = QByteArray();
    m_hidden = false;
    m_columnIndexes.clear();
    m_columnSizes.clear();
    for (int i = 0; i < m_columns; ++i)
    {
        m_columnIndexes.append(i);
        m_columnSizes.append(-1);
    }
}

void SELCALDemodSettings::normalize()
{
    m_modulation = m_modulation == AM ? AM : USB;
    m_rfBandwidth = std::isfinite(m_rfBandwidth) ? std::clamp(m_rfBandwidth, 2000.0f, 6000.0f) : 3000.0f;
    if (m_modulation == USB) {
        m_rfBandwidth = std::min(m_rfBandwidth, 3600.0f);
    }
    m_toneFrequencyOffset = std::isfinite(m_toneFrequencyOffset) ? std::clamp(m_toneFrequencyOffset, -100.0f, 100.0f) : 0;
    m_threshold = std::isfinite(m_threshold) ? std::clamp(m_threshold, 6.0f, 20.0f) : static_cast<Real>(SELCALDecoder::m_defaultThreshold);
    m_volume = std::isfinite(m_volume) ? std::clamp(m_volume, 0.0f, 10.0f) : 1;
    m_streamIndex = std::max(0, m_streamIndex);
    m_squelch = std::clamp(m_squelch, -100, 0);

    if (!m_udpPort || m_udpPort > 65535) {
        m_udpPort = 9999;
    }
    if (!m_reverseAPIPort || m_reverseAPIPort > 65535) {
        m_reverseAPIPort = 8888;
    }

    m_reverseAPIDeviceIndex = std::min(m_reverseAPIDeviceIndex, 99u);
    m_reverseAPIChannelIndex = std::min(m_reverseAPIChannelIndex, 99u);
    if (m_columnIndexes.size() != m_columns || m_columnSizes.size() != m_columns)
    {
        m_columnIndexes.clear();
        m_columnSizes.clear();
        for (int i = 0; i < m_columns; ++i)
        {
            m_columnIndexes.append(i);
            m_columnSizes.append(-1);
        }
    }
    bool used[m_columns] = {};
    bool valid = true;
    for (int i = 0; i < m_columns; ++i)
    {
        int index = m_columnIndexes[i];
        if (index < 0 || index >= m_columns || used[index]) {
            valid = false;
        } else {
            used[index] = true;
        }
        m_columnSizes[i] = std::clamp(m_columnSizes[i], -1, 10000);
    }
    if (!valid)
    {
        for (int i = 0; i < m_columns; ++i) {
            m_columnIndexes[i] = i;
        }
    }
}

QByteArray SELCALDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeS32(3, m_modulation);
    s.writeFloat(4, m_toneFrequencyOffset);
    s.writeBool(30, m_autoToneFrequencyOffset);
    s.writeFloat(32, m_threshold);
    s.writeFloat(6, m_volume);
    s.writeBool(7, m_audioMute);
    s.writeS32(31, m_squelch);
    s.writeString(8, m_audioDeviceName);
    s.writeString(9, m_filter);
    s.writeBool(10, m_udpEnabled);
    s.writeString(11, m_udpAddress);
    s.writeU32(12, m_udpPort);
    s.writeString(13, m_logFilename);
    s.writeBool(14, m_logEnabled);
    s.writeU32(15, m_rgbColor);
    s.writeString(16, m_title);
    s.writeS32(17, m_streamIndex);
    s.writeBool(18, m_useReverseAPI);
    s.writeString(19, m_reverseAPIAddress);
    s.writeU32(20, m_reverseAPIPort);
    s.writeU32(21, m_reverseAPIDeviceIndex);
    s.writeU32(22, m_reverseAPIChannelIndex);
    s.writeS32(23, m_workspaceIndex);
    s.writeBlob(24, m_geometryBytes);
    s.writeBool(25, m_hidden);
    s.writeList(28, m_columnIndexes);
    s.writeList(29, m_columnSizes);
    if (m_channelMarker) {
        s.writeBlob(40, m_channelMarker->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(41, m_rollupState->serialize());
    }
    if (m_spectrumGUI) {
        s.writeBlob(42, m_spectrumGUI->serialize());
    }
    return s.final();
}

bool SELCALDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    resetToDefaults();

    if (!d.isValid() || d.getVersion() != 1) {
        return false;
    }

    d.readS32(1, &m_inputFrequencyOffset, 0);
    d.readFloat(2, &m_rfBandwidth, 3000.0f);
    d.readS32(3, &m_modulation, 0);
    d.readFloat(4, &m_toneFrequencyOffset, 0.0f);
    d.readBool(30, &m_autoToneFrequencyOffset, true);
    d.readFloat(32, &m_threshold, SELCALDecoder::m_defaultThreshold);
    d.readFloat(6, &m_volume, 1.0f);
    d.readBool(7, &m_audioMute, false);
    d.readS32(31, &m_squelch, -80);
    d.readString(8, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
    d.readString(9, &m_filter, QString());
    d.readBool(10, &m_udpEnabled, false);
    d.readString(11, &m_udpAddress, QStringLiteral("127.0.0.1"));
    d.readU32(12, &m_udpPort, 9999);
    d.readString(13, &m_logFilename, QStringLiteral("selcal_log.csv"));
    d.readBool(14, &m_logEnabled, false);
    d.readU32(15, &m_rgbColor, QColor(181, 230, 29).rgb());
    d.readString(16, &m_title, QStringLiteral("SELCAL Demodulator"));
    d.readS32(17, &m_streamIndex, 0);
    d.readBool(18, &m_useReverseAPI, false);
    d.readString(19, &m_reverseAPIAddress, QStringLiteral("127.0.0.1"));
    d.readU32(20, &m_reverseAPIPort, 8888);
    d.readU32(21, &m_reverseAPIDeviceIndex, 0);
    d.readU32(22, &m_reverseAPIChannelIndex, 0);
    d.readS32(23, &m_workspaceIndex, 0);
    d.readBlob(24, &m_geometryBytes);
    d.readBool(25, &m_hidden, false);
    d.readList(28, &m_columnIndexes);
    d.readList(29, &m_columnSizes);

    QByteArray blob;
    if (m_channelMarker && d.readBlob(40, &blob)) {
        m_channelMarker->deserialize(blob);
    }
    if (m_rollupState && d.readBlob(41, &blob)) {
        m_rollupState->deserialize(blob);
    }
    if (m_spectrumGUI && d.readBlob(42, &blob)) {
        m_spectrumGUI->deserialize(blob);
    }
    normalize();

    return true;
}

void SELCALDemodSettings::applySettings(const QStringList& keys, const SELCALDemodSettings& settings)
{
    if (keys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (keys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (keys.contains("modulation")) {
        m_modulation = settings.m_modulation;
    }
    if (keys.contains("toneFrequencyOffset")) {
        m_toneFrequencyOffset = settings.m_toneFrequencyOffset;
    }
    if (keys.contains("autoToneFrequencyOffset")) {
        m_autoToneFrequencyOffset = settings.m_autoToneFrequencyOffset;
    }
    if (keys.contains("threshold")) {
        m_threshold = settings.m_threshold;
    }
    if (keys.contains("volume")) {
        m_volume = settings.m_volume;
    }
    if (keys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (keys.contains("squelch")) {
        m_squelch = settings.m_squelch;
    }
    if (keys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (keys.contains("filter")) {
        m_filter = settings.m_filter;
    }
    if (keys.contains("udpEnabled")) {
        m_udpEnabled = settings.m_udpEnabled;
    }
    if (keys.contains("udpAddress")) {
        m_udpAddress = settings.m_udpAddress;
    }
    if (keys.contains("udpPort")) {
        m_udpPort = settings.m_udpPort;
    }
    if (keys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (keys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (keys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (keys.contains("title")) {
        m_title = settings.m_title;
    }
    if (keys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (keys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (keys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (keys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (keys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (keys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (keys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (keys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (keys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    normalize();
}

QString SELCALDemodSettings::getDebugString(const QStringList& keys, bool force) const
{
    QStringList result;

    if (force || keys.contains("inputFrequencyOffset")) {
        result << QString("inputFrequencyOffset: %1").arg(m_inputFrequencyOffset);
    }
    if (force || keys.contains("rfBandwidth")) {
        result << QString("rfBandwidth: %1").arg(m_rfBandwidth);
    }
    if (force || keys.contains("modulation")) {
        result << QString("modulation: %1").arg(m_modulation);
    }
    if (force || keys.contains("toneFrequencyOffset")) {
        result << QString("toneFrequencyOffset: %1").arg(m_toneFrequencyOffset);
    }
    if (force || keys.contains("autoToneFrequencyOffset")) {
        result << QString("autoToneFrequencyOffset: %1").arg(m_autoToneFrequencyOffset);
    }
    if (force || keys.contains("threshold")) {
        result << QString("threshold: %1").arg(m_threshold);
    }
    if (force || keys.contains("volume")) {
        result << QString("volume: %1").arg(m_volume);
    }
    if (force || keys.contains("audioMute")) {
        result << QString("audioMute: %1").arg(m_audioMute);
    }
    if (force || keys.contains("squelch")) {
        result << QString("squelch: %1").arg(m_squelch);
    }
    if (force || keys.contains("audioDeviceName")) {
        result << QString("audioDeviceName: %1").arg(m_audioDeviceName);
    }
    if (force || keys.contains("filter")) {
        result << QString("filter: %1").arg(m_filter);
    }
    if (force || keys.contains("udpEnabled")) {
        result << QString("udpEnabled: %1").arg(m_udpEnabled);
    }
    if (force || keys.contains("udpAddress")) {
        result << QString("udpAddress: %1").arg(m_udpAddress);
    }
    if (force || keys.contains("udpPort")) {
        result << QString("udpPort: %1").arg(m_udpPort);
    }
    if (force || keys.contains("logFilename")) {
        result << QString("logFilename: %1").arg(m_logFilename);
    }
    if (force || keys.contains("logEnabled")) {
        result << QString("logEnabled: %1").arg(m_logEnabled);
    }
    if (force || keys.contains("rgbColor")) {
        result << QString("rgbColor: %1").arg(m_rgbColor);
    }
    if (force || keys.contains("title")) {
        result << QString("title: %1").arg(m_title);
    }
    if (force || keys.contains("streamIndex")) {
        result << QString("streamIndex: %1").arg(m_streamIndex);
    }
    if (force || keys.contains("useReverseAPI")) {
        result << QString("useReverseAPI: %1").arg(m_useReverseAPI);
    }
    if (force || keys.contains("reverseAPIAddress")) {
        result << QString("reverseAPIAddress: %1").arg(m_reverseAPIAddress);
    }
    if (force || keys.contains("reverseAPIPort")) {
        result << QString("reverseAPIPort: %1").arg(m_reverseAPIPort);
    }
    if (force || keys.contains("reverseAPIDeviceIndex")) {
        result << QString("reverseAPIDeviceIndex: %1").arg(m_reverseAPIDeviceIndex);
    }
    if (force || keys.contains("reverseAPIChannelIndex")) {
        result << QString("reverseAPIChannelIndex: %1").arg(m_reverseAPIChannelIndex);
    }
    if (force || keys.contains("workspaceIndex")) {
        result << QString("workspaceIndex: %1").arg(m_workspaceIndex);
    }
    if (force || keys.contains("hidden")) {
        result << QString("hidden: %1").arg(m_hidden);
    }

    return result.join(" ");
}
