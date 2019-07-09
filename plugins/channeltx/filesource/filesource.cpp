///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB                                   //
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

#include "filesource.h"

#if (defined _WIN32_) || (defined _MSC_VER)
#include "windows_time.h"
#include <stdint.h>
#else
#include <sys/time.h>
#include <unistd.h>
#endif

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGFileSourceReport.h"

#include "device/deviceapi.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplesink.h"
#include "dsp/upchannelizer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/filerecord.h"

MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSource, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceName, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceWork, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgReportFileSourceAcquisition, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgPlayPause, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgReportFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(FileSource::MsgReportHeaderCRC, Message)

const QString FileSource::m_channelIdURI = "sdrangel.channeltx.filesource";
const QString FileSource::m_channelId ="FileSource";

FileSource::FileSource(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
	m_fileName("..."),
	m_sampleSize(0),
	m_centerFrequency(0),
    m_frequencyOffset(0),
    m_fileSampleRate(0),
    m_samplesCount(0),
	m_sampleRate(0),
    m_deviceSampleRate(0),
	m_recordLength(0),
    m_startingTimeStamp(0),
    m_running(false)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addChannelSource(m_threadedChannelizer);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

FileSource::~FileSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void FileSource::pull(Sample& sample)
{
    struct Sample16
    {
        int16_t real;
        int16_t imag;
    };

    struct Sample24
    {
        int32_t real;
        int32_t imag;
    };

    if (!m_running)
    {
        sample.setReal(0);
        sample.setImag(0);
        return;
    }

	if (m_sampleSize == 16)
	{
        Sample16 sample16;
        m_ifstream.read(reinterpret_cast<char*>(&sample16), sizeof(Sample16));

        if (m_ifstream.eof()) {
            handleEOF();
        } else {
            m_samplesCount++;
        }

		if (SDR_TX_SAMP_SZ == 16)
		{
            sample.setReal(sample16.real);
            sample.setImag(sample16.imag);
		}
		else if (SDR_TX_SAMP_SZ == 24)
		{
            sample.setReal(sample16.real << 8);
            sample.setImag(sample16.imag << 8);
		}
        else
        {
            sample.setReal(0);
            sample.setImag(0);
        }
	}
	else if (m_sampleSize == 24)
	{
        Sample24 sample24;
        m_ifstream.read(reinterpret_cast<char*>(&sample24), sizeof(Sample24));

        if (m_ifstream.eof()) {
            handleEOF();
        } else {
            m_samplesCount++;
        }

		if (SDR_TX_SAMP_SZ == 24)
		{
            sample.setReal(sample24.real);
            sample.setImag(sample24.imag);
		}
		else if (SDR_TX_SAMP_SZ == 16)
		{
            sample.setReal(sample24.real >> 8);
            sample.setImag(sample24.imag >> 8);
		}
        else
        {
            sample.setReal(0);
            sample.setImag(0);
        }
	}
    else
    {
        sample.setReal(0);
        sample.setImag(0);
    }
}

void FileSource::pullAudio(int nbSamples)
{
    (void) nbSamples;
}

void FileSource::start()
{
    qDebug("FileSource::start");
    m_running = true;

	if (getMessageQueueToGUI())
    {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(true); // acquisition on
        getMessageQueueToGUI()->push(report);
	}
}

void FileSource::stop()
{
    qDebug("FileSource::stop");
    m_running = false;

	if (getMessageQueueToGUI())
    {
        MsgReportFileSourceAcquisition *report = MsgReportFileSourceAcquisition::create(false); // acquisition off
        getMessageQueueToGUI()->push(report);
	}
}

bool FileSource::handleMessage(const Message& cmd)
{
	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        int sampleRate = notif.getSampleRate();

        qDebug() << "FileSource::handleMessage: MsgChannelizerNotification:"
                << " channelSampleRate: " << sampleRate
                << " offsetFrequency: " << notif.getFrequencyOffset();

        if (sampleRate > 0) {
            setSampleRate(sampleRate);
        }

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "FileSource::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        setCenterFrequency(notif.getCenterFrequency());
        m_deviceSampleRate = notif.getSampleRate();
        calculateFrequencyOffset(); // This is when device sample rate changes

        // Redo the channelizer stuff with the new sample rate to re-synchronize everything
        m_channelizer->set(m_channelizer->getInputMessageQueue(),
            m_settings.m_log2Interp,
            m_settings.m_filterChainHash);

        if (m_guiMessageQueue)
        {
            MsgSampleRateNotification *msg = MsgSampleRateNotification::create(notif.getSampleRate());
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    else if (MsgConfigureFileSource::match(cmd))
    {
        MsgConfigureFileSource& cfg = (MsgConfigureFileSource&) cmd;
        qDebug() << "FileSource::handleMessage: MsgConfigureFileSource";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        m_settings.m_log2Interp = cfg.getLog2Interp();
        m_settings.m_filterChainHash =  cfg.getFilterChainHash();

        qDebug() << "FileSource::handleMessage: MsgConfigureChannelizer:"
                << " log2Interp: " << m_settings.m_log2Interp
                << " filterChainHash: " << m_settings.m_filterChainHash;

        m_channelizer->set(m_channelizer->getInputMessageQueue(),
            m_settings.m_log2Interp,
            m_settings.m_filterChainHash);

        calculateFrequencyOffset(); // This is when decimation or filter chain changes

        return true;
    }
    else if (MsgConfigureFileSourceName::match(cmd))
	{
		MsgConfigureFileSourceName& conf = (MsgConfigureFileSourceName&) cmd;
		m_fileName = conf.getFileName();
		openFileStream();
		return true;
	}
	else if (MsgConfigureFileSourceWork::match(cmd))
	{
		MsgConfigureFileSourceWork& conf = (MsgConfigureFileSourceWork&) cmd;

        if (conf.isWorking()) {
            start();
        } else {
            stop();
        }

		return true;
	}
	else if (MsgConfigureFileSourceSeek::match(cmd))
	{
		MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
		int seekMillis = conf.getMillis();
		seekFileStream(seekMillis);

		return true;
	}
	else if (MsgConfigureFileSourceStreamTiming::match(cmd))
	{
		MsgReportFileSourceStreamTiming *report;

        if (getMessageQueueToGUI())
        {
            report = MsgReportFileSourceStreamTiming::create(getSamplesCount());
            getMessageQueueToGUI()->push(report);
        }

		return true;
	}
    else
    {
        return false;
    }
}

QByteArray FileSource::serialize() const
{
    return m_settings.serialize();
}

bool FileSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureFileSource *msg = MsgConfigureFileSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFileSource *msg = MsgConfigureFileSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void FileSource::openFileStream()
{
	//stop();

	if (m_ifstream.is_open()) {
		m_ifstream.close();
	}

#ifdef Q_OS_WIN
	m_ifstream.open(m_fileName.toStdWString().c_str(), std::ios::binary | std::ios::ate);
#else
	m_ifstream.open(m_fileName.toStdString().c_str(), std::ios::binary | std::ios::ate);
#endif
	quint64 fileSize = m_ifstream.tellg();
    m_samplesCount = 0;

	if (fileSize > sizeof(FileRecord::Header))
	{
	    FileRecord::Header header;
	    m_ifstream.seekg(0,std::ios_base::beg);
		bool crcOK = FileRecord::readHeader(m_ifstream, header);
		m_fileSampleRate = header.sampleRate;
		m_centerFrequency = header.centerFrequency;
		m_startingTimeStamp = header.startTimeStamp;
		m_sampleSize = header.sampleSize;
		QString crcHex = QString("%1").arg(header.crc32 , 0, 16);

	    if (crcOK)
	    {
	        qDebug("FileSource::openFileStream: CRC32 OK for header: %s", qPrintable(crcHex));
	        m_recordLength = (fileSize - sizeof(FileRecord::Header)) / ((m_sampleSize == 24 ? 8 : 4) * m_fileSampleRate);
	    }
	    else
	    {
	        qCritical("FileSource::openFileStream: bad CRC32 for header: %s", qPrintable(crcHex));
	        m_recordLength = 0;
	    }

		if (getMessageQueueToGUI()) {
			MsgReportHeaderCRC *report = MsgReportHeaderCRC::create(crcOK);
			getMessageQueueToGUI()->push(report);
		}
	}
	else
	{
		m_recordLength = 0;
	}

	qDebug() << "FileSource::openFileStream: " << m_fileName.toStdString().c_str()
			<< " fileSize: " << fileSize << " bytes"
			<< " length: " << m_recordLength << " seconds"
			<< " sample rate: " << m_fileSampleRate << " S/s"
			<< " center frequency: " << m_centerFrequency << " Hz"
			<< " sample size: " << m_sampleSize << " bits"
            << " starting TS: " << m_startingTimeStamp << "s";

	if (getMessageQueueToGUI()) {
	    MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_fileSampleRate,
	            m_sampleSize,
	            m_centerFrequency,
	            m_startingTimeStamp,
	            m_recordLength); // file stream data
	    getMessageQueueToGUI()->push(report);
	}

	if (m_recordLength == 0) {
	    m_ifstream.close();
	}
}

void FileSource::seekFileStream(int seekMillis)
{
	QMutexLocker mutexLocker(&m_mutex);

	if ((m_ifstream.is_open()) && !m_running)
	{
        quint64 seekPoint = ((m_recordLength * seekMillis) / 1000) * m_fileSampleRate;
        m_samplesCount = seekPoint;
        seekPoint *= (m_sampleSize == 24 ? 8 : 4); // + sizeof(FileSink::Header)
		m_ifstream.clear();
		m_ifstream.seekg(seekPoint + sizeof(FileRecord::Header), std::ios::beg);
	}
}

void FileSource::handleEOF()
{
    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamTiming *report = MsgReportFileSourceStreamTiming::create(getSamplesCount());
        getMessageQueueToGUI()->push(report);
    }

    if (m_settings.m_loop)
    {
        stop();
        seekFileStream(0);
        start();
    }
    else
    {
        stop();

        if (getMessageQueueToGUI())
        {
            MsgPlayPause *report = MsgPlayPause::create(false);
            getMessageQueueToGUI()->push(report);
        }
    }
}

