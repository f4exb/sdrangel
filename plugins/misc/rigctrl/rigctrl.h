///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#ifndef INCLUDE_RIGCTRL_H
#define INCLUDE_RIGCTRL_H

#include <QThread>
#include <QtNetwork>

#include "rigctrlsettings.h"

class RigCtrl : public QObject {
	Q_OBJECT

public:

    RigCtrl();
    ~RigCtrl();
    void getSettings(RigCtrlSettings *settings);
    void setSettings(RigCtrlSettings *settings);
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);

private slots:
    void acceptConnection();
    void getCommand();
    void processAPIResponse(QNetworkReply *reply);

protected:
    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientConnection;
    QNetworkAccessManager *m_netman;

    enum RigCtrlState
    {
        idle,
        set_freq, set_freq_no_offset, set_freq_center, set_freq_center_no_offset, set_freq_set_offset, set_freq_offset,
        get_freq_center, get_freq_offset,
        set_mode_mod, set_mode_settings, set_mode_reply,
        get_power,
        set_power_on, set_power_off
    };
    RigCtrlState m_state;

    double m_targetFrequency;
    double m_targetOffset;
    const char *m_targetModem;
    int m_targetBW;

    RigCtrlSettings m_settings;
};

#endif // INCLUDE_RIGCTRL_H
