///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2022 Edouard Griffiths, F4EXB                                   //
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

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"

#include "device/deviceapi.h"
#include "dsp/hbfilterchainconverter.h"
#include "dsp/dspcommands.h"
#include "feature/feature.h"
#include "util/db.h"
#include "maincore.h"

#include "doa2baseband.h"
#include "doa2.h"

MESSAGE_CLASS_DEFINITION(DOA2::MsgConfigureDOA2, Message)
MESSAGE_CLASS_DEFINITION(DOA2::MsgBasebandNotification, Message)

const char* const DOA2::m_channelIdURI = "sdrangel.channel.doa2";
const char* const DOA2::m_channelId = "DOA2";
const int DOA2::m_fftSize = 4096;

DOA2::DOA2(DeviceAPI *deviceAPI) :
    ChannelAPI(m_channelIdURI, ChannelAPI::StreamMIMO),
    m_deviceAPI(deviceAPI),
    m_guiMessageQueue(nullptr),
    m_frequencyOffset(0),
    m_deviceSampleRate(48000),
    m_deviceCenterFrequency(435000000)
{
    setObjectName(m_channelId);

    m_thread = new QThread(this);
    m_basebandSink = new DOA2Baseband(m_fftSize);
    m_basebandSink->setScopeSink(&m_scopeSink);
    m_basebandSink->moveToThread(m_thread);
    m_deviceAPI->addMIMOChannel(this);
    m_deviceAPI->addMIMOChannelAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DOA2::networkManagerFinished
    );
}

DOA2::~DOA2()
{
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DOA2::networkManagerFinished
    );
    delete m_networkManager;

    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeMIMOChannel(this);
    delete m_basebandSink;
    delete m_thread;
}

