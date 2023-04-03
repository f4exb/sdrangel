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

#include "spidprotocol.h"

SPIDProtocol::SPIDProtocol() :
    m_spidStatusSent(false),
    m_spidSetSent(false),
    m_spidSetOutstanding(false)
{
}

void SPIDProtocol::setAzimuthElevation(float azimuth, float elevation)
{
    if (!m_spidSetSent && !m_spidStatusSent)
    {
        QByteArray cmd(13, (char)0);

        cmd[0] = 0x57;  // Start
        int h = std::round((azimuth + 360.0f) * 2.0f);
        cmd[1] = 0x30 | (h / 1000);
        cmd[2] = 0x30 | ((h % 1000) / 100);
        cmd[3] = 0x30 | ((h % 100) / 10);
        cmd[4] = 0x30 | (h % 10);
        cmd[5] = 2; // 2 degree per impulse
        int v = std::round((elevation + 360.0f) * 2.0f);
        cmd[6] = 0x30 | (v / 1000);
        cmd[7] = 0x30 | ((v % 1000) / 100);
        cmd[8] = 0x30 | ((v % 100) / 10);
        cmd[9] = 0x30 | (v % 10);
        cmd[10] = 2; // 2 degree per impulse
        cmd[11] = 0x2f; // Set cmd
        cmd[12] = 0x20;  // End

        m_device->write(cmd);

        m_spidSetSent = true;
    }
    else
    {
        qDebug() << "SPIDProtocol::setAzimuthElevation: Not sent, waiting for status reply";
        m_spidSetOutstanding = true;
    }
    ControllerProtocol::setAzimuthElevation(azimuth, elevation);
}

// Handle data received from controller
void SPIDProtocol::readData()
{
    char buf[1024];
    qint64 len;

    while (m_device->bytesAvailable() >= 12)
    {
        len = m_device->read(buf, 12);
        if ((len == 12) && (buf[0] == 0x57))
        {
            double az;
            double el;
            az = buf[1] * 100.0 + buf[2] * 10.0 + buf[3] + buf[4] / 10.0 - 360.0;
            el = buf[6] * 100.0 + buf[7] * 10.0 + buf[8] + buf[9] / 10.0 - 360.0;
            //qDebug() << "SPIDProtocol::readData read Az " << az << " El " << el;
            reportAzEl(az, el);
            if (m_spidStatusSent && m_spidSetSent) {
                qDebug() << "SPIDProtocol::readData - m_spidStatusSent and m_spidSetSent set simultaneously";
            }
            if (m_spidStatusSent) {
                m_spidStatusSent = false;
            }
            if (m_spidSetSent) {
                m_spidSetSent = false;
            }
            if (m_spidSetOutstanding)
            {
                m_spidSetOutstanding = false;
                setAzimuthElevation(m_lastAzimuth, m_lastElevation);
            }
        }
        else
        {
            QByteArray bytes(buf, (int)len);
            qWarning() << "SPIDProtocol::readData - unexpected SPID rot2prog response \"" << bytes.toHex() << "\"";
            reportError(QString("Unexpected SPID rot2prog response: %1").arg(bytes.toHex().data()));
        }
    }
}

// Request current Az/El from controller
void SPIDProtocol::update()
{
    // Don't send a new status command, if waiting for a previous reply
    if (!m_spidSetSent && !m_spidStatusSent)
    {
        // Status
        QByteArray cmd;
        cmd.append((char)0x57); // Start
        for (int i = 0; i < 10; i++) {
            cmd.append((char)0x0);
        }
        cmd.append((char)0x1f); // Status
        cmd.append((char)0x20); // End
        m_device->write(cmd);
        m_spidStatusSent = true;
    }
}

