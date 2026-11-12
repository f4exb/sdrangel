///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2026 Jon Beniston, M7RCE <jon@beniston.com>                     //
// Some code by AI                                                               //
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

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QThread>
#include <QRegularExpression>
#include <QJsonArray>
#include <algorithm>

#include "dsp/dv/dvangel61file.h"

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGChannelActions.h"
#include "SWGDSEModActions.h"
#include "SWGDSEModTransmit.h"
#include "SWGDSEModMessage.h"
#include "SWGDSEModPCM.h"
#include "SWGDSEModFrame.h"
#include "SWGDSEModCapabilities.h"
#include "SWGDSEModMessageStatus.h"
#include "SWGDSEModTransmitSession.h"
#include "SWGDSEModTransmitStatus.h"
#include "SWGCWKeyerSettings.h"

#include "dsp/dspcommands.h"
#include "dsp/cwkeyer.h"
#include "device/deviceapi.h"
#include "device/deviceset.h"
#include "pipes/messagepipes.h"
#include "util/messagequeue.h"
#include "feature/feature.h"
#include "feature/featureset.h"
#include "util/db.h"
#include "maincore.h"
#include "dsp/dv/dvangel61.h"
#include "dsp/dv/dvangel61ip.h"
#include "dsp/dv/dvangel61hc.h"
#include "tun/iptunnel.h"

#include "dsemodbaseband.h"
#include "dsemodangel61.h"
#include "dsemodprocessor.h"
#include "dsemod.h"

MESSAGE_CLASS_DEFINITION(DSEMod::MsgConfigureDSEMod, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportLinkState, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgConfigureFileSourceSeek, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgConfigureFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportFileSourceStreamData, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportFileSourceStreamTiming, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgQueryAvailableAMBEFeatures, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportAvailableAMBEFeatures, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgQueryAvailableDemodulators, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportAvailableDemodulators, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportTunnelStatus, Message)
MESSAGE_CLASS_DEFINITION(DSEMod::MsgReportTunnelTraffic, Message)

static const char* const DSDDemodURI = "sdrangel.channel.dsddemod"; //!< The Angel61 rate request comes from these

const char* const DSEMod::m_channelIdURI = "sdrangel.channeltx.moddse";
const char* const DSEMod::m_channelId = "DSEMod";

DSEMod::DSEMod(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSource),
    m_deviceAPI(deviceAPI),
    m_spectrumVis(SDR_TX_SCALEF),
    m_recordLength(0),
    m_fileSampleRate(DSEModAudioFile::RawSampleRate),
    m_dtmfAutoTransmit(false),
    m_messageAutoTransmit(false),
    m_fileAutoTransmit(false),
    m_fileState(0),
    m_ipTunnel(nullptr),
    m_tunnelAutoTransmit(false),
    m_tunnelPeerValid(false),
    m_tunnelSent(0),
    m_tunnelReceived(0),
    m_tunnelRefused(0),
    m_tunnelRxRefused(0),
    m_tunnelStatus(QStringLiteral("Tunnel: off")),
    m_tunnelRefusedBy{},
    m_tunnelUnroutable(0),
    m_tunnelTxInTransmission(0),
    m_tunnelOwed(false),
    m_lastTransmissionEndMs(-1),
    m_tunnelTxBytes(0),
    m_tunnelRxBytes(0),
    m_angel61DemodSelected(nullptr),
    m_linkRequestedTier(0),
    m_linkSendTier(0),
    m_linkHeard(false)
{
    setObjectName(m_channelId);

    // The Angel61 source callsign starts as the station name of the preferences when it is a
    // valid callsign (ANGEL61.md 2.6); a preset or the settings overwrite it
    QString stationName = MainCore::instance()->getSettings().getStationName().trimmed().toUpper().section(' ', 0, 0);
    uint64_t callsign = DVAngel61::encodeCallsign(stationName.toStdString());

    if ((callsign != DVAngel61::CallsignNone) && !DVAngel61::isGroup(callsign)) {
        m_settings.m_angel61Source = stationName;
    }

    m_thread = new QThread(this);
    m_basebandSource = new DSEModBaseband();
    m_basebandSource->setSpectrumSampleSink(&m_spectrumVis);
    m_basebandSource->setInputFile(&m_audioFile, &m_fileMutex);
    m_basebandSource->setChannel(this);
    m_basebandSource->moveToThread(m_thread);

    scanAvailableAMBEFeatures();
    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSource(this);
    m_deviceAPI->addChannelSourceAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DSEMod::networkManagerFinished
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureAdded,
        this,
        &DSEMod::handleFeatureAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::featureRemoved,
        this,
        &DSEMod::handleFeatureRemoved
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelAdded,
        this,
        &DSEMod::handleChannelAdded
    );
    QObject::connect(
        MainCore::instance(),
        &MainCore::channelRemoved,
        this,
        &DSEMod::handleChannelRemoved
    );
    scanDSDDemodulators();

    m_fileAnswerTimer.setSingleShot(true);
    QObject::connect(&m_fileAnswerTimer, &QTimer::timeout, this, &DSEMod::fileAnswerTimeout);
    m_clock.start();
    m_tunnelAccessTimer.setSingleShot(true);
    QObject::connect(&m_tunnelAccessTimer, &QTimer::timeout, this, [this]() { takeTunnelPackets(); });
    QObject::connect(&m_tunnelAdvertiseTimer, &QTimer::timeout, this, &DSEMod::tunnelAdvertiseTimeout);
    m_tunnelAdvertiseTimer.start(DSEModAngel61::TunnelAdvertiseMs);
    updateChannelAccessTiming();
}

DSEMod::~DSEMod()
{
    m_fileAnswerTimer.stop();
    m_tunnelAccessTimer.stop();
    m_tunnelAdvertiseTimer.stop();
    destroyTunnel();
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DSEMod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSourceAPI(this);
    m_deviceAPI->removeChannelSource(this, true);
    stop();
    delete m_basebandSource;
    delete m_thread;
}

void DSEMod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSourceAPI(this);
        m_deviceAPI->removeChannelSource(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSource(this);
        m_deviceAPI->addChannelSourceAPI(this);
    }
}

void DSEMod::start()
{
    qDebug("DSEMod::start");
    m_basebandSource->reset();
    m_thread->start();

    SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
    spectrumSettings.m_ssb = m_settings.m_spectrumAudio; // audio is real: its positive half
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis.getInputMessageQueue()->push(msg);

    // (in the channel's own thread: start() runs in the device engine's, and the feature list is
    // the main thread's, changed by its feature handlers and its settings)
    QMetaObject::invokeMethod(this, [this]() {
        scanAvailableAMBEFeatures();
        notifyUpdateAMBEFeatures();
        applyAMBEFeature(m_settings);
    }, Qt::QueuedConnection);
}

void DSEMod::stop()
{
    qDebug("DSEMod::stop");
    auto processor = m_basebandSource->getProcessor();
    QMetaObject::invokeMethod(processor, [processor]() { processor->cancelAPI("Device stopped"); }, Qt::BlockingQueuedConnection);
    m_thread->exit();
    m_thread->wait();
    m_basebandSource->reset();
}

void DSEMod::pull(SampleVector::iterator& begin, unsigned int nbSamples)
{
    m_basebandSource->pull(begin, nbSamples);
}