void DOA2::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeMIMOChannel(this);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addMIMOChannel(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

void DOA2::startSinks()
{
    if (m_deviceSampleRate != 0) {
        m_basebandSink->setBasebandSampleRate(m_deviceSampleRate);
    }

    m_basebandSink->reset();
    m_thread->start();

    DOA2Baseband::MsgConfigureChannelizer *msg = DOA2Baseband::MsgConfigureChannelizer::create(
        m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void DOA2::stopSinks()
{
	m_thread->exit();
	m_thread->wait();
}

void DOA2::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, unsigned int sinkIndex)
{
    m_basebandSink->feed(begin, end, sinkIndex);
}

void DOA2::pull(SampleVector::iterator& begin, unsigned int nbSamples, unsigned int sourceIndex)
{
    (void) begin;
    (void) nbSamples;
    (void) sourceIndex;
}

void DOA2::applySettings(const DOA2Settings& settings, bool force)
{
    qDebug() << "DOA2::applySettings: "
        << "m_correlationType: " << settings.m_correlationType
        << "m_filterChainHash: " << settings.m_filterChainHash
        << "m_log2Decim: " << settings.m_log2Decim
        << "m_phase: " << settings.m_phase
        << "m_antennaAz:" << settings.m_antennaAz
        << "m_basebandDistance: " << settings.m_basebandDistance
        << "m_squelchdB: " << settings.m_squelchdB
        << "m_fftAveragingIndex: "<< settings.m_fftAveragingIndex
        << "m_useReverseAPI: " << settings.m_useReverseAPI
        << "m_reverseAPIAddress: " << settings.m_reverseAPIAddress
        << "m_reverseAPIPort: " << settings.m_reverseAPIPort
        << "m_reverseAPIDeviceIndex: " << settings.m_reverseAPIDeviceIndex
        << "m_reverseAPIChannelIndex: " << settings.m_reverseAPIChannelIndex
        << "m_title: " << settings.m_title;

    QList<QString> reverseAPIKeys;

    if ((m_settings.m_correlationType != settings.m_correlationType) || force) {
        reverseAPIKeys.append("correlationType");
    }
    if ((m_settings.m_filterChainHash != settings.m_filterChainHash) || force) {
        reverseAPIKeys.append("filterChainHash");
    }
    if ((m_settings.m_log2Decim != settings.m_log2Decim) || force) {
        reverseAPIKeys.append("log2Decim");
    }
    if ((m_settings.m_phase != settings.m_phase) || force) {
        reverseAPIKeys.append("phase");
    }
    if ((m_settings.m_title != settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((m_settings.m_antennaAz != settings.m_antennaAz) || force) {
        reverseAPIKeys.append("antennaAz");
    }
    if ((m_settings.m_basebandDistance != settings.m_basebandDistance) || force) {
        reverseAPIKeys.append("basebandDistance");
    }

    if ((m_settings.m_squelchdB != settings.m_squelchdB) || force)
    {
        reverseAPIKeys.append("squelchdB");
        m_basebandSink->setMagThreshold(CalcDb::powerFromdB(settings.m_squelchdB));
    }

    if ((m_settings.m_fftAveragingIndex != settings.m_fftAveragingIndex) || force)
    {
        reverseAPIKeys.append("m_fftAveragingIndex");
        m_basebandSink->setFFTAveraging(DOA2Settings::getAveragingValue(settings.m_fftAveragingIndex));
    }

    if ((m_settings.m_log2Decim != settings.m_log2Decim)
     || (m_settings.m_filterChainHash != settings.m_filterChainHash) || force)
    {
        DOA2Baseband::MsgConfigureChannelizer *msg = DOA2Baseband::MsgConfigureChannelizer::create(
            settings.m_log2Decim, settings.m_filterChainHash);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_correlationType != settings.m_correlationType) || force)
    {
        DOA2Baseband::MsgConfigureCorrelation *msg = DOA2Baseband::MsgConfigureCorrelation::create(
            settings.m_correlationType);
        m_basebandSink->getInputMessageQueue()->push(msg);
    }

    if ((m_settings.m_phase != settings.m_phase) || force) {
        m_basebandSink->setPhase(settings.m_phase);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;
}

void DOA2::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

bool DOA2::handleMessage(const Message& cmd)
{
    if (MsgConfigureDOA2::match(cmd))
    {
        MsgConfigureDOA2& cfg = (MsgConfigureDOA2&) cmd;
        qDebug() << "DOA2::handleMessage: MsgConfigureDOA2";
        applySettings(cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPMIMOSignalNotification::match(cmd))
    {
        DSPMIMOSignalNotification& notif = (DSPMIMOSignalNotification&) cmd;

        qDebug() << "DOA2::handleMessage: DSPMIMOSignalNotification:"
                << " inputSampleRate: " << notif.getSampleRate()
                << " centerFrequency: " << notif.getCenterFrequency()
                << " sourceElseSink: " << notif.getSourceOrSink()
                << " streamIndex: " << notif.getIndex();

        if (notif.getSourceOrSink()) // deals with source messages only
        {
            m_deviceSampleRate = notif.getSampleRate();
            m_deviceCenterFrequency = notif.getCenterFrequency();
            calculateFrequencyOffset(); // This is when device sample rate changes

            // Notify baseband sink of input sample rate change
            DOA2Baseband::MsgSignalNotification *sig = DOA2Baseband::MsgSignalNotification::create(
                m_deviceSampleRate, notif.getCenterFrequency(), notif.getIndex()
            );
            qDebug() << "DOA2::handleMessage: DSPMIMOSignalNotification: push to sink";
            m_basebandSink->getInputMessageQueue()->push(sig);

            if (getMessageQueueToGUI())
            {
                qDebug() << "DOA2::handleMessage: DSPMIMOSignalNotification: push to GUI";
                MsgBasebandNotification *msg = MsgBasebandNotification::create(
                    notif.getSampleRate(), notif.getCenterFrequency());
                getMessageQueueToGUI()->push(msg);
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}

QByteArray DOA2::serialize() const
{
    return m_settings.serialize();
}

bool DOA2::deserialize(const QByteArray& data)
{
    (void) data;
    if (m_settings.deserialize(data))
    {
        MsgConfigureDOA2 *msg = MsgConfigureDOA2::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDOA2 *msg = MsgConfigureDOA2::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DOA2::validateFilterChainHash(DOA2Settings& settings)
{
    unsigned int s = 1;

    for (unsigned int i = 0; i < settings.m_log2Decim; i++) {
        s *= 3;
    }

    settings.m_filterChainHash = settings.m_filterChainHash >= s ? s-1 : settings.m_filterChainHash;
}

void DOA2::calculateFrequencyOffset()
{
    double shiftFactor = HBFilterChainConverter::getShiftFactor(m_settings.m_log2Decim, m_settings.m_filterChainHash);
    m_frequencyOffset = m_deviceSampleRate * shiftFactor;
}

void DOA2::applyChannelSettings(uint32_t log2Decim, uint32_t filterChainHash)
{
    DOA2Baseband::MsgConfigureChannelizer *msg = DOA2Baseband::MsgConfigureChannelizer::create(log2Decim, filterChainHash);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

float DOA2::getPhi() const
{
    return m_basebandSink ? m_basebandSink->getPhi() : 0.0f;
}

float DOA2::getPositiveDOA() const
{
    return std::acos(getPhi()/M_PI)*(180/M_PI);
}

int DOA2::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDoa2Settings(new SWGSDRangel::SWGDOA2Settings());
    response.getDoa2Settings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DOA2::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DOA2::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DOA2Settings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDOA2 *msg = MsgConfigureDOA2::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (getMessageQueueToGUI()) // forward to GUI if any
    {
        MsgConfigureDOA2 *msgToGUI = MsgConfigureDOA2::create(settings, force);
        getMessageQueueToGUI()->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

int DOA2::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDoa2Report(new SWGSDRangel::SWGDOA2Report());
    response.getDoa2Report()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DOA2::webapiUpdateChannelSettings(
        DOA2Settings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDoa2Settings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDoa2Settings()->getTitle();
    }
    if (channelSettingsKeys.contains("log2Decim")) {
        settings.m_log2Decim = response.getDoa2Settings()->getLog2Decim();
    }

    if (channelSettingsKeys.contains("filterChainHash"))
    {
        settings.m_filterChainHash = response.getDoa2Settings()->getFilterChainHash();
        validateFilterChainHash(settings);
    }

    if (channelSettingsKeys.contains("phase")) {
        settings.m_phase = response.getDoa2Settings()->getPhase();
    }
    if (channelSettingsKeys.contains("antennaAz")) {
        settings.m_antennaAz = response.getDoa2Settings()->getAntennaAz();
    }
    if (channelSettingsKeys.contains("basebandDistance")) {
        settings.m_basebandDistance = response.getDoa2Settings()->getBasebandDistance();
    }
    if (channelSettingsKeys.contains("squelchdB")) {
        settings.m_squelchdB = response.getDoa2Settings()->getSquelchdB();
    }
    if (channelSettingsKeys.contains("fftAveragingValue")) {
        settings.m_fftAveragingIndex = DOA2Settings::getAveragingIndex(response.getDoa2Settings()->getFftAveragingValue());
    }

    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDoa2Settings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDoa2Settings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDoa2Settings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDoa2Settings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDoa2Settings()->getReverseApiChannelIndex();
    }
    if (settings.m_scopeGUI && channelSettingsKeys.contains("scopeConfig")) {
        settings.m_scopeGUI->updateFrom(channelSettingsKeys, response.getDoa2Settings()->getScopeConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDoa2Settings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDoa2Settings()->getRollupState());
    }
}

void DOA2::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DOA2Settings& settings)
{
    response.getDoa2Settings()->setRgbColor(settings.m_rgbColor);

    if (response.getDoa2Settings()->getTitle()) {
        *response.getDoa2Settings()->getTitle() = settings.m_title;
    } else {
        response.getDoa2Settings()->setTitle(new QString(settings.m_title));
    }

    response.getDoa2Settings()->setLog2Decim(settings.m_log2Decim);
    response.getDoa2Settings()->setFilterChainHash(settings.m_filterChainHash);
    response.getDoa2Settings()->setPhase(settings.m_phase);
    response.getDoa2Settings()->setAntennaAz(settings.m_antennaAz);
    response.getDoa2Settings()->setBasebandDistance(settings.m_basebandDistance);
    response.getDoa2Settings()->setSquelchdB(settings.m_squelchdB);
    response.getDoa2Settings()->setFftAveragingValue(DOA2Settings::getAveragingValue(settings.m_fftAveragingIndex));
    response.getDoa2Settings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDoa2Settings()->getReverseApiAddress()) {
        *response.getDoa2Settings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDoa2Settings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDoa2Settings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDoa2Settings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDoa2Settings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_scopeGUI)
    {
        if (response.getDoa2Settings()->getScopeConfig())
        {
            settings.m_scopeGUI->formatTo(response.getDoa2Settings()->getScopeConfig());
        }
        else
        {
            SWGSDRangel::SWGGLScope *swgGLScope = new SWGSDRangel::SWGGLScope();
            settings.m_scopeGUI->formatTo(swgGLScope);
            response.getDoa2Settings()->setScopeConfig(swgGLScope);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getDoa2Settings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getDoa2Settings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getDoa2Settings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDoa2Settings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDoa2Settings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDoa2Settings()->setRollupState(swgRollupState);
        }
    }
}

void DOA2::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    float phi = normalizeAngle(getPhi() * (180/M_PI), 180.0f);
    response.getDoa2Report()->setPhi(phi);

    float hwl = 1.5e8 / (m_deviceCenterFrequency + m_frequencyOffset);
    float cosTheta = (getPhi()/M_PI) * ((hwl * 1000.0) / m_settings.m_basebandDistance);
    float blindAngle = (m_settings.m_basebandDistance > hwl * 1000.0) ?
        std::acos((hwl * 1000.0) / m_settings.m_basebandDistance) * (180/M_PI) :
        0;
    response.getDoa2Report()->setBlindAngle((int) blindAngle);
    float doaAngle = std::acos(cosTheta < -1.0 ? -1.0 : cosTheta > 1.0 ? 1.0 : cosTheta) * (180/M_PI);
    qDebug("DOA2::webapiFormatChannelReport: phi: %f cosT: %f DOAngle: %f", getPhi(), cosTheta, doaAngle);
    float posAngle = normalizeAngle(m_settings.m_antennaAz - doaAngle, 360.0f); // DOA angles are trigonometric but displayed angles are clockwise
    float negAngle = normalizeAngle(m_settings.m_antennaAz + doaAngle, 360.0f);
    response.getDoa2Report()->setPosAz(posAngle);
    response.getDoa2Report()->setNegAz(negAngle);

    response.getDoa2Report()->setFftSize(m_fftSize);
    int channelSampleRate = m_deviceSampleRate / (1<<m_settings.m_log2Decim);
    response.getDoa2Report()->setChannelSampleRate(channelSampleRate);
}

void DOA2::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const DOA2Settings& settings, bool force)
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
    reply->setParent(buffer);

    delete swgChannelSettings;
}

void DOA2::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const DOA2Settings& settings,
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

void DOA2::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DOA2Settings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(2); // MIMO sink
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString("DOA2"));
    swgChannelSettings->setDoa2Settings(new SWGSDRangel::SWGDOA2Settings());
    SWGSDRangel::SWGDOA2Settings *swgDOA2Settings = swgChannelSettings->getDoa2Settings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDOA2Settings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgDOA2Settings->setTitle(new QString(settings.m_title));
    }
    if (channelSettingsKeys.contains("log2Decim") || force) {
        swgDOA2Settings->setLog2Decim(settings.m_log2Decim);
    }
    if (channelSettingsKeys.contains("filterChainHash") || force) {
        swgDOA2Settings->setFilterChainHash(settings.m_filterChainHash);
    }
    if (channelSettingsKeys.contains("phase") || force) {
        swgDOA2Settings->setPhase(settings.m_phase);
    }
    if (channelSettingsKeys.contains("antennaAz") || force) {
        swgDOA2Settings->setAntennaAz(settings.m_antennaAz);
    }
    if (channelSettingsKeys.contains("basebandDistance") || force) {
        swgDOA2Settings->setBasebandDistance(settings.m_basebandDistance);
    }
    if (channelSettingsKeys.contains("squelchdB") || force) {
        swgDOA2Settings->setSquelchdB(settings.m_squelchdB);
    }
    if (channelSettingsKeys.contains("fftAveragingValue") || force) {
        swgDOA2Settings->setFftAveragingValue(DOA2Settings::getAveragingValue(settings.m_fftAveragingIndex));
    }

    if (settings.m_scopeGUI)
    {
        if (channelSettingsKeys.contains("scopeConfig") || force) {
            settings.m_scopeGUI->formatTo(swgDOA2Settings->getScopeConfig());
        }
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgDOA2Settings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDOA2Settings->setRollupState(swgRollupState);
    }
}

void DOA2::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DOA2::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DOA2::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

float DOA2::normalizeAngle(float angle, float max)
{
    if (angle < 0)   { return max + angle; }
    if (angle > max) { return angle - max; }
    return angle;
}
