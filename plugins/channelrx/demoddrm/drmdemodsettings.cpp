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

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>

#include <QColor>

#include "audio/audiodevicemanager.h"
#include "modemdrm/drmparameters.h"
#include "settings/serializable.h"
#include "util/simpleserializer.h"

#include "drmdemodsettings.h"

DRMDemodSettings::DRMDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void DRMDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_acquisitionSearchRangeHz = 0;
    m_forcedMode = -1;
    m_forcedOccupancy = -1;
    m_selectedServiceIdentifier = -1;
    m_volume = 1.0f;
    m_audioMute = false;
    m_audioDeviceName = AudioDeviceManager::m_defaultDeviceName;
    m_rgbColor = QColor(170, 85, 255).rgb();
    m_title = "DRM Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_geometryBytes.clear();
    m_hidden = false;
}

int DRMDemodSettings::sanitizeSearchRange(int searchRangeHz)
{
    // Auto (0) or 100..5000 Hz; 1..99 means 100, as the GUI takes it.
    if (searchRangeHz > 0 && searchRangeHz < 100) {
        searchRangeHz = 100;
    }

    return modemdrm::Parameters::isSearchRangeValid(searchRangeHz) ? searchRangeHz : 0;
}

int DRMDemodSettings::sanitizeForcedMode(int mode)
{
    return mode >= -1 && mode <= static_cast<int>(modemdrm::Mode::E)
        ? mode : -1;
}

int DRMDemodSettings::sanitizeForcedOccupancy(int occupancy)
{
    return occupancy >= -1 && occupancy <= 5 ? occupancy : -1;
}

qint64 DRMDemodSettings::sanitizeServiceIdentifier(qint64 serviceIdentifier)
{
    return serviceIdentifier >= 0 && serviceIdentifier <= 0xFFFFFF ? serviceIdentifier : -1;
}

void DRMDemodSettings::sanitize()
{
    m_acquisitionSearchRangeHz = sanitizeSearchRange(m_acquisitionSearchRangeHz);
    m_forcedMode = sanitizeForcedMode(m_forcedMode);
    m_forcedOccupancy = sanitizeForcedOccupancy(m_forcedOccupancy);
    // A pair the mode does not define (mode E has occupancy 0 only; C and D
    // have 3 and 5) would only fail as a passband error: search occupancy.
    if (m_forcedMode >= 0 && m_forcedOccupancy >= 0
        && !modemdrm::Parameters::forSpectrum(static_cast<modemdrm::Mode>(m_forcedMode), m_forcedOccupancy)) {
        m_forcedOccupancy = -1;
    }

    m_selectedServiceIdentifier = sanitizeServiceIdentifier(m_selectedServiceIdentifier);
    if (!std::isfinite(m_volume)) {
        m_volume = 1.0f;
    }

    m_volume = std::clamp(m_volume, 0.0f, 10.0f);
    m_streamIndex = std::max(m_streamIndex, 0);
    if (m_reverseAPIPort <= 1023 || m_reverseAPIPort > 65535) {
        m_reverseAPIPort = 8888;
    }

    m_reverseAPIDeviceIndex = std::min<std::uint16_t>(m_reverseAPIDeviceIndex, 99);
    m_reverseAPIChannelIndex = std::min<std::uint16_t>(m_reverseAPIChannelIndex, 99);
}

QStringList DRMDemodSettings::sanitizedKeys(const DRMDemodSettings& before) const
{
    QStringList keys;
    const auto add = [&keys](bool differs, const char* key) {
        if (differs) {
            keys.append(key);
        }
    };
    add(m_acquisitionSearchRangeHz != before.m_acquisitionSearchRangeHz, "acquisitionSearchRangeHz");
    add(m_forcedMode != before.m_forcedMode, "forcedMode");
    add(m_forcedOccupancy != before.m_forcedOccupancy, "forcedOccupancy");
    add(m_selectedServiceIdentifier != before.m_selectedServiceIdentifier, "selectedServiceIdentifier");
    add(m_volume != before.m_volume, "volume");
    add(m_streamIndex != before.m_streamIndex, "streamIndex");
    add(m_reverseAPIPort != before.m_reverseAPIPort, "reverseAPIPort");
    add(m_reverseAPIDeviceIndex != before.m_reverseAPIDeviceIndex, "reverseAPIDeviceIndex");
    add(m_reverseAPIChannelIndex != before.m_reverseAPIChannelIndex, "reverseAPIChannelIndex");
    return keys;
}

QByteArray DRMDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_acquisitionSearchRangeHz);
    // Tag 3 is unused.
    s.writeS64(4, m_selectedServiceIdentifier);
    s.writeReal(5, m_volume);
    s.writeBool(6, m_audioMute);
    s.writeString(7, m_audioDeviceName);
    s.writeU32(8, m_rgbColor);
    s.writeString(9, m_title);
    s.writeS32(10, m_streamIndex);
    s.writeBool(11, m_useReverseAPI);
    s.writeString(12, m_reverseAPIAddress);
    s.writeU32(13, m_reverseAPIPort);
    s.writeU32(14, m_reverseAPIDeviceIndex);
    s.writeU32(15, m_reverseAPIChannelIndex);
    s.writeS32(16, m_workspaceIndex);
    s.writeBlob(17, m_geometryBytes);
    s.writeBool(18, m_hidden);
    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    if (m_rollupState) {
        s.writeBlob(20, m_rollupState->serialize());
    }

    s.writeS32(21, m_forcedMode);
    s.writeS32(22, m_forcedOccupancy);
    return s.final();
}

