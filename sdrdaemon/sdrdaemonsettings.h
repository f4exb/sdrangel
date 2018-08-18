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

#ifndef SDRDAEMON_SDRDAEMONSETTINGS_H_
#define SDRDAEMON_SDRDAEMONSETTINGS_H_

#include "sdrdaemonpreferences.h"

class SDRDaemonSettings
{
public:
    SDRDaemonSettings();
    ~SDRDaemonSettings();

    void load();
    void save() const;

    void resetToDefaults();

    void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setConsoleMinLogLevel(minLogLevel); }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_preferences.setFileMinLogLevel(minLogLevel); }
    void setUseLogFile(bool useLogFile) { m_preferences.setUseLogFile(useLogFile); }
    void setLogFileName(const QString& value) { m_preferences.setLogFileName(value); }
    QtMsgType getConsoleMinLogLevel() const { return m_preferences.getConsoleMinLogLevel(); }
    QtMsgType getFileMinLogLevel() const { return m_preferences.getFileMinLogLevel(); }
    bool getUseLogFile() const { return m_preferences.getUseLogFile(); }
    const QString& getLogFileName() const { return m_preferences.getLogFileName(); }

private:
    SDRDaemonPreferences m_preferences;
};



#endif /* SDRDAEMON_SDRDAEMONSETTINGS_H_ */
