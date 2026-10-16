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

#include <algorithm>
#include <sstream>

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "ieee_802_15_4_demodsettings.h"

IEEE_802_15_4_DemodSettings::IEEE_802_15_4_DemodSettings() :
    m_channelMarker(nullptr),
    m_scopeGUI(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void IEEE_802_15_4_DemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    setPHY("250kbps >2GHz O-QPSK");
    m_samplesPerChip = 4;
    m_threshold = 0.05f;
    m_showBadFCS = false;
    m_filterFrameType = FilterAll;
    m_filterPANID = "";
    m_filterAddress = "";
    m_udpEnabled = false;
    m_udpAddress = "127.0.0.1";
    m_udpPort = m_zepPort;
    m_udpFormat = ZEP;
    m_logFilename = "802_15_4_log.csv";
    m_logEnabled = false;
    m_useFileTime = false;
    m_rgbColor = QColor(255, 0, 255).rgb();
    m_title = "802.15.4 Demodulator";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;

    for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++)
    {
        m_columnIndexes[i] = i;
        m_columnSizes[i] = -1; // Autosize
    }
}

IEEE_802_15_4_PHY IEEE_802_15_4_DemodSettings::getPHY() const
{
    IEEE_802_15_4_PHY phy;

    if (!phy.fromString(m_phy)) {
        phy.fromString("250kbps >2GHz O-QPSK");
    }

    return phy;
}

int IEEE_802_15_4_DemodSettings::getChannelSampleRate() const
{
    return m_samplesPerChip * getPHY().getChipRate();
}

Real IEEE_802_15_4_DemodSettings::getDefaultMaxFrequencyOffset(const IEEE_802_15_4_PHY& phy)
{
    double frequency;

    if (phy.m_modulation == IEEE_802_15_4_PHY::BPSK) {
        frequency = phy.m_bitRate == 20000 ? 868.6e6 : 928e6;
    } else if (!phy.m_subGHzBand) {
        frequency = 2483.5e6;
    } else if (phy.m_bitRate == 100000) {
        frequency = 868.6e6;
    } else if (phy.m_pulseShaping == IEEE_802_15_4_PHY::RC) {
        frequency = 787e6;
    } else {
        frequency = 928e6;
    }

    return (Real) (frequency * 80e-6);
}

bool IEEE_802_15_4_DemodSettings::setPHY(const QString& phyName)
{
    IEEE_802_15_4_PHY phy;

    if (!phy.fromString(phyName)) {
        return false;
    }

    m_phy = phy.toString().isEmpty() ? phyName : phy.toString();
    m_rfBandwidth = phy.getOccupiedBandwidth();
    m_maxFrequencyOffset = getDefaultMaxFrequencyOffset(phy);

    return true;
}

QByteArray IEEE_802_15_4_DemodSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS64(1, m_inputFrequencyOffset);
    s.writeString(2, m_phy);
    s.writeFloat(3, m_rfBandwidth);
    s.writeS32(4, m_samplesPerChip);
    s.writeFloat(5, m_maxFrequencyOffset);
    s.writeFloat(6, m_threshold);
    s.writeBool(7, m_showBadFCS);
    s.writeS32(8, (int) m_filterFrameType);
    s.writeString(9, m_filterPANID);
    s.writeString(10, m_filterAddress);
    s.writeBool(11, m_udpEnabled);
    s.writeString(12, m_udpAddress);
    s.writeU32(13, m_udpPort);
    s.writeS32(14, (int) m_udpFormat);
    s.writeString(15, m_logFilename);
    s.writeBool(16, m_logEnabled);
    s.writeBool(17, m_useFileTime);

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

    if (m_scopeGUI) {
        s.writeBlob(29, m_scopeGUI->serialize());
    }
    if (m_rollupState) {
        s.writeBlob(30, m_rollupState->serialize());
    }

    s.writeS32(31, m_workspaceIndex);
    s.writeBlob(32, m_geometryBytes);
    s.writeBool(33, m_hidden);

    for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
        s.writeS32(100 + i, m_columnIndexes[i]);
    }
    for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
        s.writeS32(200 + i, m_columnSizes[i]);
    }

    return s.final();
}

