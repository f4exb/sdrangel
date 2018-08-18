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

#ifndef SDRDAEMON_SDRDAEMONPREFERENCES_H_
#define SDRDAEMON_SDRDAEMONPREFERENCES_H_

#include <QString>

class SDRDaemonPreferences
{
public:
    SDRDaemonPreferences();

    void resetToDefaults();
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

    void setConsoleMinLogLevel(const QtMsgType& minLogLevel) { m_consoleMinLogLevel = minLogLevel; }
    void setFileMinLogLevel(const QtMsgType& minLogLevel) { m_fileMinLogLevel = minLogLevel; }
    void setUseLogFile(bool useLogFile) { m_useLogFile = useLogFile; }
    void setLogFileName(const QString& value) { m_logFileName = value; }
    QtMsgType getConsoleMinLogLevel() const { return m_consoleMinLogLevel; }
    QtMsgType getFileMinLogLevel() const { return m_fileMinLogLevel; }
    bool getUseLogFile() const { return m_useLogFile; }
    const QString& getLogFileName() const { return m_logFileName; }

private:
    QtMsgType m_consoleMinLogLevel;
    QtMsgType m_fileMinLogLevel;
    bool m_useLogFile;
    QString m_logFileName;
};

#endif /* SDRDAEMON_SDRDAEMONPREFERENCES_H_ */
