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

#pragma once

#include <QQueue>

#include "logger.h"
#include "export.h"

namespace qtwebapp {

// A ring-buffer like logger for holding m_maxSize recent log messages
class LOGGING_API BufferLogger : public Logger {
    Q_OBJECT
    Q_DISABLE_COPY(BufferLogger)

public:
    BufferLogger(int maxSize, QObject *parent = nullptr);
    virtual ~BufferLogger();

    void write(const LogMessage* logMessage) override;
    QString getLog() const;     // Get last m_maxSize messages as one big string

private:

    int m_maxSize;              // Maximum number of messages to store
    QQueue<QString> m_messages; // Queue containing last m_maxSize messages

};

}
