//////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                  //
// Copyright (C) 2015-2019 Edouard Griffiths, F4EXB <f4exb06@gmail.com>         //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                    //
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

#include <algorithm>

#include <QDebug>

#include "profiler.h"

QHash<QString, ProfileData> GlobalProfileData::m_profileData;
QRecursiveMutex GlobalProfileData::m_mutex;
QElapsedTimer GlobalProfileData::m_startTimer;

// Format a time in nanoseconds as the Profile dialog does
static QString formatNanoSecs(double timeInNanoSec)
{
    if (timeInNanoSec < 1e3) {
        return QString("%1 ns").arg(timeInNanoSec, 0, 'f', 3);
    } else if (timeInNanoSec < 1e6) {
        return QString("%1 us").arg(timeInNanoSec/1e3, 0, 'f', 3);
    } else if (timeInNanoSec < 1e9) {
        return QString("%1 ms").arg(timeInNanoSec/1e6, 0, 'f', 3);
    } else {
        return QString("%1 s").arg(timeInNanoSec/1e9, 0, 'f', 3);
    }
}

QHash<QString, ProfileData>& GlobalProfileData::getProfileData()
{
    m_mutex.lock();
    // The timer is only otherwise started by resetProfileData, so without this the
    // elapsed time - and therefore the percentages - would be meaningless unless
    // the profile data happened to have been reset
    if (!m_startTimer.isValid()) {
        m_startTimer.start();
    }
    return m_profileData;
}

void GlobalProfileData::releaseProfileData()
{
    m_mutex.unlock();
}

void GlobalProfileData::resetProfileData()
{
    m_mutex.lock();
    m_profileData.clear();
    m_startTimer.start();
    m_mutex.unlock();
}

qint64 GlobalProfileData::getMSSinceStart()
{
    m_mutex.lock();
    qint64 elapsed = m_startTimer.isValid() ? m_startTimer.elapsed() : 0;
    m_mutex.unlock();
    return elapsed;
}

void GlobalProfileData::logProfileData()
{
    QHash<QString, ProfileData>& profileData = getProfileData();

    if (profileData.isEmpty())
    {
        // Either the profiler wasn't enabled at build time, or nothing profiled ran
        releaseProfileData();
        return;
    }

    // Sort by total time, so the most expensive code is first
    QList<QString> names = profileData.keys();
    std::sort(names.begin(), names.end(), [&profileData] (const QString& a, const QString& b) {
        return profileData[a].getTotal() > profileData[b].getTotal();
    });

    const qint64 msecSinceStart = m_startTimer.isValid() ? m_startTimer.elapsed() : 0;

    qInfo() << "Profile data over" << formatNanoSecs(msecSinceStart * 1e6)
            << "- percentages are of that elapsed time, so they can exceed 100% in total"
            << "when code runs on more than one thread";
    qInfo("%-40s %14s %8s %16s %14s %14s %12s",
        "Name", "Total", "%", "Average", "Max", "Last", "Samples");

    for (const auto& name : names)
    {
        const ProfileData& data = profileData[name];
        double percent = (msecSinceStart > 0) ? data.getTotal() / (msecSinceStart * 1e6) * 100.0 : 0.0;

        qInfo("%-40s %14s %7.2f%% %16s %14s %14s %12llu",
            qPrintable(name),
            qPrintable(formatNanoSecs(data.getTotal())),
            percent,
            qPrintable(formatNanoSecs(data.getAverage())),
            qPrintable(formatNanoSecs(data.getMax())),
            qPrintable(formatNanoSecs(data.getLast())),
            (unsigned long long) data.getNumSamples());
    }

    releaseProfileData();
}
