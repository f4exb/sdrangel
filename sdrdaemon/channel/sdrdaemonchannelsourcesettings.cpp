///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon sink channel (Rx) main settings                                     //
//                                                                               //
// SDRdaemon is a detached SDR front end that handles the interface with a       //
// physical device and sends or receives the I/Q samples stream to or from a     //
// SDRangel instance via UDP. It is controlled via a Web REST API.               //
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
#include "settings/serializable.h"
#include "channel/sdrdaemonchannelsourcesettings.h"

SDRDaemonChannelSourceSettings::SDRDaemonChannelSourceSettings()
{
    resetToDefaults();
}

void SDRDaemonChannelSourceSettings::resetToDefaults()
{
    m_dataAddress = "127.0.0.1";
    m_dataPort = 9090;
}

QByteArray SDRDaemonChannelSourceSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeString(1, m_dataAddress);
    s.writeU32(2, m_dataPort);

    return s.final();
}

bool SDRDaemonChannelSourceSettings::deserialize(const QByteArray& data)
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

        d.readString(1, &m_dataAddress, "127.0.0.1");
        d.readU32(2, &tmp, 0);

        if ((tmp > 1023) && (tmp < 65535)) {
            m_dataPort = tmp;
        } else {
            m_dataPort = 9090;
        }

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}







