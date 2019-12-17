///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
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

#include <stdio.h>
#include <complex.h>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGNFMDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGNFMDemodReport.h"

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "dsp/devicesamplemimo.h"
#include "device/deviceapi.h"
#include "util/db.h"

#include "nfmtestdemod.h"

MESSAGE_CLASS_DEFINITION(NFMDemodTest::MsgConfigureNFMDemod, Message)

const QString NFMDemodTest::m_channelIdURI = "sdrangel.channel.nfmtestdemod";
const QString NFMDemodTest::m_channelId = "NFMTestDemod";

const int NFMDemodTest::m_udpBlockSize = 512;

NFMDemodTest::NFMDemodTest(DeviceAPI *devieAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(devieAPI),
        m_basebandSampleRate(0)
{
    qDebug("NFMDemodTest::NFMDemodTest");
	setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new NFMDemodBaseband();
    m_basebandSink->moveToThread(m_thread);

	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

NFMDemodTest::~NFMDemodTest()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this);
    delete m_basebandSink;
    delete m_thread;
}

uint32_t NFMDemodTest::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void NFMDemodTest::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    m_basebandSink->feed(begin, end);
}

void NFMDemodTest::start()
{
    qDebug() << "NFMDemodTest::start";

    if (m_basebandSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_basebandSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();
}

void NFMDemodTest::stop()
{
    qDebug() << "NFMDemodTest::stop";
	m_thread->exit();
	m_thread->wait();
}

bool NFMDemodTest::handleMessage(const Message& cmd)
{
	if (MsgConfigureNFMDemod::match(cmd))
	{
	    MsgConfigureNFMDemod& cfg = (MsgConfigureNFMDemod&) cmd;
		qDebug() << "NFMDemodTest::handleMessage: MsgConfigureNFMDemod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
	}
	else if (DSPSignalNotification::match(cmd))
	{
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "NFMDemodTest::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);

	    return true;
	}
	else
	{
		return false;
	}
}

void NFMDemodTest::applySettings(const NFMDemodTestSettings& settings, bool force)
{
    qDebug() << "NFMDemodTest::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_afBandwidth: " << settings.m_afBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_volume: " << settings.m_volume
            << " m_squelchGate: " << settings.m_squelchGate
            << " m_deltaSquelch: " << settings.m_deltaSquelch
            << " m_squelch: " << settings.m_squelch
            << " m_ctcssIndex: " << settings.m_ctcssIndex
            << " m_ctcssOn: " << settings.m_ctcssOn
            << " m_highPass: " << settings.m_highPass
            << " m_audioMute: " << settings.m_audioMute
            << " m_audioDeviceName: " << settings.m_audioDeviceName
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIPort: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_ctcssOn != m_settings.m_ctcssOn) || force) {
        reverseAPIKeys.append("ctcssOn");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
    }
    if ((settings.m_afBandwidth != m_settings.m_afBandwidth) || force) {
        reverseAPIKeys.append("afBandwidth");
    }
    if ((settings.m_squelchGate != m_settings.m_squelchGate) || force) {
        reverseAPIKeys.append("squelchGate");
    }
    if ((settings.m_squelch != m_settings.m_squelch) || force) {
        reverseAPIKeys.append("squelch");
    }
    if ((settings.m_deltaSquelch != m_settings.m_deltaSquelch) || force) {
        reverseAPIKeys.append("deltaSquelch");
    }
    if ((settings.m_ctcssIndex != m_settings.m_ctcssIndex) || force) {
        reverseAPIKeys.append("ctcssIndex");
    }
    if ((settings.m_highPass != m_settings.m_highPass) || force) {
        reverseAPIKeys.append("highPass");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this, m_settings.m_streamIndex);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this, settings.m_streamIndex);
        }

        reverseAPIKeys.append("streamIndex");
    }


    NFMDemodBaseband::MsgConfigureNFMDemodBaseband *msg = NFMDemodBaseband::MsgConfigureNFMDemodBaseband::create(settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);


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

QByteArray NFMDemodTest::serialize() const
{
    return m_settings.serialize();
}