void DSEMod::setCenterFrequency(qint64 frequency)
{
    DSEModSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList{"inputFrequencyOffset"}, settings, false);

    // Forward to GUI if any: the offset that took effect (an API session keeps its own, refusing it)
    if (m_guiMessageQueue)
    {
        MsgConfigureDSEMod *msgToGUI = MsgConfigureDSEMod::create(QStringList{"inputFrequencyOffset"}, m_settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

bool DSEMod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDSEMod::match(cmd))
    {
        MsgConfigureDSEMod& cfg = (MsgConfigureDSEMod&) cmd;
        qDebug() << "DSEMod::handleMessage: MsgConfigureDSEMod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (MsgConfigureFileSourceSeek::match(cmd))
    {
        MsgConfigureFileSourceSeek& conf = (MsgConfigureFileSourceSeek&) cmd;
        int seekPercentage = conf.getPercentage();
        seekFileStream(seekPercentage);
        qDebug() << "DSEMod::handleMessage: MsgConfigureFileSourceSeek: seekPercentage: " << seekPercentage;
        return true;
    }
    else if (MsgConfigureFileSourceStreamTiming::match(cmd))
    {
        std::size_t samplesCount;

        {
            QMutexLocker mutexLocker(&m_fileMutex);
            samplesCount = (std::size_t) m_audioFile.getPosition();
        }

        if (getMessageQueueToGUI())
        {
            MsgReportFileSourceStreamTiming *report = MsgReportFileSourceStreamTiming::create(samplesCount);
            getMessageQueueToGUI()->push(report);
        }

        return true;
    }
    else if (MsgQueryAvailableAMBEFeatures::match(cmd))
    {
        scanAvailableAMBEFeatures();
        notifyUpdateAMBEFeatures();
        return true;
    }
    else if (MsgQueryAvailableDemodulators::match(cmd))
    {
        scanDSDDemodulators();
        notifyUpdateDemodulators();
        return true;
    }
    else if (DSEModSource::MsgSendDTMF::match(cmd))
    {
        if (m_basebandSource->getProcessor()->isAPIActive()) { return true; }
        const DSEModSource::MsgSendDTMF& msg = (DSEModSource::MsgSendDTMF&) cmd;
        QString digits = msg.getDigits().isEmpty() ? m_settings.m_dtmfSequence : msg.getDigits();
        qDebug() << "DSEMod::handleMessage: MsgSendDTMF:" << digits;

        if (digits.isEmpty()) {
            return true;
        }

        if (!m_settings.m_transmit)
        {
            setTransmit(true); // key up for the sequence, MsgReportDTMFDone keys down again
            m_dtmfAutoTransmit = true;
        }

        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendDTMF::create(digits, m_dtmfAutoTransmit));
        return true;
    }
    else if (DSEModSource::MsgSendMessage::match(cmd))
    {
        if (m_basebandSource->getProcessor()->isAPIActive()) { return true; }
        // Angel61 text: in the slots of a running transmission, or a data transmission of its own
        const DSEModSource::MsgSendMessage& msg = (DSEModSource::MsgSendMessage&) cmd;
        QString text = msg.getText().isEmpty() ? m_settings.m_angel61Message : msg.getText();
        qDebug() << "DSEMod::handleMessage: MsgSendMessage:" << text;

        // Angel61, and M17 (its SMS: the GUI's M17 Send), whose transmission with a message queued is
        // a packet one that ends by itself
        const bool m17 = m_settings.m_mode == DSEModSettings::DSEModModeM17;

        if (m17) {
            text = msg.getText();
        }

        if (text.isEmpty() || !(DSEModSettings::isAngel61(m_settings.m_mode) || m17)) {
            return true;
        }

        // The message goes ahead of the key up (the same queue): it has to be queued, and the
        // transmission told it is for the message alone, when the protocol starts
        const bool keyUp = !m_settings.m_transmit;
        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendMessage::create(text, keyUp));

        if (keyUp)
        {
            m_messageAutoTransmit = true;
            setTransmit(true); // key up for the message, MsgReportTransmissionEnded keys down again
        }

        return true;
    }
    else if (DSEModSource::MsgSendAPRS::match(cmd))
    {
        // An APRS information field (3.3): a data transmission of its own, keyed up and down here
        if (m_basebandSource->getProcessor()->isAPIActive()) { return true; }
        const DSEModSource::MsgSendAPRS& msg = (DSEModSource::MsgSendAPRS&) cmd;

        if (msg.getInfo().isEmpty() || !DSEModSettings::isAngel61(m_settings.m_mode)) {
            return true;
        }

        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendAPRS::create(msg.getInfo()));

        if (!m_settings.m_transmit)
        {
            m_messageAutoTransmit = true;
            setTransmit(true);
        }

        return true;
    }
    else if (DSEModSource::MsgSendFile::match(cmd))
    {
        // A file to offer over Angel61 (3.4): the transmission is keyed up for it and again for
        // every round the receiving station asks for
        if (m_basebandSource->getProcessor()->isAPIActive()) { return true; }
        const DSEModSource::MsgSendFile& msg = (DSEModSource::MsgSendFile&) cmd;
        QString name = msg.getName().isEmpty() ? m_settings.m_angel61FileName : msg.getName();
        return sendFile(name);
    }
    else if (DSEModSource::MsgReportFileTransfer::match(cmd))
    {
        const DSEModSource::MsgReportFileTransfer& report = (DSEModSource::MsgReportFileTransfer&) cmd;

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSEModSource::MsgReportFileTransfer(report));
        }

        // The answer's timer: from the round's end while the transfer waits for the receiver (the
        // report after the transmission ended), stopped when anything else is reported
        m_fileState = report.getState();

        if (m_fileState != DVAngel61FileSender::Waiting) {
            m_fileAnswerTimer.stop();
        } else if (!m_settings.m_transmit && !m_fileAnswerTimer.isActive()) {
            m_fileAnswerTimer.start(fileAnswerTimeoutMs());
        }

        if (report.wantsTransmission() && !m_settings.m_transmit)
        {
            qDebug() << "DSEMod::handleMessage: MsgReportFileTransfer: keying up for round" << report.getRounds();
            m_fileAutoTransmit = true;
            setTransmit(true);
        }
        else if (!report.wantsTransmission() && m_fileAutoTransmit && (report.getState() != DVAngel61FileSender::Waiting))
        {
            m_fileAutoTransmit = false; // done or failed: the operator has the key back
        }

        return true;
    }
    else if (DSEModSource::MsgReportTransmissionEnded::match(cmd))
    {
        qDebug() << "DSEMod::handleMessage: MsgReportTransmissionEnded: autoTransmit:" << m_messageAutoTransmit;

        // Channel access (3.5): the peer's turn now, and an answer expected when datagrams went
        m_lastTransmissionEndMs = m_clock.elapsed();
        m_channelAccess.transmitted(m_lastTransmissionEndMs, m_tunnelTxInTransmission > 0);
        m_tunnelTxInTransmission = 0;
        m_tunnelAccessTimer.start(0);   // what the tunnel still has waits its turn

        // (Angel61 DTMF keyed up for itself ends the same way: the protocol's last frame, 3.8)
        if (m_messageAutoTransmit || m_fileAutoTransmit || m_dtmfAutoTransmit || m_tunnelAutoTransmit)
        {
            m_messageAutoTransmit = false;
            m_dtmfAutoTransmit = false;
            m_tunnelAutoTransmit = false;   // (the tunnel's key up was never given back: later datagrams waited on a key already up)
            setTransmit(false);
        }

        return true;
    }
    else if (DSEModSource::MsgReportDTMFDone::match(cmd))
    {
        qDebug() << "DSEMod::handleMessage: MsgReportDTMFDone: autoTransmit:" << m_dtmfAutoTransmit;

        if (m_dtmfAutoTransmit)
        {
            m_dtmfAutoTransmit = false;
            setTransmit(false);
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        // Forward to the source
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "DSEMod::handleMessage: DSPSignalNotification";
        m_basebandSource->getInputMessageQueue()->push(rep);

        // Forward to GUI if any
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (CWKeyer::MsgConfigureCWKeyer::match(cmd))
    {
        const CWKeyer::MsgConfigureCWKeyer& cfg = (CWKeyer::MsgConfigureCWKeyer&) cmd;

        if (m_settings.m_useReverseAPI) {
            webapiReverseSendCWSettings(cfg.getSettings());
        }

        return true;
    }
    else
    {
        return false;
    }
}

void DSEMod::setTransmit(bool on)
{
    DSEModSettings settings = m_settings;
    settings.m_transmit = on;
    applySettings(QStringList{"transmit"}, settings, false);

    if (getMessageQueueToGUI())
    {
        MsgConfigureDSEMod *msgToGUI = MsgConfigureDSEMod::create(QStringList{"transmit"}, settings, false);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

int DSEMod::webapiActionsPost(
        const QStringList& channelActionsKeys,
        SWGSDRangel::SWGChannelActions& query,
        QString& errorMessage)
{
    SWGSDRangel::SWGDSEModActions *swgActions = query.getDseModActions();

    if (!swgActions)
    {
        errorMessage = "Missing DSEModActions in query";
        return 400;
    }

    // The keys of the request's DSEModActions object as sent: the generated model reads an absent string
    // as an empty one, and an empty dtmf or file is an action of its own (the configured sequence or file)
    if (channelActionsKeys.size() != 1) {
        errorMessage = "Exactly one DSEMod action is required"; return 400;
    }
    const QString actionKey = channelActionsKeys.first();
    if (!m_thread->isRunning()) { errorMessage = "Transmit device is not running"; return 503; }
    auto processor = m_basebandSource->getProcessor();
    if (actionKey == "dtmf")
    {
        if (processor->isAPIActive()) { errorMessage = "API transmission owns the channel"; return 409; }
        QString digits = swgActions->getDtmf() ? *swgActions->getDtmf() : QString();
        if (digits.isEmpty()) { digits = m_settings.m_dtmfSequence; }
        if (!QRegularExpression("^[0-9A-D*#]{1,64}$").match(digits).hasMatch()) {
            errorMessage = "DTMF requires 1-64 digits (0-9, A-D, *, #)"; return 400;
        }
        getInputMessageQueue()->push(DSEModSource::MsgSendDTMF::create(digits));
        return 202;
    }
    if (actionKey == "aprs")
    {
        // An APRS information field (3.3) as a data transmission of content type 2
        if (processor->isAPIActive()) { errorMessage = "API transmission owns the channel"; return 409; }
        QString info = swgActions->getAprs() ? *swgActions->getAprs() : QString();
        if (info.isEmpty() || !DSEModSettings::isAngel61(m_settings.m_mode)) {
            errorMessage = "aprs requires an information field and the Angel61 mode"; return 400;
        }
        getInputMessageQueue()->push(DSEModSource::MsgSendAPRS::create(info));
        return 202;
    }
    if (actionKey == "file")
    {
        // A file transfer (3.4): the path, or the angel61FileName setting when it is empty
        if (processor->isAPIActive()) { errorMessage = "API transmission owns the channel"; return 409; }
        QString path = swgActions->getFile() ? *swgActions->getFile() : QString();
        if (path.isEmpty()) { path = m_settings.m_angel61FileName; }
        if (path.isEmpty() || !DSEModSettings::isAngel61(m_settings.m_mode)) {
            errorMessage = "file requires a path and the Angel61 mode"; return 400;
        }
        if (!QFile::exists(path)) { errorMessage = "file not found: " + path; return 400; }

        // Refused here rather than accepted and dropped later: 202 used to be the answer to a
        // file the transfer could only have carried the front of
        if (QFileInfo(path).size() > angel61FileCapacity())
        {
            errorMessage = QString("file is larger than the %1 bytes this mode carries")
                .arg(angel61FileCapacity());
            return 400;
        }

        getInputMessageQueue()->push(DSEModSource::MsgSendFile::create(path, QByteArray()));
        return 202;
    }

    // transmit and message: the session and the processor validate the request as JSON. Anything else is
    // passed on as it is, which they refuse
    QJsonObject action;

    if (actionKey == "transmit") {
        action["transmit"] = swgActions->getTransmit() ? QJsonValue(webapiTransmitToJson(swgActions->getTransmit())) : QJsonValue();
    } else if (actionKey == "message") {
        action["message"] = swgActions->getMessage() ? QJsonValue(webapiMessageToJson(swgActions->getMessage())) : QJsonValue();
    } else {
        action[actionKey] = QJsonValue();
    }

    int status = 500;
    QMetaObject::invokeMethod(processor, [&]() { status = processor->submitAction(action, errorMessage); }, Qt::BlockingQueuedConnection);
    return status;
}

// The generated model cannot say which of a request's fields were sent: an absent string arrives empty,
// an absent number as 0 (and fromJsonObject marks no field present), and the pcm object and frames array
// are there empty when not sent (the model factory init()s the transmit object it creates). The request
// DSEModTxSession takes is rebuilt with each field where it has a meaning: strings when not empty;
// sequence only on append and protocol only with frames input (their presence elsewhere is refused); on
// an opening request (start, send) the pcm object for pcm input and the frames for frames or angel61
// input, even empty (an empty start is valid), and otherwise either when it carries something; the PCM
// sample rate and channel count when not 0 (they are only on the opening request) and its data always;
// a frame's slot when not 0 and its sequence always.
QJsonObject DSEMod::webapiTransmitToJson(SWGSDRangel::SWGDSEModTransmit *transmit)
{
    QJsonObject json;
    auto text = [](QString *value) { return value ? *value : QString(); };
    auto putString = [](QJsonObject& object, const char *key, QString *value) {
        if (value && !value->isEmpty()) {
            object[key] = *value;
        }
    };

    putString(json, "operation", transmit->getOperation());
    putString(json, "sessionId", transmit->getSessionId());
    putString(json, "input", transmit->getInput());
    putString(json, "source", transmit->getSource());
    putString(json, "destination", transmit->getDestination());
    putString(json, "via", transmit->getVia());

    if (text(transmit->getOperation()) == "append") {
        json["sequence"] = transmit->getSequence();
    }
    if (text(transmit->getInput()) == "frames") {
        json["protocol"] = transmit->getProtocol();
    }

    const QString operation = text(transmit->getOperation());
    const QString input = text(transmit->getInput());
    const bool opening = (operation == "start") || (operation == "send");
    SWGSDRangel::SWGDSEModPCM *pcm = transmit->getPcm();
    QList<SWGSDRangel::SWGDSEModFrame*> *frames = transmit->getFrames();
    const bool pcmSent = pcm && (opening ? (input == "pcm") : !text(pcm->getData()).isEmpty());
    const bool framesSent = frames && (opening ? ((input == "frames") || (input == "angel61")) : !frames->isEmpty());

    if (pcmSent)
    {
        QJsonObject pcmJson;
        putString(pcmJson, "encoding", pcm->getEncoding());

        if (pcm->getSampleRate() != 0) {
            pcmJson["sampleRate"] = pcm->getSampleRate();
        }
        if (pcm->getChannels() != 0) {
            pcmJson["channels"] = pcm->getChannels();
        }

        pcmJson["data"] = text(pcm->getData());
        json["pcm"] = pcmJson;
    }

    if (framesSent)
    {
        QJsonArray framesJson;

        for (SWGSDRangel::SWGDSEModFrame *frame : *frames)
        {
            QJsonObject frameJson;
            putString(frameJson, "format", frame->getFormat());
            putString(frameJson, "frameType", frame->getFrameType());

            if (frame->getSlot() != 0) {
                frameJson["slot"] = frame->getSlot();
            }

            frameJson["sequence"] = frame->getSequence();
            frameJson["data"] = text(frame->getData());
            framesJson.append(frameJson);
        }

        json["frames"] = framesJson;
    }

    return json;
}

QJsonObject DSEMod::webapiMessageToJson(SWGSDRangel::SWGDSEModMessage *message)
{
    QJsonObject json;
    auto putString = [&json](const char *key, QString *value) {
        if (value && !value->isEmpty()) {
            json[key] = *value;
        }
    };

    putString("requestId", message->getRequestId());
    putString("type", message->getType());
    putString("delivery", message->getDelivery());
    putString("sessionId", message->getSessionId());
    putString("service", message->getService());
    putString("text", message->getText());
    putString("data", message->getData());
    return json;
}

void DSEMod::openFileStream()
{
    QMutexLocker mutexLocker(&m_fileMutex);

    // A .wav file brings its own sample rate and channel count, a .raw file is 16 bit mono at 8 kHz
    if (m_settings.m_audioFileName.isEmpty())
    {
        m_audioFile.close();
        m_fileSampleRate = DSEModAudioFile::RawSampleRate;
        m_recordLength = 0;
    }
    else if (m_audioFile.open(m_settings.m_audioFileName))
    {
        m_fileSampleRate = m_audioFile.getSampleRate();
        m_recordLength = m_audioFile.getLengthSeconds();
    }
    else
    {
        m_fileSampleRate = DSEModAudioFile::RawSampleRate;
        m_recordLength = 0;
    }

    qDebug() << "DSEMod::openFileStream: " << m_settings.m_audioFileName.toStdString().c_str()
        << " sampleRate: " << m_fileSampleRate << " length: " << m_recordLength << " seconds";

    if (getMessageQueueToGUI())
    {
        MsgReportFileSourceStreamData *report = MsgReportFileSourceStreamData::create(m_fileSampleRate, m_recordLength);
        getMessageQueueToGUI()->push(report);
    }
}

void DSEMod::seekFileStream(int seekPercentage)
{
    QMutexLocker mutexLocker(&m_fileMutex);

    if (m_audioFile.isOpen()) {
        m_audioFile.seekPercentage(seekPercentage);
    }
}

void DSEMod::scanAvailableAMBEFeatures()
{
    MainCore *mainCore = MainCore::instance();
    m_availableAMBEFeatures.clear();

    if (mainCore->getFeatureeSets().size() == 0) {
        return;
    }

    int nbFeatures = mainCore->getFeatureeSets()[0]->getNumberOfFeatures();

    for (int i = 0; i < nbFeatures; i++)
    {
        Feature *feature = mainCore->getFeatureeSets()[0]->getFeatureAt(i);

        // (its index in the set, as handleFeatureAdded takes it: the list position was not always it)
        if (feature->getURI() == "sdrangel.feature.ambe") {
            m_availableAMBEFeatures[feature] = DSEModSettings::AvailableAMBEFeature{feature->getIndexInFeatureSet(), feature};
        }
    }
}

void DSEMod::notifyUpdateAMBEFeatures()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableAMBEFeatures *msg = MsgReportAvailableAMBEFeatures::create();
        msg->getFeatures() = m_availableAMBEFeatures.values();
        getMessageQueueToGUI()->push(msg);
    }
}

void DSEMod::applyAMBEFeature(const DSEModSettings& settings)
{
    Feature *feature = nullptr;

    if (settings.m_connectAMBE)
    {
        for (const auto& available : m_availableAMBEFeatures)
        {
            if (available.m_featureIndex == settings.m_ambeFeatureIndex) {
                feature = available.m_feature;
            }
        }
    }

    // The feature's port, asked for here in its own thread: the processor's vocoder calls only that
    std::shared_ptr<MbeEncoderPort> port;

    if (feature)
    {
        DSPGetMbeEncoderPort msg;

        if (feature->handleMessage(msg)) {
            port = msg.getPort();
        }
    }

    m_connectedAMBEFeature = port ? feature : nullptr;
    m_basebandSource->setAMBEFeature(m_connectedAMBEFeature, port);
}

void DSEMod::handleFeatureAdded(int featureSetIndex, Feature *feature)
{
    if (featureSetIndex != 0) {
        return;
    }

    if ((feature->getURI() == "sdrangel.feature.ambe") && !m_availableAMBEFeatures.contains(feature))
    {
        m_availableAMBEFeatures[feature] = DSEModSettings::AvailableAMBEFeature{feature->getIndexInFeatureSet(), feature};
        applyAMBEFeature(m_settings);
        notifyUpdateAMBEFeatures();
    }
}

// The Angel61 rate request (ANGEL61.md 2.10) comes from the receiver side: every DSD Demodulator
// in the instance is listened to through an "angel61" message pipe, on which it reports, per
// station heard, the request bit that station sent and the one this station would send it

void DSEMod::scanDSDDemodulators()
{
    MainCore *mainCore = MainCore::instance();

    for (int d = 0; d < (int) mainCore->getDeviceSets().size(); d++)
    {
        DeviceSet *deviceSet = mainCore->getDeviceSets()[d];

        for (int c = 0; c < deviceSet->getNumberOfChannels(); c++) {
            listenToDemodulator(deviceSet->getChannelAt(c));
        }
    }

    m_availableDemodulators = mainCore->getAvailableChannels(QStringList{ DSDDemodURI });
    applyDemodulatorSelection(m_settings);
}

void DSEMod::notifyUpdateDemodulators()
{
    if (getMessageQueueToGUI())
    {
        MsgReportAvailableDemodulators *msg = MsgReportAvailableDemodulators::create();
        msg->getDemodulators() = m_availableDemodulators;
        getMessageQueueToGUI()->push(msg);
    }
}

void DSEMod::applyDemodulatorSelection(const DSEModSettings& settings)
{
    // Which receiver the rate request is taken from: the one the setting names, or all of them
    // when it is empty or names a channel that is not there (any more)
    const QObject *selected = nullptr;

    if (!settings.m_angel61Demod.isEmpty())
    {
        int index = m_availableDemodulators.indexOfId(settings.m_angel61Demod);

        if (index >= 0) {
            selected = m_availableDemodulators[index].m_object;
        }
    }

    if (selected != m_angel61DemodSelected)
    {
        // What the other receivers reported is not this one's: start again
        m_angel61DemodSelected = selected;
        m_angel61Stations.clear();
        updateLinkState();
        reportTunnelToDemodulators();
    }
}

void DSEMod::listenToDemodulator(ChannelAPI *channel)
{
    if (!channel || (channel->getURI() != DSDDemodURI) || m_angel61Demodulators.contains(channel)) {
        return;
    }

    ObjectPipe *pipe = MainCore::instance()->getMessagePipes().registerProducerToConsumer(channel, this, "angel61");
    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
    QObject::connect(
        messageQueue,
        &MessageQueue::messageEnqueued,
        this,
        [=](){ this->handleDemodulatorMessageQueue(messageQueue); },
        Qt::QueuedConnection
    );
    m_angel61Demodulators.insert(channel);
    qDebug("DSEMod::listenToDemodulator: DSD Demodulator %p", (void *) channel);
}

void DSEMod::handleChannelAdded(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;

    if (!channel || (channel->getURI() != DSDDemodURI)) {
        return;
    }

    listenToDemodulator(channel);
    m_availableDemodulators = MainCore::instance()->getAvailableChannels(QStringList{ DSDDemodURI });
    applyDemodulatorSelection(m_settings);
    notifyUpdateDemodulators();
    reportTunnelToDemodulators(); // (the new one has not heard it)
}

void DSEMod::handleChannelRemoved(int deviceSetIndex, ChannelAPI *channel)
{
    (void) deviceSetIndex;

    if (!m_angel61Demodulators.contains(channel)) {
        return;
    }

    m_angel61Demodulators.remove(channel); // the pipe is garbage collected with the channel

    if (m_angel61DemodSelected == channel)
    {
        // The receiver the setting names has gone: back to all of them until it comes back
        m_angel61DemodSelected = nullptr;
        m_angel61Stations.clear();
        updateLinkState();
    }

    int index = m_availableDemodulators.indexOfObject(channel);

    if (index >= 0) {
        m_availableDemodulators.removeAt(index);
    }

    notifyUpdateDemodulators();
}

void DSEMod::handleDemodulatorMessageQueue(MessageQueue *messageQueue)
{
    Message *message;

    while ((message = messageQueue->pop()) != nullptr)
    {
        if (MainCore::MsgPacket::match(*message))
        {
            // "ANGEL61 source destination request send profile" (the tiers of 2.10)
            MainCore::MsgPacket& packet = (MainCore::MsgPacket&) *message;
            QStringList fields = QString::fromUtf8(packet.getPacket()).split(' ');
            bool wanted = !m_angel61DemodSelected || (packet.getPipeSource() == m_angel61DemodSelected);

            if (wanted && (fields.size() >= 3) && (fields[0] == "ANGEL61FILE")
                && angel61FileControlIsOurs(fields[1]))
            {
                // The answer of a station receiving a file from this one (3.4), heard off the air
                QByteArray control = QByteArray::fromHex(fields[2].toLatin1());
                m_fileAnswerTimer.stop();

                if (!control.isEmpty())
                {
                    qDebug() << "DSEMod::handleDemodulatorMessageQueue: file control" << fields[1] << fields[2];
                    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgFileControl::create(control));
                }
            }
            else if (wanted && (fields.size() >= 4) && (fields[0] == "ANGEL61CH"))
            {
                // The receiver's channel state (3.5): a transmission heard begins, or ends, whose and to whom
                const qint64 now = m_clock.elapsed();

                if (fields[1] == "1")
                {
                    m_channelAccess.heardStart(now);
                }
                else
                {
                    const bool fromPeer = !m_settings.m_angel61TunnelPeer.trimmed().isEmpty()
                        && DVAngel61::sameStation(fields[2].toStdString(), m_settings.m_angel61TunnelPeer.toStdString());
                    const bool toMe = DVAngel61::sameStation(fields[3].toStdString(), m_settings.m_angel61Source.toStdString());
                    m_channelAccess.heardEnd(now, fromPeer, toMe);
                    m_tunnelAccessTimer.start(0);   // a reply slot, or the channel free: what waits may go
                }
            }
            else if (wanted && (fields.size() >= 3) && (fields[0] == "ANGEL61FILEQ"))
            {
                // The answer the demodulator's station owes the station sending it a file (3.4)
                sendAngel61FileControl(fields[1], QByteArray::fromHex(fields[2].toLatin1()));
            }
            else if (wanted && (fields.size() >= 3) && (fields[0] == "ANGEL61IP"))
            {
                // A datagram heard by a DSD Demodulator of this instance (3.5)
                injectTunnelDatagram(fields[1], QByteArray::fromHex(fields[2].toLatin1()));
                reportTunnelToDemodulators();
            }
            else if (wanted && (fields.size() >= 3) && (fields[0] == "ANGEL61IPC"))
            {
                // What the far station says it will route
                QByteArray bytes = QByteArray::fromHex(fields[2].toLatin1());
                DVAngel61IP::Control control;

                if (DVAngel61IP::Control::decode(std::vector<uint8_t>(bytes.begin(), bytes.end()), control)
                    && DVAngel61::sameStation(fields[1].toStdString(), m_settings.m_angel61TunnelPeer.toStdString()))
                {
                    if (control.m_kind == DVAngel61IP::Control::Advertise)
                    {
                        m_tunnelPeer = control;
                        m_tunnelPeerValid = true;
                        qDebug() << "DSEMod: the tunnel peer advertises"
                            << QString::fromStdString(control.toString());
                    }
                    else if (control.m_kind == DVAngel61IP::Control::Close)
                    {
                        m_tunnelPeerValid = false;
                    }

                    // Its advertise and close say what header compression it takes, its context
                    // state which of this station's contexts it has lost (3.6), its repair which
                    // fragments it lacks (3.5): sent in the reply slot its transmission hands over
                    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgTunnelControl::create(bytes, false));

                    if (control.m_kind == DVAngel61IP::Control::Repair) {
                        m_tunnelOwed = true;
                    }
                }
            }
            else if (wanted && (fields.size() >= 3) && (fields[0] == "ANGEL61IPQ"))
            {
                // The header decompressor of a DSD Demodulator of this instance wants the station
                // whose datagrams it rebuilds to hear its context state or acknowledgement (3.6):
                // sent when that station is this tunnel's peer
                QByteArray bytes = QByteArray::fromHex(fields[2].toLatin1());

                if (m_ipTunnel && (m_ipTunnel->state() == IPTunnel::State::Open) && !bytes.isEmpty()
                    && DVAngel61::sameStation(fields[1].toStdString(), m_settings.m_angel61TunnelPeer.toStdString()))
                {
                    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgTunnelControl::create(bytes, true));
                    m_tunnelOwed = true;   // it goes in the reply slot (3.5: what is owed first)
                    m_tunnelAccessTimer.start(0);
                }
            }
            else if (wanted && (fields.size() >= 5) && (fields[0] == "ANGEL61"))
            {
                // (at most MaxAngel61Stations, the least recently heard going: they are callsigns off
                // the air, and every record scans them)
                const QString callsign = fields[1].toUpper();

                if (!m_angel61Stations.contains(callsign) && (m_angel61Stations.size() >= MaxAngel61Stations))
                {
                    auto oldest = m_angel61Stations.begin();

                    for (auto it = m_angel61Stations.begin(); it != m_angel61Stations.end(); ++it) {
                        if (it->m_heard < oldest->m_heard) { oldest = it; }
                    }

                    m_angel61Stations.erase(oldest);
                }

                Angel61Station& station = m_angel61Stations[callsign];
                station.m_destination = fields[2].toUpper();
                station.m_requestedTier = std::max(0, std::min(3, fields[3].toInt()));
                station.m_sendTier = std::max(0, std::min(3, fields[4].toInt()));
                station.m_profile = fields.size() >= 6 ? fields[5] : QString("NB");
                station.m_heard = packet.getDateTime();

                // A transmission heard while a file waits for its answer: the channel is in use (the
                // answer may be the next one), so the round's timer starts again
                if (m_fileAnswerTimer.isActive()) {
                    m_fileAnswerTimer.start(fileAnswerTimeoutMs());
                }
                qDebug() << "DSEMod::handleDemodulatorMessageQueue: heard" << fields[1] << ">" << fields[2]
                    << "tier requested" << station.m_requestedTier << "would ask" << station.m_sendTier << station.m_profile;
                updateLinkState();
            }
        }

        delete message;
    }
}

