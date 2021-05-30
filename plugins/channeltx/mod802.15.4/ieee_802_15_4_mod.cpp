///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016 Edouard Griffiths, F4EXB                                   //
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include <QTime>
#include <QDebug>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUdpSocket>
#include <QNetworkDatagram>
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGIEEE_802_15_4_ModReport.h"
#include "SWGIEEE_802_15_4_ModActions.h"
#include "SWGIEEE_802_15_4_ModActions_tx.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "feature/feature.h"
#include "util/db.h"
#include "util/crc.h"
#include "maincore.h"

#include "ieee_802_15_4_modbaseband.h"
#include "ieee_802_15_4_mod.h"

MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Mod::MsgConfigureIEEE_802_15_4_Mod, Message)
MESSAGE_CLASS_DEFINITION(IEEE_802_15_4_Mod::MsgTXIEEE_802_15_4_Mod, Message)

const char* const IEEE_802_15_4_Mod::m_channelIdURI = "sdrangel.channeltx.mod802.15.4";
const char* const IEEE_802_15_4_Mod::m_channelId = "IEEE_802_15_4_Mod";

IEEE_802_15_4_Mod::IEEE_802_15_4_Mod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_settingsMutex(QMutex::Recursive),
    m_udpSocket(nullptr)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new IEEE_802_15_4_ModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

