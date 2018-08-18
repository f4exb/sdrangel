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

#ifndef SDRDAEMON_SDRDAEMONPARSER_H_
#define SDRDAEMON_SDRDAEMONPARSER_H_

#include <QCommandLineParser>
#include <stdint.h>

class SDRDaemonParser
{
public:
    SDRDaemonParser();
    ~SDRDaemonParser();

    void parse(const QCoreApplication& app);

    const QString& getServerAddress() const { return m_serverAddress; }
    uint16_t getServerPort() const { return m_serverPort; }
    const QString& getDataAddress() const { return m_dataAddress; }
    uint16_t getDataPort() const { return m_dataPort; }
    const QString& getDeviceType() const { return m_deviceType; }
    bool getTx() const { return m_tx; }
    const QString& getSerial() const { return m_serial; }
    uint16_t getSequence() const { return m_sequence; }

    bool hasSequence() const { return m_hasSequence; }
    bool hasSerial() const { return m_hasSerial; }

private:
    QString  m_serverAddress; //!< Address of interface the API and UDP data (Tx) listens on
    uint16_t m_serverPort;    //!< Port the API listens on
    QString  m_dataAddress;   //!< Address of destination of UDP stream (Rx)
    uint16_t m_dataPort;      //!< Destination port of UDP stream (Rx) or listening port (Tx)
    QString  m_deviceType;    //!< Identifies the type of device
    bool     m_tx;            //!< True for Tx
    QString  m_serial;        //!< Serial number of the device
    uint16_t m_sequence;      //!< Sequence of the device for the same type of device in enumeration process
    bool     m_hasSerial;     //!< True if serial was specified
    bool     m_hasSequence;   //!< True if sequence was specified

    QCommandLineParser m_parser;
    QCommandLineOption m_serverAddressOption;
    QCommandLineOption m_serverPortOption;
    QCommandLineOption m_dataAddressOption;
    QCommandLineOption m_dataPortOption;
    QCommandLineOption m_deviceTypeOption;
    QCommandLineOption m_txOption;
    QCommandLineOption m_serialOption;
    QCommandLineOption m_sequenceOption;
};



#endif /* SDRDAEMON_SDRDAEMONPARSER_H_ */
