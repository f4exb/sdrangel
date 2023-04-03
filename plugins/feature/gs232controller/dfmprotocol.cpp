///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                        //
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

#include <QDebug>

#include "util/astronomy.h"

#include "dfmprotocol.h"

MESSAGE_CLASS_DEFINITION(DFMProtocol::MsgReportDFMStatus, Message)

DFMProtocol::DFMProtocol() :
    m_packetCnt(0)
{
    // Call periodicTask() every 500ms
    connect(&m_timer, &QTimer::timeout, this, &DFMProtocol::periodicTask);
    m_timer.start(500);
}

DFMProtocol::~DFMProtocol()
{
    m_timer.stop();
}

void DFMProtocol::setAzimuthElevation(float azimuth, float elevation)
{
    // This gets position from source plugin in track is enabled (E.g. Star Tracker / Satellite tracker)
    // or My Position preference, if not tracking
    float latitude, longitude;
    getPosition(latitude, longitude);

    // Convert az/el to RA/Dec
    AzAlt aa;
    aa.az = azimuth;
    aa.alt = elevation;
    QDateTime dt = QDateTime::currentDateTime();
    RADec rd = Astronomy::azAltToRaDec(aa, latitude, longitude, dt);
    // Save as target
    m_targetRA = rd.ra;
    m_targetDec = rd.dec;

    // Call parent method to save m_lastAzimuth and m_lastElevation
    ControllerProtocol::setAzimuthElevation(azimuth, elevation);
}

// Handle data received from LCU
// Packets are of the form #L,L,f,f,..,f;
void DFMProtocol::readData()
{
    char c;
    while (m_device->getChar(&c))
    {
        if (c == '#')
        {
            // Start packet
            m_rxBuffer = QString(c);
        }
        else if (c == ';')
        {
            // End packet
            m_rxBuffer.append(c);

            // Only process if we have valid packet
            if (m_rxBuffer.startsWith('#'))
            {
                parseLCUResponse(m_rxBuffer);
                m_rxBuffer = "";
            }
            else
            {
                qDebug() << "DFMProtocol::readData - Ignoring partial packet: " << m_rxBuffer;
            }
        }
        else
        {
            m_rxBuffer.append(c);
        }
    }
}

