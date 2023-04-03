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

#include <cmath>

#include <QDebug>
#include <QRegularExpression>

#include "gs232protocol.h"

GS232Protocol::GS232Protocol()
{
}

void GS232Protocol::setAzimuth(float azimuth)
{
    QString cmd = QString("M%1\r\n").arg((int)std::round(azimuth), 3, 10, QLatin1Char('0'));
    QByteArray data = cmd.toLatin1();
    m_device->write(data);
    m_lastAzimuth = azimuth;
}

void GS232Protocol::setAzimuthElevation(float azimuth, float elevation)
{
    QString cmd = QString("W%1 %2\r\n").arg((int)std::round(azimuth), 3, 10, QLatin1Char('0')).arg((int)std::round(elevation), 3, 10, QLatin1Char('0'));
    QByteArray data = cmd.toLatin1();
    m_device->write(data);
    ControllerProtocol::setAzimuthElevation(azimuth, elevation);
}

// Handle data received from controller
void GS232Protocol::readData()
{
    char buf[1024];
    qint64 len;

    while (m_device->canReadLine())
    {
        len = m_device->readLine(buf, sizeof(buf));
        if (len != -1)
        {
            QString response = QString::fromUtf8(buf, len);
            // MD-02 can return AZ=-00 EL=-00 and other negative angles
            QRegularExpression re("AZ=([-\\d]\\d\\d) *EL=([-\\d]\\d\\d)");
            QRegularExpressionMatch match = re.match(response);
            if (match.hasMatch())
            {
                QString az = match.captured(1);
                QString el = match.captured(2);
                //qDebug() << "SPIDProtocol::readData read Az " << az << " El " << el;
                reportAzEl(az.toFloat(), el.toFloat());
            }
            else if (response == "\r\n")
            {
                // Ignore
            }
            else
            {
                qWarning() << "SPIDProtocol::readData - unexpected GS-232 response \"" << response << "\"";
                reportError(QString("Unexpected GS-232 response: %1").arg(response));
            }
        }
    }
}

// Request current Az/El from controller
void GS232Protocol::update()
{
    QByteArray cmd("C2\r\n");
    m_device->write(cmd);
}

