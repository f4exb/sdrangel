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
#include "sdrdaemonsourcesettings.h"

SDRdaemonSourceSettings::SDRdaemonSourceSettings()
{
    resetToDefaults();
}

void SDRdaemonSourceSettings::resetToDefaults()
{
    m_centerFrequency = 435000*1000;
    m_sampleRate = 256000;
    m_log2Decim = 1;
    m_txDelay = 0.5;
    m_nbFECBlocks = 0;
    m_address = "127.0.0.1";
    m_dataPort = 9092;
    m_controlPort = 9093;
    m_specificParameters = "";
    m_dcBlock = false;
    m_iqCorrection = false;
    m_fcPos = 2; // center
}

QByteArray SDRdaemonSourceSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeU64(1, m_sampleRate);
    s.writeU32(2, m_log2Decim);
    s.writeFloat(3, m_txDelay);
    s.writeU32(4, m_nbFECBlocks);
    s.writeString(5, m_address);
    s.writeU32(6, m_dataPort);
    s.writeU32(7, m_controlPort);
    s.writeString(8, m_specificParameters);
    s.writeBool(9, m_dcBlock);
    s.writeBool(10, m_iqCorrection);
    s.writeU32(11, m_fcPos);

    return s.final();
}

bool SDRdaemonSourceSettings::deserialize(const QByteArray& data)
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
        d.readU64(1, &m_sampleRate, 48000);
        d.readU32(2, &m_log2Decim, 0);
        d.readFloat(3, &m_txDelay, 0.5);
        d.readU32(4, &m_nbFECBlocks, 0);
        d.readString(5, &m_address, "127.0.0.1");
        d.readU32(6, &uintval, 9090);
        m_dataPort = uintval % (1<<16);
        d.readU32(7, &uintval, 9090);
        m_controlPort = uintval % (1<<16);
        d.readString(8, &m_specificParameters, "");
        d.readBool(9, &m_dcBlock, false);
        d.readBool(10, &m_iqCorrection, false);
        d.readU32(11, &m_fcPos, 2);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}