void DFMProtocol::parseLCUResponse(const QString& packet)
{
    qDebug() << "DFMProtocol::parseLCUResponse - " << packet;

    // Check packet starts with expected header
    if (!packet.startsWith("#L,L,"))
    {
        qDebug() << "DFMProtocol::readData - Ignoring non LCU packet: " << m_rxBuffer;
        return;
    }

    // Strip off header and footer
    QString strippedPacket = packet.mid(5, packet.length() - 6);

    // Convert packet to list of strings
    QStringList dataStrings = strippedPacket.split(",");

    // Extract values we are interested in
    DFMStatus status;

    int statl = (int)dataStrings[1].toFloat();
    status.m_initialized = statl & 1;
    status.m_brakesOn = (statl >> 1) & 1;
    status.m_trackOn = (statl >> 2) & 1;
    status.m_slewEnabled = (statl >> 3) & 1;
    status.m_lubePumpsOn = (statl >> 4) & 1;
    status.m_approachingSWLimit = (statl >> 5) & 1;
    status.m_finalSWLimit = (statl >> 6) & 1;
    status.m_slewing = (statl >> 7) & 1;

    int stath = (int)dataStrings[2].toFloat();
    status.m_setting = stath & 1;
    status.m_haltMotorsIn = (stath >> 1) & 1;
    status.m_excomSwitchOn = (stath >> 2) & 1;
    status.m_servoPackAlarm = (stath >> 3) & 1;
    status.m_targetOutOfRange = (stath >> 4) & 1;
    status.m_cosdecOn = (stath >> 5) & 1;
    status.m_rateCorrOn = (stath >> 6) & 1;
    status.m_drivesOn = (stath >> 7) & 1;

    int statlh = (int)dataStrings[3].toFloat();
    status.m_pumpsReady = statlh & 1;
    // Bit 1 unknown
    status.m_minorPlus = (statlh >> 2) & 1;
    status.m_minorMinus = (statlh >> 3) & 1;
    status.m_majorPlus = (statlh >> 4) & 1;
    status.m_majorMinus = (statlh >> 5) & 1;
    status.m_nextObjectActive = (statlh >> 6) & 1;
    status.m_auxTrackRate = (statlh >> 7) & 1;

    status.m_siderealTime = dataStrings[5].toFloat();
    status.m_universalTime = dataStrings[6].toFloat();

    status.m_currentHA = dataStrings[7].toFloat();
    status.m_currentRA = dataStrings[8].toFloat();
    status.m_currentDec = dataStrings[9].toFloat();

    status.m_currentX = dataStrings[20].toFloat();
    status.m_currentY = dataStrings[21].toFloat();

    status.m_siderealRateX = dataStrings[30].toFloat();
    status.m_siderealRateY = dataStrings[31].toFloat();

    status.m_torqueX = dataStrings[34].toFloat();
    status.m_torqueY = dataStrings[35].toFloat();

    status.m_controller = (DFMStatus::Controller)dataStrings[38].toInt();

    status.m_rateX = dataStrings[39].toFloat();
    status.m_rateY = dataStrings[40].toFloat();

    // Display status in GUI
    sendMessage(MsgReportDFMStatus::create(status));

    // Convert current X/Y to Az/El
    AzAlt aa = Astronomy::xy85ToAzAlt(status.m_currentX, status.m_currentY);
    float az = aa.az;
    float el = aa.alt;
    reportAzEl(az, el);

    // If this is the second LCU packet, we send a commmand
    m_packetCnt++;
    if (m_packetCnt == 2)
    {
        m_packetCnt = 0;
        sendCommand();
    }
}

void DFMProtocol::sendCommand()
{
    // TODO: Use m_lastAzimuth/m_lastElevation or m_targetRA/m_targetDec to calculate position commands

    // Send a command to the LCU
    int cmdId = 98;
    int handPaddle = 0;
    int frontPanel = (m_settings.m_dfmDrivesOn << 2)
                    | (m_settings.m_dfmTrackOn << 3)
                    | (m_settings.m_dfmLubePumpsOn << 4)
                    | (m_settings.m_dfmBrakesOn << 7);

    QString cmd = QString("#M,R,%1,%2.000000,%3.000000;").arg(cmdId).arg(handPaddle).arg(frontPanel);
    m_device->write(cmd.toLatin1());

    qDebug() << "DFMProtocol::sendCommand - " << cmd;
}

// Request current Az/El
void DFMProtocol::update()
{
    // This is called periodically for protocols that need to send a command to get current az/el
    // However, for this protocol, we might not need to do anything here,
    // if we're continually calling reportAzEl() in response to packets received from the LCU.
    //sendCommand();
}

void DFMProtocol::periodicTask()
{
    // Just as an example, this will be called every 500ms. Can be removed if not needed
}

// This is called when new settings are available from GUI (or API).
void DFMProtocol::applySettings(const GS232ControllerSettings& settings, const QList<QString>& settingsKeys, bool force)
{
    if (settingsKeys.contains("dfmTrackOn") || force)
    {
        // Do something with settings.m_dfmTrackOn if needed
    }
    if (settingsKeys.contains("dfmLubePumpsOn") || force)
    {
        // Do something with settings.m_dfmLubePumpsOn if needed
    }
    if (settingsKeys.contains("dfmBrakesOn") || force)
    {
        // Do something with settings.m_dfmBreaksOn if needed
    }
    if (settingsKeys.contains("dfmDrivesOn") || force)
    {
        // Do something with settings.m_dfmDrivesOn if needed
    }

    // Call parent method to set m_settings to settings
    ControllerProtocol::applySettings(settings, settingsKeys, force);
}

