///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 F4EXB                                                      //
// written by Edouard Griffiths                                                  //
//                                                                               //
// Object unique id calculator loosely inspired by MongoDB object id             //
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

#include <QDateTime>
#include <QTime>
#include <QCoreApplication>
#include <QHostInfo>
#include <QCryptographicHash>

#ifdef _WIN32_
#include <windows.h>
#else
#include <sys/time.h>
#endif

#include "uid.h"

uint64_t UidCalculator::getNewObjectId()
{
    QDateTime currentDateTime = QDateTime::currentDateTime();
    uint64_t uid = currentDateTime.toTime_t();
    uid *= 1000000UL; // make room for microseconds

// Fallback to milliseconds:
//    QTime timeNow = QTime::currentTime();
//    uint64_t usecs = timeNow.msec() * 1000UL;
//    uid += usecs;
    uid += getCurrentMiroseconds();

    return uid;
}

uint32_t UidCalculator::getNewInstanceId()
{
    uint32_t uid = (QCoreApplication::applicationPid() % (1<<16));

    QString hostname = QHostInfo::localHostName();
    QByteArray hashKey = QCryptographicHash::hash(hostname.toUtf8(), QCryptographicHash::Sha1);
    uint32_t hashHost = 0;

    for (int i = 0; i < hashKey.size(); i++) {
        char c = hashKey.at(i);
        hashHost += (uint32_t) c;
    }

    hashHost %= (1<<16);
    uid += (hashHost<<16);

    return uid;
}

uint64_t UidCalculator::getCurrentMiroseconds()
{
#ifdef _WIN32_
    LARGE_INTEGER tickPerSecond;
    LARGE_INTEGER tick; // a point in time

    // get the high resolution counter's accuracy
    QueryPerformanceFrequency(&tickPerSecond);

    // what time is it ?
    QueryPerformanceCounter(&tick);

    // and here we get the current microsecond! \o/
    return (tick.QuadPart % tickPerSecond.QuadPart);
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return tv.tv_usec;
#endif
}

