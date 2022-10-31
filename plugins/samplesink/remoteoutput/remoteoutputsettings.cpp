///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
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

#include "util/simpleserializer.h"
#include "remoteoutputsettings.h"

RemoteOutputSettings::RemoteOutputSettings()
{
    resetToDefaults();
}

void RemoteOutputSettings::resetToDefaults()
{
    m_nbFECBlocks = 0;
    m_nbTxBytes = 2;
    m_apiAddress = "127.0.0.1";
    m_apiPort = 9091;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_deviceIndex = 0;
    m_channelIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray RemoteOutputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU32(3, m_nbTxBytes);
    s.writeU32(4, m_nbFECBlocks);
    s.writeString(5, m_apiAddress);
    s.writeU32(6, m_apiPort);
    s.writeString(7, m_dataAddress);
    s.writeU32(8, m_dataPort);
    s.writeU32(10, m_deviceIndex);
    s.writeU32(11, m_channelIndex);
    s.writeBool(12, m_useReverseAPI);
    s.writeString(13, m_reverseAPIAddress);
    s.writeU32(14, m_reverseAPIPort);
    s.writeU32(15, m_reverseAPIDeviceIndex);

    return s.final();
}

bool RemoteOutputSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        quint32 uintval;

        d.readU32(4, &m_nbTxBytes, 2);
        d.readU32(4, &m_nbFECBlocks, 0);
        d.readString(5, &m_apiAddress, "127.0.0.1");
        d.readU32(6, &uintval, 9090);
        m_apiPort = uintval % (1<<16);
        d.readString(7, &m_dataAddress, "127.0.0.1");
        d.readU32(8, &uintval, 9090);
        m_dataPort = uintval % (1<<16);
        d.readU32(10, &m_deviceIndex, 0);
        d.readU32(11, &m_channelIndex, 0);
        d.readBool(12, &m_useReverseAPI, false);
        d.readString(13, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(14, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(15, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RemoteOutputSettings::applySettings(const QStringList& settingsKeys, const RemoteOutputSettings& settings)
{
    if (settingsKeys.contains("nbFECBlocks")) {
        m_nbFECBlocks = settings.m_nbFECBlocks;
    }
    if (settingsKeys.contains("nbTxBytes")) {
        m_nbTxBytes = settings.m_nbTxBytes;
    }
    if (settingsKeys.contains("apiAddress")) {
        m_apiAddress = settings.m_apiAddress;
    }
    if (settingsKeys.contains("apiPort")) {
        m_apiPort = settings.m_apiPort;
    }
    if (settingsKeys.contains("dataAddress")) {
        m_dataAddress = settings.m_dataAddress;
    }
    if (settingsKeys.contains("dataPort")) {
        m_dataPort = settings.m_dataPort;
    }
    if (settingsKeys.contains("deviceIndex")) {
        m_deviceIndex = settings.m_deviceIndex;
    }
    if (settingsKeys.contains("channelIndex")) {
        m_channelIndex = settings.m_channelIndex;
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

QString RemoteOutputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("nbFECBlocks") || force) {
        ostr << " m_nbFECBlocks: " << m_nbFECBlocks;
    }
    if (settingsKeys.contains("nbTxBytes") || force) {
        ostr << " m_nbTxBytes: " << m_nbTxBytes;
    }
    if (settingsKeys.contains("apiAddress") || force) {
        ostr << " m_apiAddress: " << m_apiAddress.toStdString();
    }
    if (settingsKeys.contains("apiPort") || force) {
        ostr << " m_apiPort: " << m_apiPort;
    }
    if (settingsKeys.contains("dataAddress") || force) {
        ostr << " m_dataAddress: " << m_dataAddress.toStdString();
    }
    if (settingsKeys.contains("dataPort") || force) {
        ostr << " m_dataPort: " << m_dataPort;
    }
    if (settingsKeys.contains("deviceIndex") || force) {
        ostr << " m_deviceIndex: " << m_deviceIndex;
    }
    if (settingsKeys.contains("channelIndex") || force) {
        ostr << " m_channelIndex: " << m_channelIndex;
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

