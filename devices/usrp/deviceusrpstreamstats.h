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

#ifndef DEVICES_USRP_DEVICEUSRPSTREAMSTATS_H_
#define DEVICES_USRP_DEVICEUSRPSTREAMSTATS_H_

#include <deque>
#include <utility>

#include <QString>
#include <QDateTime>
#include <QElapsedTimer>

#include "export.h"

/**
 * Statistics for stream events (E.g. overruns, underruns, timeouts or dropped packets),
 * calculated from the event counts that are periodically reported by the streaming threads.
 *
 * The streaming threads reset their counts whenever they are restarted, which can happen
 * while the stream is running (E.g. when paused while a buddy creates its stream),
 * so counts are accumulated, and only reset when the stream is started.
 */
class DEVICES_API DeviceUSRPStreamStats
{
public:
    DeviceUSRPStreamStats();

    //! Update with latest report. count is the event count reported by the streaming thread.
    void update(bool running, quint32 count);
    //! Total number of events since the stream was started
    quint64 getCount() const { return m_offset + m_lastCount; }
    //! Get tooltip, with statistics appended to description. eventName is plural (E.g. "Overruns")
    QString getToolTip(const QString& description, const QString& eventName) const;

private:
    bool m_running;
    bool m_started;                 //!< Stream has been started at least once
    quint32 m_lastCount;            //!< Last count reported by streaming thread
    quint64 m_offset;               //!< Accumulated count from before the streaming thread's count was reset
    QElapsedTimer m_runTimer;       //!< Time since stream was started
    qint64 m_runTimeMs;             //!< Run time when stopped
    QDateTime m_lastEventTime;
    std::deque<std::pair<qint64, quint64>> m_history; //!< (run time ms, total count) for last minute

    static const qint64 m_historyMs = 60000;
};

#endif // DEVICES_USRP_DEVICEUSRPSTREAMSTATS_H_
