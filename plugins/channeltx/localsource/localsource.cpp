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

#include "localsource.h"

#include <boost/crc.hpp>
#include <boost/cstdint.hpp>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include "SWGChannelSettings.h"

#include "util/simpleserializer.h"
#include "dsp/threadedbasebandsamplesource.h"
#include "dsp/upchannelizer.h"
#include "dsp/dspcommands.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesink.h"
#include "dsp/hbfilterchainconverter.h"
#include "device/deviceapi.h"

#include "localsourcethread.h"

MESSAGE_CLASS_DEFINITION(LocalSource::MsgConfigureLocalSource, Message)
MESSAGE_CLASS_DEFINITION(LocalSource::MsgSampleRateNotification, Message)
MESSAGE_CLASS_DEFINITION(LocalSource::MsgConfigureChannelizer, Message)

const QString LocalSource::m_channelIdURI = "sdrangel.channel.localsource";
const QString LocalSource::m_channelId = "LocalSource";

LocalSource::LocalSource(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_sinkThread(nullptr),
        m_localSampleSourceFifo(nullptr),
        m_chunkSize(0),
        m_localSamplesIndex(0),
        m_localSamplesIndexOffset(0),
        m_centerFrequency(0),
        m_frequencyOffset(0),
        m_sampleRate(48000),
        m_deviceSampleRate(48000),
        m_settingsMutex(QMutex::Recursive)
{
    setObjectName(m_channelId);

    m_channelizer = new UpChannelizer(this);
    m_threadedChannelizer = new ThreadedBasebandSampleSource(m_channelizer, this);
    m_deviceAPI->addChannelSource(m_threadedChannelizer);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

LocalSource::~LocalSource()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(m_threadedChannelizer);
    delete m_threadedChannelizer;
    delete m_channelizer;
}

void LocalSource::pull(Sample& sample)
{
    if (m_localSampleSourceFifo)
    {
        QMutexLocker mutexLocker(&m_settingsMutex);
        sample = m_localSamples[m_localSamplesIndex + m_localSamplesIndexOffset];

        if (m_localSamplesIndex < m_chunkSize - 1)
        {
            m_localSamplesIndex++;
        }
        else
        {
            m_localSamplesIndex = 0;

            if (m_localSamplesIndexOffset == 0) {
                m_localSamplesIndexOffset = m_chunkSize;
            } else {
                m_localSamplesIndexOffset = 0;
            }

            emit pullSamples(m_chunkSize);
        }
    }
    else
    {
        sample = Sample{0, 0};
    }
}

void LocalSource::processSamples(int offset)
{
    if (m_localSampleSourceFifo)
    {
        int destOffset = (m_localSamplesIndexOffset == 0 ? m_chunkSize : 0);
        SampleVector::iterator beginSource;
        SampleVector::iterator beginDestination = m_localSamples.begin() + destOffset;
        m_localSampleSourceFifo->setIteratorFromOffset(beginSource, offset);
        std::copy(beginSource, beginSource + m_chunkSize, beginDestination);
    }
}

void LocalSource::pullAudio(int nbSamples)
{
    (void) nbSamples;
}

void LocalSource::start()
{
    qDebug("LocalSource::start");

    if (m_running) {
        stop();
    }

    m_sinkThread = new LocalSourceThread();
    DeviceSampleSink *deviceSink = getLocalDevice(m_settings.m_localDeviceIndex);

    if (deviceSink)
    {
        m_localSampleSourceFifo = deviceSink->getSampleFifo();
        m_chunkSize = m_localSampleSourceFifo->size() / 16;
        m_localSamples.resize(2*m_chunkSize);
        m_localSamplesIndex = 0;
        m_sinkThread->setSampleFifo(m_localSampleSourceFifo);
    }
    else
    {
        m_localSampleSourceFifo = nullptr;
    }

    connect(this,
            SIGNAL(pullSamples(unsigned int)),
            m_sinkThread,
            SLOT(pullSamples(unsigned int)),
            Qt::QueuedConnection);

    connect(m_sinkThread,
            SIGNAL(samplesAvailable(int)),
            this,
            SLOT(processSamples(int)),
            Qt::QueuedConnection);

    m_sinkThread->startStop(true);
    m_running = true;
}

void LocalSource::stop()
{
    qDebug("LocalSource::stop");

    if (m_sinkThread != 0)
    {
        m_sinkThread->startStop(false);
        m_sinkThread->deleteLater();
        m_sinkThread = 0;
    }

    m_running = false;
}

bool LocalSource::handleMessage(const Message& cmd)
{
	if (UpChannelizer::MsgChannelizerNotification::match(cmd))
	{
		UpChannelizer::MsgChannelizerNotification& notif = (UpChannelizer::MsgChannelizerNotification&) cmd;
        int sampleRate = notif.getSampleRate();

        qDebug() << "LocalSource::handleMessage: MsgChannelizerNotification:"
                << " channelSampleRate: " << sampleRate
                << " offsetFrequency: " << notif.getFrequencyOffset();

        if (sampleRate > 0)
        {
            if (m_localSampleSourceFifo)
            {
                QMutexLocker mutexLocker(&m_settingsMutex);
                m_localSampleSourceFifo->resize(sampleRate);
                m_chunkSize = sampleRate / 8;
                m_localSamplesIndex = 0;
                m_localSamplesIndexOffset = 0;
                m_localSamples.resize(2*m_chunkSize);
            }

            setSampleRate(sampleRate);
        }

		return true;
	}
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;

        qDebug() << "LocalSource::handleMessage: DSPSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency();

        setCenterFrequency(notif.getCenterFrequency());
        m_deviceSampleRate = notif.getSampleRate();
        calculateFrequencyOffset(); // This is when device sample rate changes
        propagateSampleRateAndFrequency(m_settings.m_localDeviceIndex);

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
    else if (MsgConfigureLocalSource::match(cmd))
    {
        MsgConfigureLocalSource& cfg = (MsgConfigureLocalSource&) cmd;
        qDebug() << "LocalSource::handleMessage: MsgConfigureLocalSink";
        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgConfigureChannelizer::match(cmd))
    {
        MsgConfigureChannelizer& cfg = (MsgConfigureChannelizer&) cmd;
        m_settings.m_log2Interp = cfg.getLog2Interp();
        m_settings.m_filterChainHash =  cfg.getFilterChainHash();

        qDebug() << "LocalSource::handleMessage: MsgConfigureChannelizer:"
                << " log2Interp: " << m_settings.m_log2Interp
                << " filterChainHash: " << m_settings.m_filterChainHash;

        m_channelizer->set(m_channelizer->getInputMessageQueue(),
            m_settings.m_log2Interp,
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

QByteArray LocalSource::serialize() const
{
    return m_settings.serialize();
}

bool LocalSource::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void LocalSource::getLocalDevices(std::vector<uint32_t>& indexes)
{
    indexes.clear();
    DSPEngine *dspEngine = DSPEngine::instance();

    for (uint32_t i = 0; i < dspEngine->getDeviceSinkEnginesNumber(); i++)
    {
        DSPDeviceSinkEngine *deviceSinkEngine = dspEngine->getDeviceSinkEngineByIndex(i);
        DeviceSampleSink *deviceSink = deviceSinkEngine->getSink();

        if (deviceSink->getDeviceDescription() == "LocalOutput") {
            indexes.push_back(i);
        }
    }
}

DeviceSampleSink *LocalSource::getLocalDevice(uint32_t index)
{
    DSPEngine *dspEngine = DSPEngine::instance();

    if (index < dspEngine->getDeviceSinkEnginesNumber())
    {
        DSPDeviceSinkEngine *deviceSinkEngine = dspEngine->getDeviceSinkEngineByIndex(index);
        DeviceSampleSink *deviceSink = deviceSinkEngine->getSink();

        if (deviceSink->getDeviceDescription() == "LocalOutput")
        {
            if (!getDeviceAPI()) {
                qDebug("LocalSource::getLocalDevice: the parent device is unset");
            } else if (getDeviceAPI()->getDeviceUID() == deviceSinkEngine->getUID()) {
                qDebug("LocalSource::getLocalDevice: sink device at index %u is the parent device", index);
            } else {
                return deviceSink;
            }
        }
        else
        {
            qDebug("LocalSource::getLocalDevice: sink device at index %u is not a Local Output source", index);
        }
    }
    else
    {
        qDebug("LocalSource::getLocalDevice: non existent sink device index: %u", index);
    }

    return nullptr;
}

void LocalSource::propagateSampleRateAndFrequency(uint32_t index)
{
    DeviceSampleSink *deviceSink = getLocalDevice(index);

    if (deviceSink)
    {
        deviceSink->setSampleRate(m_deviceSampleRate / (1<<m_settings.m_log2Interp));
        deviceSink->setCenterFrequency(m_centerFrequency + m_frequencyOffset);
    }
}

void LocalSource::applySettings(const LocalSourceSettings& settings, bool force)
{
    qDebug() << "LocalSource::applySettings:"
            << " m_localDeviceIndex: " << settings.m_localDeviceIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_localDeviceIndex != m_settings.m_localDeviceIndex) || force)
    {
        reverseAPIKeys.append("localDeviceIndex");
        DeviceSampleSink *deviceSink = getLocalDevice(settings.m_localDeviceIndex);

        if (deviceSink)
        {
            if (m_sinkThread) {
                m_sinkThread->setSampleFifo(deviceSink->getSampleFifo());
            }

            propagateSampleRateAndFrequency(settings.m_localDeviceIndex);
        }
        else
        {
            qWarning("LocalSource::applySettings: invalid local device for index %u", settings.m_localDeviceIndex);
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

void LocalSource::validateFilterChainHash(LocalSourceSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Interp; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void LocalSource::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Interp, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

int LocalSource::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
    response.getLocalSourceSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int LocalSource::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    LocalSourceSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureLocalSource *msg = MsgConfigureLocalSource::create(settings, force);
    m_inputMessageQueue.push(msg);

    if ((settings.m_log2Interp != m_settings.m_log2Interp) || (settings.m_filterChainHash != m_settings.m_filterChainHash) || force)
    {
        MsgConfigureChannelizer *msg = MsgConfigureChannelizer::create(settings.m_log2Interp, settings.m_filterChainHash);
        m_inputMessageQueue.push(msg);
    }

    qDebug("LocalSource::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureLocalSource *msgToGUI = MsgConfigureLocalSource::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void LocalSource::webapiUpdateChannelSettings(
        LocalSourceSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("localDeviceIndex")) {
        settings.m_localDeviceIndex = response.getLocalSourceSettings()->getLocalDeviceIndex();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getLocalSourceSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getLocalSourceSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Interp")) {
        settings.m_log2Interp = response.getLocalSourceSettings()->getLog2Interp();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getLocalSourceSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getLocalSourceSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getLocalSourceSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getLocalSourceSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getLocalSourceSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getLocalSourceSettings()->getReverseApiChannelIndex();
    }
}

void LocalSource::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const LocalSourceSettings& settings)
{
    response.getLocalSourceSettings()->setLocalDeviceIndex(settings.m_localDeviceIndex);
    response.getLocalSourceSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getLocalSourceSettings()->getTitle()) {
        *response.getLocalSourceSettings()->getTitle() = settings.m_title;
    } else {
        response.getLocalSourceSettings()->setTitle(new QString(settings.m_title));
    }

    response.getLocalSourceSettings()->setLog2Interp(settings.m_log2Interp);
    response.getLocalSourceSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getLocalSourceSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getLocalSourceSettings()->getReverseApiAddress()) {
        *response.getLocalSourceSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getLocalSourceSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getLocalSourceSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getLocalSourceSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getLocalSourceSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void LocalSource::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const LocalSourceSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("LocalSource"));
    swgChannelSettings->setLocalSourceSettings(new SWGSDRangel::SWGLocalSourceSettings());
    SWGSDRangel::SWGLocalSourceSettings *swgLocalSourceSettings = swgChannelSettings->getLocalSourceSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("localDeviceIndex") || force) {
        swgLocalSourceSettings->setLocalDeviceIndex(settings.m_localDeviceIndex);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgLocalSourceSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgLocalSourceSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Interp") || force) {
        swgLocalSourceSettings->setLog2Interp(settings.m_log2Interp);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgLocalSourceSettings->setFilterChainHash(settings.m_filterChainHash);
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

void LocalSource::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "LocalSource::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("LocalSource::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
