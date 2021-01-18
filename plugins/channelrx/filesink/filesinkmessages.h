///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Edouard Griffiths, F4EXB.                                  //
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

#ifndef INCLUDE_FILESINKMESSAGES_H_
#define INCLUDE_FILESINKMESSAGES_H_

#include <QObject>

#include "util/message.h"

class FileSinkMessages : public QObject {
    Q_OBJECT
public:
    class MsgConfigureSpectrum : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        int64_t getCenterFrequency() const { return m_centerFrequency; }
        int getSampleRate() const { return m_sampleRate; }

        static MsgConfigureSpectrum* create(int64_t centerFrequency, int sampleRate) {
            return new MsgConfigureSpectrum(centerFrequency, sampleRate);
        }

    private:
        int64_t m_centerFrequency;
        int m_sampleRate;

        MsgConfigureSpectrum(int64_t centerFrequency, int sampleRate) :
            Message(),
            m_centerFrequency(centerFrequency),
            m_sampleRate(sampleRate)
        { }
    };

    class MsgReportSquelch : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getOpen() const { return m_open; }

        static MsgReportSquelch* create(bool open) {
            return new MsgReportSquelch(open);
        }

    private:
        bool m_open;

        MsgReportSquelch(bool open) :
            Message(),
            m_open(open)
        { }
    };

    class MsgReportRecording : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getRecording() const { return m_recording; }

        static MsgReportRecording* create(bool recording) {
            return new MsgReportRecording(recording);
        }

    private:
        bool m_recording;

        MsgReportRecording(bool recording) :
            Message(),
            m_recording(recording)
        { }
    };

    class MsgReportRecordFileName : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getFileName() const { return m_fileName; }

        static MsgReportRecordFileName* create(const QString& fileName) {
            return new MsgReportRecordFileName(fileName);
        }

    private:
        QString m_fileName;

        MsgReportRecordFileName(const QString& fileName) :
            Message(),
            m_fileName(fileName)
        { }
    };

    class MsgReportRecordFileError : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const QString& getMessage() const { return m_message; }

        static MsgReportRecordFileError* create(const QString& message) {
            return new MsgReportRecordFileError(message);
        }

    private:
        QString m_message;

        MsgReportRecordFileError(const QString& message) :
            Message(),
            m_message(message)
        { }
    };
};

#endif // INCLUDE_FILESINKMESSAGES_H_
