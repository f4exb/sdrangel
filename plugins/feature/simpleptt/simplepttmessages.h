///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2023 Edouard Griffiths, F4EXB                                   //
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
#ifndef INCLUDE_FEATURE_SIMPLEPTTMESSAGES_H_
#define INCLUDE_FEATURE_SIMPLEPTTMESSAGES_H_

#include <QObject>

#include "util/message.h"

class SimplePTTMessages : public QObject {
    Q_OBJECT
public:
    class MsgCommandError : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint64_t getFinishedTimeStamp() { return m_finishedTimeStamp; }
        int getError() { return m_error; }
        QString& getLog() { return m_log; }

        static MsgCommandError* create(uint64_t finishedTimeStamp, int error) {
            return new MsgCommandError(finishedTimeStamp, error);
        }

    private:
        uint64_t m_finishedTimeStamp;
        int m_error;
        QString m_log;

        MsgCommandError(uint64_t finishedTimeStamp, int error) :
            Message(),
            m_finishedTimeStamp(finishedTimeStamp),
            m_error(error)
        { }
    };

    class MsgCommandFinished : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        uint64_t getFinishedTimeStamp() { return m_finishedTimeStamp; }
        int getExitCode() { return m_exitCode; }
        int getExitStatus() { return m_exitStatus; }
        QString& getLog() { return m_log; }

        static MsgCommandFinished* create(uint64_t finishedTimeStamp, int exitCode, int exitStatus) {
            return new MsgCommandFinished(finishedTimeStamp, exitCode, exitStatus);
        }

    private:
        uint64_t m_finishedTimeStamp;
        int m_exitCode;
        int m_exitStatus;
        QString m_log;

        MsgCommandFinished(uint64_t finishedTimeStamp, int exitCode, int exitStatus) :
            Message(),
            m_finishedTimeStamp(finishedTimeStamp),
            m_exitCode(exitCode),
            m_exitStatus(exitStatus)
        { }
    };
};


#endif // INCLUDE_FEATURE_SIMPLEPTTMESSAGES_H_