IEEE_802_15_4_Mod::~IEEE_802_15_4_Mod()
{
    closeUDP();
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void IEEE_802_15_4_Mod::start()
{
    qDebug("IEEE_802_15_4_Mod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void IEEE_802_15_4_Mod::stop()
{
    qDebug("IEEE_802_15_4_Mod::stop");
    m_thread->exit();
    m_thread->wait();
}

void IEEE_802_15_4_Mod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool IEEE_802_15_4_Mod::handleMessage(const Message& cmd)
{
    if (MsgConfigureIEEE_802_15_4_Mod::match(cmd))
    {
        MsgConfigureIEEE_802_15_4_Mod& cfg = (MsgConfigureIEEE_802_15_4_Mod&) cmd;
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: MsgConfigureIEEE_802_15_4_Mod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTXIEEE_802_15_4_Mod::match(cmd))
    {
        // Forward a copy to baseband
        MsgTXIEEE_802_15_4_Mod* rep = new MsgTXIEEE_802_15_4_Mod((MsgTXIEEE_802_15_4_Mod&)cmd);
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: MsgTXIEEE_802_15_4_Mod";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "IEEE_802_15_4_Mod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        // Forward to GUI
        if (getMessageQueueToGUI())
        {
            DSPSignalNotification *notifToGUI = new DSPSignalNotification(notif);
            getMessageQueueToGUI()->push(notifToGUI);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void IEEE_802_15_4_Mod::applySettings(const IEEE_802_15_4_ModSettings& settings, bool force)
{
    qDebug() << "IEEE_802_15_4_Mod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatDelay: " << settings.m_repeatDelay
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_reverseAPIAddress: " << settings.m_reverseAPIAddress
            << " m_reverseAPIAddress: " << settings.m_reverseAPIPort
            << " m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
            << " m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }

    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }

    if ((settings.m_gain != m_settings.m_gain) || force) {
        reverseAPIKeys.append("gain");
    }

    if ((settings.m_channelMute != m_settings.m_channelMute) || force) {
        reverseAPIKeys.append("channelMute");
    }

    if ((settings.m_repeat != m_settings.m_repeat) || force) {
        reverseAPIKeys.append("repeat");
    }

    if ((settings.m_repeatDelay != m_settings.m_repeatDelay) || force) {
        reverseAPIKeys.append("repeatDelay");
    }

    if ((settings.m_repeatCount != m_settings.m_repeatCount) || force) {
        reverseAPIKeys.append("repeatCount");
    }

    if ((settings.m_udpEnabled != m_settings.m_udpEnabled) || force) {
        reverseAPIKeys.append("udpEnabled");
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force) {
        reverseAPIKeys.append("udpAddress");
    }

    if ((settings.m_udpPort != m_settings.m_udpPort) || force) {
        reverseAPIKeys.append("udpPort");
    }

    if (   (settings.m_udpEnabled != m_settings.m_udpEnabled)
        || (settings.m_udpAddress != m_settings.m_udpAddress)
        || (settings.m_udpPort != m_settings.m_udpPort)
        || force)
    {
        if (settings.m_udpEnabled)
            openUDP(settings);
        else
            closeUDP();
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
        }

        reverseAPIKeys.append("streamIndex");
    }

    IEEE_802_15_4_ModBaseband::MsgConfigureIEEE_802_15_4_ModBaseband *msg = IEEE_802_15_4_ModBaseband::MsgConfigureIEEE_802_15_4_ModBaseband::create(settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settings.m_useReverseAPI)
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

QByteArray IEEE_802_15_4_Mod::serialize() const
{
    return m_settings.serialize();
}

bool IEEE_802_15_4_Mod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigureIEEE_802_15_4_Mod *msg = MsgConfigureIEEE_802_15_4_Mod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int IEEE_802_15_4_Mod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
    response.getIeee802154ModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int IEEE_802_15_4_Mod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    IEEE_802_15_4_ModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureIEEE_802_15_4_Mod *msg = MsgConfigureIEEE_802_15_4_Mod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureIEEE_802_15_4_Mod *msgToGUI = MsgConfigureIEEE_802_15_4_Mod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void IEEE_802_15_4_Mod::webapiUpdateChannelSettings(
        IEEE_802_15_4_ModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getIeee802154ModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("phy")) {
        settings.setPHY(*response.getIeee802154ModSettings()->getPhy());
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getIeee802154ModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getIeee802154ModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getIeee802154ModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getIeee802154ModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatDelay")) {
        settings.m_repeatDelay = response.getIeee802154ModSettings()->getRepeatDelay();
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getIeee802154ModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("udpEnabled")) {
        settings.m_udpEnabled = response.getPacketDemodSettings()->getUdpEnabled();
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getPacketDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort")) {
        settings.m_udpPort = response.getPacketDemodSettings()->getUdpPort();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getIeee802154ModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getIeee802154ModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getIeee802154ModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getIeee802154ModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getIeee802154ModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getIeee802154ModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getIeee802154ModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getIeee802154ModSettings()->getReverseApiChannelIndex();
    }
}

int IEEE_802_15_4_Mod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setIeee802154ModReport(new SWGSDRangel::SWGIEEE_802_15_4_ModReport());
    response.getIeee802154ModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int IEEE_802_15_4_Mod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGIEEE_802_15_4_ModActions *swgIEEE_802_15_4_ModActions = query.getIeee802154ModActions();

    if (swgIEEE_802_15_4_ModActions)
    {
        if (channelActionsKeys.contains("tx"))
        {
            SWGSDRangel::SWGIEEE_802_15_4_ModActions_tx* tx = swgIEEE_802_15_4_ModActions->getTx();
            QString *dataP = tx->getData();
            if (dataP != nullptr)
            {
                QString data(*dataP);

                IEEE_802_15_4_Mod::MsgTXIEEE_802_15_4_Mod *msg = IEEE_802_15_4_Mod::MsgTXIEEE_802_15_4_Mod::create(data);
                m_basebandSource->getInputMessageQueue()->push(msg);
                return 202;
            }
            else
            {
                errorMessage = "Missing data to transmit";
                return 400;
            }
        }
        else
        {
            errorMessage = "Unknown action";
            return 400;
        }
    }
    else
    {
        errorMessage = "Missing IEEE_802_15_4_ModActions in query";
        return 400;
    }
}

void IEEE_802_15_4_Mod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const IEEE_802_15_4_ModSettings& settings)
{
    response.getIeee802154ModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getIeee802154ModSettings()->setPhy(new QString(settings.getPHY()));
    response.getIeee802154ModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getIeee802154ModSettings()->setGain(settings.m_gain);
    response.getIeee802154ModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getIeee802154ModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getIeee802154ModSettings()->setRepeatDelay(settings.m_repeatDelay);
    response.getIeee802154ModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getIeee802154ModSettings()->setUdpEnabled(settings.m_udpEnabled);
    response.getIeee802154ModSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    response.getIeee802154ModSettings()->setUdpPort(settings.m_udpPort);
    response.getIeee802154ModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getIeee802154ModSettings()->getTitle()) {
        *response.getIeee802154ModSettings()->getTitle() = settings.m_title;
    } else {
        response.getIeee802154ModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getIeee802154ModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getIeee802154ModSettings()->getReverseApiAddress()) {
        *response.getIeee802154ModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getIeee802154ModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getIeee802154ModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getIeee802154ModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getIeee802154ModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void IEEE_802_15_4_Mod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getIeee802154ModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getIeee802154ModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void IEEE_802_15_4_Mod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const IEEE_802_15_4_ModSettings& settings, bool force)
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

void IEEE_802_15_4_Mod::sendChannelSettings(
    QList<MessageQueue*> *messageQueues,
    QList<QString>& channelSettingsKeys,
    const IEEE_802_15_4_ModSettings& settings,
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

void IEEE_802_15_4_Mod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const IEEE_802_15_4_ModSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setIeee802154ModSettings(new SWGSDRangel::SWGIEEE_802_15_4_ModSettings());
    SWGSDRangel::SWGIEEE_802_15_4_ModSettings *swgIEEE_802_15_4_ModSettings = swgChannelSettings->getIeee802154ModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgIEEE_802_15_4_ModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgIEEE_802_15_4_ModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgIEEE_802_15_4_ModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgIEEE_802_15_4_ModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatDelay") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeatDelay(settings.m_repeatDelay);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgIEEE_802_15_4_ModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("udpEnabled") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpEnabled(settings.m_udpEnabled);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgIEEE_802_15_4_ModSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgIEEE_802_15_4_ModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgIEEE_802_15_4_ModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgIEEE_802_15_4_ModSettings->setStreamIndex(settings.m_streamIndex);
    }
}

void IEEE_802_15_4_Mod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "IEEE_802_15_4_Mod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("IEEE_802_15_4_Mod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double IEEE_802_15_4_Mod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void IEEE_802_15_4_Mod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t IEEE_802_15_4_Mod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

ScopeVis *IEEE_802_15_4_Mod::getScopeSink()
{
    return m_basebandSource->getScopeSink();
}

void IEEE_802_15_4_Mod::openUDP(const IEEE_802_15_4_ModSettings& settings)
{
    closeUDP();
    m_udpSocket = new QUdpSocket();
    if (!m_udpSocket->bind(QHostAddress(settings.m_udpAddress), settings.m_udpPort))
        qCritical() << "IEEE_802_15_4_Mod::openUDP: Failed to bind to port " << settings.m_udpAddress << ":" << settings.m_udpPort << ". Error: " << m_udpSocket->error();
    else
        qDebug() << "IEEE_802_15_4_Mod::openUDP: Listening for packets on " << settings.m_udpAddress << ":" << settings.m_udpPort;
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_Mod::udpRx);
}

void IEEE_802_15_4_Mod::closeUDP()
{
    if (m_udpSocket != nullptr)
    {
        disconnect(m_udpSocket, &QUdpSocket::readyRead, this, &IEEE_802_15_4_Mod::udpRx);
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void IEEE_802_15_4_Mod::udpRx()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        // Convert from binary to hex string
        QString string = datagram.data().toHex(' ');
        IEEE_802_15_4_Mod::MsgTXIEEE_802_15_4_Mod *msg = IEEE_802_15_4_Mod::MsgTXIEEE_802_15_4_Mod::create(string);
        m_basebandSource->getInputMessageQueue()->push(msg);
    }
}
