///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014-2015 John Greb <hexameron@spam.no>                         //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2022 Jiří Pinkava <jiri.pinkava@rossum.ai>                      //
// Copyright (C) 2023 Jon Beniston, M7RCE <jon@beniston.com>                     //
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


#include <string.h>
#include <stdio.h>
#include <complex.h>
#include <cmath>

#include <QTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QDateTime>
#include <QJsonDocument>
#include <QUrl>
#include <QThread>
#include <QMutexLocker>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGDSDDemodSettings.h"
#include "SWGChannelReport.h"
#include "SWGDSDDemodReport.h"
#include "SWGChannelActions.h"
#include "SWGDSDDemodActions.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

#include "dsddemod.h"

MESSAGE_CLASS_DEFINITION(DSDDemod::MsgConfigureDSDDemod, Message)
MESSAGE_CLASS_DEFINITION(DSDDemod::MsgQueryAvailableAMBEFeatures, Message)
MESSAGE_CLASS_DEFINITION(DSDDemod::MsgReportAvailableAMBEFeatures, Message)

static const char* const DSEModURI = "sdrangel.channeltx.moddse"; //!< They own the Angel61 IP tunnel

const char* const DSDDemod::m_channelIdURI = "sdrangel.channel.dsddemod";
const char* const DSDDemod::m_channelId = "DSDDemod";
const int DSDDemod::m_udpBlockSize = 512;

DSDDemod::DSDDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSink(nullptr),
        m_running(false),
        m_connectedAMBEFeature(nullptr),
        m_basebandSampleRate(0),
        m_centerFrequency(0),
        m_scopeXYSink(nullptr)
{
    qDebug("DSDDemod::DSDDemod");
	setObjectName(m_channelId);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    m_contactNetwork = new QNetworkAccessManager();
    m_contactTimer.setInterval(1000);
    QObject::connect(&m_contactTimer, &QTimer::timeout, this, [this] { pumpContacts(); });
    m_contactTimer.start();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DSDDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &DSDDemod::handleIndexInDeviceSetChanged
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &DSDDemod::handleFeatureAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &DSDDemod::handleFeatureRemoved
    );

    scanAvailableAMBEFeatures();
    start();
}

DSDDemod::~DSDDemod()
{
    m_contactTimer.stop();
    delete m_contactNetwork;
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DSDDemod::networkManagerFinished
    );
    delete m_networkManager;
	m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    stop();
}

void DSDDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t DSDDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void DSDDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;

    if (m_running) {
        m_basebandSink->feed(begin, end);
    }
}

void DSDDemod::start()
{
    QMutexLocker m_lock(&m_mutex);

    if (m_running) {
        return;
    }

    qDebug() << "DSDDemod::start";
    m_thread = new QThread();
    m_thread->setObjectName("DSDDemod");
    m_basebandSink = new DSDDemodBaseband();
    m_basebandSink->setFifoLabel(QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(getIndexInDeviceSet())
    );
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(m_thread);

    QObject::connect(
        m_thread,
        &QThread::finished,
        m_basebandSink,
        &QObject::deleteLater
    );
    QObject::connect(
        m_thread,
        &QThread::finished,
        m_thread,
        &QThread::deleteLater
    );

    m_basebandSink->setScopeXYSink(m_scopeXYSink);
    m_basebandSink->setAMBEPort(m_connectedAMBEPort); // a fresh sink, the connection as before the stop
    // and the station's position (the preferences'), whatever the GUI last sent: a GUI made while the
    // channel was stopped, or a restart, left the new sink without one
    m_basebandSink->configureMyPosition(MainCore::instance()->getSettings().getLatitude(),
        MainCore::instance()->getSettings().getLongitude());

    m_thread->start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    DSDDemodBaseband::MsgConfigureDSDDemodBaseband *msg = DSDDemodBaseband::MsgConfigureDSDDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);

    m_running = true;
}

void DSDDemod::stop()
{
    QMutexLocker m_lock(&m_mutex);

    if (!m_running) {
        return;
    }

    qDebug() << "DSDDemod::stop";
    m_running = false;
	m_thread->exit();
	m_thread->wait();
    m_basebandSink = nullptr; // Freed by deleteLater when the thread finished
}

