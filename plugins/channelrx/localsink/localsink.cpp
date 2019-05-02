///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2019 Edouard Griffiths, F4EXB.                                  //
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

#include "localsink.h"

#include <sys/time.h>
#include <unistd.h>
#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesink.h"
#include "dsp/downchannelizer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/hbfilterchainconverter.h"
#include "device/devicesourceapi.h"

#include "localsinkthread.h"

MESSAGE_CLASS_DEFINITION(LocalSink::MsgConfigureLocalSink, Message)
MESSAGE_CLASS_DEFINITION(LocalSink::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(LocalSink::MsgConfigureChannelizer, Message)

const QString LocalSink::m_channelIdURI = "sdrangel.channel.localsink";
const QString LocalSink::m_channelId = "LocalSink";

LocalSink::LocalSink(DeviceSourceAPI *deviceAPI) :
        ChannelSinkAPI(m_channelIdURI),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_sinkThread(0),
        m_txBlockIndex(0),
        m_frameCount(0),
        m_sampleIndex(0),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_sampleRate(48000),
        m_deviceSampleRate(48000)
{
    setObjectName(m_channelId);

    m_channelizer = new DownChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSink(m_channelizer, this);
    m_deviceAPI->addThreadedSink(m_threadedChannelizer);
    m_deviceAPI->addChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

LocalSink::~LocalSink()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelAPI(this);
    m_deviceAPI->removeThreadedSink(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void LocalSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    emit samplesAvailable((const quint8*) &(*begin), (end-begin)*sizeof(Sample));
}

void LocalSink::start()
{
    qDebug("LocalSink::start");

    if (m_running) {
        stop();
    }

    m_sinkThread = new LocalSinkThread();
    DeviceSampleSource *deviceSource = getLocalDevice(m_settings.m_localDeviceIndex);

    if (deviceSource) {
        m_sinkThread->setSampleFifo(deviceSource->getSampleFifo());
    }

    connect(this,
            SIGNAL(samplesAvailable(const quint8*, uint)),
            m_sinkThread,
            SLOT(processSamples(const quint8*, uint)),
            Qt::QueuedConnection);

    m_sinkThread->startStop(true);
    m_running = true;
}

void LocalSink::stop()
{
    qDebug("LocalSink::stop");

    if (m_sinkThread != 0)
    {
        m_sinkThread->startStop(false);
        m_sinkThread->deleteLater();
        m_sinkThread = 0;
    }

    m_running = false;
}

bool LocalSink::handleMessage(const Message& cmd)
{
    (void) cmd;
	if (DownChannelizer::MsgChannelizerNotification::match(cmd))
	{
		DownChannelizer::MsgChannelizerNotification& notif = (DownChannelizer::MsgChannelizerNotification&) cmd;

        qDebug() << "LocalSink::handleMessage: MsgChannelizerNotification:"
                << " channelSampleRate: " << notif.getSampleRate()
                << " offsetFrequency: " << notif.getFrequencyOffset();

        if (notif.getSampleRate() > 0)
        {
            setSampleRate(notif.getSampleRate());
        }

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "LocalSink::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        setCenterFrequency(notif.getCenterFrequency());
        m_deviceSampleRate = notif.getSampleRate();
        calculateFrequencyOffset(); // This is when device sample rate changes
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex);

        // Redo the channelizer stuff with the new sample rate to re-synchronize everything
        m_channelizer->set(m_channelizer->getInputMessageQueue(),
            m_settings.m_log2Decim,
            m_settings.m_filterChainHash);

        if (m_guiMessageQueue)
        {
            MsgSampleRateNotification *msg = MsgSampleRateNotification::create(notif.getSampleRate());
            m_guiMessageQueue->push(msg);
        }

        return true;
    }
    else if (MsgConfigureLocalSink::match(cmd))
    {
        MsgConfigureLocalSink& cfg = (MsgConfigureLocalSink&) cmd;
        qDebug() << "LocalSink::handleMessage: MsgConfigureLocalSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        m_settings.m_log2Decim = cfg.getLog2Decim();
        m_settings.m_filterChainHash =  cfg.getFilterChainHash();

        qDebug() << "LocalSink::handleMessage: MsgConfigureChannelizer:"
                << " log2Decim: " << m_settings.m_log2Decim
                << " filterChainHash: " << m_settings.m_filterChainHash;

        m_channelizer->set(m_channelizer->getInputMessageQueue(),
            m_settings.m_log2Decim,
            m_settings.m_filterChainHash);

        calculateFrequencyOffset(); // This is when decimation or filter chain changes
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex);

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray LocalSink::serialize() const
{
    return m_settings.serialize();
}

bool LocalSink::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void LocalSink::getLocalDevices(std::vector<uint32_t>& indexes)
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

DeviceSampleSource *LocalSink::getLocalDevice(uint32_t index)
{
    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < dspEngine->getDeviceSourceEnginesNumber())
    {
        DSPDeviceSourceEngine *deviceSourceEngine = dspEngine->getDeviceSourceEngineByIndex(index);
        DeviceSampleSource *deviceSource = deviceSourceEngine->getSource();

        if (deviceSource->getDeviceDescription() == "LocalInput")
        {
            if (!getDeviceSourceAPI()) {
                qDebug("LocalSink::getLocalDevice: the parent device is unset");
            } else if (getDeviceSourceAPI()->getDeviceUID() == deviceSourceEngine->getUID()) {
                qDebug("LocalSink::getLocalDevice: source device at index %u is the parent device", index);
            } else {
                return deviceSource;
            }
        }
        else
        {
            qDebug("LocalSink::getLocalDevice: source device at index %u is not a Local Input source", index);
        }
    }
    else
    {
        qDebug("LocalSink::getLocalDevice: non existent source device index: %u", index);
    }

    return nullptr;
}

void LocalSink::propagateSampleRateAndFrequency(uint32_t index)
{
    DeviceSampleSource *deviceSource = getLocalDevice(index);

    if (deviceSource)
    {
        deviceSource->setSampleRate(m_deviceSampleRate / (1<<m_settings.m_log2Decim));
        deviceSource->setCenterFrequency(m_centerFrequency + m_frequencyOffset);
    }
}

void LocalSink::applySettings(const LocalSinkSettings& settings, bool force)
{
    qDebug() << "LocalSink::applySettings:"
            << " m_localDeviceIndex: " << settings.m_localDeviceIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_localDeviceIndex != m_settings.m_localDeviceIndex) || force)
    {
        reverseAPIKeys.append("localDeviceIndex");
        DeviceSampleSource *deviceSource = getLocalDevice(settings.m_localDeviceIndex);

        if (deviceSource)
        {
            if (m_sinkThread) {
                m_sinkThread->setSampleFifo(deviceSource->getSampleFifo());
            }

            propagateSampleRateAndFrequency(settings.m_localDeviceIndex);
        }
        else
        {
            qWarning("LocalSink::applySettings: invalid local device for index %u", settings.m_localDeviceIndex);
        }
    }

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

