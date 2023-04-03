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
#include <QRegularExpression>

#include "rotctrldprotocol.h"

RotCtrlDProtocol::RotCtrlDProtocol() :
    m_rotCtlDReadAz(false)
{
}

void RotCtrlDProtocol::setAzimuthElevation(float azimuth, float elevation)
{
    QString cmd = QString("P %1 %2\n").arg(azimuth).arg(elevation);
    QByteArray data = cmd.toLatin1();
    m_device->write(data);
    ControllerProtocol::setAzimuthElevation(azimuth, elevation);
}

// Handle data received from rotctrld
void RotCtrlDProtocol::readData()
{
    char buf[1024];
    qint64 len;

    while (m_device->canReadLine())
    {
        len = m_device->readLine(buf, sizeof(buf));
        if (len != -1)
        {
            QString response = QString::fromUtf8(buf, len).trimmed();
            QRegularExpression rprt("RPRT (-?\\d+)");
            QRegularExpressionMatch matchRprt = rprt.match(response);
            QRegularExpression decimal("(-?\\d+.\\d+)");
            QRegularExpressionMatch matchDecimal = decimal.match(response);
            if (matchRprt.hasMatch())
            {
                // See rig_errcode_e in hamlib rig.h
                const QStringList errors = {
                    "OK",
                    "Invalid parameter",
                    "Invalid configuration",
                    "No memory",
                    "Not implemented",
                    "Timeout",
                    "IO error",
                    "Internal error",
                    "Protocol error",
                    "Command rejected",
                    "Arg truncated",
                    "Not available",
                    "VFO not targetable",
                    "Bus error",
                    "Collision on bus",
                    "NULL rig handled or invalid pointer parameter",
                    "Invalid VFO",
                    "Argument out of domain of function"
                };
                int rprt = matchRprt.captured(1).toInt();
                if (rprt != 0)
                {
                    qWarning() << "GS232ControllerWorker::readData - rotctld error: " << errors[-rprt];
                    // Seem to get a lot of EPROTO errors from rotctld due to extra 00 char in response to GS232 C2 command
                    // E.g: ./rotctld.exe -m 603 -r com7 -vvvvv
                    // read_string(): RX 16 characters
                    // 0000    00 41 5a 3d 31 37 35 20 20 45 4c 3d 30 33 38 0d     .AZ=175  EL=038.
                    // So don't pass these to GUI for now
                    if (rprt != -8) {
                        reportError(QString("rotctld error: %1").arg(errors[-rprt]));
                    }
                }
                m_rotCtlDReadAz = false;
            }
            else if (matchDecimal.hasMatch() && !m_rotCtlDReadAz)
            {
                m_rotCtlDAz = response;
                m_rotCtlDReadAz = true;
            }
            else if (matchDecimal.hasMatch() && m_rotCtlDReadAz)
            {
                QString az = m_rotCtlDAz;
                QString el = response;
                m_rotCtlDReadAz = false;
                //qDebug() << "GS232ControllerWorker::readData read Az " << az << " El " << el;
                reportAzEl(az.toFloat(), el.toFloat());
            }
            else
            {
                qWarning() << "GS232ControllerWorker::readData - Unexpected rotctld response \"" << response << "\"";
                reportError(QString("Unexpected rotctld response: %1").arg(response));
            }
        }
    }
}

// Request current Az/El from rotctrld
void RotCtrlDProtocol::update()
{
    QByteArray cmd("p\n");
    m_device->write(cmd);
}

