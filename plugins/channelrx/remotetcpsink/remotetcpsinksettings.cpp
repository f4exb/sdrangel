///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#include "remotetcpsinksettings.h"

#include <QColor>

#include "util/simpleserializer.h"
#include "settings/serializable.h"


RemoteTCPSinkSettings::RemoteTCPSinkSettings()
{
    resetToDefaults();
}

void RemoteTCPSinkSettings::resetToDefaults()
{
    m_channelSampleRate = 48000;
    m_inputFrequencyOffset = 0;
    m_gain = 0;
    m_sampleBits = 8;
    m_dataAddress = "0.0.0.0";
    m_dataPort = 1234;
    m_protocol = SDRA;
    m_rgbColor = QColor(140, 4, 4).rgb();
    m_title = "Remote TCP sink";
    m_channelMarker = nullptr;
    m_rollupState = nullptr;
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray RemoteTCPSinkSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_channelSampleRate);
    s.writeS32(2, m_inputFrequencyOffset);
    s.writeS32(3, m_gain);
    s.writeU32(4, m_sampleBits);
    s.writeString(5, m_dataAddress);
    s.writeU32(6, m_dataPort);
    s.writeS32(7, (int)m_protocol);
    s.writeU32(8, m_rgbColor);
    s.writeString(9, m_title);
    s.writeBool(10, m_useReverseAPI);
    s.writeString(11, m_reverseAPIAddress);
    s.writeU32(12, m_reverseAPIPort);
    s.writeU32(13, m_reverseAPIDeviceIndex);
    s.writeU32(14, m_reverseAPIChannelIndex);
    s.writeS32(17, m_streamIndex);

    if (m_rollupState) {
        s.writeBlob(18, m_rollupState->serialize());
    }

    if (m_channelMarker) {
        s.writeBlob(19, m_channelMarker->serialize());
    }

    s.writeS32(20, m_workspaceIndex);
    s.writeBlob(21, m_geometryBytes);
    s.writeBool(22, m_hidden);

    return s.final();
}

bool RemoteTCPSinkSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if(!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        uint32_t tmp;
        QString strtmp;
        QByteArray bytetmp;

        d.readS32(1, &m_channelSampleRate, 48000);
        d.readS32(2, &m_inputFrequencyOffset, 0);
        d.readS32(3, &m_gain, 0);
        d.readU32(4, &m_sampleBits, 8);
        d.readString(5, &m_dataAddress, "0.0.0.0");
        d.readU32(6, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_dataPort = tmp;
        } else {
            m_dataPort = 1234;
        }
        d.readS32(7, (int *)&m_protocol, (int)SDRA);

        d.readU32(8, &m_rgbColor, QColor(0, 255, 255).rgb());
        d.readString(9, &m_title, "Remote TCP sink");
        d.readBool(10, &m_useReverseAPI, false);
        d.readString(11, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(12, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_reverseAPIPort = tmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(13, &tmp, 0);
        m_reverseAPIDeviceIndex = tmp > 99 ? 99 : tmp;
        d.readU32(14, &tmp, 0);
        m_reverseAPIChannelIndex = tmp > 99 ? 99 : tmp;
        d.readS32(17, &m_streamIndex, 0);

        if (m_rollupState)
        {
            d.readBlob(18, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        if (m_channelMarker)
        {
            d.readBlob(19, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readS32(20, &m_workspaceIndex, 0);
        d.readBlob(21, &m_geometryBytes);
        d.readBool(22, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RemoteTCPSinkSettings::applySettings(const QStringList& settingsKeys, const RemoteTCPSinkSettings& settings)
{
    if (settingsKeys.contains("channelSampleRate")) {
        m_channelSampleRate = settings.m_channelSampleRate;
    }
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("gain")) {
        m_gain = settings.m_gain;
    }
    if (settingsKeys.contains("sampleBits")) {
        m_sampleBits = settings.m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress")) {
        m_dataAddress = settings.m_dataAddress;
    }
    if (settingsKeys.contains("dataPort")) {
        m_dataPort = settings.m_dataPort;
    }
    if (settingsKeys.contains("protocol")) {
        m_protocol = settings.m_protocol;
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

QString RemoteTCPSinkSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("channelSampleRate") || force) {
        ostr << " m_channelSampleRate: " << m_channelSampleRate;
    }
    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("gain") || force) {
        ostr << " m_gain: " << m_gain;
    }
    if (settingsKeys.contains("sampleBits") || force) {
        ostr << " m_sampleBits: " << m_sampleBits;
    }
    if (settingsKeys.contains("dataAddress") || force) {
        ostr << " m_dataAddress: " << m_dataAddress.toStdString();
    }
    if (settingsKeys.contains("dataPort") || force) {
        ostr << " m_dataPort: " << m_dataPort;
    }
    if (settingsKeys.contains("protocol") || force) {
        ostr << " m_protocol: " << m_protocol;
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
