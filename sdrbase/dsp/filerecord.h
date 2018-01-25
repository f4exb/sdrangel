#ifndef INCLUDE_FILESINK_H
#define INCLUDE_FILESINK_H

#include <dsp/basebandsamplesink.h>
#include <string>
#include <iostream>
#include <fstream>

#include <ctime>
#include "util/export.h"

class Message;

class SDRANGEL_API FileRecord : public BasebandSampleSink {
public:

    struct Header
    {
    	qint32      sampleRate;
        quint64     centerFrequency;
        std::time_t startTimeStamp;
        quint32     sampleSize;
    };

	FileRecord();
    FileRecord(const std::string& filename);
	virtual ~FileRecord();

    quint64 getByteCount() const { return m_byteCount; }

    void setFileName(const std::string& filename);

	virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool positiveOnly);
	virtual void start();
	virtual void stop();
	virtual bool handleMessage(const Message& message);
    void startRecording();
    void stopRecording();
    static void readHeader(std::ifstream& samplefile, Header& header);

private:
	std::string m_fileName;
	qint32 m_sampleRate;
	quint64 m_centerFrequency;
	bool m_recordOn;
    bool m_recordStart;
    std::ofstream m_sampleFile;
    quint64 m_byteCount;

	void handleConfigure(const std::string& fileName);
    void writeHeader();
};

#endif // INCLUDE_FILESINK_H