void FileSource::applySettings(const FileSourceSettings& settings, bool force)
{
    qDebug() << "FileSource::applySettings:"
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_loop != settings.m_loop)) {
        reverseAPIKeys.append("loop");
    }
    if ((m_settings.m_fileName != settings.m_fileName)) {
        reverseAPIKeys.append("fileName");
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    m_settings = settings;
}

void FileSource::validateFilterChainHash(FileSourceSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void FileSource::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

int FileSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    response.getFileSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FileSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    FileSourceSettings settings = m_settings;

    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getFileSourceSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getFileSourceSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFileSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFileSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getFileSourceSettings()->getReverseApiChannelIndex();
    }

    MsgConfigureFileSource *msg = MsgConfigureFileSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("FileSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileSource *msgToGUI = MsgConfigureFileSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int FileSource::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSourceReport(new SWGSDRangel::SWGFileSourceReport());
    response.getFileSourceReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void FileSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FileSourceSettings& settings)
{
    response.getFileSourceSettings()->setLog2Interp(settings.m_log2Interp);
    response.getFileSourceSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getFileSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getFileSourceSettings()->getTitle()) {
        *response.getFileSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getFileSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFileSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getFileSourceSettings()->getReverseApiAddress()) {
        *response.getFileSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getFileSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFileSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void FileSource::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getFileSourceReport()->setFileSampleRate(m_fileSampleRate);
    response.getFileSourceReport()->setFileSampleSize(m_sampleSize);
    response.getFileSourceReport()->setSampleRate(m_sampleRate);
}

void FileSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FileSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("FileSource"));
    swgChannelSettings->setFileSourceSettings(new SWGSDRangel::SWGFileSourceSettings());
    SWGSDRangel::SWGFileSourceSettings *swgFileSourceSettings = swgChannelSettings->getFileSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("log2Interp") || force) {
        swgFileSourceSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgFileSourceSettings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFileSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFileSourceSettings->setTitle(new QString(settings.m_title));
    }

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void FileSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("FileSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
