///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
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

#ifndef SDRBASE_MAINPARSER_H_
#define SDRBASE_MAINPARSER_H_

#include <QCommandLineParser>
#include <stdint.h>

#include "export.h"

class SDRBASE_API MainParser
{
public:
    MainParser();
    ~MainParser();

    void parse(const QCoreApplication& app);

    const QString& getServerAddress() const { return m_serverAddress; }
    uint16_t getServerPort() const { return m_serverPort; }
    bool getScratch() const { return m_scratch; }
    bool getSoapy() const { return m_soapy; }
    const QString& getFFTWFWisdomFileName() const { return m_fftwfWindowFileName; }
    bool getRemoteTCPSink() const { return m_remoteTCPSink; }
    const QString& getRemoteTCPSinkAddressOption() const { return m_remoteTCPSinkAddress; }
    int getRemoteTCPSinkPortOption() const { return m_remoteTCPSinkPort; }
    const QString& getRemoteTCPSinkHWType() const { return m_remoteTCPSinkHWType; }
    const QString& getRemoteTCPSinkSerial() const { return m_remoteTCPSinkSerial; }
    bool getListDevices() const { return m_listDevices; }

private:
    QString  m_serverAddress;
    uint16_t m_serverPort;
    QString  m_fftwfWindowFileName;
    bool m_scratch;
    bool m_soapy;
    bool m_remoteTCPSink;
    QString m_remoteTCPSinkAddress;
    int m_remoteTCPSinkPort;
    QString m_remoteTCPSinkHWType;
    QString m_remoteTCPSinkSerial;
    bool m_listDevices;

    QCommandLineParser m_parser;
    QCommandLineOption m_serverAddressOption;
    QCommandLineOption m_serverPortOption;
    QCommandLineOption m_fftwfWisdomOption;
    QCommandLineOption m_scratchOption;
    QCommandLineOption m_soapyOption;
    QCommandLineOption m_remoteTCPSinkOption;
    QCommandLineOption m_remoteTCPSinkAddressOption;
    QCommandLineOption m_remoteTCPSinkPortOption;
    QCommandLineOption m_remoteTCPSinkHWTypeOption;
    QCommandLineOption m_remoteTCPSinkSerialOption;
    QCommandLineOption m_listDevicesOption;
};



#endif /* SDRBASE_MAINPARSER_H_ */
