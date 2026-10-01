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

#include "wefaxdemodsettings.h"

#include <QColor>

#include <algorithm>
#include <cmath>
#include <sstream>

#include "settings/serializable.h"
#include "util/simpleserializer.h"
#include "wefaxdecoder.h"

WefaxDemodSettings::WefaxDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void WefaxDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 2400.0f;
    m_fmDeviation = 800.0f;
    m_ioc = 576;
    m_linesPerMinute = 120;
    m_autoMode = true;
    m_inverted = false;
    m_minimumPhasingLines = 8;
    m_startConfirmSeconds = 1.0;
    m_stopConfirmSeconds = 1.0;
    m_manualClockCorrectionPpm = 0.0;
    m_maxRows = 5000;
    m_autoSave = false;
    m_autoSavePath.clear();
    m_displayInverted = false;
    m_displayContrast = 0;
    m_displayThreshold = -1;
    m_horizontalAlignment = 0;
    m_displaySlantCorrectionPpm = 0.0;
    m_displayRotation = 0;
    m_displayZoomPercent = 100;
    m_autoScroll = true;
    m_autoSlant = true;

    m_rgbColor = QColor(66, 170, 255).rgb();
    m_title = "WEFAX Demodulator";
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

void WefaxDemodSettings::validate()
{
    if (!std::isfinite(m_rfBandwidth)) {
        m_rfBandwidth = 2400.0f;
    }
    m_rfBandwidth = std::clamp(m_rfBandwidth, 1000.0f, 20000.0f);

    if (!std::isfinite(m_fmDeviation)) {
        m_fmDeviation = 800.0f;
    }
    m_fmDeviation = std::clamp(m_fmDeviation, 400.0f, 1200.0f);

    if (!WefaxDecoder::isSupportedIOC(m_ioc)) {
        m_ioc = 576;
    }
    if (!WefaxDecoder::isSupportedLinesPerMinute(m_linesPerMinute)) {
        m_linesPerMinute = 120;
    }

    m_minimumPhasingLines = std::clamp(m_minimumPhasingLines, 2, 120);
    if (!std::isfinite(m_startConfirmSeconds)) {
        m_startConfirmSeconds = 1.0;
    }
    if (!std::isfinite(m_stopConfirmSeconds)) {
        m_stopConfirmSeconds = 1.0;
    }
    m_startConfirmSeconds = std::clamp(m_startConfirmSeconds, 0.1, 10.0);
    m_stopConfirmSeconds = std::clamp(m_stopConfirmSeconds, 0.1, 10.0);
    if (!std::isfinite(m_manualClockCorrectionPpm)) {
        m_manualClockCorrectionPpm = 0.0;
    }
    m_manualClockCorrectionPpm = std::clamp(m_manualClockCorrectionPpm, -1000.0, 1000.0);
    m_maxRows = std::clamp(m_maxRows, 1, MaxRowsLimit);
    m_displayContrast = std::clamp(m_displayContrast, -100, 100);
    m_displayThreshold = std::clamp(m_displayThreshold, -1, 255);
    m_horizontalAlignment = std::clamp(m_horizontalAlignment, -1809, 1809);
    if (!std::isfinite(m_displaySlantCorrectionPpm)) {
        m_displaySlantCorrectionPpm = 0.0;
    }
    m_displaySlantCorrectionPpm = std::clamp(m_displaySlantCorrectionPpm, -1000.0, 1000.0);
    if ((m_displayRotation != 0) && (m_displayRotation != 90)
        && (m_displayRotation != 180) && (m_displayRotation != 270)) {
        m_displayRotation = 0;
    }
    if ((m_displayZoomPercent != 0) && ((m_displayZoomPercent < 5) || (m_displayZoomPercent > 400))) {
        m_displayZoomPercent = 100;
    }
    m_streamIndex = std::max(0, m_streamIndex);
    m_reverseAPIPort = ((m_reverseAPIPort > 1023) && (m_reverseAPIPort < 65535))
        ? m_reverseAPIPort : 8888;
    m_reverseAPIDeviceIndex = std::min<uint16_t>(m_reverseAPIDeviceIndex, 99);
    m_reverseAPIChannelIndex = std::min<uint16_t>(m_reverseAPIChannelIndex, 99);
}

