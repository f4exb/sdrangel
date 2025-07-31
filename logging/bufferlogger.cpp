///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2025 Jon Beniston, M7RCE                                        //
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

#include "bufferlogger.h"

using namespace qtwebapp;

BufferLogger::BufferLogger(int maxSize, QObject *parent) :
    m_maxSize(maxSize)
{
}

BufferLogger::~BufferLogger()
{
}

void BufferLogger::write(const LogMessage* logMessage)
{
    QString text = qPrintable(logMessage->toString("{timestamp} {type} {msg}", "yyyy-MM-dd HH:mm:ss.zzz"));
    m_messages.enqueue(text);
    while (m_messages.size() > m_maxSize) {
        m_messages.dequeue();
    }
}

QString BufferLogger::getLog() const
{
    QString log;

    for (const auto s : m_messages) {
        log.append(s);
    }

    return log;
}
