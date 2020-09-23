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
#include <QBuffer>
#include <QThread>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGPacketModReport.h"
#include "SWGPacketModActions.h"
#include "SWGPacketModActions_tx.h"

#include <stdio.h>
#include <complex.h>
#include <algorithm>

#include "dsp/dspengine.h"
#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/db.h"
#include "util/crc.h"

#include "packetmodbaseband.h"
#include "packetmod.h"

MESSAGE_CLASS_DEFINITION(PacketMod::MsgConfigurePacketMod, Message)
MESSAGE_CLASS_DEFINITION(PacketMod::MsgTXPacketMod, Message)

const QString PacketMod::m_channelIdURI = "sdrangel.channeltx.modpacket";
const QString PacketMod::m_channelId = "PacketMod";

PacketMod::PacketMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_settingsMutex(QMutex::Recursive),
    m_sampleRate(48000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSource = new PacketModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->moveToThread(m_thread);

    applySettings(m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
}

PacketMod::~PacketMod()
{
    disconnect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(networkManagerFinished(QNetworkReply*)));
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this);
    delete m_basebandSource;
    delete m_thread;
}

void PacketMod::start()
{
    qDebug("PacketMod::start");
    m_basebandSource->reset();
    m_thread->start();
}

void PacketMod::stop()
{
    qDebug("PacketMod::stop");
    m_thread->exit();
    m_thread->wait();
}

void PacketMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

