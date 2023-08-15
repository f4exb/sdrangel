//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Jon Beniston, M7RCE                                       //
//                                                                              //
// This program is free software; you can redistribute it and/or modify         //
// it under the terms of the GNU General Public License as published by         //
// the Free Software Foundation as version 3 of the License, or                 //
// (at your option) any later version.                                          //
//                                                                              //
// This program is distributed in the hope that it will be useful,              //
// but WITHOUT ANY WARRANTY; without even the implied warranty of               //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                 //
// GNU General Public License V3 for more details.                              //
//                                                                              //
// You should have received a copy of the GNU General Public License            //
// along with this program. If not, see <http://www.gnu.org/licenses/>.         //
//////////////////////////////////////////////////////////////////////////////////

#ifndef INCLUDE_UTIL_PROFILEDATA_H_
#define INCLUDE_UTIL_PROFILEDATA_H_

#include <QHash>
#include <QMutex>
#include <QElapsedTimer>

#include <cmath>

#include "export.h"

// Profiler enables runtime collection of profile data (time taken to execute code)
// that can be displayed in the GUI
//
// PROFILER_START() and PROFILER_STOP() macros should be used in the same function:
// void func() {
//     PROFILER_START()
//     do_something();
//     PROFILER_STOP("slow_code")
// }
//
// The parameters to PROFILER_STOP are:
//   name: Name for the code being profiled. Profiles using the same name are averaged.

#ifdef ENABLE_PROFILER
#define PROFILER_START()                                                                    \
    QElapsedTimer profileTimer;                                                             \
    profileTimer.start();
#define PROFILER_RESTART()                                                                  \
    profileTimer.start();
#define PROFILER_STOP(name)                                                                 \
    {                                                                                       \
        qint64 timeNanoSec = profileTimer.nsecsElapsed();                                   \
        QHash<QString, ProfileData>& globalData = GlobalProfileData::getProfileData();      \
        if (!globalData.contains(name)) {                                                   \
            globalData.insert(name, ProfileData());                                         \
        }                                                                                   \
        ProfileData& profileData = globalData[name];                                        \
        profileData.add(timeNanoSec);                                                       \
        GlobalProfileData::releaseProfileData();                                            \
    }
#else
#define PROFILER_START()
#define PROFILER_RESTART()
#define PROFILER_STOP(name)
#endif

class ProfileData
{
public:
    ProfileData() :
        m_numSamples(0),
        m_last(0),
        m_total(0)
    { }

    void reset()
    {
        m_numSamples = 0;
        m_total = 0;
    }

    void add(qint64 sample)
    {
        m_last = sample;
        m_total += sample;
        m_numSamples++;
    }

    double getAverage() const
    {
        if (m_numSamples > 0) {
            return m_total / (double)m_numSamples;
        } else {
            return nan("");
        }
    }

    qint64 getTotal() const { return m_total; }
    qint64 getLast() const { return m_last; }
    quint64 getNumSamples() const { return m_numSamples; }

private:
    quint64 m_numSamples;
    qint64 m_last;
    qint64 m_total;
};

// Global thread-safe profile data that can be displayed in the GUI
class SDRBASE_API GlobalProfileData
{
public:

    // Calls to getProfileData must be paired with releaseProfileData
    static QHash<QString, ProfileData>& getProfileData();
    static void releaseProfileData();
    static void resetProfileData();

private:

    static QHash<QString, ProfileData> m_profileData;
    static QMutex m_mutex;

};

#endif /* INCLUDE_UTIL_PROFILEDATA_H_ */
