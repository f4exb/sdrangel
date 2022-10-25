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
#include "remoteinputsettings.h"

RemoteInputSettings::RemoteInputSettings()
{
    resetToDefaults();
}

void RemoteInputSettings::resetToDefaults()
{
    m_apiAddress = "127.0.0.1";
    m_apiPort = 8091;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_multicastAddress = "224.0.0.1";
    m_multicastJoin = false;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
}

QByteArray RemoteInputSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(3, m_multicastAddress);
    s.writeBool(4, m_multicastJoin);
    s.writeString(5, m_apiAddress);
    s.writeU32(6, m_apiPort);
    s.writeU32(7, m_dataPort);
    s.writeString(8, m_dataAddress);
    s.writeBool(9, m_dcBlock);
    s.writeBool(10, m_iqCorrection);
    s.writeBool(11, m_useReverseAPI);
    s.writeString(12, m_reverseAPIAddress);
    s.writeU32(13, m_reverseAPIPort);
    s.writeU32(14, m_reverseAPIDeviceIndex);

    return s.final();
}

bool RemoteInputSettings::deserialize(const QByteArray& data)
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

        d.readString(3, &m_multicastAddress, "224.0.0.1");
        d.readBool(4, &m_multicastJoin, false);
        d.readString(5, &m_apiAddress, "127.0.0.1");
        d.readU32(6, &uintval, 8091);
        m_apiPort = uintval % (1<<16);
        d.readU32(7, &uintval, 9090);
        m_dataPort = uintval % (1<<16);
        d.readString(8, &m_dataAddress, "127.0.0.1");
        d.readBool(9, &m_dcBlock, false);
        d.readBool(10, &m_iqCorrection, false);
        d.readBool(11, &m_useReverseAPI, false);
        d.readString(12, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(13, &uintval, 0);

        if ((uintval > 1023) && (uintval < 65535)) {
            m_reverseAPIPort = uintval;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(14, &uintval, 0);
        m_reverseAPIDeviceIndex = uintval > 99 ? 99 : uintval;

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void RemoteInputSettings::applySettings(const QStringList& settingsKeys, const RemoteInputSettings& settings)
{
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
    if (settingsKeys.contains("multicastAddress")) {
        m_multicastAddress = settings.m_multicastAddress;
    }
    if (settingsKeys.contains("multicastJoin")) {
        m_multicastJoin = settings.m_multicastJoin;
    }
    if (settingsKeys.contains("dcBlock")) {
        m_dcBlock = settings.m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection")) {
        m_iqCorrection = settings.m_iqCorrection;
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

QString RemoteInputSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

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
    if (settingsKeys.contains("multicastAddress") || force) {
        ostr << " m_multicastAddress: " << m_multicastAddress.toStdString();
    }
    if (settingsKeys.contains("multicastJoin") || force) {
        ostr << " m_multicastJoin: " << m_multicastJoin;
    }
    if (settingsKeys.contains("dcBlock") || force) {
        ostr << " m_dcBlock: " << m_dcBlock;
    }
    if (settingsKeys.contains("iqCorrection") || force) {
        ostr << " m_iqCorrection: " << m_iqCorrection;
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

