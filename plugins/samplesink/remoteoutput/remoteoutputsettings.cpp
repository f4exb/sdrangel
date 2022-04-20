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
