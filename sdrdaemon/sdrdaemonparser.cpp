///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2018 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// SDRdaemon command line parser                                                 //
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

#include "sdrdaemonparser.h"

#include <QCommandLineOption>
#include <QRegExpValidator>
#include <QDebug>

SDRDaemonParser::SDRDaemonParser() :
    m_serverAddressOption(QStringList() << "a" << "api-address",
        "Web API server address.",
        "address",
        "127.0.0.1"),
    m_serverPortOption(QStringList() << "p" << "api-port",
        "Web API server port.",
        "port",
        "9091"),
    m_deviceTypeOption(QStringList() << "T" << "device-type",
        "Device type.",
        "deviceType",
        "TestSource"),
    m_txOption(QStringList() << "t" << "tx",
        "Tx indicator.",
        "tx"),
    m_serialOption(QStringList() << "s" << "serial",
        "Device serial number.",
        "serial"),
    m_sequenceOption(QStringList() << "n" << "sequence",
        "Device sequence number in enumeration for the same device type.",
        "serial")

{
    m_serverAddress = "127.0.0.1";
    m_serverPort = 9091;
    m_deviceType = "TestSource";
    m_tx = false;
    m_sequence = 0;
    m_hasSequence = false;
    m_hasSerial = false;

    m_parser.setApplicationDescription("Software Defined Radio RF header server");
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(m_serverAddressOption);
    m_parser.addOption(m_serverPortOption);
    m_parser.addOption(m_deviceTypeOption);
    m_parser.addOption(m_txOption);
    m_parser.addOption(m_serialOption);
    m_parser.addOption(m_sequenceOption);
}

SDRDaemonParser::~SDRDaemonParser()
{ }

void SDRDaemonParser::parse(const QCoreApplication& app)
{
    m_parser.process(app);

    int pos;
    bool ok;

    // server address

    QString serverAddress = m_parser.value(m_serverAddressOption);

    QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
    QRegExp ipRegex ("^" + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange
                     + "\\." + ipRange + "$");
    QRegExpValidator ipValidator(ipRegex);

    if (ipValidator.validate(serverAddress, pos) == QValidator::Acceptable) {
        m_serverAddress = serverAddress;
    } else {
        qWarning() << "SDRDaemonParser::parse: server address invalid. Defaulting to " << m_serverAddress;
    }

    // server port

    QString serverPortStr = m_parser.value(m_serverPortOption);
    int serverPort = serverPortStr.toInt(&ok);

    if (ok && (serverPort > 1023) && (serverPort < 65536)) {
        m_serverPort = serverPort;
    } else {
        qWarning() << "SDRDaemonParser::parse: server port invalid. Defaulting to " << m_serverPort;
    }

    // hardware Id

    QString deviceType = m_parser.value(m_deviceTypeOption);

    QRegExp deviceTypeRegex("^[A-Z][a-z][A-Za-z0-9]+$");
    QRegExpValidator deviceTypeValidator(deviceTypeRegex);

    if (deviceTypeValidator.validate(deviceType, pos) == QValidator::Acceptable) {
        m_deviceType = deviceType;
    } else {
        qWarning() << "SDRDaemonParser::parse: device type invalid. Defaulting to " << deviceType;
    }

    // tx
    m_tx = m_parser.isSet(m_txOption);

    // serial
    m_hasSerial = m_parser.isSet(m_serialOption);

    if (m_hasSerial) {
        m_serial = m_parser.value(m_serialOption);
    }

    // sequence
    m_hasSequence = m_parser.isSet(m_sequenceOption);

    if (m_hasSequence)
    {
        QString sequenceStr = m_parser.value(m_sequenceOption);
        int sequence = sequenceStr.toInt(&ok);

        if (ok && (sequence >= 0) && (sequence < 65536)) {
            m_sequence = sequence;
        } else {
            qWarning() << "SDRDaemonParser::parse: sequence invalid. Defaulting to " << m_sequence;
        }
    }
}



