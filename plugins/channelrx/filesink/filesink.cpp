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


#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "maincore.h"

#include "filesinkmessages.h"
#include "filesinkbaseband.h"
#include "filesink.h"

MESSAGE_CLASS_DEFINITION(FileSink::MsgConfigureFileSink, Message)
MESSAGE_CLASS_DEFINITION(FileSink::MsgReportStartStop, Message)

const char* const FileSink::m_channelIdURI = "sdrangel.channel.filesink";
const char* const FileSink::m_channelId = "FileSink";

FileSink::FileSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_spectrumVis(SDR_RX_SCALEF),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_basebandSampleRate(48000)
{
    setObjectName(m_channelId);

    m_basebandSink = new FileSinkBaseband();
    m_basebandSink->setSpectrumSink(&m_spectrumVis);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

FileSink::~FileSink()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void FileSink::setMessageQueueToGUI(MessageQueue* queue)
{
    ChannelAPI::setMessageQueueToGUI(queue);
    m_basebandSink->setMessageQueueToGUI(queue);
}

uint32_t FileSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void FileSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void FileSink::start()
{
	qDebug("FileSink::start");
    m_basebandSink->reset();
    m_basebandSink->setMessageQueueToGUI(getMessageQueueToGUI());
    m_basebandSink->setDeviceHwId(m_deviceAPI->getHardwareId());
    m_basebandSink->setDeviceUId(m_deviceAPI->getDeviceUID());
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    FileSinkBaseband::MsgConfigureFileSinkBaseband *msg = FileSinkBaseband::MsgConfigureFileSinkBaseband::create(m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (getMessageQueueToGUI())
    {
        MsgReportStartStop *msg = MsgReportStartStop::create(true);
        getMessageQueueToGUI()->push(msg);
    }
}

void FileSink::stop()
{
    qDebug("FileSink::stop");
    m_basebandSink->stopWork();
	m_thread.exit();
	m_thread.wait();

    if (getMessageQueueToGUI())
    {
        MsgReportStartStop *msg = MsgReportStartStop::create(false);
        getMessageQueueToGUI()->push(msg);
    }
}

bool FileSink::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& cfg = (DSPSignalNotification&) cmd;

        qDebug() << "FileSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << cfg.getSampleRate()
                << " centerFrequency: " << cfg.getCenterFrequency();

        m_basebandSampleRate = cfg.getSampleRate();
        m_centerFrequency = cfg.getCenterFrequency();
        DSPSignalNotification *notif = new DSPSignalNotification(cfg);
        m_basebandSink->getInputMessageQueue()->push(notif);

        if (getMessageQueueToGUI())
        {
            DSPSignalNotification *notifToGUI = new DSPSignalNotification(cfg);
            getMessageQueueToGUI()->push(notifToGUI);
        }

        return true;
    }
    else if (MsgConfigureFileSink::match(cmd))
    {
        MsgConfigureFileSink& cfg = (MsgConfigureFileSink&) cmd;
        qDebug() << "FileSink::handleMessage: MsgConfigureFileSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray FileSink::serialize() const
{
    return m_settings.serialize();
}

bool FileSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureFileSink *msg = MsgConfigureFileSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureFileSink *msg = MsgConfigureFileSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void FileSink::getLocalDevices(std::vector<uint32_t>& indexes)
{
    indexes.clear();
    DSPEngine *dspEngine = DSPEngine::instance();

    for (uint32_t i = 0; i < dspEngine->getDeviceSourceEnginesNumber(); i++)
    {
        DSPDeviceSourceEngine *deviceSourceEngine = dspEngine->getDeviceSourceEngineByIndex(i);
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

        if (deviceSource->getDeviceDescription() == "LocalInput") {
            indexes.push_back(i);
        }
    }
}

DeviceSampleSource *FileSink::getLocalDevice(uint32_t index)
{
    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < dspEngine->getDeviceSourceEnginesNumber())
    {
        DSPDeviceSourceEngine *deviceSourceEngine = dspEngine->getDeviceSourceEngineByIndex(index);
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

        if (deviceSource->getDeviceDescription() == "LocalInput")
        {
            if (!getDeviceAPI()) {
                qDebug("FileSink::getLocalDevice: the parent device is unset");
            } else if (getDeviceAPI()->getDeviceUID() == deviceSourceEngine->getUID()) {
                qDebug("FileSink::getLocalDevice: source device at index %u is the parent device", index);
            } else {
                return deviceSource;
            }
        }
        else
        {
            qDebug("FileSink::getLocalDevice: source device at index %u is not a SigMF File sink", index);
        }
    }
    else
    {
        qDebug("FileSink::getLocalDevice: non existent source device index: %u", index);
    }

    return nullptr;
}

void FileSink::applySettings(const FileSinkSettings& settings, bool force)
{
    qDebug() << "FileSink::applySettings:"
        << "m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
        << "m_log2Decim: " << settings.m_log2Decim
        << "m_fileRecordName: " << settings.m_fileRecordName
        << "force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_fileRecordName != m_settings.m_fileRecordName) || force) {
        reverseAPIKeys.append("fileRecordName");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_log2Decim != m_settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((settings.m_spectrumSquelchMode != m_settings.m_spectrumSquelchMode) || force) {
        reverseAPIKeys.append("spectrumSquelchMode");
    }
    if ((settings.m_spectrumSquelch != m_settings.m_spectrumSquelch) || force) {
        reverseAPIKeys.append("spectrumSquelch");
    }
    if ((settings.m_preRecordTime != m_settings.m_preRecordTime) || force) {
        reverseAPIKeys.append("preRecordTime");
    }
    if ((settings.m_squelchPostRecordTime != m_settings.m_squelchPostRecordTime) || force) {
        reverseAPIKeys.append("squelchPostRecordTime");
    }
    if ((settings.m_squelchRecordingEnable != m_settings.m_squelchRecordingEnable) || force) {
        reverseAPIKeys.append("squelchRecordingEnable");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    FileSinkBaseband::MsgConfigureFileSinkBaseband *msg = FileSinkBaseband::MsgConfigureFileSinkBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if ((settings.m_useReverseAPI) && (reverseAPIKeys.size() != 0))
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    QList<MessageQueue*> *messageQueues = MainCore::instance()->getMessagePipes().getMessageQueues(this, "settings");

    if (messageQueues) {
        sendChannelSettings(messageQueues, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void FileSink::record(bool record)
{
    FileSinkBaseband::MsgConfigureFileSinkWork *msg = FileSinkBaseband::MsgConfigureFileSinkWork::create(record);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

uint64_t FileSink::getMsCount() const
{
    if (m_basebandSink) {
        return m_basebandSink->getMsCount();
    } else {
        return 0;
    }
}

uint64_t FileSink::getByteCount() const
{
    if (m_basebandSink) {
        return m_basebandSink->getByteCount();
    } else {
        return 0;
    }
}

unsigned int FileSink::getNbTracks() const
{
    if (m_basebandSink) {
        return m_basebandSink->getNbTracks();
    } else {
        return 0;
    }
}

int FileSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSinkSettings(new SWGSDRangel::SWGFileSinkSettings());
    response.getFileSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int FileSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    FileSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureFileSink *msg = MsgConfigureFileSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("FileSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureFileSink *msgToGUI = MsgConfigureFileSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int FileSink::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setFileSinkReport(new SWGSDRangel::SWGFileSinkReport());
    response.getFileSinkReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int FileSink::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGFileSinkActions *swgFileSinkActions = query.getFileSinkActions();

    if (swgFileSinkActions)
    {
        if (channelActionsKeys.contains("record"))
        {
            bool record = swgFileSinkActions->getRecord() != 0;

            if (!m_settings.m_squelchRecordingEnable)
            {
                FileSinkBaseband::MsgConfigureFileSinkWork *msg = FileSinkBaseband::MsgConfigureFileSinkWork::create(record);
                m_basebandSink->getInputMessageQueue()->push(msg);

                if (getMessageQueueToGUI())
                {
                    FileSinkMessages::MsgReportRecording *msgToGUI = FileSinkMessages::MsgReportRecording::create(record);
                    getMessageQueueToGUI()->push(msgToGUI);
                }
            }
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing FileSinkActions in query";
        return 400;
    }
}

void FileSink::webapiUpdateChannelSettings(
        FileSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getFileSinkSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("fileRecordName")) {
        settings.m_fileRecordName = *response.getFileSinkSettings()->getFileRecordName();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getFileSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getFileSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getFileSinkSettings()->getLog2Decim();
    }
    if (channelSettingsKeys.contains("spectrumSquelchMode")) {
        settings.m_spectrumSquelchMode = response.getFileSinkSettings()->getSpectrumSquelchMode() != 0;
    }
    if (channelSettingsKeys.contains("spectrumSquelch")) {
        settings.m_spectrumSquelch = response.getFileSinkSettings()->getSpectrumSquelch();
    }
    if (channelSettingsKeys.contains("preRecordTime")) {
        settings.m_preRecordTime = response.getFileSinkSettings()->getPreRecordTime();
    }
    if (channelSettingsKeys.contains("squelchPostRecordTime")) {
        settings.m_squelchPostRecordTime = response.getFileSinkSettings()->getSquelchPostRecordTime();
    }
    if (channelSettingsKeys.contains("squelchRecordingEnable")) {
        settings.m_squelchRecordingEnable = response.getFileSinkSettings()->getSquelchRecordingEnable() != 0;
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
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_reverseAPIChannelIndex = response.getFileSinkSettings()->getInputFrequencyOffset();
    }
}

void FileSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const FileSinkSettings& settings)
{
    response.getFileSinkSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);

    if (response.getFileSinkSettings()->getFileRecordName()) {
        *response.getFileSinkSettings()->getFileRecordName() = settings.m_fileRecordName;
    } else {
        response.getFileSinkSettings()->setFileRecordName(new QString(settings.m_fileRecordName));
    }

    response.getFileSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getFileSinkSettings()->getTitle()) {
        *response.getFileSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getFileSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getFileSinkSettings()->setLog2Decim(settings.m_log2Decim);
    response.getFileSinkSettings()->setSpectrumSquelchMode(settings.m_spectrumSquelchMode ? 1 : 0);
    response.getFileSinkSettings()->setSpectrumSquelch(settings.m_spectrumSquelch);
    response.getFileSinkSettings()->setPreRecordTime(settings.m_preRecordTime);
    response.getFileSinkSettings()->setSquelchPostRecordTime(settings.m_squelchPostRecordTime);
    response.getFileSinkSettings()->setSquelchRecordingEnable(settings.m_squelchRecordingEnable ? 1 : 0);
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

void FileSink::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getFileSinkReport()->setSpectrumSquelch(m_basebandSink->isSquelchOpen() ? 1 : 0);
    response.getFileSinkReport()->setSpectrumMax(m_basebandSink->getSpecMax());
    response.getFileSinkReport()->setSinkSampleRate(m_basebandSink->getSinkSampleRate());
    response.getFileSinkReport()->setRecordTimeMs(getMsCount());
    response.getFileSinkReport()->setRecordSize(getByteCount());
    response.getFileSinkReport()->setRecording(m_basebandSink->isRecording() ? 1 : 0);
    response.getFileSinkReport()->setRecordCaptures(getNbTracks());
    response.getFileSinkReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void FileSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const FileSinkSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
            .arg(settings.m_reverseAPIAddress)
            .arg(settings.m_reverseAPIPort)
            .arg(settings.m_reverseAPIDeviceIndex)
            .arg(settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(QUrl(channelSettingsURL));
    m_networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QBuffer *buffer = new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    QNetworkReply *reply = m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);
    buffer->setParent(reply);

    delete swgChannelSettings;
}

void FileSink::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const FileSinkSettings& settings,
    bool force)
{
    QList<MessageQueue*>::iterator it = messageQueues->begin();

    for (; it != messageQueues->end(); ++it)
    {
        SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
        webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
        MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
            this,
            channelSettingsKeys,
            swgChannelSettings,
            force
        );
        (*it)->push(msg);
    }
}

void FileSink::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const FileSinkSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setFileSinkSettings(new SWGSDRangel::SWGFileSinkSettings());
    SWGSDRangel::SWGFileSinkSettings *swgFileSinkSettings = swgChannelSettings->getFileSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        swgFileSinkSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("fileRecordName")) {
        swgFileSinkSettings->setTitle(new QString(settings.m_fileRecordName));
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgFileSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgFileSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgFileSinkSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("spectrumSquelchMode")) {
        swgFileSinkSettings->setSpectrumSquelchMode(settings.m_spectrumSquelchMode ? 1 : 0);
    }
    if (channelSettingsKeys.contains("spectrumSquelch")) {
        swgFileSinkSettings->setSpectrumSquelch(settings.m_spectrumSquelch);
    }
    if (channelSettingsKeys.contains("preRecordTime")) {
        swgFileSinkSettings->setPreRecordTime(settings.m_preRecordTime);
    }
    if (channelSettingsKeys.contains("squelchPostRecordTime")) {
        swgFileSinkSettings->setSquelchPostRecordTime(settings.m_squelchPostRecordTime);
    }
    if (channelSettingsKeys.contains("squelchRecordingEnable")) {
        swgFileSinkSettings->setSquelchRecordingEnable(settings.m_squelchRecordingEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        swgFileSinkSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void FileSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "FileSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("FileSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