void DSDDemod::setScopeXYSink(BasebandSampleSink* sampleSink)
{
    m_scopeXYSink = sampleSink;
    if (m_running) {
        m_basebandSink->setScopeXYSink(sampleSink);
    }
}

bool DSDDemod::handleMessage(const Message& cmd)
{
	qDebug() << "DSDDemod::handleMessage";

    if (MsgConfigureDSDDemod::match(cmd))
    {
        MsgConfigureDSDDemod& cfg = (MsgConfigureDSDDemod&) cmd;
        qDebug("DSDDemod::handleMessage: MsgConfigureDSDDemod: m_rfBandwidth");

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        qDebug() << "DSDDemod::handleMessage: DSPSignalNotification";
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        if (m_running) {
            m_basebandSink->getInputMessageQueue()->push(new DSPSignalNotification(notif));
        }
        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

	    return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "DSDDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else if (MsgQueryAvailableAMBEFeatures::match(cmd))
    {
        notifyUpdateAMBEFeatures();
        return true;
    }
    else if (MainCore::MsgPacket::match(cmd))
    {
        // "ANGEL61TUNNEL listening datagrams bytes refused status", from a DSE Modulator
        const MainCore::MsgPacket& packet = (const MainCore::MsgPacket&) cmd;
        const QString text = QString::fromUtf8(packet.getPacket());

        if (text.section(' ', 0, 0) != QStringLiteral("ANGEL61TUNNEL")) {
            return false;
        }

        Angel61Tunnel tunnel;
        tunnel.m_listening = text.section(' ', 1, 1) != QStringLiteral("0");
        tunnel.m_datagrams = text.section(' ', 2, 2).toULongLong();
        tunnel.m_bytes = text.section(' ', 3, 3).toULongLong();
        tunnel.m_refused = text.section(' ', 4, 4).toULongLong();
        tunnel.m_status = text.section(' ', 5);
        m_angel61Tunnels.insert(packet.getPipeSource(), tunnel);

        return true;
    }
	else
	{
		return false;
	}
}

QList<DSDDemod::Angel61Tunnel> DSDDemod::getAngel61Tunnels()
{
    // The modulators there are now, rather than those that spoke: one that was deleted says nothing
    // more, and a preset's load announces no removal until after the deletions
    QList<Angel61Tunnel> tunnels;
    QHash<const QObject*, Angel61Tunnel> present;
    MainCore *mainCore = MainCore::instance();

    for (int d = 0; d < mainCore->getDeviceSets().size(); d++)
    {
        DeviceSet *deviceSet = mainCore->getDeviceSets()[d];

        for (int c = 0; c < deviceSet->getNumberOfChannels(); c++)
        {
            ChannelAPI *channel = deviceSet->getChannelAt(c);

            if (channel && (channel->getURI() == DSEModURI))
            {
                const Angel61Tunnel tunnel = m_angel61Tunnels.value(channel);
                present.insert(channel, tunnel);
                tunnels.append(tunnel);
            }
        }
    }

    m_angel61Tunnels = present;
    return tunnels;
}

void DSDDemod::setCenterFrequency(qint64 frequency)
{
    DSDDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList("inputFrequencyOffset"), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDSDDemod *msgToGUI = MsgConfigureDSDDemod::create(QStringList("inputFrequencyOffset"), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void DSDDemod::applySettings(const QStringList& settingsKeys, const DSDDemodSettings& settings, bool force)
{
    qDebug() << "DSDDemod::applySettings: " << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("connectAMBE") && (m_settings.m_connectAMBE != settings.m_connectAMBE))
    ||  (settingsKeys.contains("ambeFeatureIndex") && (m_settings.m_ambeFeatureIndex != settings.m_ambeFeatureIndex)) || force)
    {
        // The feature the settings select, or none: an index that matches nothing disconnects
        // rather than leaving the previous feature in place
        Feature *selected = nullptr;

        if (settings.m_connectAMBE)
        {
            for (const auto& feature : m_availableAMBEFeatures)
            {
                if (feature.m_featureIndex == settings.m_ambeFeatureIndex) {
                    selected = feature.m_feature;
                }
            }
        }

        connectAMBEFeature(selected);
    }

    if (settingsKeys.contains("streamIndex") && (m_settings.m_streamIndex != settings.m_streamIndex))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    if (m_running)
    {
        DSDDemodBaseband::MsgConfigureDSDDemodBaseband *msg = DSDDemodBaseband::MsgConfigureDSDDemodBaseband::create(settingsKeys, settings, force);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((settingsKeys.contains("useReverseAPI") && (m_settings.m_useReverseAPI != settings.m_useReverseAPI)) && settings.m_useReverseAPI) ||
                (settingsKeys.contains("reverseAPIAddress") && (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress)) ||
                (settingsKeys.contains("reverseAPIPort") && (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort)) ||
                (settingsKeys.contains("reverseAPIDeviceIndex") && (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex)) ||
                (settingsKeys.contains("reverseAPIChannelIndex") && (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, settingsKeys, settings, force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

void DSDDemod::pumpContacts()
{
    if (m_running) {
        const QVector<QJsonObject> received = m_basebandSink->takeContactReports();
        if (m_settings.m_contactReportEnabled) {
            for (QJsonObject contact : received) {
                if (m_pendingContacts.size() >= 64) m_pendingContacts.dequeue();
                m_pendingContacts.enqueue(contact);
            }
        }
    }
    if (!m_settings.m_contactReportEnabled) {
        m_pendingContacts.clear();
        m_contactStatus = "Off";
        return;
    }
    if (m_pendingContacts.isEmpty() && !m_contactInFlight) {
        if (m_contactStatus == "Off") m_contactStatus = "Ready";
        return;
    }
    if (m_contactInFlight || m_pendingContacts.isEmpty()
        || QDateTime::currentMSecsSinceEpoch() < m_contactNextAttemptMs) return;

    if (m_settings.m_contactReportCallsign.trimmed().isEmpty()) {
        m_contactStatus = "Configure RX callsign";
        return;
    }
    // the public server; the station's position goes with every report (reporting implies it)
    const QUrl url(QStringLiteral("http://angel61.sdrangel.org/api/v1/contacts"));
    QJsonObject contact = m_pendingContacts.head();
    contact["rxCallsign"] = m_settings.m_contactReportCallsign.trimmed().toUpper();
    const double latitude = MainCore::instance()->getSettings().getLatitude();
    const double longitude = MainCore::instance()->getSettings().getLongitude();
    if (std::isfinite(latitude) && std::isfinite(longitude)
        && latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0) {
        contact["rxLat"] = latitude;
        contact["rxLon"] = longitude;
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QNetworkReply *reply = m_contactNetwork->post(request, QJsonDocument(contact).toJson(QJsonDocument::Compact));
    m_contactInFlight = true;
    m_contactStatus = QString("Sending (%1 queued)").arg(m_pendingContacts.size());
    QTimer::singleShot(5000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    // The report sent is removed by its id when the server has it: the queue may have moved on
    // meanwhile (the oldest dropped at 64, or all of it when reporting was turned off and on)
    const QString sentId = contact["id"].toString();
    auto removeSent = [this, sentId] {
        for (int i = 0; i < m_pendingContacts.size(); i++)
        {
            if (m_pendingContacts[i]["id"].toString() == sentId)
            {
                m_pendingContacts.removeAt(i);
                return;
            }
        }
    };
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, removeSent] {
        m_contactInFlight = false;
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool success = status >= 200 && status < 300;
        if (success) {
            removeSent();
            m_contactFailures = 0;
            m_contactNextAttemptMs = 0;
            m_contactStatus = QString("Last report sent; %1 queued").arg(m_pendingContacts.size());
        } else if (status == 400 || status == 413 || status == 415) {
            removeSent();
            m_contactFailures = 0;
            m_contactNextAttemptMs = 0;
            m_contactStatus = QString("Report rejected (%1); %2 queued").arg(status).arg(m_pendingContacts.size());
        } else {
            ++m_contactFailures;
            m_contactNextAttemptMs = QDateTime::currentMSecsSinceEpoch()
                + qMin(60000, 1000 * (1 << qMin(m_contactFailures, 6)));
            m_contactStatus = ((status >= 300) && (status < 400)) ? QString("Server redirected (%1), not followed; retrying").arg(status)
                : status ? QString("Server returned %1; retrying").arg(status)
                : QString("Connection failed; retrying");
        }
        reply->deleteLater();
    });
}

QByteArray DSDDemod::serialize() const
{
    return m_settings.serialize();
}

bool DSDDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DSDDemod::scanAvailableAMBEFeatures()
{
    MainCore *mainCore = MainCore::instance();
    int nbFeatures = mainCore->getFeatureeSets()[0]->getNumberOfFeatures();
    m_availableAMBEFeatures.clear();

    for (int i = 0; i < nbFeatures; i++)
    {
        Feature *feature = mainCore->getFeatureeSets()[0]->getFeatureAt(i);

        if (feature->getURI() == "sdrangel.feature.ambe") {
            m_availableAMBEFeatures[feature] = DSDDemodSettings::AvailableAMBEFeature{i, feature};
        }
    }
}

void DSDDemod::notifyUpdateAMBEFeatures()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableAMBEFeatures *msg = MsgReportAvailableAMBEFeatures::create();
        msg->getFeatures() = m_availableAMBEFeatures.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void DSDDemod::sendSampleRateToDemodAnalyzer()
{
    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "reportdemod", pipes);

    if (pipes.size() > 0)
    {
        for (const auto& pipe : pipes)
        {
            MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
            MainCore::MsgChannelDemodReport *msg = MainCore::MsgChannelDemodReport::create(
                this,
                getAudioSampleRate()
            );
            messageQueue->push(msg);
        }
    }
}

int DSDDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
    response.getDsdDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DSDDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DSDDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DSDDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDSDDemod *msg = MsgConfigureDSDDemod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DSDDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDSDDemod *msgToGUI = MsgConfigureDSDDemod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DSDDemod::webapiUpdateChannelSettings(
        DSDDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getDsdDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        // (to the GUI's ranges: a value beyond them the GUI clamped, kept and saved over the one set)
        settings.m_rfBandwidth = std::max(1000.0f, std::min(50000.0f, response.getDsdDemodSettings()->getRfBandwidth()));
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = std::max(0.0f, std::min(10.0f, response.getDsdDemodSettings()->getVolume()));
    }
    if (channelSettingsKeys.contains("protocol"))
    {
        // 0 to 13; another value is Auto
        settings.m_protocol = DSDDemodSettings::protocolOf(response.getDsdDemodSettings()->getProtocol());
    }
    if (channelSettingsKeys.contains("p25p2Wacn")) {
        settings.m_p25p2Wacn = response.getDsdDemodSettings()->getP25p2Wacn() & 0xFFFFF;
    }
    if (channelSettingsKeys.contains("p25p2Sysid")) {
        settings.m_p25p2Sysid = response.getDsdDemodSettings()->getP25p2Sysid() & 0xFFF;
    }
    if (channelSettingsKeys.contains("p25p2Nac")) {
        settings.m_p25p2Nac = response.getDsdDemodSettings()->getP25p2Nac() & 0xFFF;
    }
    if (channelSettingsKeys.contains("coherent")) {
        settings.m_coherent = response.getDsdDemodSettings()->getCoherent() != 0;
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDsdDemodSettings()->getAudioMute() != 0;
    }
    if (channelSettingsKeys.contains("slot1On")) {
        settings.m_slot1On = response.getDsdDemodSettings()->getSlot1On() != 0;
    }
    if (channelSettingsKeys.contains("slot2On")) {
        settings.m_slot2On = response.getDsdDemodSettings()->getSlot2On() != 0;
    }
    if (channelSettingsKeys.contains("tdmaStereo")) {
        settings.m_tdmaStereo = response.getDsdDemodSettings()->getTdmaStereo() != 0;
    }
    if (channelSettingsKeys.contains("angel61FileDirectory")) {
        settings.m_angel61FileDirectory = *response.getDsdDemodSettings()->getAngel61FileDirectory();
    }
    if (channelSettingsKeys.contains("codecModelDirectory")) {
        settings.m_codecModelDirectory = *response.getDsdDemodSettings()->getCodecModelDirectory();
    }
    if (channelSettingsKeys.contains("angel61FileConfirm")) {
        settings.m_angel61FileConfirm = response.getDsdDemodSettings()->getAngel61FileConfirm() != 0;
    }
    if (channelSettingsKeys.contains("angel61FileMaxKB")) {
        settings.m_angel61FileMaxKB = std::max(0, std::min(65536, response.getDsdDemodSettings()->getAngel61FileMaxKb()));
    }
    if (channelSettingsKeys.contains("angel61Groups")) {
        settings.m_angel61Groups = *response.getDsdDemodSettings()->getAngel61Groups();
    }
    if (channelSettingsKeys.contains("contactReportEnabled"))
        settings.m_contactReportEnabled = response.getDsdDemodSettings()->getContactReportEnabled() != 0;
    if (channelSettingsKeys.contains("contactReportCallsign"))
        settings.m_contactReportCallsign = response.getDsdDemodSettings()->getContactReportCallsign();
    if (channelSettingsKeys.contains("dmrColorCode"))
    {
        int colorCode = response.getDsdDemodSettings()->getDmrColorCode();
        settings.m_dmrColorCode = ((colorCode >= 0) && (colorCode <= 63)) ? colorCode : -1;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDsdDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDsdDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDsdDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("highPassFilter")) {
        settings.m_highPassFilter = response.getDsdDemodSettings()->getHighPassFilter() != 0;
    }
    if (channelSettingsKeys.contains("traceLengthMutliplier")) {
        settings.m_traceLengthMutliplier = response.getDsdDemodSettings()->getTraceLengthMutliplier();
    }
    if (channelSettingsKeys.contains("traceStroke")) {
        settings.m_traceStroke = response.getDsdDemodSettings()->getTraceStroke();
    }
    if (channelSettingsKeys.contains("traceDecay")) {
        settings.m_traceDecay = response.getDsdDemodSettings()->getTraceDecay();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDsdDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDsdDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDsdDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDsdDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDsdDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDsdDemodSettings()->getReverseApiChannelIndex();
    }
    if (channelSettingsKeys.contains("ambeFeatureIndex")) {
        settings.m_ambeFeatureIndex = response.getDsdDemodSettings()->getAmbeFeatureIndex();
    }
    if (channelSettingsKeys.contains("connectAMBE")) {
        settings.m_connectAMBE = response.getDsdDemodSettings()->getConnectAmbe() != 0;
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDsdDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDsdDemodSettings()->getRollupState());
    }
}

int DSDDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDsdDemodReport(new SWGSDRangel::SWGDSDDemodReport());
    response.getDsdDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DSDDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DSDDemodSettings& settings)
{
    response.getDsdDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getDsdDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    response.getDsdDemodSettings()->setVolume(settings.m_volume);
    response.getDsdDemodSettings()->setProtocol(settings.m_protocol);
    response.getDsdDemodSettings()->setP25p2Wacn(settings.m_p25p2Wacn);
    response.getDsdDemodSettings()->setP25p2Sysid(settings.m_p25p2Sysid);
    response.getDsdDemodSettings()->setP25p2Nac(settings.m_p25p2Nac);
    response.getDsdDemodSettings()->setCoherent(settings.m_coherent ? 1 : 0);
    response.getDsdDemodSettings()->setAudioMute(settings.m_audioMute ? 1 : 0);
    response.getDsdDemodSettings()->setSlot1On(settings.m_slot1On ? 1 : 0);
    response.getDsdDemodSettings()->setSlot2On(settings.m_slot2On ? 1 : 0);
    response.getDsdDemodSettings()->setTdmaStereo(settings.m_tdmaStereo ? 1 : 0);
    response.getDsdDemodSettings()->setDmrColorCode(settings.m_dmrColorCode);
    response.getDsdDemodSettings()->setAngel61FileDirectory(new QString(settings.m_angel61FileDirectory));
    response.getDsdDemodSettings()->setCodecModelDirectory(new QString(settings.m_codecModelDirectory));
    response.getDsdDemodSettings()->setAngel61FileMaxKb(settings.m_angel61FileMaxKB);
    response.getDsdDemodSettings()->setAngel61FileConfirm(settings.m_angel61FileConfirm ? 1 : 0);
    response.getDsdDemodSettings()->setAngel61Groups(new QString(settings.m_angel61Groups));
    response.getDsdDemodSettings()->setContactReportEnabled(settings.m_contactReportEnabled ? 1 : 0);
    response.getDsdDemodSettings()->setContactReportCallsign(settings.m_contactReportCallsign);
    response.getDsdDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getDsdDemodSettings()->getTitle()) {
        *response.getDsdDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getDsdDemodSettings()->setTitle(new QString(settings.m_title));
    }

    if (response.getDsdDemodSettings()->getAudioDeviceName()) {
        *response.getDsdDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getDsdDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getDsdDemodSettings()->setHighPassFilter(settings.m_highPassFilter ? 1 : 0);
    response.getDsdDemodSettings()->setTraceLengthMutliplier(settings.m_traceLengthMutliplier);
    response.getDsdDemodSettings()->setTraceStroke(settings.m_traceStroke);
    response.getDsdDemodSettings()->setTraceDecay(settings.m_traceDecay);
    response.getDsdDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getDsdDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDsdDemodSettings()->getReverseApiAddress()) {
        *response.getDsdDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDsdDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDsdDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDsdDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDsdDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);
    response.getDsdDemodSettings()->setAmbeFeatureIndex(settings.m_ambeFeatureIndex);
    response.getDsdDemodSettings()->setConnectAmbe(settings.m_connectAMBE ? 1 : 0);

    if (settings.m_channelMarker)
    {
        if (response.getDsdDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getDsdDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getDsdDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDsdDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDsdDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDsdDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

int DSDDemod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGDSDDemodActions *actions = query.getDsdDemodActions();

    if (!actions)
    {
        errorMessage = "Missing DSDDemodActions in query";
        return 400;
    }

    if (channelActionsKeys.contains("angel61File"))
    {
        // The operator's decision on an Angel61 file offer awaiting confirmation (3.4)
        const QString decision = actions->getAngel61File() ? actions->getAngel61File()->trimmed().toLower() : QString();

        if ((decision != "accept") && (decision != "reject"))
        {
            errorMessage = "angel61File must be \"accept\" or \"reject\"";
            return 400;
        }

        if (getReport().m_angel61FilePending.isEmpty())
        {
            errorMessage = "No Angel61 file offer is awaiting confirmation";
            return 409;
        }

        angel61FileDecision(decision == "accept");
        return 202;
    }

    errorMessage = "Unknown DSDDemodActions";
    return 400;
}

void DSDDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    if (!m_running) {
        return;
    }

    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    response.getDsdDemodReport()->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    response.getDsdDemodReport()->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    response.getDsdDemodReport()->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    DSDDemodSink::Report report = getReport();
    response.getDsdDemodReport()->setSyncLocked(report.m_locked ? 1 : 0);
    response.getDsdDemodReport()->setSlot1On(report.m_slot1Voice ? 1 : 0);
    response.getDsdDemodReport()->setSlot2On(report.m_slot2Voice ? 1 : 0);
    response.getDsdDemodReport()->setProtocol(new QString(report.m_protocol));
    response.getDsdDemodReport()->setSyncType(new QString(report.m_syncType));
    response.getDsdDemodReport()->setPolarity(report.m_inverted ? 1 : 0);
    response.getDsdDemodReport()->setDeviationHz(report.m_deviationHz);
    response.getDsdDemodReport()->setFrequencyErrorHz(report.m_frequencyErrorHz);
    response.getDsdDemodReport()->setClockErrorPpm(report.m_clockPpm);
    response.getDsdDemodReport()->setSnrDb(report.m_snrDB);
    response.getDsdDemodReport()->setBerEstimate(report.m_berEstimate);
    response.getDsdDemodReport()->setSyncCount(report.m_syncCount);
    response.getDsdDemodReport()->setVoiceFrames(report.m_voiceFrames);
    response.getDsdDemodReport()->setVoiceFramesErased(report.m_voiceFramesErased);
    response.getDsdDemodReport()->setCorrectedBits(report.m_correctedBits);
    response.getDsdDemodReport()->setProtectedBits(report.m_protectedBits);
    response.getDsdDemodReport()->setStatusText(new QString(report.m_statusText));
    response.getDsdDemodReport()->setDtmf(new QString(report.m_dtmf));
    response.getDsdDemodReport()->setAngel61FilePending(new QString(report.m_angel61FilePending));
    response.getDsdDemodReport()->setLastMessage(new QString(report.m_lastMessage.m_text));
    response.getDsdDemodReport()->setLastMessageProtocol(new QString(report.m_lastMessage.m_protocol));
    response.getDsdDemodReport()->setLastMessageSource(new QString(report.m_lastMessage.m_source));
    response.getDsdDemodReport()->setLastMessageDestination(new QString(report.m_lastMessage.m_destination));
    response.getDsdDemodReport()->setLastMessageTime(new QString(report.m_lastMessageTime));
    response.getDsdDemodReport()->setLastMessageSequence((qint32) report.m_lastMessage.m_sequence);

    QList<QString*> *heardCallsigns = response.getDsdDemodReport()->getAngel61HeardCallsigns();
    for (const QString& callsign : report.m_angel61HeardCallsigns) {
        heardCallsigns->append(new QString(callsign));
    }
}

void DSDDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DSDDemodSettings& settings, bool force)
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

void DSDDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QList<QString>& channelSettingsKeys,
    const DSDDemodSettings& settings,
    bool force)
{
    for (const auto& pipe : pipes)
    {
        MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);

        if (messageQueue)
        {
            SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
            webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);
            MainCore::MsgChannelSettings *msg = MainCore::MsgChannelSettings::create(
                this,
                channelSettingsKeys,
                swgChannelSettings,
                force
            );
            messageQueue->push(msg);
        }
    }
}

void DSDDemod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DSDDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = m_channelId;
    } else {
        swgChannelSettings->setChannelType(new QString(m_channelId));
    }
    swgChannelSettings->setDsdDemodSettings(new SWGSDRangel::SWGDSDDemodSettings());
    SWGSDRangel::SWGDSDDemodSettings *swgDSDDemodSettings = swgChannelSettings->getDsdDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgDSDDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgDSDDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgDSDDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("protocol") || force) {
        swgDSDDemodSettings->setProtocol(settings.m_protocol);
    }
    if (channelSettingsKeys.contains("p25p2Wacn") || force) {
        swgDSDDemodSettings->setP25p2Wacn(settings.m_p25p2Wacn);
    }
    if (channelSettingsKeys.contains("p25p2Sysid") || force) {
        swgDSDDemodSettings->setP25p2Sysid(settings.m_p25p2Sysid);
    }
    if (channelSettingsKeys.contains("p25p2Nac") || force) {
        swgDSDDemodSettings->setP25p2Nac(settings.m_p25p2Nac);
    }
    if (channelSettingsKeys.contains("coherent") || force) {
        swgDSDDemodSettings->setCoherent(settings.m_coherent ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgDSDDemodSettings->setAudioMute(settings.m_audioMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("slot1On") || force) {
        swgDSDDemodSettings->setSlot1On(settings.m_slot1On ? 1 : 0);
    }
    if (channelSettingsKeys.contains("slot2On") || force) {
        swgDSDDemodSettings->setSlot2On(settings.m_slot2On ? 1 : 0);
    }
    if (channelSettingsKeys.contains("tdmaStereo") || force) {
        swgDSDDemodSettings->setTdmaStereo(settings.m_tdmaStereo ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dmrColorCode") || force) {
        swgDSDDemodSettings->setDmrColorCode(settings.m_dmrColorCode);
    }
    if (channelSettingsKeys.contains("angel61FileDirectory") || force) {
        swgDSDDemodSettings->setAngel61FileDirectory(new QString(settings.m_angel61FileDirectory));
    }
    if (channelSettingsKeys.contains("codecModelDirectory") || force) {
        swgDSDDemodSettings->setCodecModelDirectory(new QString(settings.m_codecModelDirectory));
    }
    if (channelSettingsKeys.contains("angel61FileMaxKB") || force) {
        swgDSDDemodSettings->setAngel61FileMaxKb(settings.m_angel61FileMaxKB);
    }
    if (channelSettingsKeys.contains("angel61FileConfirm") || force) {
        swgDSDDemodSettings->setAngel61FileConfirm(settings.m_angel61FileConfirm ? 1 : 0);
    }
    if (channelSettingsKeys.contains("angel61Groups") || force) {
        swgDSDDemodSettings->setAngel61Groups(new QString(settings.m_angel61Groups));
    }
    if (channelSettingsKeys.contains("contactReportEnabled") || force)
        swgDSDDemodSettings->setContactReportEnabled(settings.m_contactReportEnabled ? 1 : 0);
    if (channelSettingsKeys.contains("contactReportCallsign") || force)
        swgDSDDemodSettings->setContactReportCallsign(settings.m_contactReportCallsign);
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDSDDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgDSDDemodSettings->getTitle()) {
            *swgDSDDemodSettings->getTitle() = settings.m_title;
        } else {
            swgDSDDemodSettings->setTitle(new QString(settings.m_title));
        }
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        if (swgDSDDemodSettings->getAudioDeviceName()) {
            *swgDSDDemodSettings->getAudioDeviceName() = settings.m_audioDeviceName;
        } else {
            swgDSDDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
        }
    }
    if (channelSettingsKeys.contains("highPassFilter") || force) {
        swgDSDDemodSettings->setHighPassFilter(settings.m_highPassFilter ? 1 : 0);
    }
    if (channelSettingsKeys.contains("traceLengthMutliplier") || force) {
        swgDSDDemodSettings->setTraceLengthMutliplier(settings.m_traceLengthMutliplier);
    }
    if (channelSettingsKeys.contains("traceStroke") || force) {
        swgDSDDemodSettings->setTraceStroke(settings.m_traceStroke);
    }
    if (channelSettingsKeys.contains("traceDecay") || force) {
        swgDSDDemodSettings->setTraceDecay(settings.m_traceDecay);
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDSDDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("ambeFeatureIndex") || force) {
        swgDSDDemodSettings->setAmbeFeatureIndex(settings.m_ambeFeatureIndex);
    }
    if (channelSettingsKeys.contains("connectAMBE") || force) {
        swgDSDDemodSettings->setConnectAmbe(settings.m_connectAMBE ? 1 : 0);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgDSDDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDSDDemodSettings->setRollupState(swgRollupState);
    }
}

void DSDDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DSDDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DSDDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DSDDemod::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}

void DSDDemod::connectAMBEFeature(Feature *feature)
{
    // The port, asked for here in the feature's own (the main) thread; a feature without one is
    // not connected
    std::shared_ptr<MbeEncoderPort> port;

    if (feature)
    {
        DSPGetMbeEncoderPort msg;

        if (feature->handleMessage(msg)) {
            port = msg.getPort();
        }
    }

    m_connectedAMBEFeature = port ? feature : nullptr;
    m_connectedAMBEPort = port;

    if (m_running) {
        m_basebandSink->setAMBEPort(port);
    }
}

void DSDDemod::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    if (featureSetIndex != 0) {
        return;
    }

    if ((feature->getURI() == "sdrangel.feature.ambe") && !m_availableAMBEFeatures.contains(feature))
    {
        m_availableAMBEFeatures[feature] = DSDDemodSettings::AvailableAMBEFeature{feature->getIndexInFeatureSet(), feature};

        if (m_settings.m_connectAMBE && (m_settings.m_ambeFeatureIndex == feature->getIndexInFeatureSet()))
        {
            connectAMBEFeature(feature);
        }

        notifyUpdateAMBEFeatures();
    }
}

void DSDDemod::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    if (featureSetIndex != 0) {
        return;
    }

    // The feature's port is closed already (its destructor, if it is gone); the connection ends here.
    // The settings keep asking for it: when a feature at that index comes back it is connected again
    // (handleFeatureAdded), and the GUI shows the box unchecked meanwhile
    if (feature == m_connectedAMBEFeature) {
        connectAMBEFeature(nullptr);
    }

    if (m_availableAMBEFeatures.contains(feature))
    {
        m_availableAMBEFeatures.remove(feature);
        notifyUpdateAMBEFeatures();
    }
}

int DSDDemod::getChannelSampleRate() const
{
    return m_basebandSink ? m_basebandSink->getChannelSampleRate() : 0;
}
