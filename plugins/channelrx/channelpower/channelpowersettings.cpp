///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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
#include "channelpowersettings.h"

ChannelPowerSettings::ChannelPowerSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void ChannelPowerSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 10000.0f;
    m_pulseThreshold= -50.0f;
    m_averagePeriodUS = 100000;
    m_frequencyMode = Offset;
    m_frequency = 0;
    m_rgbColor = QColor(102, 40, 220).rgb();
    m_title = "Channel Power";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray ChannelPowerSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeS32(1, m_inputFrequencyOffset);
    s.writeFloat(2, m_rfBandwidth);
    s.writeFloat(3, m_pulseThreshold);
    s.writeS32(4, m_averagePeriodUS);
    s.writeS32(5, (int) m_frequencyMode);
    s.writeS64(6, m_frequency);

    s.writeU32(21, m_rgbColor);
    s.writeString(22, m_title);

    if (m_channelMarker) {
        s.writeBlob(23, m_channelMarker->serialize());
    }

    s.writeS32(24, m_streamIndex);
    s.writeBool(25, m_useReverseAPI);
    s.writeString(26, m_reverseAPIAddress);
    s.writeU32(27, m_reverseAPIPort);
    s.writeU32(28, m_reverseAPIDeviceIndex);
    s.writeU32(29, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(30, m_rollupState->serialize());
    }

    s.writeS32(32, m_workspaceIndex);
    s.writeBlob(33, m_geometryBytes);
    s.writeBool(34, m_hidden);

    return s.final();
}

bool ChannelPowerSettings::deserialize(const QByteArray& data)
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
        d.readFloat(2, &m_rfBandwidth, 10000.0f);
        d.readFloat(3, &m_pulseThreshold, 50.0f);
        d.readS32(4, &m_averagePeriodUS, 100000);
        d.readS32(5, (int *) &m_frequencyMode, (int) Offset);
        d.readS64(6, &m_frequency);

        d.readU32(21, &m_rgbColor, QColor(102, 40, 220).rgb());
        d.readString(22, &m_title, "Channel Power");

        if (m_channelMarker)
        {
            d.readBlob(23, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(24, &m_streamIndex, 0);
        d.readBool(25, &m_useReverseAPI, false);
        d.readString(26, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(27, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(28, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(29, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(30, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(32, &m_workspaceIndex, 0);
        d.readBlob(33, &m_geometryBytes);
        d.readBool(34, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void ChannelPowerSettings::applySettings(const QStringList& settingsKeys, const ChannelPowerSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("pulseThreshold")) {
        m_pulseThreshold = settings.m_pulseThreshold;
    }
    if (settingsKeys.contains("averagePeriodUS")) {
        m_averagePeriodUS = settings.m_averagePeriodUS;
    }
    if (settingsKeys.contains("frequencyMode")) {
        m_frequencyMode = settings.m_frequencyMode;
    }
    if (settingsKeys.contains("frequency")) {
        m_frequency = settings.m_frequency;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
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
}

QString ChannelPowerSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("pulseThreshold") || force) {
        ostr << " m_pulseThreshold: " << m_pulseThreshold;
    }
    if (settingsKeys.contains("averagePeriodUS") || force) {
        ostr << " m_averagePeriodUS: " << m_averagePeriodUS;
    }
    if (settingsKeys.contains("frequencyMode") || force) {
        ostr << " m_frequencyMode: " << m_frequencyMode;
    }
    if (settingsKeys.contains("frequency") || force) {
        ostr << " m_frequency: " << m_frequency;
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

    return QString(ostr.str().c_str());
}