void LocalSink::validateFilterChainHash(LocalSinkSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void LocalSink::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

int LocalSink::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    response.getLocalSinkSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int LocalSink::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    LocalSinkSettings settings = m_settings;

    if (channelSettingsKeys.contains("localDeviceIndex")) {
        settings.m_localDeviceIndex = response.getLocalSinkSettings()->getLocalDeviceIndex();
    }
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

    MsgConfigureLocalSink *msg = MsgConfigureLocalSink::create(settings, force);
    m_inputMessageQueue.push(msg);

    if ((settings.m_log2Decim != m_settings.m_log2Decim) || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        MsgConfigureChannelizer *msg = MsgConfigureChannelizer::create(settings.m_log2Decim, settings.m_filterChainHash);
        m_inputMessageQueue.push(msg);
    }

    qDebug("LocalSink::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalSink *msgToGUI = MsgConfigureLocalSink::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LocalSink::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LocalSinkSettings& settings)
{
    response.getLocalSinkSettings()->setLocalDeviceIndex(settings.m_localDeviceIndex);
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

void LocalSink::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSinkSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setTx(0);
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("LocalSink"));
    swgChannelSettings->setLocalSinkSettings(new SWGSDRangel::SWGLocalSinkSettings());
    SWGSDRangel::SWGLocalSinkSettings *swgLocalSinkSettings = swgChannelSettings->getLocalSinkSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("localDeviceIndex") || force) {
        swgLocalSinkSettings->setLocalDeviceIndex(settings.m_localDeviceIndex);
    }
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

    QBuffer *buffer=new QBuffer();
    buffer->open((QBuffer::ReadWrite));
    buffer->write(swgChannelSettings->asJson().toUtf8());
    buffer->seek(0);

    // Always use PATCH to avoid passing reverse API settings
    m_networkManager->sendCustomRequest(m_networkRequest, "PATCH", buffer);

    delete swgChannelSettings;
}

void LocalSink::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalSink::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("LocalSink::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
