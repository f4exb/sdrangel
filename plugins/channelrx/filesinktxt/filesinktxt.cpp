#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QNetworkReply>
#include <QNetworkAccessManager>

#include "device/deviceapi.h"
#include "dsp/dspengine.h"
#include "maincore.h"
#include "filesinktxt.h"
#include "filesinktxtsettings.h"
#include "SWGChannelSettings.h"
#include "detection.h"

const char* const FileSinkTxt::m_channelIdURI = "sdrangel.channel.filesinktext";
const char* const FileSinkTxt::m_channelId = "FileSinkText";

FileSinkTxt::FileSinkTxt(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_fileName("output.txt"),
    m_running(false),
    m_file(nullptr),
    m_stream(nullptr),
    m_packetSize(0)
{
    setObjectName(m_channelId);
    
    m_deviceAPI->addChannelSinkAPI(this);
    m_deviceAPI->addChannelSink(this);
    
    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileSinkTxt::networkManagerFinished
    );
    
    applySettings(true);
}

FileSinkTxt::~FileSinkTxt()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &FileSinkTxt::networkManagerFinished
    );
    delete m_networkManager;
    
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);
    
    if (m_running) {
        stop();
    }
    
    if (m_file) {
        delete m_file;
    }
    if (m_stream) {
        delete m_stream;
    }
}

void FileSinkTxt::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSinkAPI(this);
        m_deviceAPI->addChannelSink(this);
    }
}

void FileSinkTxt::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    
    if (!m_running) return;
    
    if (m_stream) {
        for (auto it = begin; it != end; ++it) {
            m_data.append(sqrt((it->real() * it->real()) + (it->imag() * it->imag())));
            m_packetSize++;
            if (m_packetSize == 4096) {
                QVector<DetectedWindowStruct> detectedWindows = detection::Detect(m_data , m_packetSize, 117, false, 50);
                // Write detections immediately
                *m_stream << detectedWindows.size() << " detected windows in this chunk\n";
                for (const DetectedWindowStruct& window : detectedWindows) {
                    *m_stream << "Start: " << window.startIndex << ", End: " << window.endIndex << "\n";
                }
                m_data.clear();
                m_packetSize = 0;
            }
        }
        m_stream->flush();
    }
}

void FileSinkTxt::start()
{
    qDebug("FileSinkTxt::start");
}

void FileSinkTxt::stop()
{
    qDebug("FileSinkTxt::stop");
    stopRecording();
}

void FileSinkTxt::startRecording()
{
    if (m_running) return;
    
    m_file = new QFile(m_fileName);
    if (m_file->open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_stream = new QTextStream(m_file);
        m_running = true;
        m_data.clear();      // Reset data buffer
        m_packetSize = 0;    // Reset packet size counter
        *m_stream << "FILESINKTXT_PLUGIN_ACTIVE\n";  // Write this once when recording starts
        m_stream->flush();
        qDebug() << "FileSinkTxt: Started recording to" << m_fileName;
    } else {
        qWarning() << "FileSinkTxt: Cannot open file for writing:" << m_fileName;
        delete m_file;
        m_file = nullptr;
    }
}

void FileSinkTxt::stopRecording()
{
    if (!m_running) return;
    
    m_running = false;
    
    if (m_stream) {
        delete m_stream;
        m_stream = nullptr;
    }
    
    if (m_file) {
        m_file->close();
        delete m_file;
        m_file = nullptr;
    }
    
    m_data.clear();      // Clear data buffer
    m_packetSize = 0;    // Reset packet size counter
    
    qDebug() << "FileSinkTxt: Stopped recording";
}

void FileSinkTxt::saveToFile()
{
    // Currently same as startRecording for this minimal implementation
    if (!m_running) {
        startRecording();
    }
}

void FileSinkTxt::loadFromFile()
{
    QFile file(m_fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        int lineCount = 0;
        while (!in.atEnd()) {
            QString line = in.readLine();
            lineCount++;
            if (lineCount <= 10) { // Only log first 10 lines to avoid spam
                qDebug() << "Loaded line:" << line;
            }
        }
        file.close();
        qDebug() << "FileSinkTxt: Loaded" << lineCount << "lines from" << m_fileName;
    } else {
        qWarning() << "FileSinkTxt: Cannot open file for reading:" << m_fileName;
    }
}

void FileSinkTxt::applySettings(bool force)
{
    (void) force;
    // Minimal implementation - settings handling would go here
}

bool FileSinkTxt::handleMessage(const Message& cmd)
{
    (void) cmd;
    return false;
}

void FileSinkTxt::networkManagerFinished(QNetworkReply *reply)
{
    reply->deleteLater();
}

void FileSinkTxt::handleInputMessages()
{
    Message* message;
    
    while ((message = m_inputMessageQueue.pop()) != nullptr) {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

int FileSinkTxt::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSinkSettings(new SWGSDRangel::SWGFileSinkSettings());
    response.getFileSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FileSinkTxt::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) force;
    (void) errorMessage;
    webapiUpdateChannelSettings(m_settings, channelSettingsKeys, response);
    return 200;
}

void FileSinkTxt::webapiFormatChannelSettings(
        SWGSDRangel::SWGChannelSettings& response,
        const FileSinkTxtSettings& settings)
{
    // Use FileSinkSettings for now since there's no specific FileSinkTxtSettings SWG class
    response.getFileSinkSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getFileSinkSettings()->getTitle()) {
        *response.getFileSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getFileSinkSettings()->setTitle(new QString(settings.m_title));
    }
    response.getFileSinkSettings()->setStreamIndex(settings.m_streamIndex);
    response.getFileSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);
    if (response.getFileSinkSettings()->getReverseApiAddress()) {
        *response.getFileSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getFileSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }
    response.getFileSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getFileSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getFileSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void FileSinkTxt::webapiUpdateChannelSettings(
        FileSinkTxtSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFileSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFileSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getFileSinkSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getFileSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getFileSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getFileSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getFileSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getFileSinkSettings()->getReverseApiChannelIndex();
    }
}
