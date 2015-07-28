#ifndef INCLUDE_FILESINK_H
#define INCLUDE_FILESINK_H

#include <string>
#include <iostream>
#include <fstream>

#include <ctime>
#include "dsp/samplesink.h"
#include "util/export.h"
#include "util/message.h"

class MessageQueue;

class SDRANGELOVE_API FileSink : public SampleSink {
public:

    struct Header
    {
        int         sampleRate;
        quint64     centerFrequency;
        std::time_t startTimeStamp;
    };

	FileSink();
	~FileSink();
    
    quint64 getByteCount() const { return m_byteCount; }

	void configure(MessageQueue* msgQueue, const std::string& filename, int sampleRate, quint64 centerFrequency);

	void feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly);
	void start();
	void stop();
    void startRecording();
    void stopRecording();
	bool handleMessage(Message* message);
    static void readHeader(std::ifstream& samplefile, Header& header);

private:
	class MsgConfigureFileSink : public Message {
		MESSAGE_CLASS_DECLARATION

	public:
		const std::string& getFileName() const { return m_fileName; }
		int getSampleRate() const { return m_sampleRate; }
		quint64 getCenterFrequency() const { return m_centerFrequency; }

		static MsgConfigureFileSink* create(const std::string& fileName, int sampleRate, quint64 centerFrequency)
		{
			return new MsgConfigureFileSink(fileName, sampleRate, centerFrequency);
		}

	private:
		std::string m_fileName;
		int m_sampleRate;
		quint64 m_centerFrequency;

		MsgConfigureFileSink(const std::string& fileName, int sampleRate, quint64 centerFrequency) :
			Message(),
			m_fileName(fileName),
			m_sampleRate(sampleRate),
			m_centerFrequency(centerFrequency)
		{ }
	};

	std::string m_fileName;
	int m_sampleRate;
	quint64 m_centerFrequency;
	bool m_recordOn;
    bool m_recordStart;
    std::ofstream m_sampleFile;
    quint64 m_byteCount;

	void handleConfigure(const std::string& fileName, int sampleRate, quint64 centerFrequency);
    void writeHeader();
};

#endif // INCLUDE_FILESINK_H
