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
    m_soapyOption("soapy", "Activate Soapy SDR support.")
{

    m_serverAddress = "";   // Bind to any address
    m_serverPort = 8091;
    m_scratch = false;
    m_soapy = false;
    m_fftwfWindowFileName = "";

    m_parser.setApplicationDescription("Software Defined Radio application");
    m_parser.addHelpOption();
    m_parser.addVersionOption();

    m_parser.addOption(m_serverAddressOption);
    m_parser.addOption(m_serverPortOption);
    m_parser.addOption(m_fftwfWisdomOption);
    m_parser.addOption(m_scratchOption);
    m_parser.addOption(m_soapyOption);
}

MainParser::~MainParser()
{ }

void MainParser::parse(const QCoreApplication& app)
{
    m_parser.process(app);

    int pos;
    bool ok;

    // server address

    QString serverAddress = m_parser.value(m_serverAddressOption);
    if (!serverAddress.isEmpty())
    {
        QString ipRange = "(?:[0-1]?[0-9]?[0-9]|2[0-4][0-9]|25[0-5])";
        QRegularExpression ipRegex ("^" + ipRange
                                     + "\\." + ipRange
                                     + "\\." + ipRange
                                     + "\\." + ipRange + "$");
        QRegularExpressionValidator ipValidator(ipRegex);

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
}
