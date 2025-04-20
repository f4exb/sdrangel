///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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
            // Handle both formats:
            // 1. AZ=XXX EL=XXX (MD-02 can return negative angles like AZ=-00 EL=-00)
            // 2. +XXXX+YYYY (direct angle format)
            QRegularExpression reAzEl("AZ=([-\\d]\\d\\d) *EL=([-\\d]\\d\\d)");
            QRegularExpression reAngles("([+-]\\d{4})([+-]\\d{4})");

            QRegularExpressionMatch matchAzEl = reAzEl.match(response);
            QRegularExpressionMatch matchAngles = reAngles.match(response);
            if (matchAzEl.hasMatch())
            {
                QString az = matchAzEl.captured(1);
                QString el = matchAzEl.captured(2);
                qDebug() << "GS232Protocol::readData read Az " << az << " El " << el;
                reportAzEl(az.toFloat(), el.toFloat());
            }
            else if (matchAngles.hasMatch())
            {
                // Convert from +XXXX format to float
                QString az = matchAngles.captured(1);
                QString el = matchAngles.captured(2);
                qDebug() << "GS232Protocol::readData read direct angles Az " << az << " El " << el;
                // The format gives angles in tenths of a degree, so divide by 10
                reportAzEl(az.toFloat()/10.0f, el.toFloat()/10.0f);
            }
            else if (response == "\r\n")
            {
                // Ignore
            }
            else
            {
                qWarning() << "GS232Protocol::readData - unexpected GS-232 response \"" << response << "\"";
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

