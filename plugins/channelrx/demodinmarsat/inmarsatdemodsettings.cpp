///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2026 Jon Beniston, M7RCE <jon@beniston.com>                //
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
#include "inmarsatdemodsettings.h"

InmarsatDemodSettings::InmarsatDemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    appendDefaultColumnSettings();
    resetToDefaults();
}

void InmarsatDemodSettings::appendDefaultColumnSettings()
{
    appendDefaultColumnIndexes(m_packetsColumnIndexes, INMARSATDEMOD_PACKETS_COLUMNS);
    appendDefaultColumnSizes(m_packetsColumnSizes, INMARSATDEMOD_PACKETS_COLUMNS);
    appendDefaultColumnIndexes(m_messagesColumnIndexes, INMARSATDEMOD_MESSAGES_COLUMNS);
    appendDefaultColumnSizes(m_messagesColumnSizes, INMARSATDEMOD_MESSAGES_COLUMNS);
}

void InmarsatDemodSettings::appendDefaultColumnIndexes(QList<int>& list, int size)
{
    while (list.size() < size) {
        list.append(list.size());
    }
}

void InmarsatDemodSettings::appendDefaultColumnSizes(QList<int>& list, int size)
{
    while (list.size() < size) {
        list.append(-1);
    }
}

void InmarsatDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 4000.0f;
    m_equalizer = NONE;
    m_rrcRolloff = 1.0f;
    m_pllBW = 2*M_PI/100.0;
    m_ssBW = 2*M_PI/100.0;
    m_filterType = "";
    m_filterMessage = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = 9999;
    m_logFilename = "inmarsat_c_log.csv";
    m_logEnabled = false;
    m_useFileTime = false;
    for (int i = 0; i < INMARSATDEMOD_PACKETS_COLUMNS; i++)
    {
        m_packetsColumnIndexes[i] = i;
        m_packetsColumnSizes[i] = -1; // Autosize
    }
    for (int i = 0; i < INMARSATDEMOD_MESSAGES_COLUMNS; i++)
    {
        m_messagesColumnIndexes[i] = i;
        m_messagesColumnSizes[i] = -1; // Autosize
    }
    m_rgbColor = QColor(40, 180, 75).rgb();
    m_title = "Inmarsat C Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray InmarsatDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeString(3, m_filterType);
    s.writeString(4, m_filterMessage);

    s.writeFloat(5, m_rfBandwidth);
    s.writeFloat(6, m_rrcRolloff);
    s.writeFloat(7, m_pllBW);
    s.writeFloat(8, m_ssBW);

    s.writeS32(9, (int) m_equalizer);

    s.writeBool(10, m_udpEnabled);
    s.writeString(11, m_udpAddress);
    s.writeU32(12, m_udpPort);

    s.writeString(13, m_logFilename);
    s.writeBool(14, m_logEnabled);

    s.writeBool(15, m_useFileTime);

    s.writeList(16, m_packetsColumnIndexes);
    s.writeList(17, m_packetsColumnSizes);
    s.writeList(18, m_messagesColumnIndexes);
    s.writeList(19, m_messagesColumnSizes);

    if (m_channelMarker) {
        s.writeBlob(20, m_channelMarker->serialize());
    }
    s.writeU32(21, m_rgbColor);
    s.writeString(22, m_title);
    s.writeBool(23, m_useReverseAPI);
    s.writeString(24, m_reverseAPIAddress);
    s.writeU32(25, m_reverseAPIPort);
    s.writeU32(26, m_reverseAPIDeviceIndex);
    s.writeU32(27, m_reverseAPIChannelIndex);
    if (m_rollupState) {
        s.writeBlob(28, m_rollupState->serialize());
    }
    s.writeS32(29, m_workspaceIndex);
    s.writeBlob(30, m_geometryBytes);
    s.writeBool(31, m_hidden);
    if (m_scopeGUI) {
        s.writeBlob(32, m_scopeGUI->serialize());
    }

    return s.final();
}

