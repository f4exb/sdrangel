///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Jon Beniston, M7RCE                                        //
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

#ifndef SDRBASE_ANDROID_H_
#define SDRBASE_ANDROID_H_

#ifdef ANDROID

#include <QtGlobal>
#include <QString>

#include "export.h"

// Android specific functions
class SDRBASE_API Android
{
public:

    static void sendIntent();
    static QStringList listUSBDeviceSerials(int vid, int pid);
    static int openUSBDevice(const QString &serial);
    static void closeUSBDevice(int fd);
    static void moveTaskToBack();
    static void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg);
    static void acquireWakeLock();
    static void releaseWakeLock();
    static void acquireScreenLock();
    static void releaseScreenLock();

};

#endif // ANDROID

#endif // SDRBASE_ANDROID_H_