bool DRMDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);
    if (!d.isValid() || d.getVersion() != 1)
    {
        resetToDefaults();
        return false;
    }

    QByteArray blob;
    uint32_t value = 0;
    d.readS32(1, &m_inputFrequencyOffset, 0);
    d.readS32(2, &m_acquisitionSearchRangeHz, 0);
    d.readS64(4, &m_selectedServiceIdentifier, -1);
    d.readReal(5, &m_volume, 1.0f);
    d.readBool(6, &m_audioMute, false);
    d.readString(7, &m_audioDeviceName, AudioDeviceManager::m_defaultDeviceName);
    d.readU32(8, &m_rgbColor, QColor(170, 85, 255).rgb());
    d.readString(9, &m_title, "DRM Demodulator");
    d.readS32(10, &m_streamIndex, 0);
    d.readBool(11, &m_useReverseAPI, false);
    d.readString(12, &m_reverseAPIAddress, "127.0.0.1");
    d.readU32(13, &value, 8888);
    m_reverseAPIPort = value > 1023 && value <= 65535 ? static_cast<std::uint16_t>(value) : 8888;
    d.readU32(14, &value, 0);
    m_reverseAPIDeviceIndex = static_cast<std::uint16_t>(std::min(value, 99U));
    d.readU32(15, &value, 0);
    m_reverseAPIChannelIndex = static_cast<std::uint16_t>(std::min(value, 99U));
    d.readS32(16, &m_workspaceIndex, 0);
    d.readBlob(17, &m_geometryBytes);
    d.readBool(18, &m_hidden, false);
    if (m_channelMarker)
    {
        d.readBlob(19, &blob);
        m_channelMarker->deserialize(blob);
    }

    if (m_rollupState)
    {
        d.readBlob(20, &blob);
        m_rollupState->deserialize(blob);
    }

    d.readS32(21, &m_forcedMode, -1);
    d.readS32(22, &m_forcedOccupancy, -1);
    sanitize();
    return true;
}

void DRMDemodSettings::applySettings(const QStringList& settingsKeys, const DRMDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("acquisitionSearchRangeHz")) {
        m_acquisitionSearchRangeHz = sanitizeSearchRange(settings.m_acquisitionSearchRangeHz);
    }
    if (settingsKeys.contains("forcedMode")) {
        m_forcedMode = sanitizeForcedMode(settings.m_forcedMode);
    }
    if (settingsKeys.contains("forcedOccupancy")) {
        m_forcedOccupancy = sanitizeForcedOccupancy(settings.m_forcedOccupancy);
    }
    if (settingsKeys.contains("selectedServiceIdentifier")) {
        m_selectedServiceIdentifier = sanitizeServiceIdentifier(settings.m_selectedServiceIdentifier);
    }
    if (settingsKeys.contains("volume")) {
        m_volume = std::isfinite(settings.m_volume) ? std::clamp(settings.m_volume, 0.0f, 10.0f) : 1.0f;
    }
    if (settingsKeys.contains("audioMute")) {
        m_audioMute = settings.m_audioMute;
    }
    if (settingsKeys.contains("audioDeviceName")) {
        m_audioDeviceName = settings.m_audioDeviceName;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = std::max(settings.m_streamIndex, 0);
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort"))
    {
        m_reverseAPIPort = settings.m_reverseAPIPort > 1023 && settings.m_reverseAPIPort < 65536
            ? settings.m_reverseAPIPort : 8888;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = std::min<std::uint16_t>(settings.m_reverseAPIDeviceIndex, 99);
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = std::min<std::uint16_t>(settings.m_reverseAPIChannelIndex, 99);
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

QString DRMDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream stream;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        stream << " inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("acquisitionSearchRangeHz") || force) {
        stream << " acquisitionSearchRangeHz: " << m_acquisitionSearchRangeHz;
    }
    if (settingsKeys.contains("forcedMode") || force) {
        stream << " forcedMode: " << m_forcedMode;
    }
    if (settingsKeys.contains("forcedOccupancy") || force) {
        stream << " forcedOccupancy: " << m_forcedOccupancy;
    }
    if (settingsKeys.contains("selectedServiceIdentifier") || force) {
        stream << " selectedServiceIdentifier: " << m_selectedServiceIdentifier;
    }
    if (settingsKeys.contains("volume") || force) {
        stream << " volume: " << m_volume;
    }
    if (settingsKeys.contains("audioMute") || force) {
        stream << " audioMute: " << m_audioMute;
    }
    if (settingsKeys.contains("audioDeviceName") || force) {
        stream << " audioDeviceName: " << m_audioDeviceName.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        stream << " rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        stream << " title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        stream << " streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        stream << " useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress") || force) {
        stream << " reverseAPIAddress: " << m_reverseAPIAddress.toStdString();
    }
    if (settingsKeys.contains("reverseAPIPort") || force) {
        stream << " reverseAPIPort: " << m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex") || force) {
        stream << " reverseAPIDeviceIndex: " << m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex") || force) {
        stream << " reverseAPIChannelIndex: " << m_reverseAPIChannelIndex;
    }

    return QString::fromStdString(stream.str());
}
