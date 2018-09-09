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
    m_apiAddress = "127.0.0.1";
    m_apiPort = 9091;
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
    m_dcBlock = false;
    m_iqCorrection = false;
    m_fileRecordName = "";
}

QByteArray SDRdaemonSourceSettings::serialize() const
{
    SimpleSerializer s(1);

    s.writeString(5, m_apiAddress);
    s.writeU32(6, m_apiPort);
    s.writeU32(7, m_dataPort);
    s.writeString(8, m_dataAddress);
    s.writeBool(9, m_dcBlock);
    s.writeBool(10, m_iqCorrection);

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
        d.readString(5, &m_apiAddress, "127.0.0.1");
        d.readU32(6, &uintval, 9090);
        m_apiPort = uintval % (1<<16);
        d.readU32(7, &uintval, 9091);
        m_dataPort = uintval % (1<<16);
        d.readString(8, &m_dataAddress, "127.0.0.1");
        d.readBool(9, &m_dcBlock, false);
        d.readBool(10, &m_iqCorrection, false);
        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}



