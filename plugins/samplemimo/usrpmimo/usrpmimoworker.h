///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#ifndef PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOWORKER_H_
#define PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOWORKER_H_

#include <QObject>

#include "util/messagequeue.h"
#include "usrpmimosettings.h"

struct DeviceUSRPParams;

/**
 * Applies settings to the device in its own thread, as some settings (E.g. sample rate,
 * clock source and bandwidth) can take a long time to set, which would otherwise block the GUI.
 * Values actually set are reported via DeviceUSRPShared::MsgReportDeviceSettings.
 */
class USRPMIMOWorker : public QObject
{
    Q_OBJECT

public:
    USRPMIMOWorker(DeviceUSRPParams *deviceParams, int nbRx, int nbTx, MessageQueue *reportQueue);
    MessageQueue *getInputMessageQueue() { return &m_inputMessageQueue; }

    static void applyDeviceSettings(
        DeviceUSRPParams *deviceParams,
        int nbRx,
        int nbTx,
        const USRPMIMOSettings& settings,
        const QList<QString>& settingsKeys,
        bool force,
        bool applyCommon,
        bool applyRx,
        bool applyTx,
        bool applyBandwidth,
        MessageQueue *reportQueue);

private:
    DeviceUSRPParams *m_deviceParams;
    int m_nbRx;
    int m_nbTx;
    MessageQueue *m_reportQueue;        //!< Where to send MsgReportDeviceSettings (USRPMIMO's input queue)
    MessageQueue m_inputMessageQueue;   //!< Settings to apply to the device

private slots:
    void handleInputMessages();
};

#endif // PLUGINS_SAMPLEMIMO_USRPMIMO_USRPMIMOWORKER_H_
