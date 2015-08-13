#include "dsp/filesink.h"
#include "util/simpleserializer.h"
#include "util/messagequeue.h"

#include <QDebug>

MESSAGE_CLASS_DEFINITION(FileSink::MsgConfigureFileSink, Message)

FileSink::FileSink() :
	SampleSink(),
    m_fileName(std::string("test.sdriq")),
    m_sampleRate(0),
    m_centerFrequency(0),
	m_recordOn(false),
    m_recordStart(false),
    m_byteCount(0)
{
	setObjectName("FileSink");
}

FileSink::~FileSink()
{
    stopRecording();
}

void FileSink::configure(MessageQueue* msgQueue, const std::string& filename, int sampleRate, quint64 centerFrequency)
{
	Message* cmd = MsgConfigureFileSink::create(filename, sampleRate, centerFrequency);
	cmd->submit(msgQueue, this);
}

bool FileSink::init(Message* cmd)
{
	if (DSPSignalNotification::match(cmd))
	{
		DSPSignalNotification* notif = (DSPSignalNotification*) cmd;
		m_sampleRate = notif->getSampleRate();
		m_centerFrequency = notif->getFrequencyOffset();
		qDebug() << "FileSink::init: DSPSignalNotification: m_inputSampleRate: " << m_sampleRate;
		cmd->completed();
		return true;
	}
}

void FileSink::feed(SampleVector::const_iterator begin, SampleVector::const_iterator end, bool positiveOnly)
{
    // if no recording is active, send the samples to /dev/null
    if(!m_recordOn)
        return;

    if (begin < end) // if there is something to put out
    {
        if (m_recordStart)
        {
            writeHeader();
            m_recordStart = false;
        }

        m_sampleFile.write(reinterpret_cast<const char*>(&*(begin)), (end - begin)*sizeof(Sample));
        m_byteCount += end - begin;
    }
}

void FileSink::start()
{
}

void FileSink::stop()
{
    stopRecording();
}

void FileSink::startRecording()
{
    if (!m_sampleFile.is_open())
    {
    	qDebug() << "FileSink::startRecording";
        m_sampleFile.open(m_fileName.c_str(), std::ios::binary);
        m_recordOn = true;
        m_recordStart = true;
        m_byteCount = 0;
    }
}

void FileSink::stopRecording()
{
    if (m_sampleFile.is_open())
    {
    	qDebug() << "FileSink::stopRecording";
        m_sampleFile.close();
        m_recordOn = false;
        m_recordStart = false;
    }
}

bool FileSink::handleMessage(Message* message)
{
    if (MsgConfigureFileSink::match(message))
    {
        MsgConfigureFileSink* conf = (MsgConfigureFileSink*) message;
        handleConfigure(conf->getFileName(), conf->getSampleRate(), conf->getCenterFrequency());
        qDebug() << "FileSink::handleMessage:"
        		<< " fileName: " << m_fileName.c_str()
				<< " sampleRate: " << m_sampleRate
				<< " centerFrequency: " << m_centerFrequency;
        message->completed();
        return true;
    }
    else
    {
        return false;
    }
}

void FileSink::handleConfigure(const std::string& fileName, int sampleRate, quint64 centerFrequency)
{
    if ((fileName != m_fileName) || (m_sampleRate != sampleRate) || (m_centerFrequency != centerFrequency))
    {
        stopRecording();
    }
    
	m_fileName = fileName;
    m_sampleRate = sampleRate;
    m_centerFrequency = centerFrequency;
}

void FileSink::writeHeader()
{
    m_sampleFile.write((const char *) &m_sampleRate, sizeof(int));
    m_sampleFile.write((const char *) &m_centerFrequency, sizeof(quint64));
    std::time_t ts = time(0);
    m_sampleFile.write((const char *) &ts, sizeof(std::time_t));
}

void FileSink::readHeader(std::ifstream& sampleFile, Header& header)
{
    sampleFile.read((char *) &(header.sampleRate), sizeof(int));
    sampleFile.read((char *) &(header.centerFrequency), sizeof(quint64));
    sampleFile.read((char *) &(header.startTimeStamp), sizeof(std::time_t));
}
