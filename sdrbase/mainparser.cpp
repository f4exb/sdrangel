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

#include <QCommandLineOption>
#include <QRegularExpressionValidator>
#include <QDebug>

#include "mainparser.h"

MainParser::MainParser() :
    m_serverAddressOption(QStringList() << "a" << "api-address",
        "Web API server address.",
        "address",
        ""),
    m_serverPortOption(QStringList() << "p" << "api-port",
        "Web API server port.",
        "port",
        "8091"),
    m_fftwfWisdomOption(QStringList() << "w" << "fftwf-wisdom",
        "FFTW Wisdom file.",
        "file",
        ""),
    m_scratchOption("scratch", "Start from scratch (no current config)."),
    m_soapyOption("soapy", "Activate Soapy SDR support."),
    m_remoteTCPSinkOption("remote-tcp", "Start Remote TCP Sink"),
    m_remoteTCPSinkAddressOption("remote-tcp-address", "Remote TCP Sink interface IP address (Default 127.0.0.1).", "address", "127.0.0.1"),
    m_remoteTCPSinkPortOption("remote-tcp-port", "Remote TCP Sink port (Default 1234).", "port", "1234"),
    m_remoteTCPSinkHWTypeOption("remote-tcp-hwtype", "Remote TCP Sink device hardware type (Optional. E.g. RTLSDR/SDRplayV3/AirspyHF).", "hwtype"),
    m_remoteTCPSinkSerialOption("remote-tcp-serial", "Remote TCP Sink device serial (Optional).", "serial"),
    m_listDevicesOption("list-devices", "List available physical devices.")
{

    m_serverAddress = "";   // Bind to any address
    m_serverPort = 8091;
    m_scratch = false;
    m_soapy = false;
    m_fftwfWindowFileName = "";
    m_remoteTCPSink = false;
    m_remoteTCPSinkAddress = "127.0.0.1";
    m_remoteTCPSinkPort = 1234;
    m_remoteTCPSinkHWType = "";
    m_remoteTCPSinkSerial = "";
    m_listDevices = false;

    m_parser.setApplicationDescription("Software Defined Radio application");
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(m_serverAddressOption);
    m_parser.addOption(m_serverPortOption);
    m_parser.addOption(m_fftwfWisdomOption);
    m_parser.addOption(m_scratchOption);
    m_parser.addOption(m_soapyOption);
    m_parser.addOption(m_remoteTCPSinkOption);
    m_parser.addOption(m_remoteTCPSinkAddressOption);
    m_parser.addOption(m_remoteTCPSinkPortOption);
    m_parser.addOption(m_remoteTCPSinkHWTypeOption);
    m_parser.addOption(m_remoteTCPSinkSerialOption);
    m_parser.addOption(m_listDevicesOption);
}

MainParser::~MainParser()
{ }

void MainParser::parse(const QCoreApplication& app)
{
    m_parser.process(app);

    int pos;
    bool ok;
    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegularExpression ipRegex ("^" + ipRange
                                 + "\\." + ipRange
                                 + "\\." + ipRange
                                 + "\\." + ipRange + "$");
    QRegularExpressionValidator ipValidator(ipRegex);

    // server address

    QString serverAddress = m_parser.value(m_serverAddressOption);
    if (!serverAddress.isEmpty())
    {
        if (ipValidator.validate(serverAddress, pos) == QValidator::Acceptable) {
            m_serverAddress = serverAddress;
        } else {
            qWarning() << "MainParser::parse: server address invalid. Defaulting to any address.";
        }
    }

    // server port

    QString serverPortStr = m_parser.value(m_serverPortOption);
    int serverPort = serverPortStr.toInt(&ok);

    if (ok && (serverPort > 1023) && (serverPort < 65536)) {
        m_serverPort = serverPort;
    } else {
        qWarning() << "MainParser::parse: server port invalid. Defaulting to " << m_serverPort;
    }

    // FFTWF wisdom file
    m_fftwfWindowFileName = m_parser.value(m_fftwfWisdomOption);

    // Scratch mode
    m_scratch = m_parser.isSet(m_scratchOption);

    // Soapy SDR support
    m_soapy = m_parser.isSet(m_soapyOption);

    // Remote TCP Sink options

    m_remoteTCPSink = m_parser.isSet(m_remoteTCPSinkOption);

    QString remoteTCPSinkAddress = m_parser.value(m_remoteTCPSinkAddressOption);
    if (!remoteTCPSinkAddress.isEmpty())
    {
        if (ipValidator.validate(remoteTCPSinkAddress, pos) == QValidator::Acceptable) {
            m_remoteTCPSinkAddress = remoteTCPSinkAddress;
        } else {
            qWarning() << "MainParser::parse: remote TCP Sink address invalid. Defaulting to " << m_remoteTCPSinkAddress;
        }
    }

    QString remoteTCPSinkPortStr = m_parser.value(m_remoteTCPSinkPortOption);
    int remoteTCPSinkPort = remoteTCPSinkPortStr.toInt(&ok);

    if (ok && (remoteTCPSinkPort > 1023) && (remoteTCPSinkPort < 65536)) {
        m_remoteTCPSinkPort = remoteTCPSinkPort;
    } else {
        qWarning() << "MainParser::parse: remote TCP Sink port invalid. Defaulting to " << m_serverPort;
    }

    m_remoteTCPSinkHWType = m_parser.value(m_remoteTCPSinkHWTypeOption);
    m_remoteTCPSinkSerial = m_parser.value(m_remoteTCPSinkSerialOption);
    m_listDevices = m_parser.isSet(m_listDevicesOption);

    if (m_remoteTCPSink && m_remoteTCPSinkHWType.isEmpty() && m_remoteTCPSinkSerial.isEmpty())
    {
        qCritical() << "You must specify a device with either -remote-tcp-hwtype or -remote-tcp-serial";
        exit (EXIT_FAILURE);
    }
}
