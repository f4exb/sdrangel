///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon instance                                                            //
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
#include "sdrdaemonpreferences.h"

SDRDaemonPreferences::SDRDaemonPreferences()
{
    resetToDefaults();
}

void SDRDaemonPreferences::resetToDefaults()
{
    m_useLogFile = false;
    m_logFileName = "sdrangel.log";
    m_consoleMinLogLevel = QtDebugMsg;
    m_fileMinLogLevel = QtDebugMsg;
}

QByteArray SDRDaemonPreferences::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, (int) m_consoleMinLogLevel);
    s.writeBool(2, m_useLogFile);
    s.writeString(3, m_logFileName);
    s.writeS32(4, (int) m_fileMinLogLevel);
    return s.final();
}

bool SDRDaemonPreferences::deserialize(const QByteArray& data)
{
    int tmpInt;

    SimpleDeserializer d(data);

    if(!d.isValid()) {
        resetToDefaults();
        return false;
    }

    if(d.getVersion() == 1)
    {
        d.readS32(1, &tmpInt, (int) QtDebugMsg);

        if ((tmpInt == (int) QtDebugMsg) ||
            (tmpInt == (int) QtInfoMsg) ||
            (tmpInt == (int) QtWarningMsg) ||
            (tmpInt == (int) QtCriticalMsg) ||
            (tmpInt == (int) QtFatalMsg)) {
            m_consoleMinLogLevel = (QtMsgType) tmpInt;
        } else {
            m_consoleMinLogLevel = QtDebugMsg;
        }

        d.readBool(2, &m_useLogFile, false);
        d.readString(3, &m_logFileName, "sdrangel.log");

        d.readS32(4, &tmpInt, (int) QtDebugMsg);

        if ((tmpInt == (int) QtDebugMsg) ||
            (tmpInt == (int) QtInfoMsg) ||
            (tmpInt == (int) QtWarningMsg) ||
            (tmpInt == (int) QtCriticalMsg) ||
            (tmpInt == (int) QtFatalMsg)) {
            m_fileMinLogLevel = (QtMsgType) tmpInt;
        } else {
            m_fileMinLogLevel = QtDebugMsg;
        }

        return true;
    } else
    {
        resetToDefaults();
        return false;
    }
}