bool PacketMod::handleMessage(const Message& cmd)
{
    if (MsgConfigurePacketMod::match(cmd))
    {
        MsgConfigurePacketMod& cfg = (MsgConfigurePacketMod&) cmd;
        qDebug() << "PacketMod::handleMessage: MsgConfigurePacketMod";

        applySettings(cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (MsgTXPacketMod::match(cmd))
    {
        // Forward a copy to baseband
        MsgTXPacketMod* rep = new MsgTXPacketMod((MsgTXPacketMod&)cmd);
        qDebug() << "PacketMod::handleMessage: MsgTXPacketMod";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "PacketMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        return true;
    }
    else
    {
        return false;
    }
}

void PacketMod::applySettings(const PacketModSettings& settings, bool force)
{
    qDebug() << "PacketMod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_rfBandwidth: " << settings.m_rfBandwidth
            << " m_fmDeviation: " << settings.m_fmDeviation
            << " m_gain: " << settings.m_gain
            << " m_channelMute: " << settings.m_channelMute
            << " m_repeat: " << settings.m_repeat
            << " m_repeatDelay: " << settings.m_repeatDelay
            << " m_repeatCount: " << settings.m_repeatCount
            << " m_ax25PreFlags: " << settings.m_ax25PreFlags
            << " m_ax25PostFlags: " << settings.m_ax25PostFlags
            << " m_preEmphasis: " << settings.m_preEmphasis
            << " m_preEmphasisTau: " << settings.m_preEmphasisTau
            << " m_preEmphasisHighFreq: " << settings.m_preEmphasisHighFreq
            << " m_bpf: " << settings.m_bpf
            << " m_bpfLowCutoff: " << settings.m_bpfLowCutoff
            << " m_bpfHighCutoff: " << settings.m_bpfHighCutoff
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

    if ((settings.m_fmDeviation != m_settings.m_fmDeviation) || force) {
        reverseAPIKeys.append("fmDeviation");
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

    if((settings.m_ax25PreFlags != m_settings.m_ax25PreFlags) || force) {
        reverseAPIKeys.append("ax25PreFlags");
    }

    if((settings.m_ax25PostFlags != m_settings.m_ax25PostFlags) || force) {
        reverseAPIKeys.append("ax25PostFlags");
    }

    if((settings.m_preEmphasis != m_settings.m_preEmphasis) || force) {
        reverseAPIKeys.append("preEmphasis");
    }

    if((settings.m_preEmphasisTau != m_settings.m_preEmphasisTau) || force) {
        reverseAPIKeys.append("preEmphasisTau");
    }

    if((settings.m_preEmphasisHighFreq != m_settings.m_preEmphasisHighFreq) || force) {
        reverseAPIKeys.append("preEmphasisHighFreq");
    }

    if((settings.m_bpf != m_settings.m_bpf) || force) {
        reverseAPIKeys.append("bpf");
    }

    if((settings.m_bpfLowCutoff != m_settings.m_bpfLowCutoff) || force) {
        reverseAPIKeys.append("bpfLowCutoff");
    }

    if((settings.m_bpfHighCutoff != m_settings.m_bpfHighCutoff) || force) {
        reverseAPIKeys.append("bpfHighCutoff");
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

    PacketModBaseband::MsgConfigurePacketModBaseband *msg = PacketModBaseband::MsgConfigurePacketModBaseband::create(settings, force);
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

    m_settings = settings;
}

QByteArray PacketMod::serialize() const
{
    return m_settings.serialize();
}

bool PacketMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    MsgConfigurePacketMod *msg = MsgConfigurePacketMod::create(m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int PacketMod::webapiSettingsGet(
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPacketModSettings(new SWGSDRangel::SWGPacketModSettings());
    response.getPacketModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int PacketMod::webapiSettingsPutPatch(
                bool force,
                const QStringList& channelSettingsKeys,
                SWGSDRangel::SWGChannelSettings& response,
                QString& errorMessage)
{
    (void) errorMessage;
    PacketModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigurePacketMod *msg = MsgConfigurePacketMod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigurePacketMod *msgToGUI = MsgConfigurePacketMod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void PacketMod::webapiUpdateChannelSettings(
        PacketModSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getPacketModSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("mode")) {
        settings.setMode(*response.getPacketModSettings()->getMode());
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getPacketModSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("fmDeviation")) {
        settings.m_fmDeviation = response.getPacketModSettings()->getFmDeviation();
    }
    if (channelSettingsKeys.contains("gain")) {
        settings.m_gain = response.getPacketModSettings()->getGain();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = response.getPacketModSettings()->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("repeat")) {
        settings.m_repeat = response.getPacketModSettings()->getRepeat() != 0;
    }
    if (channelSettingsKeys.contains("repeatDelay")) {
        settings.m_repeatDelay = response.getPacketModSettings()->getRepeatDelay();
    }
    if (channelSettingsKeys.contains("repeatCount")) {
        settings.m_repeatCount = response.getPacketModSettings()->getRepeatCount();
    }
    if (channelSettingsKeys.contains("ax25PreFlags")) {
        settings.m_ax25PreFlags = response.getPacketModSettings()->getAx25PreFlags();
    }
    if (channelSettingsKeys.contains("ax25PostFlags")) {
        settings.m_ax25PostFlags = response.getPacketModSettings()->getAx25PostFlags();
    }
    if (channelSettingsKeys.contains("preEmphasis")) {
        settings.m_preEmphasis = response.getPacketModSettings()->getPreEmphasis() != 0;
    }
    if (channelSettingsKeys.contains("preEmphasisTau")) {
        settings.m_preEmphasisTau = response.getPacketModSettings()->getPreEmphasisTau();
    }
    if (channelSettingsKeys.contains("preEmphasisHighFreq")) {
        settings.m_preEmphasisHighFreq = response.getPacketModSettings()->getPreEmphasisHighFreq();
    }
    if (channelSettingsKeys.contains("bpf")) {
        settings.m_bpf = response.getPacketModSettings()->getBpf() != 0;
    }
    if (channelSettingsKeys.contains("bpfLowCutoff")) {
        settings.m_bpfLowCutoff = response.getPacketModSettings()->getBpfLowCutoff();
    }
    if (channelSettingsKeys.contains("bpfHighCutoff")) {
        settings.m_bpfHighCutoff = response.getPacketModSettings()->getBpfHighCutoff();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getPacketModSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getPacketModSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getPacketModSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getPacketModSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getPacketModSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getPacketModSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getPacketModSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getPacketModSettings()->getReverseApiChannelIndex();
    }
}

int PacketMod::webapiReportGet(
                SWGSDRangel::SWGChannelReport& response,
                QString& errorMessage)
{
    (void) errorMessage;
    response.setPacketModReport(new SWGSDRangel::SWGPacketModReport());
    response.getPacketModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

int PacketMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGPacketModActions *swgPacketModActions = query.getPacketModActions();

    if (swgPacketModActions)
    {
        if (channelActionsKeys.contains("tx"))
        {
            SWGSDRangel::SWGPacketModActions_tx* tx = swgPacketModActions->getTx();
            QString callsign(*tx->getCallsign());
            QString to(*tx->getTo());
            QString via(*tx->getVia());
            QString data(*tx->getData());

            PacketMod::MsgTXPacketMod *msg = PacketMod::MsgTXPacketMod::create(callsign, to, via, data);
            m_basebandSource->getInputMessageQueue()->push(msg);
        }

        return 202;
    }
    else
    {
        errorMessage = "Missing PacketModActions in query";
        return 400;
    }
}

void PacketMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const PacketModSettings& settings)
{
    response.getPacketModSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getPacketModSettings()->setMode(new QString(settings.getMode()));
    response.getPacketModSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getPacketModSettings()->setFmDeviation(settings.m_fmDeviation);
    response.getPacketModSettings()->setGain(settings.m_gain);
    response.getPacketModSettings()->setChannelMute(settings.m_channelMute ? 1 : 0);
    response.getPacketModSettings()->setRepeat(settings.m_repeat ? 1 : 0);
    response.getPacketModSettings()->setRepeatDelay(settings.m_repeatDelay);
    response.getPacketModSettings()->setRepeatCount(settings.m_repeatCount);
    response.getPacketModSettings()->setAx25PreFlags(settings.m_ax25PreFlags);
    response.getPacketModSettings()->setAx25PostFlags(settings.m_ax25PostFlags);
    response.getPacketModSettings()->setPreEmphasis(settings.m_preEmphasis ? 1 : 0);
    response.getPacketModSettings()->setPreEmphasisTau(settings.m_preEmphasisTau);
    response.getPacketModSettings()->setPreEmphasisHighFreq(settings.m_preEmphasisHighFreq);
    response.getPacketModSettings()->setBpf(settings.m_bpf ? 1 : 0);
    response.getPacketModSettings()->setBpfLowCutoff(settings.m_bpfLowCutoff);
    response.getPacketModSettings()->setBpfHighCutoff(settings.m_bpfHighCutoff);
    response.getPacketModSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getPacketModSettings()->getTitle()) {
        *response.getPacketModSettings()->getTitle() = settings.m_title;
    } else {
        response.getPacketModSettings()->setTitle(new QString(settings.m_title));
    }

    response.getPacketModSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getPacketModSettings()->getReverseApiAddress()) {
        *response.getPacketModSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getPacketModSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getPacketModSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getPacketModSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getPacketModSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
}

void PacketMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    response.getPacketModReport()->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    response.getPacketModReport()->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
}

void PacketMod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const PacketModSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("PacketMod"));
    swgChannelSettings->setPacketModSettings(new SWGSDRangel::SWGPacketModSettings());
    SWGSDRangel::SWGPacketModSettings *swgPacketModSettings = swgChannelSettings->getPacketModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgPacketModSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgPacketModSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("fmDeviation") || force) {
        swgPacketModSettings->setFmDeviation(settings.m_fmDeviation);
    }
    if (channelSettingsKeys.contains("gain") || force) {
        swgPacketModSettings->setGain(settings.m_gain);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swgPacketModSettings->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeat") || force) {
        swgPacketModSettings->setRepeat(settings.m_repeat ? 1 : 0);
    }
    if (channelSettingsKeys.contains("repeatDelay") || force) {
        swgPacketModSettings->setRepeatDelay(settings.m_repeatDelay);
    }
    if (channelSettingsKeys.contains("repeatCount") || force) {
        swgPacketModSettings->setRepeatCount(settings.m_repeatCount);
    }
    if (channelSettingsKeys.contains("ax25PreFlags") || force) {
        swgPacketModSettings->setAx25PreFlags(settings.m_ax25PreFlags);
    }
    if (channelSettingsKeys.contains("ax25PostFlags") || force) {
        swgPacketModSettings->setAx25PostFlags(settings.m_ax25PostFlags);
    }
    if (channelSettingsKeys.contains("preEmphasis") || force) {
        swgPacketModSettings->setPreEmphasis(settings.m_preEmphasis);
    }
    if (channelSettingsKeys.contains("preEmphasisTau") || force) {
        swgPacketModSettings->setPreEmphasisTau(settings.m_preEmphasisTau);
    }
    if (channelSettingsKeys.contains("preEmphasisHighFreq") || force) {
        swgPacketModSettings->setPreEmphasisHighFreq(settings.m_preEmphasisHighFreq);
    }
    if (channelSettingsKeys.contains("bpf") || force) {
        swgPacketModSettings->setBpf(settings.m_preEmphasis);
    }
    if (channelSettingsKeys.contains("bpfLowCutoff") || force) {
        swgPacketModSettings->setBpfLowCutoff(settings.m_bpfLowCutoff);
    }
    if (channelSettingsKeys.contains("bpfHighCutoff") || force) {
        swgPacketModSettings->setBpfHighCutoff(settings.m_bpfHighCutoff);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgPacketModSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgPacketModSettings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgPacketModSettings->setStreamIndex(settings.m_streamIndex);
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

void PacketMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "PacketMod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("PacketMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double PacketMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

void PacketMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t PacketMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

