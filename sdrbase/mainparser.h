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

private:
    QString  m_serverAddress;
    uint16_t m_serverPort;
    QString  m_fftwfWindowFileName;
    bool m_scratch;
    bool m_soapy;

    QCommandLineParser m_parser;
    QCommandLineOption m_serverAddressOption;
    QCommandLineOption m_serverPortOption;
    QCommandLineOption m_fftwfWisdomOption;
    QCommandLineOption m_scratchOption;
    QCommandLineOption m_soapyOption;
};



#endif /* SDRBASE_MAINPARSER_H_ */