bool IEEE_802_15_4_DemodSettings::deserialize(const QByteArray& data)
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
        d.readString(2, &m_phy, "250kbps >2GHz O-QPSK");
        IEEE_802_15_4_PHY phy = getPHY();
        d.readFloat(3, &m_rfBandwidth, phy.getOccupiedBandwidth());
        d.readS32(4, &m_samplesPerChip, 4);
        m_samplesPerChip = std::max(2, std::min(16, m_samplesPerChip));
        d.readFloat(5, &m_maxFrequencyOffset, getDefaultMaxFrequencyOffset(phy));
        d.readFloat(6, &m_threshold, 0.05f);
        m_threshold = std::max(m_minThreshold, std::min(m_maxThreshold, m_threshold));
        d.readBool(7, &m_showBadFCS, false);
        d.readS32(8, &itmp, (int) FilterAll);
        m_filterFrameType = (FrameTypeFilter) std::max((int) FilterAll, std::min((int) FilterOther, itmp));
        d.readString(9, &m_filterPANID, "");
        d.readString(10, &m_filterAddress, "");
        d.readBool(11, &m_udpEnabled, false);
        d.readString(12, &m_udpAddress, "127.0.0.1");
        d.readU32(13, &utmp, m_zepPort);

        if ((utmp > 1023) && (utmp <= 65535)) {
            m_udpPort = utmp;
        } else {
            m_udpPort = m_zepPort;
        }

        d.readS32(14, &itmp, (int) ZEP);
        m_udpFormat = itmp == (int) Raw ? Raw : ZEP;
        d.readString(15, &m_logFilename, "802_15_4_log.csv");
        d.readBool(16, &m_logEnabled, false);
        d.readBool(17, &m_useFileTime, false);

        d.readU32(20, &m_rgbColor, QColor(255, 0, 255).rgb());
        d.readString(21, &m_title, "802.15.4 Demodulator");

        if (m_channelMarker)
        {
            d.readBlob(22, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(23, &m_streamIndex, 0);
        d.readBool(24, &m_useReverseAPI, false);
        d.readString(25, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(26, &utmp, 0);

        if ((utmp > 1023) && (utmp <= 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(27, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(28, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_scopeGUI)
        {
            d.readBlob(29, &bytetmp);
            m_scopeGUI->deserialize(bytetmp);
        }

        if (m_rollupState)
        {
            d.readBlob(30, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(31, &m_workspaceIndex, 0);
        d.readBlob(32, &m_geometryBytes);
        d.readBool(33, &m_hidden, false);

        for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
            d.readS32(100 + i, &m_columnIndexes[i], i);
        }
        for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
            d.readS32(200 + i, &m_columnSizes[i], -1);
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void IEEE_802_15_4_DemodSettings::applySettings(const QStringList& settingsKeys, const IEEE_802_15_4_DemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("phy")) {
        m_phy = settings.m_phy;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("samplesPerChip")) {
        m_samplesPerChip = settings.m_samplesPerChip;
    }
    if (settingsKeys.contains("maxFrequencyOffset")) {
        m_maxFrequencyOffset = settings.m_maxFrequencyOffset;
    }
    if (settingsKeys.contains("threshold")) {
        m_threshold = settings.m_threshold;
    }
    if (settingsKeys.contains("showBadFCS")) {
        m_showBadFCS = settings.m_showBadFCS;
    }
    if (settingsKeys.contains("filterFrameType")) {
        m_filterFrameType = settings.m_filterFrameType;
    }
    if (settingsKeys.contains("filterPANID")) {
        m_filterPANID = settings.m_filterPANID;
    }
    if (settingsKeys.contains("filterAddress")) {
        m_filterAddress = settings.m_filterAddress;
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
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
    if (settingsKeys.contains("columnIndexes"))
    {
        for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
            m_columnIndexes[i] = settings.m_columnIndexes[i];
        }
    }
    if (settingsKeys.contains("columnSizes"))
    {
        for (int i = 0; i < IEEE_802_15_4_DEMOD_COLUMNS; i++) {
            m_columnSizes[i] = settings.m_columnSizes[i];
        }
    }
}

QString IEEE_802_15_4_DemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("phy") || force) {
        ostr << " m_phy: " << m_phy.toStdString();
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("samplesPerChip") || force) {
        ostr << " m_samplesPerChip: " << m_samplesPerChip;
    }
    if (settingsKeys.contains("maxFrequencyOffset") || force) {
        ostr << " m_maxFrequencyOffset: " << m_maxFrequencyOffset;
    }
    if (settingsKeys.contains("threshold") || force) {
        ostr << " m_threshold: " << m_threshold;
    }
    if (settingsKeys.contains("showBadFCS") || force) {
        ostr << " m_showBadFCS: " << m_showBadFCS;
    }
    if (settingsKeys.contains("filterFrameType") || force) {
        ostr << " m_filterFrameType: " << m_filterFrameType;
    }
    if (settingsKeys.contains("filterPANID") || force) {
        ostr << " m_filterPANID: " << m_filterPANID.toStdString();
    }
    if (settingsKeys.contains("filterAddress") || force) {
        ostr << " m_filterAddress: " << m_filterAddress.toStdString();
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