QByteArray WefaxDemodSettings::serialize() const
{
    SimpleSerializer serializer(1);
    serializer.writeS32(1, m_inputFrequencyOffset);
    serializer.writeReal(2, m_rfBandwidth);
    serializer.writeReal(3, m_fmDeviation);
    serializer.writeS32(4, m_ioc);
    serializer.writeS32(5, m_linesPerMinute);
    serializer.writeBool(6, m_autoMode);
    serializer.writeBool(7, m_inverted);
    serializer.writeS32(8, m_minimumPhasingLines);
    serializer.writeDouble(9, m_manualClockCorrectionPpm);
    serializer.writeS32(10, m_maxRows);
    serializer.writeBool(11, m_autoSave);
    serializer.writeString(12, m_autoSavePath);
    serializer.writeS32(13, m_streamIndex);
    serializer.writeU32(14, m_rgbColor);
    serializer.writeString(15, m_title);
    serializer.writeBool(16, m_useReverseAPI);
    serializer.writeString(17, m_reverseAPIAddress);
    serializer.writeU32(18, m_reverseAPIPort);
    serializer.writeU32(19, m_reverseAPIDeviceIndex);
    serializer.writeU32(20, m_reverseAPIChannelIndex);
    serializer.writeS32(21, m_workspaceIndex);
    serializer.writeBlob(22, m_geometryBytes);
    serializer.writeBool(23, m_hidden);
    serializer.writeBool(26, m_displayInverted);
    serializer.writeS32(27, m_displayContrast);
    serializer.writeS32(28, m_displayRotation);
    serializer.writeS32(29, m_displayZoomPercent);
    serializer.writeBool(30, m_autoScroll);
    serializer.writeDouble(31, m_startConfirmSeconds);
    serializer.writeDouble(32, m_stopConfirmSeconds);
    serializer.writeS32(33, m_displayThreshold);
    serializer.writeS32(34, m_horizontalAlignment);
    serializer.writeDouble(35, m_displaySlantCorrectionPpm);
    serializer.writeBool(36, m_autoSlant);

    if (m_channelMarker) {
        serializer.writeBlob(24, m_channelMarker->serialize());
    }
    if (m_rollupState) {
        serializer.writeBlob(25, m_rollupState->serialize());
    }

    return serializer.final();
}

bool WefaxDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer deserializer(data);
    if (!deserializer.isValid() || (deserializer.getVersion() != 1))
    {
        resetToDefaults();
        return false;
    }

    QByteArray blob;
    quint32 unsignedValue;
    deserializer.readS32(1, &m_inputFrequencyOffset, 0);
    deserializer.readReal(2, &m_rfBandwidth, 2400.0f);
    deserializer.readReal(3, &m_fmDeviation, 800.0f);
    deserializer.readS32(4, &m_ioc, 576);
    deserializer.readS32(5, &m_linesPerMinute, 120);
    deserializer.readBool(6, &m_autoMode, true);
    deserializer.readBool(7, &m_inverted, false);
    deserializer.readS32(8, &m_minimumPhasingLines, 8);
    deserializer.readDouble(9, &m_manualClockCorrectionPpm, 0.0);
    deserializer.readS32(10, &m_maxRows, 5000);
    deserializer.readBool(11, &m_autoSave, false);
    deserializer.readString(12, &m_autoSavePath, QString());
    deserializer.readS32(13, &m_streamIndex, 0);
    deserializer.readU32(14, &m_rgbColor, QColor(66, 170, 255).rgb());
    deserializer.readString(15, &m_title, "WEFAX Demodulator");
    deserializer.readBool(16, &m_useReverseAPI, false);
    deserializer.readString(17, &m_reverseAPIAddress, "127.0.0.1");
    deserializer.readU32(18, &unsignedValue, 8888);
    m_reverseAPIPort = static_cast<uint16_t>(unsignedValue);
    deserializer.readU32(19, &unsignedValue, 0);
    m_reverseAPIDeviceIndex = static_cast<uint16_t>(unsignedValue);
    deserializer.readU32(20, &unsignedValue, 0);
    m_reverseAPIChannelIndex = static_cast<uint16_t>(unsignedValue);
    deserializer.readS32(21, &m_workspaceIndex, 0);
    deserializer.readBlob(22, &m_geometryBytes);
    deserializer.readBool(23, &m_hidden, false);
    deserializer.readBool(26, &m_displayInverted, false);
    deserializer.readS32(27, &m_displayContrast, 0);
    deserializer.readS32(28, &m_displayRotation, 0);
    deserializer.readS32(29, &m_displayZoomPercent, 100);
    deserializer.readBool(30, &m_autoScroll, true);
    deserializer.readDouble(31, &m_startConfirmSeconds, 1.0);
    deserializer.readDouble(32, &m_stopConfirmSeconds, 1.0);
    deserializer.readS32(33, &m_displayThreshold, -1);
    deserializer.readS32(34, &m_horizontalAlignment, 0);
    deserializer.readDouble(35, &m_displaySlantCorrectionPpm, 0.0);
    deserializer.readBool(36, &m_autoSlant, true);

    if (m_channelMarker && deserializer.readBlob(24, &blob)) {
        m_channelMarker->deserialize(blob);
    }
    if (m_rollupState && deserializer.readBlob(25, &blob)) {
        m_rollupState->deserialize(blob);
    }

    validate();
    return true;
}