bool InmarsatDemodSettings::deserialize(const QByteArray& data)
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
        d.readS32(2, &m_streamIndex, 0);
        d.readString(3, &m_filterType, "");
        d.readString(4, &m_filterMessage, "");

        d.readFloat(5, &m_rfBandwidth, 4000.0f);
        d.readFloat(6, &m_rrcRolloff, 1.0f);
        d.readFloat(7, &m_pllBW, 2*M_PI/100.0);
        d.readFloat(8, &m_ssBW, 2*M_PI/100.0);

        d.readS32(9, (int *) &m_equalizer, (int) NONE);

        d.readBool(10, &m_udpEnabled);
        d.readString(11, &m_udpAddress);
        d.readU32(12, &utmp);

        d.readString(13, &m_logFilename, "inmarsat_c_log.csv");
        d.readBool(14, &m_logEnabled, false);

        d.readBool(15, &m_useFileTime, false);

        d.readList(16, &m_packetsColumnIndexes);
        d.readList(17, &m_packetsColumnSizes);
        d.readList(18, &m_messagesColumnIndexes);
        d.readList(19, &m_messagesColumnSizes);
        appendDefaultColumnSettings();

        if (m_channelMarker)
        {
            d.readBlob(20, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }
        d.readU32(21, &m_rgbColor, QColor(40, 180, 75).rgb());
        d.readString(22, &m_title, "Inmarsat C Demodulator");
        d.readBool(23, &m_useReverseAPI, false);
        d.readString(24, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(25, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(26, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(27, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if ((utmp > 1023) && (utmp < 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = 9999;
        }

        if (m_rollupState)
        {
            d.readBlob(28, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(29, &m_workspaceIndex, 0);
        d.readBlob(30, &m_geometryBytes);
        d.readBool(31, &m_hidden, false);

        if (m_scopeGUI)
        {
            d.readBlob(32, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void InmarsatDemodSettings::applySettings(const QStringList& settingsKeys, const InmarsatDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("equalizer")) {
        m_equalizer = settings.m_equalizer;
    }
    if (settingsKeys.contains("rrcRolloff")) {
        m_rrcRolloff = settings.m_rrcRolloff;
    }
    if (settingsKeys.contains("pllBandwidth")) {
        m_pllBW = settings.m_pllBW;
    }
    if (settingsKeys.contains("ssBandwidth")) {
        m_ssBW = settings.m_ssBW;
    }
    if (settingsKeys.contains("filterType")) {
        m_filterType = settings.m_filterType;
    }
    if (settingsKeys.contains("filterMessage")) {
        m_filterMessage = settings.m_filterMessage;
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
    if (settingsKeys.contains("useFileTime")) {
        m_useFileTime = settings.m_useFileTime;
    }
    if (settingsKeys.contains("logFilename")) {
        m_logFilename = settings.m_logFilename;
    }
    if (settingsKeys.contains("logEnabled")) {
        m_logEnabled = settings.m_logEnabled;
    }
    if (settingsKeys.contains("packetsColumnIndexes")) {
        m_packetsColumnIndexes = settings.m_packetsColumnIndexes;
    }
    if (settingsKeys.contains("packetsColumnSizes")) {
        m_packetsColumnSizes = settings.m_packetsColumnSizes;
    }
    if (settingsKeys.contains("messagesColumnIndexes")) {
        m_messagesColumnIndexes = settings.m_messagesColumnIndexes;
    }
    if (settingsKeys.contains("messagesColumnSizes")) {
        m_messagesColumnSizes = settings.m_messagesColumnSizes;
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
}

QString InmarsatDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("equalizer") || force) {
        ostr << " m_equalizer: " << m_equalizer;
    }
    if (settingsKeys.contains("rrcRolloff") || force) {
        ostr << " m_rrcRolloff: " << m_rrcRolloff;
    }
    if (settingsKeys.contains("pllBandwidth") || force) {
        ostr << " m_pllBW: " << m_pllBW;
    }
    if (settingsKeys.contains("ssBandwidth") || force) {
        ostr << " m_ssBW: " << m_ssBW;
    }
    if (settingsKeys.contains("filterType") || force) {
        ostr << " m_filterType: " << m_filterType.toStdString();
    }
    if (settingsKeys.contains("filterMessage") || force) {
        ostr << " m_filterMessage: " << m_filterMessage.toStdString();
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
    if (settingsKeys.contains("useFileTime") || force) {
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

    return QString(ostr.str().c_str());
}
