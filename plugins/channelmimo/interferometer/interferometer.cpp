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

#include <QThread>
#include <QDebug>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>

#include "device/deviceapi.h"
#include "dsp/downchannelizer.h"
#include "dsp/threadedbasebandsamplesink.h"

#include "SWGChannelSettings.h"

#include "interferometersink.h"
#include "interferometer.h"

MESSAGE_CLASS_DEFINITION(Interferometer::MsgConfigureInterferometer, Message)
MESSAGE_CLASS_DEFINITION(Interferometer::MsgConfigureChannelizer, Message)
MESSAGE_CLASS_DEFINITION(Interferometer::MsgSampleRateNotification, Message)

const QString Interferometer::m_channelIdURI = "sdrangel.channel.interferometer";
const QString Interferometer::m_channelId = "Interferometer";

Interferometer::Interferometer(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
    m_deviceAPI(deviceAPI),
    m_spectrumSink(nullptr),
    m_scopeSink(nullptr)
{
    m_deviceAPI->addChannelSinkAPI(this);
    m_thread = new QThread(this);
    m_sink = new InterferometerSink();
    m_sink->moveToThread(m_thread);
    start();
}

Interferometer::~Interferometer()
{
    m_deviceAPI->removeChannelSinkAPI(this);

    if (m_thread->isRunning()) {
        stop();
    }

    delete m_sink;
    delete m_thread;
}

void Interferometer::setSpectrumSink(BasebandSampleSink *spectrumSink)
{
    m_spectrumSink = spectrumSink;
    m_sink->setSpectrumSink(spectrumSink);
}

void Interferometer::setScopeSink(BasebandSampleSink *scopeSink)
{
    m_scopeSink = scopeSink;
    m_sink->setScopeSink(scopeSink);
}

void Interferometer::start()
{
    m_thread->start();
}

void Interferometer::stop()
{
	m_thread->exit();
	m_thread->wait();
}

void Interferometer::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    m_sink->feed(begin, end, sinkIndex);
}

void Interferometer::applySettings(const InterferometerSettings& settings, bool force)
{
    if ((m_settings.m_log2Decim != settings.m_log2Decim)
     || (m_settings.m_filterChainHash != settings.m_filterChainHash) || force)
    {
        InterferometerSink::MsgConfigureChannelizer *msg = InterferometerSink::MsgConfigureChannelizer::create(
            settings.m_log2Decim, settings.m_filterChainHash);
        m_sink->getInputMessageQueue()->push(msg);
    }
}

bool Interferometer::handleMessage(const Message& cmd)
{
    if (MsgConfigureInterferometer::match(cmd))
    {
        MsgConfigureInterferometer& cfg = (MsgConfigureInterferometer&) cmd;
        qDebug() << "Interferometer::handleMessage: MsgConfigureInterferometer";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else
    {
        return false;
    }
}

void Interferometer::validateFilterChainHash(InterferometerSettings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

int Interferometer::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
    response.getInterferometerSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int Interferometer::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    InterferometerSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureInterferometer *msg = MsgConfigureInterferometer::create(settings, force);
    m_inputMessageQueue.push(msg);

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void Interferometer::webapiUpdateChannelSettings(
        InterferometerSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getInterferometerSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getInterferometerSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getInterferometerSettings()->getLog2Decim();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getInterferometerSettings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getInterferometerSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getInterferometerSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getInterferometerSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getInterferometerSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getInterferometerSettings()->getReverseApiChannelIndex();
    }
}

void Interferometer::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const InterferometerSettings& settings)
{
    response.getInterferometerSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getInterferometerSettings()->getTitle()) {
        *response.getInterferometerSettings()->getTitle() = settings.m_title;
    } else {
        response.getInterferometerSettings()->setTitle(new QString(settings.m_title));
    }

    response.getInterferometerSettings()->setLog2Decim(settings.m_log2Decim);
    response.getInterferometerSettings()->setFilterChainHash(settings.m_filterChainHash);
    response.getInterferometerSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getInterferometerSettings()->getReverseApiAddress()) {
        *response.getInterferometerSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getInterferometerSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getInterferometerSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getInterferometerSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getInterferometerSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void Interferometer::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const InterferometerSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(2); // MIMO sink
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("Interferometer"));
    swgChannelSettings->setInterferometerSettings(new SWGSDRangel::SWGInterferometerSettings());
    SWGSDRangel::SWGInterferometerSettings *swgInterferometerSettings = swgChannelSettings->getInterferometerSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgInterferometerSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgInterferometerSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgInterferometerSettings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgInterferometerSettings->setFilterChainHash(settings.m_filterChainHash);
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

void Interferometer::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "Interferometer::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
        return;
    }

    QString answer = reply->readAll();
    answer.chop(1); // remove last \n
    qDebug("Interferometer::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
}
