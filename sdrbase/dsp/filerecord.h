///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015-2018 Edouard Griffiths, F4EXB                              //
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

#ifndef INCLUDE_FILERECORD_H
#define INCLUDE_FILERECORD_H

#include <dsp/basebandsamplesink.h>
#include <string>
#include <iostream>
#include <fstream>
#include <ctime>

#include "dsp/filerecordinterface.h"
#include "export.h"

class Message;

class SDRBASE_API FileRecord : public FileRecordInterface {
public:

#pragma pack(push, 1)
    struct Header
    {
    	quint32 sampleRate;
        quint64 centerFrequency;
        quint64 startTimeStamp;
        quint32 sampleSize;
        quint32 filler;
        quint32 crc32;
    };
#pragma pack(pop)

	FileRecord(quint32 sampleRate=0, quint64 centerFrequency=0);
    FileRecord(const QString& fileBase);
	virtual ~FileRecord();

    quint64 getByteCount() const { return m_byteCount; }
    void setMsShift(qint64 shift) { m_msShift = shift; }
    const QString& getCurrentFileName() { return m_currentFileName; }

    void genUniqueFileName(uint deviceUID, int istream = -1);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);

    virtual void setFileName(const QString& fileBase);
    virtual bool startRecording();
    virtual bool stopRecording();
    virtual bool isRecording() const { return m_recordOn; }

    static bool readHeader(std::ifstream& samplefile, Header& header); //!< returns true if CRC checksum is correct else false
    static void writeHeader(std::ofstream& samplefile, Header& header);

private:
	QString m_fileBase;
	quint32 m_sampleRate;
	quint64 m_centerFrequency;
	bool m_recordOn;
    bool m_recordStart;
    std::ofstream m_sampleFile;
    QString m_currentFileName;
    quint64 m_byteCount;
    qint64 m_msShift;
    QRecursiveMutex m_mutex;

    void writeHeader();
};

#endif // INCLUDE_FILERECORD_H
