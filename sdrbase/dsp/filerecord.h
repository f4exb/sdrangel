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
#include "export.h"

class Message;

class SDRBASE_API FileRecord : public BasebandSampleSink {
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

	FileRecord();
    FileRecord(const QString& filename);
	virtual ~FileRecord();

    quint64 getByteCount() const { return m_byteCount; }

    void setFileName(const QString& filename);
    void genUniqueFileName(uint deviceUID, int istream = -1);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);
    void startRecording();
    void stopRecording();
    bool isRecording() const { return m_recordOn; }
    static bool readHeader(std::ifstream& samplefile, Header& header); //!< returns true if CRC checksum is correct else false
    static void writeHeader(std::ofstream& samplefile, Header& header);

private:
	QString m_fileName;
	quint32 m_sampleRate;
	quint64 m_centerFrequency;
	bool m_recordOn;
    bool m_recordStart;
    std::ofstream m_sampleFile;
    quint64 m_byteCount;

	void handleConfigure(const QString& fileName);
    void writeHeader();
};

#endif // INCLUDE_FILERECORD_H