void DSEMod::sendAngel61FileControl(const QString& destination, const QByteArray& control)
{
    if (control.isEmpty()) {
        return;
    }

    // A file this station sent itself, received by a demodulator of the same instance (a bench
    // loopback): the answer is the sender's at once, as one heard off the air would be
    if (DVAngel61::sameStation(destination.toStdString(), m_settings.m_angel61Source.toStdString()))
    {
        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgFileControl::create(control));
        return;
    }

    // Another station's file: the answer goes on the air, in this station's own transmission (the
    // channel keys up for it and down when it is out), or in the one going on (the API's excepted)
    if (!DSEModSettings::isAngel61(m_settings.m_mode) || m_basebandSource->getProcessor()->isAPIActive()) {
        return;
    }

    const bool keyUp = !m_settings.m_transmit;
    qDebug() << "DSEMod::sendAngel61FileControl: to" << destination << control.toHex() << (keyUp ? "keying up" : "in this transmission");
    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgFileControl::create(control, true,
        destination == "--" ? QString() : destination, keyUp));

    if (keyUp)
    {
        m_messageAutoTransmit = true;
        setTransmit(true); // MsgReportTransmissionEnded keys down again
    }
}

int DSEMod::fileAnswerTimeoutMs() const
{
    // The receiver's end of transmission, its key up and preamble, a data transmission of the
    // answer (twice) and its own end: a few seconds on VHF, over ten of HF's 240 ms frames
    return DSEModSettings::isAngel61HF(m_settings.m_mode) ? 25000 : 8000;
}

void DSEMod::fileAnswerTimeout()
{
    // Nothing heard back: the offer goes again, alone, so that the receiver owes an answer (3.4); if
    // the rounds have run out the transfer fails. The report that follows keys up for the round
    if ((m_fileState != DVAngel61FileSender::Waiting) || m_settings.m_transmit
        || m_basebandSource->getProcessor()->isAPIActive())
    {
        return;
    }

    qDebug() << "DSEMod::fileAnswerTimeout: no answer to the file transfer: the offer again";
    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgFileNoAnswer::create());
}

bool DSEMod::angel61FileControlIsOurs(const QString& destination) const
{
    // The demodulator addresses the answer to the station the file came from, which is this one
    // when the transfer is ours. Two modulators on one demodulator would otherwise take each
    // other's answers whenever a transfer id and a CRC-32 happened to agree. "--" is a control
    // the receiver heard without being able to say whose it was; the sender still has to match
    // its transfer id and CRC-32 before it acts on one
    if (destination == "--") {
        return true;
    }

    return DVAngel61::sameStation(destination.toStdString(), m_settings.m_angel61Source.toStdString());
}

std::vector<DVAngel61IP::Prefix> DSEMod::parsePrefixes(const QString& text)
{
    std::vector<DVAngel61IP::Prefix> prefixes;

    for (const QString& part : text.split(',', Qt::SkipEmptyParts))
    {
        QString trimmed = part.trimmed();
        int slash = trimmed.indexOf('/');

        if (slash < 0) {
            continue;
        }

        QHostAddress address(trimmed.left(slash));
        bool lengthOk = false;
        int length = trimmed.mid(slash + 1).toInt(&lengthOk);

        if (address.isNull() || !lengthOk) {
            continue;
        }

        DVAngel61IP::Prefix prefix;

        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            quint32 v4 = address.toIPv4Address();
            prefix.m_family = 4;
            prefix.m_address = { (uint8_t) (v4 >> 24), (uint8_t) (v4 >> 16), (uint8_t) (v4 >> 8), (uint8_t) v4 };
        }
        else
        {
            Q_IPV6ADDR v6 = address.toIPv6Address();
            prefix.m_family = 6;
            prefix.m_address.assign(v6.c, v6.c + 16);
        }

        prefix.m_length = length;

        if ((length >= 0) && (length <= (prefix.m_family == 6 ? 128 : 32))) {
            prefixes.push_back(prefix);
        }
    }

    return prefixes;
}

DVAngel61IP::Control DSEMod::tunnelAdvertise() const
{
    DVAngel61IP::Control advertise;
    advertise.m_kind = DVAngel61IP::Control::Advertise;
    advertise.m_mtu = m_settings.m_angel61TunnelMTU;
    advertise.m_compression = DVAngel61HC::capability(true, true, true, DVAngel61HC::DefaultContexts);
    advertise.m_prefixes = parsePrefixes(m_settings.m_angel61TunnelRoutes);
    advertise.m_flags = 0;
    bool v4 = false, v6 = false;

    for (const DVAngel61IP::Prefix& prefix : advertise.m_prefixes)
    {
        v4 = v4 || (prefix.m_family == 4);
        v6 = v6 || (prefix.m_family == 6);
    }

    // With no prefixes named the station carries IPv4, and IPv6 when its minimum MTU can be met.
    advertise.m_flags |= (v4 || !v6) ? DVAngel61IP::Control::FlagIPv4 : 0;
    advertise.m_flags |= (v6 || (!v4 && (advertise.m_mtu >= 1280))) ? DVAngel61IP::Control::FlagIPv6 : 0;
    advertise.m_flags |= DVAngel61IP::Control::FlagRepairs;   // (3.5: it answers a repair request)
    return advertise;
}

void DSEMod::reportTunnelStatus(const QString& status)
{
    m_tunnelStatus = status;

    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgReportTunnelStatus::create(status));
    }

    reportTunnelToDemodulators();
}

void DSEMod::reportTunnelTraffic()
{
    if (m_guiMessageQueue) {
        m_guiMessageQueue->push(MsgReportTunnelTraffic::create(m_tunnelTxBytes, m_tunnelRxBytes, tunnelRefusedText()));
    }
}

void DSEMod::reportTunnelToDemodulators()
{
    // A demodulator counts what it hears, but only this side knows what became of it. The channels
    // are found afresh each time, as m_angel61Demodulators can hold one that was deleted: a
    // preset's load announces the removals after the deletions (DeviceSet::clearChannels)
    MainCore *mainCore = MainCore::instance();

    for (int d = 0; d < (int) mainCore->getDeviceSets().size(); d++)
    {
        DeviceSet *deviceSet = mainCore->getDeviceSets()[d];

        for (int c = 0; c < deviceSet->getNumberOfChannels(); c++)
        {
            ChannelAPI *channel = deviceSet->getChannelAt(c);

            if (!channel || (channel->getURI() != DSDDemodURI)) {
                continue;
            }

            // "ANGEL61TUNNEL listening datagrams bytes refused status", where listening is 0 when
            // the Rx selector names another demodulator: the status, which has spaces, comes last
            const bool listening = !m_angel61DemodSelected || (channel == m_angel61DemodSelected);
            const QByteArray packet = QString("ANGEL61TUNNEL %1 %2 %3 %4 %5")
                .arg(listening ? 1 : 0)
                .arg(m_tunnelReceived)
                .arg(m_tunnelRxBytes)
                .arg(m_tunnelRxRefused)
                .arg(m_tunnelStatus)
                .toUtf8();
            channel->getInputMessageQueue()->push(MainCore::MsgPacket::create(this, packet));
        }
    }
}

QString DSEMod::tunnelRefusedText() const
{
    // What the filter kept back, and why: a tunnel that looks idle can be explained (3.5)
    QStringList parts;

    if (!m_tunnelFilterError.isEmpty()) {
        parts.append(QStringLiteral("allow list: cannot read \"%1\", nothing is sent").arg(m_tunnelFilterError));
    }

    for (int reason = DVAngel61IP::NotIP; reason < DVAngel61IP::NbRefusals; reason++)
    {
        if (m_tunnelRefusedBy[reason] > 0) {
            parts.append(QStringLiteral("%1 %2").arg(m_tunnelRefusedBy[reason]).arg(DVAngel61IP::refusalName((DVAngel61IP::Refusal) reason)));
        }
    }

    if (m_tunnelUnroutable > 0) {
        parts.append(QStringLiteral("%1 not routed by the peer").arg(m_tunnelUnroutable));
    }

    return parts.join(", ");
}

