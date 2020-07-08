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

#include "util/simpleserializer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"

#include "sigmffilesinkbaseband.h"
#include "sigmffilesink.h"

MESSAGE_CLASS_DEFINITION(SigMFFileSink::MsgConfigureSigMFFileSink, Message)
MESSAGE_CLASS_DEFINITION(SigMFFileSink::MsgBasebandSampleRateNotification, Message)

const QString SigMFFileSink::m_channelIdURI = "sdrangel.channel.sigmffilesink";
const QString SigMFFileSink::m_channelId = "SigMFFileSink";

SigMFFileSink::SigMFFileSink(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_basebandSampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new SigMFFileSinkBaseband();
    m_basebandSink->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

SigMFFileSink::~SigMFFileSink()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

uint32_t SigMFFileSink::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void SigMFFileSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void SigMFFileSink::start()
{
	qDebug("SigMFFileSink::start");
    m_basebandSink->reset();
    m_thread->start();
}

void SigMFFileSink::stop()
{
    qDebug("SigMFFileSink::stop");
	m_thread->exit();
	m_thread->wait();
}

bool SigMFFileSink::handleMessage(const Message& cmd)
{
    if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "SigMFFileSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();

        calculateFrequencyOffset(m_settings.m_log2Decim, m_settings.m_filterChainHash); // This is when device sample rate changes

        DSPSignalNotification *msg = new DSPSignalNotification(notif.getSampleRate(), notif.getCenterFrequency());
        m_basebandSink->getInputMessageQueue()->push(msg);

        if (getMessageQueueToGUI())
        {
            MsgBasebandSampleRateNotification *msg = MsgBasebandSampleRateNotification::create(notif.getSampleRate());
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else if (MsgConfigureSigMFFileSink::match(cmd))
    {
        MsgConfigureSigMFFileSink& cfg = (MsgConfigureSigMFFileSink&) cmd;
        qDebug() << "SigMFFileSink::handleMessage: MsgConfigureSigMFFileSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray SigMFFileSink::serialize() const
{
    return m_settings.serialize();
}

bool SigMFFileSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureSigMFFileSink *msg = MsgConfigureSigMFFileSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureSigMFFileSink *msg = MsgConfigureSigMFFileSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void SigMFFileSink::getLocalDevices(std::vector<uint32_t>& indexes)
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

DeviceSampleSource *SigMFFileSink::getLocalDevice(uint32_t index)
{
    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < dspEngine->getDeviceSourceEnginesNumber())
    {
        DSPDeviceSourceEngine *deviceSourceEngine = dspEngine->getDeviceSourceEngineByIndex(index);
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

        if (deviceSource->getDeviceDescription() == "LocalInput")
        {
            if (!getDeviceAPI()) {
                qDebug("SigMFFileSink::getLocalDevice: the parent device is unset");
            } else if (getDeviceAPI()->getDeviceUID() == deviceSourceEngine->getUID()) {
                qDebug("SigMFFileSink::getLocalDevice: source device at index %u is the parent device", index);
            } else {
                return deviceSource;
            }
        }
        else
        {
            qDebug("SigMFFileSink::getLocalDevice: source device at index %u is not a SigMF File sink", index);
        }
    }
    else
    {
        qDebug("SigMFFileSink::getLocalDevice: non existent source device index: %u", index);
    }

    return nullptr;
}

void SigMFFileSink::applySettings(const SigMFFileSinkSettings& settings, bool force)
{
    qDebug() << "SigMFFileSink::applySettings:"
            << "force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_log2Decim != m_settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((settings.m_filterChainHash != m_settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }

    if ((settings.m_log2Decim != m_settings.m_log2Decim)
     || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        calculateFrequencyOffset(settings.m_log2Decim, settings.m_filterChainHash);
    }

    SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkBaseband *msg = SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkBaseband::create(settings, force);
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

    m_settings = settings;
}

void SigMFFileSink::record(bool record)
{
    SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkWork *msg = SigMFFileSinkBaseband::MsgConfigureSigMFFileSinkWork::create(record);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void SigMFFileSink::validateFilterChainHash(SigMFFileSinkSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void SigMFFileSink::calculateFrequencyOffset(uint32_t log2Decim, uint32_t filterChainHash)
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(log2Decim, filterChainHash);
    m_frequencyOffset = m_basebandSampleRate * shiftFactor;
}

int SigMFFileSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    response.getLocalSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int SigMFFileSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    SigMFFileSinkSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureSigMFFileSink *msg = MsgConfigureSigMFFileSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("SigMFFileSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureSigMFFileSink *msgToGUI = MsgConfigureSigMFFileSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void SigMFFileSink::webapiUpdateChannelSettings(
        SigMFFileSinkSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLocalSinkSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLocalSinkSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getLocalSinkSettings()->getLog2Decim();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getLocalSinkSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalSinkSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalSinkSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalSinkSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalSinkSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLocalSinkSettings()->getReverseApiChannelIndex();
    }
}

void SigMFFileSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const SigMFFileSinkSettings& settings)
{
    response.getLocalSinkSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLocalSinkSettings()->getTitle()) {
        *response.getLocalSinkSettings()->getTitle() = settings.m_title;
    } else {
        response.getLocalSinkSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLocalSinkSettings()->setLog2Decim(settings.m_log2Decim);
    response.getLocalSinkSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getLocalSinkSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalSinkSettings()->getReverseApiAddress()) {
        *response.getLocalSinkSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalSinkSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalSinkSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalSinkSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLocalSinkSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void SigMFFileSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const SigMFFileSinkSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("SigMFFileSink"));
    swgChannelSettings->setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    SWGSDRangel::SWGLocalSinkSettings *swgLocalSinkSettings = swgChannelSettings->getLocalSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLocalSinkSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLocalSinkSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgLocalSinkSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgLocalSinkSettings->setFilterChainHash(settings.m_filterChainHash);
    }

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

void SigMFFileSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "v::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("SigMFFileSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