bool NFMDemodTest::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int NFMDemodTest::webapiSettingsGet(
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    response.getNfmDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int NFMDemodTest::webapiSettingsPutPatch(
            bool force,
            const QStringList& channelSettingsKeys,
            SWGSDRangel::SWGChannelSettings& response,
            QString& errorMessage)
{
    (void) errorMessage;
    NFMDemodTestSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureNFMDemod *msg = MsgConfigureNFMDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureNFMDemod *msgToGUI = MsgConfigureNFMDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void NFMDemodTest::webapiUpdateChannelSettings(
        NFMDemodTestSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("afBandwidth")) {
        settings.m_afBandwidth = response.getNfmDemodSettings()->getAfBandwidth();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getNfmDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("highPass")) {
        settings.m_highPass = response.getNfmDemodSettings()->getHighPass() != 0;
    }
    if (channelSettingsKeys.contains("ctcssIndex")) {
        settings.m_ctcssIndex = response.getNfmDemodSettings()->getCtcssIndex();
    }
    if (channelSettingsKeys.contains("ctcssOn")) {
        settings.m_ctcssOn = response.getNfmDemodSettings()->getCtcssOn() != 0;
    }
    if (channelSettingsKeys.contains("deltaSquelch")) {
        settings.m_deltaSquelch = response.getNfmDemodSettings()->getDeltaSquelch() != 0;
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getNfmDemodSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getNfmDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getNfmDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getNfmDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("squelch")) {
        settings.m_squelch = response.getNfmDemodSettings()->getSquelch();
    }
    if (channelSettingsKeys.contains("squelchGate")) {
        settings.m_squelchGate = response.getNfmDemodSettings()->getSquelchGate();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getNfmDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getNfmDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getNfmDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getNfmDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getNfmDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getNfmDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getNfmDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getNfmDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getNfmDemodSettings()->getReverseApiChannelIndex();
    }
}

int NFMDemodTest::webapiReportGet(
            SWGSDRangel::SWGChannelReport& response,
            QString& errorMessage)
{
    (void) errorMessage;
    response.setNfmDemodReport(new SWGSDRangel::SWGNFMDemodReport());
    response.getNfmDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void NFMDemodTest::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const NFMDemodTestSettings& settings)
{
    response.getNfmDemodSettings()->setAfBandwidth(settings.m_afBandwidth);
    response.getNfmDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getNfmDemodSettings()->setHighPass(settings.m_highPass ? 1 : 0);
    response.getNfmDemodSettings()->setCtcssIndex(settings.m_ctcssIndex);
    response.getNfmDemodSettings()->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    response.getNfmDemodSettings()->setDeltaSquelch(settings.m_deltaSquelch ? 1 : 0);
    response.getNfmDemodSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getNfmDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getNfmDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getNfmDemodSettings()->setRgbColor(settings.m_rgbColor);
    response.getNfmDemodSettings()->setSquelch(settings.m_squelch);
    response.getNfmDemodSettings()->setSquelchGate(settings.m_squelchGate);
    response.getNfmDemodSettings()->setVolume(settings.m_volume);

    if (response.getNfmDemodSettings()->getTitle()) {
        *response.getNfmDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getNfmDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getNfmDemodSettings()->getAudioDeviceName()) {
        *response.getNfmDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getNfmDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getNfmDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getNfmDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getNfmDemodSettings()->getReverseApiAddress()) {
        *response.getNfmDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getNfmDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getNfmDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getNfmDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getNfmDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void NFMDemodTest::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);

    response.getNfmDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    int nbCtcssToneFrequencies;
    const Real *ctcssToneFrequencies = m_basebandSink->getCtcssToneSet(nbCtcssToneFrequencies);
    response.getNfmDemodReport()->setCtcssTone(
        m_settings.m_ctcssOn ?
            m_settings.m_ctcssIndex < 0 ?
                0
                : m_settings.m_ctcssIndex < nbCtcssToneFrequencies ?
                    ctcssToneFrequencies[m_settings.m_ctcssIndex-1]
                    : 0
            : 0
    );
    response.getNfmDemodReport()->setSquelch(m_basebandSink->getSquelchOpen() ? 1 : 0);
    response.getNfmDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    response.getNfmDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
}

void NFMDemodTest::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const NFMDemodTestSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(0); // single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("NFMDemodTest"));
    swgChannelSettings->setNfmDemodSettings(new SWGSDRangel::SWGNFMDemodSettings());
    SWGSDRangel::SWGNFMDemodSettings *swgNFMDemodSettings = swgChannelSettings->getNfmDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("afBandwidth") || force) {
        swgNFMDemodSettings->setAfBandwidth(settings.m_afBandwidth);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgNFMDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("highPass") || force) {
        swgNFMDemodSettings->setAudioMute(settings.m_highPass ? 1 : 0);
    }
    if (channelSettingsKeys.contains("ctcssIndex") || force) {
        swgNFMDemodSettings->setCtcssIndex(settings.m_ctcssIndex);
    }
    if (channelSettingsKeys.contains("ctcssOn") || force) {
        swgNFMDemodSettings->setCtcssOn(settings.m_ctcssOn ? 1 : 0);
    }
    if (channelSettingsKeys.contains("deltaSquelch") || force) {
        swgNFMDemodSettings->setDeltaSquelch(settings.m_deltaSquelch ? 1 : 0);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgNFMDemodSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgNFMDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgNFMDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgNFMDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("squelch") || force) {
        swgNFMDemodSettings->setSquelch(settings.m_squelch);
    }
    if (channelSettingsKeys.contains("squelchGate") || force) {
        swgNFMDemodSettings->setSquelchGate(settings.m_squelchGate);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgNFMDemodSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgNFMDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swgNFMDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgNFMDemodSettings->setStreamIndex(settings.m_streamIndex);
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

void NFMDemodTest::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "NFMDemodTest::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("NFMDemodTest::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}