void DSEMod::updateTunnelFilter(const DSEModSettings& settings)
{
    // The operator's allow list and multicast groups (3.5). A list that cannot be read passes
    // nothing rather than everything: it is a list of what may be sent
    DVAngel61IP::Filter filter;
    std::string error;
    m_tunnelFilterError.clear();

    if (!DVAngel61IP::Filter::parseAllow(settings.m_angel61TunnelAllow.toStdString(), filter.m_allow, error)) {
        m_tunnelFilterError = QString::fromStdString(error);
    }

    for (const QString& part : settings.m_angel61TunnelGroups.split(',', Qt::SkipEmptyParts))
    {
        QHostAddress group(part.trimmed());

        if (group.protocol() == QAbstractSocket::IPv4Protocol)
        {
            const quint32 v4 = group.toIPv4Address();
            filter.m_groups.push_back({ (uint8_t) (v4 >> 24), (uint8_t) (v4 >> 16), (uint8_t) (v4 >> 8), (uint8_t) v4 });
        }
        else if (group.protocol() == QAbstractSocket::IPv6Protocol)
        {
            const Q_IPV6ADDR v6 = group.toIPv6Address();
            filter.m_groups.push_back(std::vector<uint8_t>(v6.c, v6.c + 16));
        }
        else if (m_tunnelFilterError.isEmpty())
        {
            m_tunnelFilterError = part.trimmed();
        }
    }

    m_tunnelFilter = filter;
    reportTunnelTraffic();
}

void DSEMod::setTunnelProtocolEnabled(bool enabled)
{
    QByteArray advertiseBytes;

    if (enabled)
    {
        DVAngel61IP::Control advertise = tunnelAdvertise();
        std::vector<uint8_t> bytes = advertise.encode();
        advertiseBytes = QByteArray((const char *) bytes.data(), (int) bytes.size());
    }

    m_basebandSource->getInputMessageQueue()->push(
        DSEModSource::MsgTunnelState::create(enabled, advertiseBytes));
}

void DSEMod::destroyTunnel()
{
    if (!m_ipTunnel) {
        return;
    }

    disconnect(m_ipTunnel, nullptr, this, nullptr);
    m_ipTunnel->close();
    delete m_ipTunnel;
    m_ipTunnel = nullptr;
}

void DSEMod::updateTunnel(const QStringList& settingsKeys, bool force)
{
    const bool enabled = m_settings.m_angel61Tunnel;
    const bool protocolEnabled = enabled && DSEModSettings::isAngel61(m_settings.m_mode);

    // Another peer, or the tunnel off: what the last one advertised no longer filters the datagrams
    if (!enabled || force || settingsKeys.contains("angel61TunnelPeer")) {
        m_tunnelPeerValid = false;
    }

    // (and channel access starts afresh: the backoff and the unanswered count were the old peer's)
    if (!enabled || force || settingsKeys.contains("angel61TunnelPeer") || settingsKeys.contains("angel61Tunnel")) {
        m_channelAccess.reset();
    }

    updateChannelAccessTiming();

    if (enabled && settingsKeys.contains("angel61Tunnel"))
    {
        m_tunnelSent = 0;
        m_tunnelReceived = 0;
        m_tunnelRefused = 0;
        m_tunnelRxRefused = 0;
        std::fill(std::begin(m_tunnelRefusedBy), std::end(m_tunnelRefusedBy), 0);
        m_tunnelUnroutable = 0;
        m_tunnelTxBytes = 0;
        m_tunnelRxBytes = 0;
        reportTunnelTraffic();
    }

    if (!enabled)
    {
        setTunnelProtocolEnabled(false);
        destroyTunnel();
        reportTunnelStatus(QStringLiteral("Tunnel: off"));
        return;
    }

    // Do not prompt merely because a saved tunnel is enabled while another mode is selected.
    // Once Angel61 has opened it, however, retain the helper and interface across mode changes so
    // that returning to Angel61 does not require another UAC elevation.
    if (!protocolEnabled && !m_ipTunnel)
    {
        setTunnelProtocolEnabled(false);
        reportTunnelStatus(QStringLiteral("Tunnel: ready when Angel61 is selected"));
        return;
    }

    // The tunnel is point to point (3.5): CQ or a talkgroup is no peer
    if (DVAngel61::isGroupText(m_settings.m_angel61TunnelPeer.trimmed().toStdString()))
    {
        setTunnelProtocolEnabled(false);
        destroyTunnel();
        reportTunnelStatus(QStringLiteral("Tunnel: the peer must be a station, not CQ or a talkgroup"));
        return;
    }

    IPTunnel::Configuration configuration;
    configuration.logicalId = "angel61";
    configuration.interfaceName = m_settings.m_angel61TunnelInterface;
    configuration.displayName = "SDRangel Angel61";
    configuration.mtu = m_settings.m_angel61TunnelMTU;
    // An MTU below 1280 is valid for IPv4 and is the DSEMod default, but it cannot carry IPv6.
    // Explicit IPv6 addresses/routes below that size are rejected by IPTunnel validation.
    configuration.ipv4 = true;
    configuration.ipv6 = configuration.mtu >= 1280;

    QString address = m_settings.m_angel61TunnelAddress.trimmed();
    int slash = address.indexOf('/');

    if (!address.isEmpty())
    {
        IPTunnelTypes::Address local;
        bool prefixOk = false;

        if (slash > 0)
        {
            local.address = QHostAddress(address.left(slash));
            local.prefixLength = address.mid(slash + 1).toInt(&prefixOk);
        }

        // Append malformed non-empty input as well, so validation reports it instead of silently
        // creating an unnumbered interface.
        if (!prefixOk) {
            local.prefixLength = -1;
        }
        configuration.addresses.append(local);
    }

    for (const DVAngel61IP::Prefix& prefix : parsePrefixes(m_settings.m_angel61TunnelRoutes))
    {
        IPTunnelTypes::Prefix route;
        route.length = prefix.m_length;

        if (prefix.m_family == 4)
        {
            quint32 v4 = ((quint32) prefix.m_address[0] << 24) | ((quint32) prefix.m_address[1] << 16)
                | ((quint32) prefix.m_address[2] << 8) | prefix.m_address[3];
            route.address = QHostAddress(v4);
        }
        else
        {
            Q_IPV6ADDR v6;
            std::copy(prefix.m_address.begin(), prefix.m_address.end(), v6.c);
            route.address = QHostAddress(v6);
        }

        configuration.routing.include.append(route);
    }

    IPTunnel::ValidationResult validation = IPTunnel::validate(configuration);

    if (!validation.valid)
    {
        qWarning() << "DSEMod::updateTunnel: the configuration is not one the host can use:"
            << validation.error.message;
        setTunnelProtocolEnabled(false);
        destroyTunnel();
        reportTunnelStatus(QStringLiteral("Tunnel error: %1").arg(validation.error.message));
        return;
    }

    bool hostConfigurationChanged = force || settingsKeys.contains("angel61TunnelInterface")
        || settingsKeys.contains("angel61TunnelAddress") || settingsKeys.contains("angel61TunnelRoutes")
        || settingsKeys.contains("angel61TunnelMTU");

    if (m_ipTunnel && ((m_ipTunnel->state() == IPTunnel::State::Error) || hostConfigurationChanged))
    {
        setTunnelProtocolEnabled(false);
        destroyTunnel();
    }

    // A failed tunnel is retried only when Angel61 actually needs it. This also prevents an
    // unrelated mode change from presenting a UAC prompt after an earlier tunnel error.
    if (!protocolEnabled && !m_ipTunnel)
    {
        reportTunnelStatus(QStringLiteral("Tunnel: ready when Angel61 is selected"));
        return;
    }

    if (!m_ipTunnel)
    {
        m_ipTunnel = new IPTunnel(this);

        // Everything the host puts into the interface, as it arrives
        connect(m_ipTunnel, &IPTunnel::outboundPacketsAvailable, this, [this]() { takeTunnelPackets(); });
        connect(m_ipTunnel, &IPTunnel::errorOccurred, this, [this](IPTunnelTypes::Error error) {
            qWarning() << "DSEMod: IP tunnel error:" << error.message;
            setTunnelProtocolEnabled(false);
            reportTunnelStatus(QStringLiteral("Tunnel error: %1").arg(error.message));
        });
        connect(m_ipTunnel, &IPTunnel::stateChanged, this, [this](IPTunnelTypes::State state) {
            qDebug() << "DSEMod: IP tunnel state" << (int) state;

            if (state == IPTunnel::State::Opening)
            {
                setTunnelProtocolEnabled(false);
                reportTunnelStatus(QStringLiteral("Tunnel: opening %1...")
                    .arg(m_settings.m_angel61TunnelInterface));
            }
            else if (state == IPTunnel::State::Open)
            {
                const bool active = DSEModSettings::isAngel61(m_settings.m_mode);
                setTunnelProtocolEnabled(active);
                reportTunnelStatus(active ? QStringLiteral("Tunnel: %1 to %2, MTU %3")
                    .arg(m_ipTunnel->interfaceInfo().name)
                    .arg(m_settings.m_angel61TunnelPeer.isEmpty() ? QStringLiteral("nobody") : m_settings.m_angel61TunnelPeer)
                    .arg(m_settings.m_angel61TunnelMTU) :
                    QStringLiteral("Tunnel: open, inactive outside Angel61"));
            }
            else if ((state == IPTunnel::State::Closing) || (state == IPTunnel::State::Closed))
            {
                setTunnelProtocolEnabled(false);
            }
            else if (state == IPTunnel::State::Error)
            {
                setTunnelProtocolEnabled(false);
            }
        });

        m_ipTunnel->open(configuration);
    }
    else if (m_ipTunnel->state() == IPTunnel::State::Open)
    {
        // Peer/source changes do not require a host-interface restart, but do change the terms
        // advertised on the air. Other modes retain the elevated helper but do not carry IP.
        setTunnelProtocolEnabled(protocolEnabled);
        reportTunnelStatus(protocolEnabled ? QStringLiteral("Tunnel: %1 to %2, MTU %3")
            .arg(m_ipTunnel->interfaceInfo().name)
            .arg(m_settings.m_angel61TunnelPeer.isEmpty() ? QStringLiteral("nobody") : m_settings.m_angel61TunnelPeer)
            .arg(m_settings.m_angel61TunnelMTU) :
            QStringLiteral("Tunnel: open, inactive outside Angel61"));
    }
}

void DSEMod::takeTunnelPackets()
{
    if (!m_ipTunnel) {
        return;
    }

    QByteArray packet;

    // The interface is deliberately kept open across mode changes to avoid repeated elevation.
    // Drain rather than retain host traffic while another protocol is selected: stale datagrams
    // must not be transmitted if the operator later returns to Angel61.
    // Likewise while the device is stopped: nothing takes them, and they piled up to key the
    // transmitter with a burst of stale datagrams at the next start
    if (!DSEModSettings::isAngel61(m_settings.m_mode) || !m_thread->isRunning())
    {
        while (m_ipTunnel->takeOutboundPacket(packet)) {
            m_tunnelRefused++;
        }
        return;
    }

    bool queued = false;
    bool refused = false;
    const bool keyUp = !m_settings.m_transmit;   // the transmission keyed up for them alone: data only
    const bool waiting = m_ipTunnel->statistics().outboundQueuedPackets > 0;

    // Channel access (3.5): a key up for the tunnel waits its turn - the reply slot, the peer's turn
    // after this station's, the backoff after listening - and datagrams are held (the host's queue
    // ages them) while the peer does not answer; what is owed it still goes. A transmission going on
    // takes them now, under its voice or in its data
    if (keyUp)
    {
        const bool datagrams = waiting && !m_channelAccess.isHeld();

        if ((!datagrams && !m_tunnelOwed) || !tunnelKeyUpAllowed(datagrams)) {
            return;
        }
    }

    const bool takeDatagrams = !keyUp || !m_channelAccess.isHeld();

    while (takeDatagrams && m_ipTunnel->takeOutboundPacket(packet))
    {
        std::vector<uint8_t> datagram(packet.begin(), packet.end());
        // (an allow list that could not be read sends nothing)
        DVAngel61IP::Refusal refusal = m_tunnelFilterError.isEmpty()
            ? DVAngel61IP::classify(datagram, m_settings.m_angel61TunnelMTU, m_tunnelFilter) : DVAngel61IP::NotAllowed;

        // The filter of 3.5, and it is not optional: a host with a route pointed at a tunnel
        // produces neighbour discovery, mDNS and DHCP of its own accord, and any one of them
        // fills 333 bytes a second. The operator's allow list and groups are part of it
        if (refusal != DVAngel61IP::Accepted)
        {
            m_tunnelRefused++;
            m_tunnelRefusedBy[refusal]++;
            refused = true;
            continue;
        }

        // And nothing is sent that the far station said it would not route: on a link this slow,
        // sending something that will only be dropped at the other end is worse than not sending
        if (m_tunnelPeerValid && !DVAngel61IP::routable(datagram, m_tunnelPeer.m_prefixes))
        {
            m_tunnelRefused++;
            m_tunnelUnroutable++;
            refused = true;
            continue;
        }

        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendDatagram::create(packet, keyUp));
        m_tunnelSent++;
        m_tunnelTxBytes += quint64(packet.size());
        m_tunnelTxInTransmission++;
        queued = true;
    }

    if (queued || refused) {
        reportTunnelTraffic();
    }

    // What is owed the peer and nothing else: the key up is for it (an empty datagram)
    if (keyUp && !queued && m_tunnelOwed) {
        m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendDatagram::create(QByteArray(), true));
    }

    // The transmitter keys up for it as it does for a file, and gives the key back when the
    // queue is empty (the processor reports the transmission ended)
    if ((queued || m_tunnelOwed) && !m_settings.m_transmit)
    {
        m_channelAccess.keyed(m_clock.elapsed());
        m_tunnelAutoTransmit = true;
        setTransmit(true);
    }

    m_tunnelOwed = false;
}

bool DSEMod::tunnelKeyUpAllowed(bool datagrams)
{
    // Now, or the timer set for when (a transmission heard: its end asks again)
    const qint64 now = m_clock.elapsed();
    const int64_t at = m_channelAccess.mayKeyAt(now, datagrams);

    if (at < 0) {
        return false;
    }

    if (at > now)
    {
        m_tunnelAccessTimer.start((int) std::min<int64_t>(at - now, 60000));
        return false;
    }

    return true;
}

void DSEMod::updateChannelAccessTiming()
{
    // The spec's windows are the air's; these are measured from the pipe's events, which come a
    // receiver's end detection and a key up later than the air's: VHF's reply window is a second,
    // HF's three (its frames are 240 ms), and a backoff slot is a frame
    DSEModChannelAccess::Timing timing;
    const bool hf = DSEModSettings::isAngel61HF(m_settings.m_mode);
    timing.m_turnaroundMs = 100;
    timing.m_replyWindowMs = hf ? 3000 : 1000;
    timing.m_slotMs = hf ? 240 : 80;
    m_channelAccess.setTiming(timing);
}

