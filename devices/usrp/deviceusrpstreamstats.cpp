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

#include <QStringList>

#include "deviceusrpstreamstats.h"

DeviceUSRPStreamStats::DeviceUSRPStreamStats() :
    m_running(false),
    m_started(false),
    m_lastCount(0),
    m_offset(0),
    m_runTimeMs(0)
{
}

void DeviceUSRPStreamStats::update(bool running, quint32 count)
{
    if (running && !m_running)
    {
        // Stream started - reset statistics
        m_started = true;
        m_lastCount = 0;
        m_offset = 0;
        m_runTimer.start();
        m_runTimeMs = 0;
        m_lastEventTime = QDateTime();
        m_history.clear();
    }
    else if (!running && m_running)
    {
        // Stream stopped - keep statistics from last run
        m_runTimeMs = m_runTimer.elapsed();
    }

    m_running = running;

    if (!running) {
        return;
    }

    if (count < m_lastCount)
    {
        // Streaming thread was restarted, which resets its count
        m_offset += m_lastCount;

        if (count > 0) { // Events since restart
            m_lastEventTime = QDateTime::currentDateTime();
        }
    }
    else if (count > m_lastCount)
    {
        m_lastEventTime = QDateTime::currentDateTime();
    }

    m_lastCount = count;

    // Keep history for last minute, so we can count recent events
    qint64 now = m_runTimer.elapsed();
    m_history.push_back({now, getCount()});

    while ((m_history.size() > 1) && (now - m_history.front().first > m_historyMs)) {
        m_history.pop_front();
    }
}

QString DeviceUSRPStreamStats::getToolTip(const QString& description, const QString& eventName) const
{
    QStringList lines;

    lines.append(description);

    if (!m_started) {
        return lines.join("\n");
    }

    quint64 count = getCount();
    qint64 runTimeMs = m_running ? m_runTimer.elapsed() : m_runTimeMs;

    lines.append("");
    lines.append(QString("%1%2: %3").arg(eventName).arg(m_running ? "" : " (last run)").arg(count));

    if (runTimeMs >= 1000)
    {
        double perMinute = count / (runTimeMs / 60000.0);
        lines.append(QString("Average: %1 per minute").arg(QString::number(perMinute, 'f', 1)));
    }

    if (m_running && !m_history.empty()) {
        lines.append(QString("Last minute: %1").arg(count - m_history.front().second));
    }

    if (m_lastEventTime.isValid())
    {
        qint64 secsAgo = m_lastEventTime.secsTo(QDateTime::currentDateTime());
        lines.append(QString("Last: %1 (%2 s ago)").arg(m_lastEventTime.toString("hh:mm:ss")).arg(secsAgo));
    }

    return lines.join("\n");
}
