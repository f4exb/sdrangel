///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB                                   //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
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
#include "sdrdaemonsinksettings.h"

SDRdaemonSinkSettings::SDRdaemonSinkSettings()
{
    resetToDefaults();
}

void SDRdaemonSinkSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_sampleRate = 48000;
    m_txDelay = 0.35;
    m_nbFECBlocks = 0;
    m_apiAddress = "127.0.0.1";
    m_apiPort = 9091;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_deviceIndex = 0;
    m_channelIndex = 0;
}

QByteArray SDRdaemonSinkSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU64(1, m_centerFrequency);
    s.writeU32(2, m_sampleRate);
    s.writeFloat(3, m_txDelay);
    s.writeU32(4, m_nbFECBlocks);
    s.writeString(5, m_apiAddress);
    s.writeU32(6, m_apiPort);
    s.writeString(7, m_dataAddress);
    s.writeU32(8, m_dataPort);
    s.writeU32(10, m_deviceIndex);
    s.writeU32(11, m_channelIndex);

    return s.final();
}

bool SDRdaemonSinkSettings::deserialize(const QByteArray& data)
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

        d.readU64(1, &m_centerFrequency, 435000*1000);
        d.readU32(2, &m_sampleRate, 48000);
        d.readFloat(3, &m_txDelay, 0.5);
        d.readU32(4, &m_nbFECBlocks, 0);
        d.readString(5, &m_apiAddress, "127.0.0.1");
        d.readU32(6, &uintval, 9090);
        m_apiPort = uintval % (1<<16);
        d.readString(7, &m_dataAddress, "127.0.0.1");
        d.readU32(8, &uintval, 9090);
        m_dataPort = uintval % (1<<16);
        d.readU32(10, &m_deviceIndex, 0);
        d.readU32(11, &m_channelIndex, 0);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}