void DSEMod::tunnelAdvertiseTimeout()
{
    // The advertise every 30 s while the tunnel is up (3.5), so that a station that came on late
    // learns what this one routes: a transmission for it when none has gone out in that time (the
    // protocol puts it at the head of one that has)
    const qint64 now = m_clock.elapsed();

    if (!m_ipTunnel || (m_ipTunnel->state() != IPTunnel::State::Open) || !DSEModSettings::isAngel61(m_settings.m_mode)
        || !m_thread->isRunning() || m_settings.m_transmit
        || ((m_lastTransmissionEndMs >= 0) && (now - m_lastTransmissionEndMs < DSEModAngel61::TunnelAdvertiseMs))) {
        return;
    }

    m_tunnelOwed = true;
    takeTunnelPackets();
}

void DSEMod::injectTunnelDatagram(const QString& source, const QByteArray& datagram)
{
    if (!m_ipTunnel || (m_ipTunnel->state() != IPTunnel::State::Open)) {
        return;
    }

    // Only from the station this tunnel names: a datagram from anyone else is not this tunnel's
    if (!DVAngel61::sameStation(source.toStdString(), m_settings.m_angel61TunnelPeer.toStdString()))
    {
        m_tunnelRefused++;
        m_tunnelRxRefused++;
        return;
    }

    std::vector<uint8_t> bytes(datagram.begin(), datagram.end());

    if (DVAngel61IP::classify(bytes, m_settings.m_angel61TunnelMTU) != DVAngel61IP::Accepted)
    {
        m_tunnelRefused++;
        m_tunnelRxRefused++;
        return;
    }

    IPTunnel::EnqueueResult result = m_ipTunnel->injectInboundPacket(datagram);

    if (result == IPTunnel::EnqueueResult::Accepted) {
        m_tunnelReceived++;
        m_tunnelRxBytes += quint64(datagram.size());
        reportTunnelTraffic();
    } else {
        m_tunnelRxRefused++;
        qDebug() << "DSEMod::injectTunnelDatagram: the host would not take it:" << (int) result;
    }
}

qint64 DSEMod::angel61FileCapacity() const
{
    // 65 536 blocks of the message field the rate uses (3.4): a data frame's field, HF's a data
    // group's (2.3). "Auto" is not decided until the transmission starts, so tier 0's field is what
    // a file has to fit
    if (!DSEModSettings::isAngel61(m_settings.m_mode)) {
        return 0;
    }

    DSEModProtocol *protocol = DSEModProtocol::create(m_settings.m_mode);
    DSEModAngel61 *angel61 = dynamic_cast<DSEModAngel61*>(protocol);
    DSEModSettings settings = m_settings;

    if (settings.m_angel61Rate == DSEModSettings::DSEModAngel61RateAuto) {
        settings.m_angel61Rate = DSEModSettings::DSEModAngel61RateLow;
    }

    const qint64 capacity = angel61 ? (qint64) 65536 * angel61->fieldBytes(angel61->tierOf(settings)) : 0;
    delete protocol;
    return capacity;
}

bool DSEMod::sendFile(const QString& fileName)
{
    if (fileName.trimmed().isEmpty() || !DSEModSettings::isAngel61(m_settings.m_mode))
    {
        qWarning("DSEMod::sendFile: no file, or not an Angel61 transmission");
        return true;
    }

    QFile file(fileName);

    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning("DSEMod::sendFile: cannot read %s", qPrintable(fileName));
        return true;
    }

    // The 16 bit block index holds 65 536 blocks. Reading that many and sending them would make
    // a transfer that is complete, CRC and all, under the name of a file it is only the front of:
    // a file that does not fit is refused instead, and the transmitter is not keyed for it
    const qint64 capacity = angel61FileCapacity();

    if (file.size() > capacity)
    {
        qWarning("DSEMod::sendFile: %s is %lld bytes, more than the %lld this mode carries",
            qPrintable(fileName), (long long) file.size(), (long long) capacity);
        file.close();
        return true;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty())
    {
        qWarning("DSEMod::sendFile: %s is empty", qPrintable(fileName));
        return true;
    }

    QFileInfo info(fileName);
    qDebug() << "DSEMod::sendFile:" << info.fileName() << data.size() << "bytes";
    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgSendFile::create(info.fileName(), data));

    if (!m_settings.m_transmit)
    {
        m_fileAutoTransmit = true;
        setTransmit(true);
    }

    return true;
}

void DSEMod::updateLinkState()
{
    // The tier the destination last asked of this station decides an "auto" rate; the tier this
    // station asks of it is our receiver's judgement of its transmissions, whoever they were to.
    // A record heard on the other profile does not count: the tiers mean different things there.
    // Stations are matched by identity, base callsign and SSID (ANGEL61.md 2.6): G4ABC/P is G4ABC
    const QString profile = m_settings.m_mode == DSEModSettings::DSEModModeAngel61WB ? "WB"
        : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFNB ? "HF-NB"
        : m_settings.m_mode == DSEModSettings::DSEModModeAngel61HFWB ? "HF-WB" : "NB";
    QString destination = m_settings.m_angel61Destination.trimmed().toUpper();
    QString source = m_settings.m_angel61Source.trimmed().toUpper();
    bool heard = false;
    int requestedTier = 0, sendTier = 0;
    qint64 timestampMs = 0;
    const Angel61Station *record = nullptr;

    // A group - CQ or a talkgroup (2.6) - has no correspondent to ask a tier of: Auto sends at tier 0
    if (!destination.isEmpty() && !DVAngel61::isGroupText(destination.toStdString()))
    {
        for (auto it = m_angel61Stations.cbegin(); it != m_angel61Stations.cend(); ++it)
        {
            // The latest record of that station, whatever prefix or suffix it was heard with
            if (DVAngel61::sameStation(it.key().toStdString(), destination.toStdString())
                && (!record || (it.value().m_heard > record->m_heard))) {
                record = &it.value();
            }
        }
    }

    if (record)
    {
        const Angel61Station& station = *record;
        heard = station.m_heard.isValid() && (station.m_heard.msecsTo(QDateTime::currentDateTime()) < 60000)
            && (station.m_profile == profile);
        requestedTier = heard && DVAngel61::sameStation(station.m_destination.toStdString(), source.toStdString()) ? station.m_requestedTier : 0;
        sendTier = heard ? station.m_sendTier : 0;
        timestampMs = station.m_heard.toMSecsSinceEpoch();
    }

    m_linkHeard = heard;
    m_linkRequestedTier = requestedTier;
    m_linkSendTier = sendTier;
    m_basebandSource->getInputMessageQueue()->push(DSEModSource::MsgLinkState::create(requestedTier, sendTier, heard ? timestampMs : 0));

    if (getMessageQueueToGUI()) {
        getMessageQueueToGUI()->push(MsgReportLinkState::create(heard, requestedTier, sendTier));
    }
}

void DSEMod::handleFeatureRemoved(int featureSetIndex, Feature *feature)
{
    if (featureSetIndex != 0) {
        return;
    }

    // The connected feature by itself, not by an index that may have gone stale; the settings keep
    // asking for it (handleFeatureAdded connects it again when it comes back)
    if (feature == m_connectedAMBEFeature)
    {
        m_connectedAMBEFeature = nullptr;
        m_basebandSource->setAMBEFeature(nullptr, nullptr);
    }

    if (m_availableAMBEFeatures.contains(feature))
    {
        m_availableAMBEFeatures.remove(feature);
        notifyUpdateAMBEFeatures();
    }
}