void WefaxDemodSettings::applySettings(const QStringList& keys, const WefaxDemodSettings& settings)
{
    if (keys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (keys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (keys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (keys.contains("ioc")) {
        m_ioc = settings.m_ioc;
    }
    if (keys.contains("linesPerMinute")) {
        m_linesPerMinute = settings.m_linesPerMinute;
    }
    if (keys.contains("autoMode")) {
        m_autoMode = settings.m_autoMode;
    }
    if (keys.contains("inverted")) {
        m_inverted = settings.m_inverted;
    }
    if (keys.contains("minimumPhasingLines")) {
        m_minimumPhasingLines = settings.m_minimumPhasingLines;
    }
    if (keys.contains("startConfirmSeconds")) {
        m_startConfirmSeconds = settings.m_startConfirmSeconds;
    }
    if (keys.contains("stopConfirmSeconds")) {
        m_stopConfirmSeconds = settings.m_stopConfirmSeconds;
    }
    if (keys.contains("manualClockCorrectionPpm")) {
        m_manualClockCorrectionPpm = settings.m_manualClockCorrectionPpm;
    }
    if (keys.contains("maxRows")) {
        m_maxRows = settings.m_maxRows;
    }
    if (keys.contains("autoSave")) {
        m_autoSave = settings.m_autoSave;
    }
    if (keys.contains("autoSavePath")) {
        m_autoSavePath = settings.m_autoSavePath;
    }
    if (keys.contains("displayInverted")) {
        m_displayInverted = settings.m_displayInverted;
    }
    if (keys.contains("displayContrast")) {
        m_displayContrast = settings.m_displayContrast;
    }
    if (keys.contains("displayThreshold")) {
        m_displayThreshold = settings.m_displayThreshold;
    }
    if (keys.contains("horizontalAlignment")) {
        m_horizontalAlignment = settings.m_horizontalAlignment;
    }
    if (keys.contains("displaySlantCorrectionPpm")) {
        m_displaySlantCorrectionPpm = settings.m_displaySlantCorrectionPpm;
    }
    if (keys.contains("displayRotation")) {
        m_displayRotation = settings.m_displayRotation;
    }
    if (keys.contains("displayZoomPercent")) {
        m_displayZoomPercent = settings.m_displayZoomPercent;
    }
    if (keys.contains("autoScroll")) {
        m_autoScroll = settings.m_autoScroll;
    }
    if (keys.contains("autoSlant")) {
        m_autoSlant = settings.m_autoSlant;
    }
    if (keys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (keys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (keys.contains("title")) {
        m_title = settings.m_title;
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
    validate();
}

QString WefaxDemodSettings::getDebugString(const QStringList& keys, bool force) const
{
    std::ostringstream stream;

    if (force || keys.contains("inputFrequencyOffset")) {
        stream << " inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (force || keys.contains("rfBandwidth")) {
        stream << " rfBandwidth: " << m_rfBandwidth;
    }
    if (force || keys.contains("fmDeviation")) {
        stream << " fmDeviation: " << m_fmDeviation;
    }
    if (force || keys.contains("ioc")) {
        stream << " ioc: " << m_ioc;
    }
    if (force || keys.contains("linesPerMinute")) {
        stream << " linesPerMinute: " << m_linesPerMinute;
    }
    if (force || keys.contains("autoMode")) {
        stream << " autoMode: " << m_autoMode;
    }
    if (force || keys.contains("inverted")) {
        stream << " inverted: " << m_inverted;
    }
    if (force || keys.contains("minimumPhasingLines")) {
        stream << " minimumPhasingLines: " << m_minimumPhasingLines;
    }
    if (force || keys.contains("startConfirmSeconds")) {
        stream << " startConfirmSeconds: " << m_startConfirmSeconds;
    }
    if (force || keys.contains("stopConfirmSeconds")) {
        stream << " stopConfirmSeconds: " << m_stopConfirmSeconds;
    }
    if (force || keys.contains("manualClockCorrectionPpm")) {
        stream << " manualClockCorrectionPpm: " << m_manualClockCorrectionPpm;
    }
    if (force || keys.contains("maxRows")) {
        stream << " maxRows: " << m_maxRows;
    }
    if (force || keys.contains("autoSave")) {
        stream << " autoSave: " << m_autoSave;
    }
    if (force || keys.contains("displayThreshold")) {
        stream << " displayThreshold: " << m_displayThreshold;
    }
    if (force || keys.contains("horizontalAlignment")) {
        stream << " horizontalAlignment: " << m_horizontalAlignment;
    }
    if (force || keys.contains("displaySlantCorrectionPpm")) {
        stream << " displaySlantCorrectionPpm: " << m_displaySlantCorrectionPpm;
    }
    if (force || keys.contains("streamIndex")) {
        stream << " streamIndex: " << m_streamIndex;
    }
    return QString::fromStdString(stream.str());
}

bool WefaxDemodSettings::endsCapture(const QStringList& settingsKeys)
{
    // The frequency offset is deliberately absent: fine tuning during
    // reception only moves the channel NCO.
    static const QStringList keys {
        "fmDeviation",
        "ioc",
        "linesPerMinute",
        "autoMode",
        "inverted",
        "minimumPhasingLines",
        "streamIndex"
    };

    return std::any_of(keys.begin(), keys.end(), [&settingsKeys](const QString& key) {
        return settingsKeys.contains(key);
    });
}

int WefaxDemodSettings::internalSampleRate() const
{
    if (m_autoMode) {
        return 24000;
    }
    return ((m_ioc == 576) && (m_linesPerMinute >= 180)) ? 24000 : 12000;
}
