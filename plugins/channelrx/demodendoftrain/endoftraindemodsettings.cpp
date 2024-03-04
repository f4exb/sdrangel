///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2024 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "endoftraindemodsettings.h"

EndOfTrainDemodSettings::EndOfTrainDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    for (int i = 0; i < ENDOFTRAINDEMOD_COLUMNS; i++)
    {
        m_columnIndexes.append(i);
        m_columnSizes.append(-1);
    }
    resetToDefaults();
}

void EndOfTrainDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 20000.0f;
    m_fmDeviation = 3000.0f;
    m_filterFrom = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "endoftrain_log.csv";
    m_logEnabled = false;
    m_useFileTime = false;

    m_rgbColor = QColor(170, 85, 0).rgb();
    m_title = "End-of-Train Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < ENDOFTRAINDEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

QByteArray EndOfTrainDemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(3, m_fmDeviation);
    s.writeString(4, m_filterFrom);
    s.writeBool(5, m_udpEnabled);
    s.writeString(6, m_udpAddress);
    s.writeU32(7, m_udpPort);
    s.writeString(8, m_logFilename);
    s.writeBool(9, m_logEnabled);
    s.writeBool(10, m_useFileTime);

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

    s.writeList(33, m_columnIndexes);
    s.writeList(34, m_columnSizes);

    return s.final();
}

bool EndOfTrainDemodSettings::deserialize(const QByteArray& data)
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
        d.readFloat(2, &m_rfBandwidth, 20000.0f);
        d.readFloat(3, &m_fmDeviation, 3000.0f);
        d.readString(4, &m_filterFrom, "");
        d.readBool(5, &m_udpEnabled);
        d.readString(6, &m_udpAddress);
        d.readU32(7, &utmp);
        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }
        d.readString(8, &m_logFilename, "endoftrain_log.csv");
        d.readBool(9, &m_logEnabled, false);
        d.readBool(10, &m_useFileTime, false);

        d.readU32(20, &m_rgbColor, QColor(170, 85, 0).rgb());
        d.readString(21, &m_title, "End-of-Train Demodulator");
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

        d.readList(33, &m_columnIndexes);
        d.readList(34, &m_columnSizes);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void EndOfTrainDemodSettings::applySettings(const QStringList& settingsKeys, const EndOfTrainDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        m_fmDeviation = settings.m_fmDeviation;
    }
    if (settingsKeys.contains("filterFrom")) {
        m_filterFrom = settings.m_filterFrom;
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
    if (settingsKeys.contains("useFileTime")) {
        m_useFileTime = settings.m_useFileTime;
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
    if (settingsKeys.contains("columnIndexes")) {
        m_columnIndexes = settings.m_columnIndexes;
    }
    if (settingsKeys.contains("columnSizes")) {
        m_columnSizes = settings.m_columnSizes;
    }
}

QString EndOfTrainDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("fmDeviation")) {
        ostr << " m_fmDeviation: " << m_fmDeviation;
    }
    if (settingsKeys.contains("filterFrom")) {
        ostr << " m_filterFrom: " << m_filterFrom.toStdString();
    }
    if (settingsKeys.contains("udpEnabled")) {
        ostr << " m_udpEnabled: " << m_udpEnabled;
    }
    if (settingsKeys.contains("udpAddress")) {
        ostr << " m_udpAddress: " << m_udpAddress.toStdString();
    }
    if (settingsKeys.contains("udpPort")) {
        ostr << " m_udpPort: " << m_udpPort;
    }
    if (settingsKeys.contains("logFilename")) {
        ostr << " m_logFilename: " << m_logFilename.toStdString();
    }
    if (settingsKeys.contains("logEnabledp")) {
        ostr << " m_logEnabled: " << m_logEnabled;
    }
    if (settingsKeys.contains("useFileTime")) {
        ostr << " m_useFileTime: " << m_useFileTime;
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
    if (settingsKeys.contains("columnIndexes") || force) {
        // Don't display
    }
    if (settingsKeys.contains("columnSizes") || force) {
        // Don't display
    }

    return QString(ostr.str().c_str());
}