void DSEMod::applySettings(const QStringList& settingsKeys, const DSEModSettings& settings, bool force)
{
    if (m_basebandSource->getProcessor()->isAPIActive()) {
        if (settingsKeys.contains("transmit") && !settings.m_transmit) {
            auto processor = m_basebandSource->getProcessor();
            QMetaObject::invokeMethod(processor, [processor]() { processor->cancelAPI("Stopped by local control"); }, Qt::BlockingQueuedConnection);
        } else {
            if (getMessageQueueToGUI()) { getMessageQueueToGUI()->push(MsgConfigureDSEMod::create({}, m_settings, true)); }
            return;
        }
    }
    qDebug() << "DSEMod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex"))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSourceAPI(this);
            m_deviceAPI->removeChannelSource(this, false, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSource(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSourceAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    if (settingsKeys.contains("transmit") && !settings.m_transmit) {
        m_dtmfAutoTransmit = false;
    }

    {
        // The tunnel follows the settings: opened, reconfigured or closed, and the protocol told
        // what to advertise. Other modes pause its radio transport but retain the elevated helper.
        // (forced, as a preset's: the whole of the new settings; an empty key list applied nothing,
        // and the tunnel and the link state went on the old settings - a tunnel turned off stayed open)
        DSEModSettings current = m_settings;
        if (force) { current = settings; } else { current.applySettings(settingsKeys, settings); }
        bool tunnelKeys = force;

        if (force || settingsKeys.contains("angel61TunnelAllow") || settingsKeys.contains("angel61TunnelGroups")) {
            updateTunnelFilter(current);
        }

        for (const char *key : { "angel61Tunnel", "angel61TunnelPeer", "angel61TunnelInterface",
            "angel61TunnelAddress", "angel61TunnelRoutes", "angel61TunnelMTU", "mode", "angel61Source" }) {
            tunnelKeys = tunnelKeys || settingsKeys.contains(key);
        }

        if (tunnelKeys)
        {
            DSEModSettings previous = m_settings;
            m_settings = current;
            updateTunnel(settingsKeys, force);
            m_settings = previous;
        }
    }

    // The speech file is opened when its name changes (and at the first, forced, application: the
    // name restored with the settings)
    bool fileChanged = (settingsKeys.contains("audioFileName") && (settings.m_audioFileName != m_settings.m_audioFileName)) || force;

    if (settingsKeys.contains("angel61Destination") || settingsKeys.contains("angel61Source") || settingsKeys.contains("mode")
        || (settingsKeys.contains("transmit") && settings.m_transmit) || force)
    {
        // Before the key up reaches the processor: the destination's rate request as of now, on
        // the profile of the mode (a record heard on the other one does not count)
        // (forced, as a preset's: the whole of the new settings; an empty key list applied nothing,
        // and the tunnel and the link state went on the old settings - a tunnel turned off stayed open)
        DSEModSettings current = m_settings;
        if (force) { current = settings; } else { current.applySettings(settingsKeys, settings); }
        m_settings.m_angel61Destination = current.m_angel61Destination;
        m_settings.m_angel61Source = current.m_angel61Source;
        m_settings.m_mode = current.m_mode;
        updateLinkState();
    }

    DSEModBaseband::MsgConfigureDSEModBaseband *msg = DSEModBaseband::MsgConfigureDSEModBaseband::create(settingsKeys, settings, force);
    m_basebandSource->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("connectAMBE") || settingsKeys.contains("ambeFeatureIndex") || force) {
        applyAMBEFeature(settings);
    }

    if (settingsKeys.contains("angel61Demod") || force) {
        applyDemodulatorSelection(settings);
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
            (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
            (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
            (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
            (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
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

    if (fileChanged) {
        openFileStream();
    }
}

QByteArray DSEMod::serialize() const
{
    return m_settings.serialize();
}

bool DSEMod::deserialize(const QByteArray& data)
{
    bool success = true;

    if (!m_settings.deserialize(data))
    {
        m_settings.resetToDefaults();
        success = false;
    }

    m_settings.m_transmit = false; // never start transmitting from a stored setting
    MsgConfigureDSEMod *msg = MsgConfigureDSEMod::create(QStringList(), m_settings, true);
    m_inputMessageQueue.push(msg);

    return success;
}

int DSEMod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setDseModSettings(new SWGSDRangel::SWGDSEModSettings());
    response.getDseModSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getDseModSettings()->getCwKeyer();
    const CWKeyerSettings& cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
    CWKeyer::webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    return 200;
}

int DSEMod::webapiWorkspaceGet(
    SWGSDRangel::SWGWorkspaceInfo& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DSEMod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    if (m_basebandSource->getProcessor()->isAPIActive()) {
        errorMessage = "Settings are latched during an API session; finish or abort it first"; return 409;
    }
    DSEModSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    if (channelSettingsKeys.contains("cwKeyer"))
    {
        SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = response.getDseModSettings()->getCwKeyer();
        CWKeyerSettings cwKeyerSettings = m_basebandSource->getCWKeyer().getSettings();
        CWKeyer::webapiSettingsPutPatch(channelSettingsKeys, cwKeyerSettings, apiCwKeyerSettings);

        CWKeyer::MsgConfigureCWKeyer *msgCwKeyer = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
        m_basebandSource->getCWKeyer().getInputMessageQueue()->push(msgCwKeyer);

        if (m_guiMessageQueue) // forward to GUI if any
        {
            CWKeyer::MsgConfigureCWKeyer *msgCwKeyerToGUI = CWKeyer::MsgConfigureCWKeyer::create(cwKeyerSettings, force);
            m_guiMessageQueue->push(msgCwKeyerToGUI);
        }
    }

    MsgConfigureDSEMod *msg = MsgConfigureDSEMod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDSEMod *msgToGUI = MsgConfigureDSEMod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DSEMod::webapiUpdateChannelSettings(
    DSEModSettings& settings,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response)
{
    SWGSDRangel::SWGDSEModSettings *swg = response.getDseModSettings();

    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = swg->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = swg->getRfBandwidth();
    }
    // 0 to 12; another value is ignored
    if (channelSettingsKeys.contains("mode") && (swg->getMode() >= 0) && (swg->getMode() <= (int) DSEModSettings::DSEModModeAngel61HFWB)) {
        settings.m_mode = DSEModSettings::modeOf(swg->getMode());
    }
    if (channelSettingsKeys.contains("transmit")) {
        settings.m_transmit = swg->getTransmit() != 0;
    }
    if (channelSettingsKeys.contains("invert")) {
        settings.m_invert = swg->getInvert() != 0;
    }
    if (channelSettingsKeys.contains("modAFInput")) {
        settings.m_modAFInput = (DSEModSettings::DSEModInputAF) swg->getModAfInput();
    }
    if (channelSettingsKeys.contains("toneFrequency")) {
        settings.m_toneFrequency = swg->getToneFrequency();
    }
    if (channelSettingsKeys.contains("volumeFactor")) {
        settings.m_volumeFactor = swg->getVolumeFactor();
    }
    if (channelSettingsKeys.contains("channelMute")) {
        settings.m_channelMute = swg->getChannelMute() != 0;
    }
    if (channelSettingsKeys.contains("playLoop")) {
        settings.m_playLoop = swg->getPlayLoop() != 0;
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *swg->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("vocoder")) {
        settings.m_vocoder = (DSEModSettings::DSEModVocoderType) swg->getVocoder();
    }
    if (channelSettingsKeys.contains("ambeFeatureIndex")) {
        settings.m_ambeFeatureIndex = swg->getAmbeFeatureIndex();
    }
    if (channelSettingsKeys.contains("connectAMBE")) {
        settings.m_connectAMBE = swg->getConnectAmbe() != 0;
    }
    if (channelSettingsKeys.contains("vocoderGain")) {
        settings.m_vocoderGain = swg->getVocoderGain();
    }
    if (channelSettingsKeys.contains("toneDetect")) {
        settings.m_toneDetect = swg->getToneDetect() != 0;
    }
    if (channelSettingsKeys.contains("noiseSuppress")) {
        settings.m_noiseSuppress = swg->getNoiseSuppress() != 0;
    }
    if (channelSettingsKeys.contains("dtmfSequence")) {
        settings.m_dtmfSequence = *swg->getDtmfSequence();
    }
    if (channelSettingsKeys.contains("dstarMyCall")) {
        settings.m_dstarMyCall = *swg->getDstarMyCall();
    }
    if (channelSettingsKeys.contains("dstarMySuffix")) {
        settings.m_dstarMySuffix = *swg->getDstarMySuffix();
    }
    if (channelSettingsKeys.contains("dstarUrCall")) {
        settings.m_dstarUrCall = *swg->getDstarUrCall();
    }
    if (channelSettingsKeys.contains("dstarRpt1")) {
        settings.m_dstarRpt1 = *swg->getDstarRpt1();
    }
    if (channelSettingsKeys.contains("dstarRpt2")) {
        settings.m_dstarRpt2 = *swg->getDstarRpt2();
    }
    if (channelSettingsKeys.contains("dstarText")) {
        settings.m_dstarText = *swg->getDstarText();
    }
    if (channelSettingsKeys.contains("dstarFlag1")) {
        settings.m_dstarFlag1 = swg->getDstarFlag1();
    }
    if (channelSettingsKeys.contains("dstarFlag2")) {
        settings.m_dstarFlag2 = swg->getDstarFlag2();
    }
    if (channelSettingsKeys.contains("dstarFlag3")) {
        settings.m_dstarFlag3 = swg->getDstarFlag3();
    }
    if (channelSettingsKeys.contains("dmrColorCode")) {
        settings.m_dmrColorCode = swg->getDmrColorCode();
    }
    if (channelSettingsKeys.contains("dmrSourceId")) {
        settings.m_dmrSourceId = swg->getDmrSourceId();
    }
    if (channelSettingsKeys.contains("dmrDestinationId")) {
        settings.m_dmrDestinationId = swg->getDmrDestinationId();
    }
    if (channelSettingsKeys.contains("dmrGroupCall")) {
        settings.m_dmrGroupCall = swg->getDmrGroupCall() != 0;
    }
    if (channelSettingsKeys.contains("dmrSlot")) {
        settings.m_dmrSlot = swg->getDmrSlot();
    }
    if (channelSettingsKeys.contains("dmrDirect")) {
        settings.m_dmrDirect = swg->getDmrDirect() != 0;
    }
    if (channelSettingsKeys.contains("dmrFid")) {
        settings.m_dmrFid = swg->getDmrFid();
    }
    if (channelSettingsKeys.contains("dmrServiceOptions")) {
        settings.m_dmrServiceOptions = swg->getDmrServiceOptions();
    }
    if (channelSettingsKeys.contains("ysfCallsign")) {
        settings.m_ysfCallsign = *swg->getYsfCallsign();
    }
    if (channelSettingsKeys.contains("ysfDestination")) {
        settings.m_ysfDestination = *swg->getYsfDestination();
    }
    if (channelSettingsKeys.contains("ysfDownlink")) {
        settings.m_ysfDownlink = *swg->getYsfDownlink();
    }
    if (channelSettingsKeys.contains("ysfUplink")) {
        settings.m_ysfUplink = *swg->getYsfUplink();
    }
    if (channelSettingsKeys.contains("ysfMode")) {
        settings.m_ysfMode = (DSEModSettings::DSEModYSFMode) std::max((int) DSEModSettings::DSEModYSFModeVD2, std::min((int) DSEModSettings::DSEModYSFModeVW, swg->getYsfMode()));
    }
    if (channelSettingsKeys.contains("nxdnRan")) {
        settings.m_nxdnRan = swg->getNxdnRan();
    }
    if (channelSettingsKeys.contains("nxdnSourceId")) {
        settings.m_nxdnSourceId = swg->getNxdnSourceId();
    }
    if (channelSettingsKeys.contains("nxdnDestinationId")) {
        settings.m_nxdnDestinationId = swg->getNxdnDestinationId();
    }
    if (channelSettingsKeys.contains("nxdnGroupCall")) {
        settings.m_nxdnGroupCall = swg->getNxdnGroupCall() != 0;
    }
    if (channelSettingsKeys.contains("dpmrColorCode")) {
        settings.m_dpmrColorCode = swg->getDpmrColorCode();
    }
    if (channelSettingsKeys.contains("dpmrOwnId")) {
        settings.m_dpmrOwnId = swg->getDpmrOwnId();
    }
    if (channelSettingsKeys.contains("dpmrCalledId")) {
        settings.m_dpmrCalledId = swg->getDpmrCalledId();
    }
    if (channelSettingsKeys.contains("p25Nac")) {
        settings.m_p25Nac = swg->getP25Nac();
    }
    if (channelSettingsKeys.contains("p25TalkGroup")) {
        settings.m_p25TalkGroup = swg->getP25TalkGroup();
    }
    if (channelSettingsKeys.contains("p25SourceId")) {
        settings.m_p25SourceId = swg->getP25SourceId();
    }
    if (channelSettingsKeys.contains("p25p2Wacn")) {
        settings.m_p25p2Wacn = swg->getP25p2Wacn();
    }
    if (channelSettingsKeys.contains("p25p2Sysid")) {
        settings.m_p25p2Sysid = swg->getP25p2Sysid();
    }
    if (channelSettingsKeys.contains("angel61Rate")) {
        // 0 Auto, 1 Low (tier 0), 2 High (tier 2), 3 Medium (tier 1), 4 Tier3
        settings.m_angel61Rate = (DSEModSettings::DSEModAngel61Rate) std::max(0, std::min((int) DSEModSettings::DSEModAngel61RateTier3, swg->getAngel61Rate()));
    }
    if (channelSettingsKeys.contains("angel61Source")) {
        settings.m_angel61Source = *swg->getAngel61Source();
    }
    if (channelSettingsKeys.contains("angel61Destination")) {
        settings.m_angel61Destination = *swg->getAngel61Destination();
    }
    if (channelSettingsKeys.contains("angel61Path")) {
        settings.m_angel61Path = *swg->getAngel61Path();
    }
    if (channelSettingsKeys.contains("angel61Message")) {
        settings.m_angel61Message = *swg->getAngel61Message();
    }
    if (channelSettingsKeys.contains("sendPosition")) {
        settings.m_sendPosition = swg->getSendPosition() != 0;
    }
    if (channelSettingsKeys.contains("angel61Demod")) {
        settings.m_angel61Demod = *swg->getAngel61Demod();
    }
    if (channelSettingsKeys.contains("angel61Symbol")) {
        settings.m_angel61Symbol = *swg->getAngel61Symbol();
    }
    if (channelSettingsKeys.contains("angel61FileName")) {
        settings.m_angel61FileName = *swg->getAngel61FileName();
    }
    if (channelSettingsKeys.contains("m17Source")) {
        settings.m_m17Source = *swg->getM17Source();
    }
    if (channelSettingsKeys.contains("m17Destination")) {
        settings.m_m17Destination = *swg->getM17Destination();
    }
    if (channelSettingsKeys.contains("m17CAN")) {
        settings.m_m17CAN = std::max(0, std::min(15, swg->getM17Can()));
    }
    if (channelSettingsKeys.contains("m17Message")) {
        settings.m_m17Message = *swg->getM17Message();
    }
    if (channelSettingsKeys.contains("codecModelDirectory")) {
        settings.m_codecModelDirectory = *swg->getCodecModelDirectory();
    }
    if (channelSettingsKeys.contains("audioFileName")) {
        settings.m_audioFileName = *swg->getAudioFileName();
    }
    if (channelSettingsKeys.contains("angel61Tunnel")) {
        settings.m_angel61Tunnel = swg->getAngel61Tunnel() != 0;
    }
    if (channelSettingsKeys.contains("angel61TunnelPeer")) {
        settings.m_angel61TunnelPeer = *swg->getAngel61TunnelPeer();
    }
    if (channelSettingsKeys.contains("angel61TunnelInterface")) {
        settings.m_angel61TunnelInterface = *swg->getAngel61TunnelInterface();
    }
    if (channelSettingsKeys.contains("angel61TunnelAddress")) {
        settings.m_angel61TunnelAddress = *swg->getAngel61TunnelAddress();
    }
    if (channelSettingsKeys.contains("angel61TunnelRoutes")) {
        settings.m_angel61TunnelRoutes = *swg->getAngel61TunnelRoutes();
    }
    if (channelSettingsKeys.contains("angel61TunnelAllow") && swg->getAngel61TunnelAllow()) {
        settings.m_angel61TunnelAllow = *swg->getAngel61TunnelAllow();
    }
    if (channelSettingsKeys.contains("angel61TunnelGroups") && swg->getAngel61TunnelGroups()) {
        settings.m_angel61TunnelGroups = *swg->getAngel61TunnelGroups();
    }
    if (channelSettingsKeys.contains("angel61TunnelMTU")) {
        settings.m_angel61TunnelMTU = std::max(68, std::min(1500, swg->getAngel61TunnelMtu()));
    }
    if (channelSettingsKeys.contains("feedbackAudioDeviceName")) {
        settings.m_feedbackAudioDeviceName = *swg->getFeedbackAudioDeviceName();
    }
    if (channelSettingsKeys.contains("feedbackVolumeFactor")) {
        settings.m_feedbackVolumeFactor = std::max(0.0f, std::min(1.0f, swg->getFeedbackVolumeFactor()));
    }
    if (channelSettingsKeys.contains("feedbackAudioEnable")) {
        settings.m_feedbackAudioEnable = swg->getFeedbackAudioEnable() != 0;
    }
    if (channelSettingsKeys.contains("spectrumAudio")) {
        settings.m_spectrumAudio = swg->getSpectrumAudio() != 0;
    }
    if (channelSettingsKeys.contains("frameSessionMaxDurationMs")) {
        settings.m_frameSessionMaxDurationMs = std::max(DSEModTxSession::MinFrameMaxDurationMs,
            std::min(DSEModTxSession::MaxFrameMaxDurationMs, swg->getFrameSessionMaxDurationMs()));
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = swg->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *swg->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = swg->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = swg->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *swg->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = swg->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = swg->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = swg->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, swg->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, swg->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, swg->getRollupState());
    }
}

int DSEMod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setDseModReport(new SWGSDRangel::SWGDSEModReport());
    response.getDseModReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

// A string of the response: the request's, when a PUT or PATCH left it there, assigned (set anew, the
// request's leaked)
template <typename Set>
static void assignString(QString *current, const QString& value, Set set)
{
    if (current) {
        *current = value;
    } else {
        set(new QString(value));
    }
}

void DSEMod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DSEModSettings& settings)
{
    SWGSDRangel::SWGDSEModSettings *swg = response.getDseModSettings();

    swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    swg->setRfBandwidth(settings.m_rfBandwidth);
    swg->setMode((int) settings.m_mode);
    swg->setTransmit(settings.m_transmit ? 1 : 0);
    swg->setInvert(settings.m_invert ? 1 : 0);
    swg->setModAfInput((int) settings.m_modAFInput);
    swg->setToneFrequency(settings.m_toneFrequency);
    swg->setVolumeFactor(settings.m_volumeFactor);
    swg->setChannelMute(settings.m_channelMute ? 1 : 0);
    swg->setPlayLoop(settings.m_playLoop ? 1 : 0);

    if (swg->getAudioDeviceName()) {
        *swg->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        swg->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    swg->setVocoder((int) settings.m_vocoder);
    swg->setAmbeFeatureIndex(settings.m_ambeFeatureIndex);
    swg->setConnectAmbe(settings.m_connectAMBE ? 1 : 0);
    swg->setVocoderGain(settings.m_vocoderGain);
    swg->setToneDetect(settings.m_toneDetect ? 1 : 0);
    swg->setNoiseSuppress(settings.m_noiseSuppress ? 1 : 0);

    if (swg->getDtmfSequence()) {
        *swg->getDtmfSequence() = settings.m_dtmfSequence;
    } else {
        swg->setDtmfSequence(new QString(settings.m_dtmfSequence));
    }

    if (swg->getDstarMyCall()) {
        *swg->getDstarMyCall() = settings.m_dstarMyCall;
    } else {
        swg->setDstarMyCall(new QString(settings.m_dstarMyCall));
    }

    if (swg->getDstarMySuffix()) {
        *swg->getDstarMySuffix() = settings.m_dstarMySuffix;
    } else {
        swg->setDstarMySuffix(new QString(settings.m_dstarMySuffix));
    }

    if (swg->getDstarUrCall()) {
        *swg->getDstarUrCall() = settings.m_dstarUrCall;
    } else {
        swg->setDstarUrCall(new QString(settings.m_dstarUrCall));
    }

    if (swg->getDstarRpt1()) {
        *swg->getDstarRpt1() = settings.m_dstarRpt1;
    } else {
        swg->setDstarRpt1(new QString(settings.m_dstarRpt1));
    }

    if (swg->getDstarRpt2()) {
        *swg->getDstarRpt2() = settings.m_dstarRpt2;
    } else {
        swg->setDstarRpt2(new QString(settings.m_dstarRpt2));
    }

    if (swg->getDstarText()) {
        *swg->getDstarText() = settings.m_dstarText;
    } else {
        swg->setDstarText(new QString(settings.m_dstarText));
    }

    swg->setDstarFlag1(settings.m_dstarFlag1);
    swg->setDstarFlag2(settings.m_dstarFlag2);
    swg->setDstarFlag3(settings.m_dstarFlag3);
    swg->setDmrColorCode(settings.m_dmrColorCode);
    swg->setDmrSourceId(settings.m_dmrSourceId);
    swg->setDmrDestinationId(settings.m_dmrDestinationId);
    swg->setDmrGroupCall(settings.m_dmrGroupCall ? 1 : 0);
    swg->setDmrSlot(settings.m_dmrSlot);
    swg->setDmrDirect(settings.m_dmrDirect ? 1 : 0);
    swg->setDmrFid(settings.m_dmrFid);
    swg->setDmrServiceOptions(settings.m_dmrServiceOptions);
    if (swg->getYsfCallsign()) {
        *swg->getYsfCallsign() = settings.m_ysfCallsign;
    } else {
        swg->setYsfCallsign(new QString(settings.m_ysfCallsign));
    }
    if (swg->getYsfDestination()) {
        *swg->getYsfDestination() = settings.m_ysfDestination;
    } else {
        swg->setYsfDestination(new QString(settings.m_ysfDestination));
    }
    if (swg->getYsfDownlink()) {
        *swg->getYsfDownlink() = settings.m_ysfDownlink;
    } else {
        swg->setYsfDownlink(new QString(settings.m_ysfDownlink));
    }
    if (swg->getYsfUplink()) {
        *swg->getYsfUplink() = settings.m_ysfUplink;
    } else {
        swg->setYsfUplink(new QString(settings.m_ysfUplink));
    }
    swg->setYsfMode((int) settings.m_ysfMode);
    swg->setNxdnRan(settings.m_nxdnRan);
    swg->setNxdnSourceId(settings.m_nxdnSourceId);
    swg->setNxdnDestinationId(settings.m_nxdnDestinationId);
    swg->setNxdnGroupCall(settings.m_nxdnGroupCall ? 1 : 0);
    swg->setDpmrColorCode(settings.m_dpmrColorCode);
    swg->setDpmrOwnId(settings.m_dpmrOwnId);
    swg->setDpmrCalledId(settings.m_dpmrCalledId);
    swg->setP25Nac(settings.m_p25Nac);
    swg->setP25TalkGroup(settings.m_p25TalkGroup);
    swg->setP25SourceId(settings.m_p25SourceId);
    swg->setP25p2Wacn(settings.m_p25p2Wacn);
    swg->setP25p2Sysid(settings.m_p25p2Sysid);
    swg->setAngel61Rate((int) settings.m_angel61Rate);
    assignString(swg->getAngel61Source(), settings.m_angel61Source, [swg](QString *v) { swg->setAngel61Source(v); });
    assignString(swg->getAngel61Destination(), settings.m_angel61Destination, [swg](QString *v) { swg->setAngel61Destination(v); });
    assignString(swg->getAngel61Path(), settings.m_angel61Path, [swg](QString *v) { swg->setAngel61Path(v); });
    assignString(swg->getAngel61Message(), settings.m_angel61Message, [swg](QString *v) { swg->setAngel61Message(v); });
    swg->setSendPosition(settings.m_sendPosition ? 1 : 0);
    assignString(swg->getAngel61Demod(), settings.m_angel61Demod, [swg](QString *v) { swg->setAngel61Demod(v); });
    assignString(swg->getAngel61Symbol(), settings.m_angel61Symbol, [swg](QString *v) { swg->setAngel61Symbol(v); });
    assignString(swg->getAngel61FileName(), settings.m_angel61FileName, [swg](QString *v) { swg->setAngel61FileName(v); });
    assignString(swg->getM17Source(), settings.m_m17Source, [swg](QString *v) { swg->setM17Source(v); });
    assignString(swg->getM17Destination(), settings.m_m17Destination, [swg](QString *v) { swg->setM17Destination(v); });
    swg->setM17Can(settings.m_m17CAN);
    assignString(swg->getM17Message(), settings.m_m17Message, [swg](QString *v) { swg->setM17Message(v); });
    assignString(swg->getCodecModelDirectory(), settings.m_codecModelDirectory, [swg](QString *v) { swg->setCodecModelDirectory(v); });
    assignString(swg->getAudioFileName(), settings.m_audioFileName, [swg](QString *v) { swg->setAudioFileName(v); });
    swg->setAngel61Tunnel(settings.m_angel61Tunnel ? 1 : 0);
    assignString(swg->getAngel61TunnelPeer(), settings.m_angel61TunnelPeer, [swg](QString *v) { swg->setAngel61TunnelPeer(v); });
    assignString(swg->getAngel61TunnelInterface(), settings.m_angel61TunnelInterface, [swg](QString *v) { swg->setAngel61TunnelInterface(v); });
    assignString(swg->getAngel61TunnelAddress(), settings.m_angel61TunnelAddress, [swg](QString *v) { swg->setAngel61TunnelAddress(v); });
    assignString(swg->getAngel61TunnelRoutes(), settings.m_angel61TunnelRoutes, [swg](QString *v) { swg->setAngel61TunnelRoutes(v); });
    assignString(swg->getAngel61TunnelAllow(), settings.m_angel61TunnelAllow, [swg](QString *v) { swg->setAngel61TunnelAllow(v); });
    assignString(swg->getAngel61TunnelGroups(), settings.m_angel61TunnelGroups, [swg](QString *v) { swg->setAngel61TunnelGroups(v); });
    swg->setAngel61TunnelMtu(settings.m_angel61TunnelMTU);
    assignString(swg->getFeedbackAudioDeviceName(), settings.m_feedbackAudioDeviceName, [swg](QString *v) { swg->setFeedbackAudioDeviceName(v); });
    swg->setFeedbackVolumeFactor(settings.m_feedbackVolumeFactor);
    swg->setFeedbackAudioEnable(settings.m_feedbackAudioEnable ? 1 : 0);
    swg->setSpectrumAudio(settings.m_spectrumAudio ? 1 : 0);
    swg->setFrameSessionMaxDurationMs(settings.m_frameSessionMaxDurationMs);
    swg->setRgbColor(settings.m_rgbColor);

    if (swg->getTitle()) {
        *swg->getTitle() = settings.m_title;
    } else {
        swg->setTitle(new QString(settings.m_title));
    }

    swg->setStreamIndex(settings.m_streamIndex);
    swg->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (swg->getReverseApiAddress()) {
        *swg->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        swg->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    swg->setReverseApiPort(settings.m_reverseAPIPort);
    swg->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    swg->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (!swg->getCwKeyer()) {
        swg->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings);
    }

    if (settings.m_spectrumGUI)
    {
        if (swg->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(swg->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            swg->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (swg->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(swg->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            swg->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (swg->getRollupState())
        {
            settings.m_rollupState->formatTo(swg->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            swg->setRollupState(swgRollupState);
        }
    }
}

namespace {

// The processor and the transmit session report in JSON. The report's classes are generated from
// DSEMod.yaml: they are filled through their setters, which is what marks a field as present (the
// generated fromJsonObject() does not). Only the fields the JSON has are set.

QString *swgString(const QJsonObject& json, const char *key)
{
    return new QString(json[key].toString());
}

QList<QString*> *swgStringList(const QJsonArray& array)
{
    QList<QString*> *list = new QList<QString*>();

    for (const QJsonValue& value : array) {
        list->append(new QString(value.toString()));
    }

    return list;
}

template <typename T>
void setSessionFields(T *swg, const QJsonObject& json)
{
    if (json.contains("sessionId")) {
        swg->setSessionId(swgString(json, "sessionId"));
    }
    if (json.contains("input")) {
        swg->setInput(swgString(json, "input"));
    }
    if (json.contains("state")) {
        swg->setState(swgString(json, "state"));
    }
    if (json.contains("protocol")) {
        swg->setProtocol(json["protocol"].toInt());
    }
    if (json.contains("sourceProtocol")) {
        swg->setSourceProtocol(json["sourceProtocol"].toInt());
    }
    if (json.contains("source")) {
        swg->setSource(swgString(json, "source"));
    }
    if (json.contains("destination")) {
        swg->setDestination(swgString(json, "destination"));
    }
    if (json.contains("via")) {
        swg->setVia(swgString(json, "via"));
    }
    if (json.contains("nextSequence")) {
        swg->setNextSequence(json["nextSequence"].toInt());
    }
    if (json.contains("accepted")) {
        swg->setAccepted(json["accepted"].toDouble());
    }
    if (json.contains("consumed")) {
        swg->setConsumed(json["consumed"].toDouble());
    }
    if (json.contains("maxDurationMs")) {
        swg->setMaxDurationMs(json["maxDurationMs"].toInt());
    }
    if (json.contains("queuedSamples")) {
        swg->setQueuedSamples(json["queuedSamples"].toInt());
    }
    if (json.contains("queuedFrames")) {
        swg->setQueuedFrames(json["queuedFrames"].toInt());
    }
    if (json.contains("availableSamples")) {
        swg->setAvailableSamples(json["availableSamples"].toInt());
    }
    if (json.contains("availableFrames")) {
        swg->setAvailableFrames(json["availableFrames"].toInt());
    }
    if (json.contains("lastError")) {
        swg->setLastError(swgString(json, "lastError"));
    }
}

}

void DSEMod::webapiFormatTransmitStatus(SWGSDRangel::SWGDSEModTransmitStatus *swg, const QJsonObject& json)
{
    setSessionFields(swg, json);

    if (json.contains("pendingEncoderFrames")) {
        swg->setPendingEncoderFrames(json["pendingEncoderFrames"].toInt());
    }
    if (json.contains("encodedQueueFrames")) {
        swg->setEncodedQueueFrames(json["encodedQueueFrames"].toInt());
    }
    if (json.contains("transcodeDecodedFrames")) {
        swg->setTranscodeDecodedFrames(json["transcodeDecodedFrames"].toInt());
    }
    if (json.contains("relayInvalidFrames")) {
        swg->setRelayInvalidFrames(json["relayInvalidFrames"].toInt());
    }
    if (json.contains("relayHoldFrames")) {
        swg->setRelayHoldFrames(json["relayHoldFrames"].toInt());
    }

    if (json.contains("history"))
    {
        QList<SWGSDRangel::SWGDSEModTransmitSession*> *history = new QList<SWGSDRangel::SWGDSEModTransmitSession*>();

        for (const QJsonValue& value : json["history"].toArray())
        {
            SWGSDRangel::SWGDSEModTransmitSession *session = new SWGSDRangel::SWGDSEModTransmitSession();
            setSessionFields(session, value.toObject());
            history->append(session);
        }

        swg->setHistory(history);
    }

    if (json.contains("messages"))
    {
        QList<SWGSDRangel::SWGDSEModMessageStatus*> *messages = new QList<SWGSDRangel::SWGDSEModMessageStatus*>();

        for (const QJsonValue& value : json["messages"].toArray())
        {
            const QJsonObject item = value.toObject();
            SWGSDRangel::SWGDSEModMessageStatus *message = new SWGSDRangel::SWGDSEModMessageStatus();

            if (item.contains("requestId")) {
                message->setRequestId(swgString(item, "requestId"));
            }
            if (item.contains("state")) {
                message->setState(swgString(item, "state"));
            }
            if (item.contains("error")) {
                message->setError(swgString(item, "error"));
            }

            messages->append(message);
        }

        swg->setMessages(messages);
    }
}

void DSEMod::webapiFormatCapabilities(SWGSDRangel::SWGDSEModCapabilities *swg, const QJsonObject& json)
{
    if (json.contains("pcmEncoding")) {
        swg->setPcmEncoding(swgString(json, "pcmEncoding"));
    }
    if (json.contains("pcmChannels")) {
        swg->setPcmChannels(json["pcmChannels"].toInt());
    }
    if (json.contains("pcmSampleRates"))
    {
        QList<qint32> *rates = new QList<qint32>();

        for (const QJsonValue& value : json["pcmSampleRates"].toArray()) {
            rates->append(value.toInt());
        }

        swg->setPcmSampleRates(rates);
    }
    if (json.contains("maxPCMChunkMs")) {
        swg->setMaxPcmChunkMs(json["maxPCMChunkMs"].toInt());
    }
    if (json.contains("maxPCMQueueMs")) {
        swg->setMaxPcmQueueMs(json["maxPCMQueueMs"].toInt());
    }
    if (json.contains("maxFrameBatch")) {
        swg->setMaxFrameBatch(json["maxFrameBatch"].toInt());
    }
    if (json.contains("maxQueuedFrames")) {
        swg->setMaxQueuedFrames(json["maxQueuedFrames"].toInt());
    }
    if (json.contains("idleTimeoutMs")) {
        swg->setIdleTimeoutMs(json["idleTimeoutMs"].toInt());
    }
    if (json.contains("pcmMaxDurationMs")) {
        swg->setPcmMaxDurationMs(json["pcmMaxDurationMs"].toInt());
    }
    if (json.contains("frameMaxDurationMs")) {
        swg->setFrameMaxDurationMs(json["frameMaxDurationMs"].toInt());
    }
    if (json.contains("minFrameMaxDurationMs")) {
        swg->setMinFrameMaxDurationMs(json["minFrameMaxDurationMs"].toInt());
    }
    if (json.contains("maxFrameMaxDurationMs")) {
        swg->setMaxFrameMaxDurationMs(json["maxFrameMaxDurationMs"].toInt());
    }
    if (json.contains("frameFormats")) {
        swg->setFrameFormats(swgStringList(json["frameFormats"].toArray()));
    }
    if (json.contains("angel61NetworkInput")) {
        swg->setAngel61NetworkInput(json["angel61NetworkInput"].toBool());
    }
    if (json.contains("angel61TranscodeAvailable")) {
        swg->setAngel61TranscodeAvailable(json["angel61TranscodeAvailable"].toBool());
    }
    if (json.contains("angel61TranscodeError")) {
        swg->setAngel61TranscodeError(swgString(json, "angel61TranscodeError"));
    }
    if (json.contains("messageServices")) {
        swg->setMessageServices(swgStringList(json["messageServices"].toArray()));
    }
}

void DSEMod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    auto processor = m_basebandSource->getProcessor();
    QJsonObject status;
    QMetaObject::invokeMethod(processor, [&]() { status = processor->getAPIReport(); }, Qt::BlockingQueuedConnection);
    SWGSDRangel::SWGDSEModTransmitStatus *swgStatus = new SWGSDRangel::SWGDSEModTransmitStatus();
    webapiFormatTransmitStatus(swgStatus, status);
    delete response.getDseModReport()->getTransmitStatus();  // (the empty one of the report's init())
    response.getDseModReport()->setTransmitStatus(swgStatus);
    QJsonObject capabilities = DSEModTxSession::capabilities(m_settings);
    // Network calls are relayed by truncating and repacking, without a vocoder (ANGEL61.md 3.1.1): only
    // an Opus call relayed at a V1 tier needs the V1 encoder, which that call checks
    const bool relayAvailable = DSEModSettings::isAngel61(m_settings.m_mode);
    capabilities["angel61TranscodeAvailable"] = relayAvailable;
    if (!relayAvailable) {
        capabilities["angel61TranscodeError"] = "Selected DSEMod mode is not Angel61";
    }
    SWGSDRangel::SWGDSEModCapabilities *swgCapabilities = new SWGSDRangel::SWGDSEModCapabilities();
    webapiFormatCapabilities(swgCapabilities, capabilities);
    delete response.getDseModReport()->getCapabilities();
    response.getDseModReport()->setCapabilities(swgCapabilities);
    SWGSDRangel::SWGDSEModReport *report = response.getDseModReport();
    report->setChannelPowerDb(CalcDb::dbPower(getMagSq()));
    report->setAudioSampleRate(m_basebandSource->getAudioSampleRate());
    report->setChannelSampleRate(m_basebandSource->getChannelSampleRate());
    report->setTransmitting(isTransmitting() ? 1 : 0);
    report->setVocoderReady(isVocoderReady() ? 1 : 0);
    report->setEncodedFrames(getEncodedFrameCount());
    report->setUnderrunFrames(getUnderrunCount());
    report->setOverrunFrames(getOverrunCount());
    report->setTrimFrames(getTrimCount());
    report->setTxFrames(getTxFrameCount());
    report->setAudioUnderruns(getAudioUnderrunCount());
    report->setAudioOverruns(getAudioOverrunCount());
}

void DSEMod::webapiReverseSendSettings(const QStringList& channelSettingsKeys, const DSEModSettings& settings, bool force)
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

void DSEMod::webapiReverseSendCWSettings(const CWKeyerSettings& cwKeyerSettings)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setDseModSettings(new SWGSDRangel::SWGDSEModSettings());
    SWGSDRangel::SWGDSEModSettings *swgDSEModSettings = swgChannelSettings->getDseModSettings();

    swgDSEModSettings->setCwKeyer(new SWGSDRangel::SWGCWKeyerSettings());
    SWGSDRangel::SWGCWKeyerSettings *apiCwKeyerSettings = swgDSEModSettings->getCwKeyer();
    m_basebandSource->getCWKeyer().webapiFormatChannelSettings(apiCwKeyerSettings, cwKeyerSettings);

    QString channelSettingsURL = QString("http://%1:%2/sdrangel/deviceset/%3/channel/%4/settings")
        .arg(m_settings.m_reverseAPIAddress)
        .arg(m_settings.m_reverseAPIPort)
        .arg(m_settings.m_reverseAPIDeviceIndex)
        .arg(m_settings.m_reverseAPIChannelIndex);
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

void DSEMod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    const QStringList& channelSettingsKeys,
    const DSEModSettings& settings,
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

void DSEMod::webapiFormatChannelSettings(
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings *swgChannelSettings,
    const DSEModSettings& settings,
    bool force)
{
    swgChannelSettings->setDirection(1); // single source (Tx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setDseModSettings(new SWGSDRangel::SWGDSEModSettings());
    SWGSDRangel::SWGDSEModSettings *swg = swgChannelSettings->getDseModSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swg->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swg->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("mode") || force) {
        swg->setMode((int) settings.m_mode);
    }
    if (channelSettingsKeys.contains("transmit") || force) {
        swg->setTransmit(settings.m_transmit ? 1 : 0);
    }
    if (channelSettingsKeys.contains("invert") || force) {
        swg->setInvert(settings.m_invert ? 1 : 0);
    }
    if (channelSettingsKeys.contains("modAFInput") || force) {
        swg->setModAfInput((int) settings.m_modAFInput);
    }
    if (channelSettingsKeys.contains("toneFrequency") || force) {
        swg->setToneFrequency(settings.m_toneFrequency);
    }
    if (channelSettingsKeys.contains("volumeFactor") || force) {
        swg->setVolumeFactor(settings.m_volumeFactor);
    }
    if (channelSettingsKeys.contains("channelMute") || force) {
        swg->setChannelMute(settings.m_channelMute ? 1 : 0);
    }
    if (channelSettingsKeys.contains("playLoop") || force) {
        swg->setPlayLoop(settings.m_playLoop ? 1 : 0);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        swg->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }
    if (channelSettingsKeys.contains("vocoder") || force) {
        swg->setVocoder((int) settings.m_vocoder);
    }
    if (channelSettingsKeys.contains("ambeFeatureIndex") || force) {
        swg->setAmbeFeatureIndex(settings.m_ambeFeatureIndex);
    }
    if (channelSettingsKeys.contains("connectAMBE") || force) {
        swg->setConnectAmbe(settings.m_connectAMBE ? 1 : 0);
    }
    if (channelSettingsKeys.contains("vocoderGain") || force) {
        swg->setVocoderGain(settings.m_vocoderGain);
    }
    if (channelSettingsKeys.contains("toneDetect") || force) {
        swg->setToneDetect(settings.m_toneDetect ? 1 : 0);
    }
    if (channelSettingsKeys.contains("noiseSuppress") || force) {
        swg->setNoiseSuppress(settings.m_noiseSuppress ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dtmfSequence") || force) {
        swg->setDtmfSequence(new QString(settings.m_dtmfSequence));
    }
    if (channelSettingsKeys.contains("dstarMyCall") || force) {
        swg->setDstarMyCall(new QString(settings.m_dstarMyCall));
    }
    if (channelSettingsKeys.contains("dstarMySuffix") || force) {
        swg->setDstarMySuffix(new QString(settings.m_dstarMySuffix));
    }
    if (channelSettingsKeys.contains("dstarUrCall") || force) {
        swg->setDstarUrCall(new QString(settings.m_dstarUrCall));
    }
    if (channelSettingsKeys.contains("dstarRpt1") || force) {
        swg->setDstarRpt1(new QString(settings.m_dstarRpt1));
    }
    if (channelSettingsKeys.contains("dstarRpt2") || force) {
        swg->setDstarRpt2(new QString(settings.m_dstarRpt2));
    }
    if (channelSettingsKeys.contains("dstarText") || force) {
        swg->setDstarText(new QString(settings.m_dstarText));
    }
    if (channelSettingsKeys.contains("dstarFlag1") || force) {
        swg->setDstarFlag1(settings.m_dstarFlag1);
    }
    if (channelSettingsKeys.contains("dstarFlag2") || force) {
        swg->setDstarFlag2(settings.m_dstarFlag2);
    }
    if (channelSettingsKeys.contains("dstarFlag3") || force) {
        swg->setDstarFlag3(settings.m_dstarFlag3);
    }
    if (channelSettingsKeys.contains("dmrColorCode") || force) {
        swg->setDmrColorCode(settings.m_dmrColorCode);
    }
    if (channelSettingsKeys.contains("dmrSourceId") || force) {
        swg->setDmrSourceId(settings.m_dmrSourceId);
    }
    if (channelSettingsKeys.contains("dmrDestinationId") || force) {
        swg->setDmrDestinationId(settings.m_dmrDestinationId);
    }
    if (channelSettingsKeys.contains("dmrGroupCall") || force) {
        swg->setDmrGroupCall(settings.m_dmrGroupCall ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dmrSlot") || force) {
        swg->setDmrSlot(settings.m_dmrSlot);
    }
    if (channelSettingsKeys.contains("dmrDirect") || force) {
        swg->setDmrDirect(settings.m_dmrDirect ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dmrFid") || force) {
        swg->setDmrFid(settings.m_dmrFid);
    }
    if (channelSettingsKeys.contains("dmrServiceOptions") || force) {
        swg->setDmrServiceOptions(settings.m_dmrServiceOptions);
    }
    if (channelSettingsKeys.contains("ysfCallsign") || force) {
        swg->setYsfCallsign(new QString(settings.m_ysfCallsign));
    }
    if (channelSettingsKeys.contains("ysfDestination") || force) {
        swg->setYsfDestination(new QString(settings.m_ysfDestination));
    }
    if (channelSettingsKeys.contains("ysfDownlink") || force) {
        swg->setYsfDownlink(new QString(settings.m_ysfDownlink));
    }
    if (channelSettingsKeys.contains("ysfUplink") || force) {
        swg->setYsfUplink(new QString(settings.m_ysfUplink));
    }
    if (channelSettingsKeys.contains("ysfMode") || force) {
        swg->setYsfMode((int) settings.m_ysfMode);
    }
    if (channelSettingsKeys.contains("nxdnRan") || force) {
        swg->setNxdnRan(settings.m_nxdnRan);
    }
    if (channelSettingsKeys.contains("nxdnSourceId") || force) {
        swg->setNxdnSourceId(settings.m_nxdnSourceId);
    }
    if (channelSettingsKeys.contains("nxdnDestinationId") || force) {
        swg->setNxdnDestinationId(settings.m_nxdnDestinationId);
    }
    if (channelSettingsKeys.contains("nxdnGroupCall") || force) {
        swg->setNxdnGroupCall(settings.m_nxdnGroupCall ? 1 : 0);
    }
    if (channelSettingsKeys.contains("dpmrColorCode") || force) {
        swg->setDpmrColorCode(settings.m_dpmrColorCode);
    }
    if (channelSettingsKeys.contains("dpmrOwnId") || force) {
        swg->setDpmrOwnId(settings.m_dpmrOwnId);
    }
    if (channelSettingsKeys.contains("dpmrCalledId") || force) {
        swg->setDpmrCalledId(settings.m_dpmrCalledId);
    }
    if (channelSettingsKeys.contains("p25Nac") || force) {
        swg->setP25Nac(settings.m_p25Nac);
    }
    if (channelSettingsKeys.contains("p25TalkGroup") || force) {
        swg->setP25TalkGroup(settings.m_p25TalkGroup);
    }
    if (channelSettingsKeys.contains("p25SourceId") || force) {
        swg->setP25SourceId(settings.m_p25SourceId);
    }
    if (channelSettingsKeys.contains("p25p2Wacn") || force) {
        swg->setP25p2Wacn(settings.m_p25p2Wacn);
    }
    if (channelSettingsKeys.contains("p25p2Sysid") || force) {
        swg->setP25p2Sysid(settings.m_p25p2Sysid);
    }
    if (channelSettingsKeys.contains("angel61Rate") || force) {
        swg->setAngel61Rate((int) settings.m_angel61Rate);
    }
    if (channelSettingsKeys.contains("angel61Source") || force) {
        swg->setAngel61Source(new QString(settings.m_angel61Source));
    }
    if (channelSettingsKeys.contains("angel61Destination") || force) {
        swg->setAngel61Destination(new QString(settings.m_angel61Destination));
    }
    if (channelSettingsKeys.contains("angel61Path") || force) {
        swg->setAngel61Path(new QString(settings.m_angel61Path));
    }
    if (channelSettingsKeys.contains("angel61Message") || force) {
        swg->setAngel61Message(new QString(settings.m_angel61Message));
    }
    if (channelSettingsKeys.contains("sendPosition") || force) {
        swg->setSendPosition(settings.m_sendPosition ? 1 : 0);
    }
    if (channelSettingsKeys.contains("angel61Demod") || force) {
        swg->setAngel61Demod(new QString(settings.m_angel61Demod));
    }
    if (channelSettingsKeys.contains("angel61Symbol") || force) {
        swg->setAngel61Symbol(new QString(settings.m_angel61Symbol));
    }
    if (channelSettingsKeys.contains("angel61FileName") || force) {
        swg->setAngel61FileName(new QString(settings.m_angel61FileName));
    }
    if (channelSettingsKeys.contains("m17Source") || force) {
        swg->setM17Source(new QString(settings.m_m17Source));
    }
    if (channelSettingsKeys.contains("m17Destination") || force) {
        swg->setM17Destination(new QString(settings.m_m17Destination));
    }
    if (channelSettingsKeys.contains("m17CAN") || force) {
        swg->setM17Can(settings.m_m17CAN);
    }
    if (channelSettingsKeys.contains("m17Message") || force) {
        swg->setM17Message(new QString(settings.m_m17Message));
    }
    if (channelSettingsKeys.contains("codecModelDirectory") || force) {
        swg->setCodecModelDirectory(new QString(settings.m_codecModelDirectory));
    }
    if (channelSettingsKeys.contains("audioFileName") || force) {
        swg->setAudioFileName(new QString(settings.m_audioFileName));
    }
    if (channelSettingsKeys.contains("angel61Tunnel") || force) {
        swg->setAngel61Tunnel(settings.m_angel61Tunnel ? 1 : 0);
    }
    if (channelSettingsKeys.contains("angel61TunnelPeer") || force) {
        swg->setAngel61TunnelPeer(new QString(settings.m_angel61TunnelPeer));
    }
    if (channelSettingsKeys.contains("angel61TunnelInterface") || force) {
        swg->setAngel61TunnelInterface(new QString(settings.m_angel61TunnelInterface));
    }
    if (channelSettingsKeys.contains("angel61TunnelAddress") || force) {
        swg->setAngel61TunnelAddress(new QString(settings.m_angel61TunnelAddress));
    }
    if (channelSettingsKeys.contains("angel61TunnelRoutes") || force) {
        swg->setAngel61TunnelRoutes(new QString(settings.m_angel61TunnelRoutes));
    }
    if (channelSettingsKeys.contains("angel61TunnelAllow") || force) {
        swg->setAngel61TunnelAllow(new QString(settings.m_angel61TunnelAllow));
    }
    if (channelSettingsKeys.contains("angel61TunnelGroups") || force) {
        swg->setAngel61TunnelGroups(new QString(settings.m_angel61TunnelGroups));
    }
    if (channelSettingsKeys.contains("angel61TunnelMTU") || force) {
        swg->setAngel61TunnelMtu(settings.m_angel61TunnelMTU);
    }
    if (channelSettingsKeys.contains("feedbackAudioDeviceName") || force) {
        swg->setFeedbackAudioDeviceName(new QString(settings.m_feedbackAudioDeviceName));
    }
    if (channelSettingsKeys.contains("feedbackVolumeFactor") || force) {
        swg->setFeedbackVolumeFactor(settings.m_feedbackVolumeFactor);
    }
    if (channelSettingsKeys.contains("feedbackAudioEnable") || force) {
        swg->setFeedbackAudioEnable(settings.m_feedbackAudioEnable ? 1 : 0);
    }
    if (channelSettingsKeys.contains("spectrumAudio") || force) {
        swg->setSpectrumAudio(settings.m_spectrumAudio ? 1 : 0);
    }
    if (channelSettingsKeys.contains("frameSessionMaxDurationMs") || force) {
        swg->setFrameSessionMaxDurationMs(settings.m_frameSessionMaxDurationMs);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swg->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swg->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swg->setStreamIndex(settings.m_streamIndex);
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrumConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swg->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swg->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swg->setRollupState(swgRollupState);
    }
}

void DSEMod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DSEMod::networkManagerFinished:"
            << " error(" << (int) replyError
            << "): " << replyError
            << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DSEMod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

double DSEMod::getMagSq() const
{
    return m_basebandSource->getMagSq();
}

CWKeyer *DSEMod::getCWKeyer()
{
    return &m_basebandSource->getCWKeyer();
}

void DSEMod::setLevelMeter(QObject *levelMeter)
{
    connect(m_basebandSource, SIGNAL(levelChanged(qreal, qreal, int)), levelMeter, SLOT(levelChanged(qreal, qreal, int)));
}

uint32_t DSEMod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSinkStreams();
}

int DSEMod::getPcmSampleRate() const
{
    return m_basebandSource->getPcmSampleRate();
}

int DSEMod::getAudioSampleRate() const
{
    return m_basebandSource->getAudioSampleRate();
}

uint32_t DSEMod::getAudioUnderrunCount() const
{
    return m_basebandSource->getAudioUnderrunCount();
}

uint32_t DSEMod::getAudioOverrunCount() const
{
    return m_basebandSource->getAudioOverrunCount();
}

bool DSEMod::isTransmitting() const
{
    return m_basebandSource->getProcessor()->isTransmitting();
}

bool DSEMod::isVocoderReady() const
{
    return m_basebandSource->getProcessor()->isVocoderReady();
}

uint32_t DSEMod::getEncodedFrameCount() const
{
    return m_basebandSource->getProcessor()->getEncodedFrameCount();
}

uint32_t DSEMod::getUnderrunCount() const
{
    return m_basebandSource->getProcessor()->getUnderrunCount();
}

uint32_t DSEMod::getOverrunCount() const
{
    return m_basebandSource->getProcessor()->getOverrunCount();
}

uint32_t DSEMod::getTrimCount() const
{
    return m_basebandSource->getProcessor()->getTrimCount();
}

uint32_t DSEMod::getTxFrameCount() const
{
    return m_basebandSource->getProcessor()->getTxFrameCount();
}
