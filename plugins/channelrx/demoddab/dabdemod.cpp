///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2014 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2015-2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2020-2021, 2023 Jon Beniston, M7RCE <jon@beniston.com>          //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
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

#include "dabdemod.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QThread>

#include <stdio.h>
#include <complex.h>
#include <cmath>

#include "SWGChannelSettings.h"
#include "SWGChannelReport.h"
#include "SWGWorkspaceInfo.h"
#include "SWGDABDemodReport.h"
#include "SWGDABDemodSettings.h"
#include "SWGDABProgram.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "util/db.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(DABDemod::MsgConfigureDABDemod, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABEnsembleName, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABProgramName, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABProgramData, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABSystemData, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABProgramQuality, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABFIBQuality, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABSampleRate, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABData, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABMOTData, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABTII, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABDLPlus, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABJournaline, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABServiceInfo, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABSPI, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABSPICache, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABWebsite, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABSignalQuality, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABCarrierCN, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABReset, Message)
MESSAGE_CLASS_DEFINITION(DABDemod::MsgDABResetService, Message)

const char * const DABDemod::m_channelIdURI = "sdrangel.channel.dabdemod";
const char * const DABDemod::m_channelId = "DABDemod";

DABDemod::DABDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0),
        m_reportSync(false),
        m_reportSNR(0),
        m_reportFrequencyOffset(0),
        m_reportEnsembleId(0),
        m_reportAudioActive(false),
        m_reportBitrate(0),
        m_reportFrameQuality(0),
        m_reportReedSolomonQuality(0),
        m_reportAACQuality(0),
        m_reportFIBQuality(0),
        m_reportTII(TIINone),
        m_spiCacheEnsemble(-1)
{
    setObjectName(m_channelId);
    m_thread.setObjectName("DABDemodBB");

    m_basebandSink = new DABDemodBaseband();
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    m_networkManager = new QNetworkAccessManager();
    QObject::connect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DABDemod::networkManagerFinished
    );
    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &DABDemod::handleIndexInDeviceSetChanged
    );
}

DABDemod::~DABDemod()
{
    qDebug("DABDemod::~DABDemod");
    QObject::disconnect(
        m_networkManager,
        &QNetworkAccessManager::finished,
        this,
        &DABDemod::networkManagerFinished
    );
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void DABDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t DABDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void DABDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void DABDemod::start()
{
    qDebug("DABDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    resetReceiver();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    DABDemodBaseband::MsgConfigureDABDemodBaseband *msg = DABDemodBaseband::MsgConfigureDABDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void DABDemod::stop()
{
    qDebug("DABDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
    clearReport();
}

bool DABDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureDABDemod::match(cmd))
    {
        MsgConfigureDABDemod& cfg = (MsgConfigureDABDemod&) cmd;
        qDebug() << "DABDemod::handleMessage: MsgConfigureDABDemod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        if ((m_basebandSampleRate != notif.getSampleRate()) || (m_centerFrequency != notif.getCenterFrequency())) {
            resetReceiver();
        }
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "DABDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (MsgDABSystemData::match(cmd))
    {
        MsgDABSystemData& report = (MsgDABSystemData&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportSync = report.getSync();
            m_reportSNR = report.getSNR();
            m_reportFrequencyOffset = report.getFrequencyOffset();
            if (!m_reportSync) {
                m_reportAudioActive = false;
            }
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABSystemData(report));
        }

        return true;
    }
    else if (MsgDABProgramQuality::match(cmd))
    {
        MsgDABProgramQuality& report = (MsgDABProgramQuality&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportFrameQuality = report.getFrames();
            m_reportReedSolomonQuality = report.getRS();
            m_reportAACQuality = report.getAAC();
            // Audio is decoding again after a loss of sync (the programme data is not resent)
            if (m_reportSync && (report.getAAC() > 0)) {
                m_reportAudioActive = true;
            }
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABProgramQuality(report));
        }

        return true;
    }
    else if (MsgDABSignalQuality::match(cmd))
    {
        MsgDABSignalQuality& report = (MsgDABSignalQuality&)cmd;
        SignalQuality quality = report.getQuality();
        {
            QMutexLocker locker(&m_reportMutex);
            quality.gainStatus = gainAdvice(quality, m_reportSync, (float) m_reportSNR, quality.gainAdvice);
            m_reportQuality = quality;
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(MsgDABSignalQuality::create(quality));
        }

        return true;
    }
    else if (MsgDABCarrierCN::match(cmd))
    {
        MsgDABCarrierCN& report = (MsgDABCarrierCN&)cmd;
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABCarrierCN(report));
        }

        return true;
    }
    else if (MsgDABFIBQuality::match(cmd))
    {
        MsgDABFIBQuality& report = (MsgDABFIBQuality&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportFIBQuality = report.getPercent();
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABFIBQuality(report));
        }

        return true;
    }
    else if (MsgDABSampleRate::match(cmd))
    {
        MsgDABSampleRate& report = (MsgDABSampleRate&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportAudioActive = report.getSampleRate() > 0;
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABSampleRate(report));
        }

        return true;
    }
    else if (MsgDABEnsembleName::match(cmd))
    {
        MsgDABEnsembleName& report = (MsgDABEnsembleName&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportEnsembleName = report.getName();
            m_reportEnsembleId = report.getId();
        }
        // The SPI kept from before for this ensemble, until its carousel sends it again
        if ((report.getId() >= 0) && (report.getId() != m_spiCacheEnsemble)) {
            loadSPICache(report.getId());
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABEnsembleName(report));
        }

        return true;
    }
    else if (MsgDABProgramName::match(cmd))
    {
        MsgDABProgramName& report = (MsgDABProgramName&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportPrograms.insert(report.getId(), report.getName());
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABProgramName(report));
        }

        return true;
    }
    else if (MsgDABProgramData::match(cmd))
    {
        MsgDABProgramData& report = (MsgDABProgramData&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportAudioActive = true;
            m_reportBitrate = report.getBitrate();
            m_reportAudio = report.getAudio();
            m_reportLanguage = report.getLanguage();
            m_reportProgramType = report.getProgramType();
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABProgramData(report));
        }

        return true;
    }
    else if (MsgDABData::match(cmd))
    {
        MsgDABData& report = (MsgDABData&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportData = report.getData();
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABData(report));
        }

        return true;
    }
    else if (MsgDABMOTData::match(cmd))
    {
        MsgDABMOTData& report = (MsgDABMOTData&)cmd;
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABMOTData(report));
        }

        return true;
    }
    else if (MsgDABDLPlus::match(cmd))
    {
        MsgDABDLPlus& report = (MsgDABDLPlus&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportDLPlus = report.getTags();
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABDLPlus(report));
        }

        return true;
    }
    else if (MsgDABServiceInfo::match(cmd))
    {
        MsgDABServiceInfo& report = (MsgDABServiceInfo&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportAnnouncement = report.getInfo().announcement;
            m_reportProgrammeNow = report.getInfo().programmeNow;
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABServiceInfo(report));
        }

        return true;
    }
    else if (MsgDABSPI::match(cmd))
    {
        MsgDABSPI& report = (MsgDABSPI&)cmd;
        if (report.getData() && (report.getEnsembleId() >= 0)) {
            saveSPICache(*report.getData(), report.getEnsembleId());
        }
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABSPI(report));
        }

        return true;
    }
    else if (MsgDABWebsite::match(cmd))
    {
        MsgDABWebsite& report = (MsgDABWebsite&)cmd;
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABWebsite(report));
        }

        return true;
    }
    else if (MsgDABJournaline::match(cmd))
    {
        MsgDABJournaline& report = (MsgDABJournaline&)cmd;
        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MsgDABJournaline(report));
        }

        return true;
    }
    else if (MsgDABTII::match(cmd))
    {
        MsgDABTII& report = (MsgDABTII&)cmd;
        {
            QMutexLocker locker(&m_reportMutex);
            m_reportTII = report.getTII();
        }
        if (getMessageQueueToGUI())
        {
            getMessageQueueToGUI()->push(new MsgDABTII(report));
        }

        return true;
    }
    else if (MsgDABReset::match(cmd))
    {
        MsgDABReset& report = (MsgDABReset&)cmd;
        clearReport();
        m_basebandSink->getInputMessageQueue()->push(new MsgDABReset(report));

        return true;
    }
    else if (MsgDABResetService::match(cmd))
    {
        MsgDABResetService& report = (MsgDABResetService&)cmd;
        clearProgramReport();
        m_basebandSink->getInputMessageQueue()->push(new MsgDABResetService(report));

        return true;
    }
    else if (MainCore::MsgChannelDemodQuery::match(cmd))
    {
        qDebug() << "DABDemod::handleMessage: MsgChannelDemodQuery";
        sendSampleRateToDemodAnalyzer();

        return true;
    }
    else
    {
        return false;
    }
}

void DABDemod::setCenterFrequency(qint64 frequency)
{
    DABDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList({"inputFrequencyOffset"}), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDABDemod *msgToGUI = MsgConfigureDABDemod::create(QStringList({"inputFrequencyOffset"}), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void DABDemod::applySettings(const QStringList& settingsKeys, const DABDemodSettings& settings, bool force)
{
    qDebug() << "DABDemod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        resetReceiver();
    } else if (settings.m_program != m_settings.m_program) {
        clearProgramReport();
    }

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }
    if ((settings.m_rfBandwidth != m_settings.m_rfBandwidth) || force) {
        reverseAPIKeys.append("rfBandwidth");
    }
    if ((settings.m_program != m_settings.m_program) || force) {
        reverseAPIKeys.append("program");
    }
    if ((settings.m_volume != m_settings.m_volume) || force) {
        reverseAPIKeys.append("volume");
    }
    if ((settings.m_audioMute != m_settings.m_audioMute) || force) {
        reverseAPIKeys.append("audioMute");
    }
    if ((settings.m_audioDeviceName != m_settings.m_audioDeviceName) || force) {
        reverseAPIKeys.append("audioDeviceName");
    }
    if (settingsKeys.contains("streamIndex") && (settings.m_streamIndex != m_settings.m_streamIndex))
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

        reverseAPIKeys.append("streamIndex");
    }
    if (settingsKeys.contains("coherent") && (settings.m_coherent != m_settings.m_coherent)) {
        reverseAPIKeys.append("coherent");
    }
    if (settingsKeys.contains("announcementTypes") && (settings.m_announcementTypes != m_settings.m_announcementTypes)) {
        reverseAPIKeys.append("announcementTypes");
    }

    DABDemodBaseband::MsgConfigureDABDemodBaseband *msg = DABDemodBaseband::MsgConfigureDABDemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (settingsKeys.contains("useReverseAPI") && settings.m_useReverseAPI)
    {
        bool fullUpdate = ((settingsKeys.contains("useReverseAPI") && (m_settings.m_useReverseAPI != settings.m_useReverseAPI)) && settings.m_useReverseAPI) ||
                (settingsKeys.contains("reverseAPIAddress") && (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress)) ||
                (settingsKeys.contains("reverseAPIPort") && (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort)) ||
                (settingsKeys.contains("reverseAPIDeviceIndex") && (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex)) ||
                (settingsKeys.contains("reverseAPIChannelIndex") && (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex));
        webapiReverseSendSettings(settingsKeys, settings, fullUpdate || force);
    }

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray DABDemod::serialize() const
{
    return m_settings.serialize();
}

bool DABDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureDABDemod *msg = MsgConfigureDABDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureDABDemod *msg = MsgConfigureDABDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void DABDemod::sendSampleRateToDemodAnalyzer()
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

int DABDemod::webapiSettingsGet(
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDabDemodSettings(new SWGSDRangel::SWGDABDemodSettings());
    response.getDabDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);
    return 200;
}

int DABDemod::webapiReportGet(
        SWGSDRangel::SWGChannelReport& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setDabDemodReport(new SWGSDRangel::SWGDABDemodReport());
    response.getDabDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void DABDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    SWGSDRangel::SWGDABDemodReport *report = response.getDabDemodReport();
    double magsqAvg;
    double magsqPeak;
    int nbMagsqSamples;
    getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    report->setChannelPowerDb(CalcDb::dbPower(magsqAvg));
    report->setChannelSampleRate(m_basebandSink->getChannelSampleRate());
    report->setAudioSampleRate(m_basebandSink->getAudioSampleRate());
    if (report->getSelectedProgram()) {
        *report->getSelectedProgram() = m_settings.m_program.trimmed();
    } else {
        report->setSelectedProgram(new QString(m_settings.m_program.trimmed()));
    }

    QMutexLocker locker(&m_reportMutex);
    report->setSync(m_reportSync ? 1 : 0);
    report->setSnr((int) std::lround(m_reportSNR));
    report->setFrequencyOffset(m_reportFrequencyOffset);
    // DAB labels are a fixed 16 characters, space padded
    if (report->getEnsembleName()) {
        *report->getEnsembleName() = m_reportEnsembleName.trimmed();
    } else {
        report->setEnsembleName(new QString(m_reportEnsembleName.trimmed()));
    }
    report->setEnsembleId(m_reportEnsembleId);

    for (auto it = m_reportPrograms.cbegin(); it != m_reportPrograms.cend(); ++it)
    {
        SWGSDRangel::SWGDABProgram *program = new SWGSDRangel::SWGDABProgram();
        program->init();
        if (program->getName()) {
            *program->getName() = it.value().trimmed();
        } else {
            program->setName(new QString(it.value().trimmed()));
        }
        program->setId(it.key());
        report->getPrograms()->append(program);
    }

    report->setAudioActive(m_reportAudioActive ? 1 : 0);
    report->setBitrate(m_reportBitrate);
    if (report->getAudio()) {
        *report->getAudio() = m_reportAudio.trimmed();
    } else {
        report->setAudio(new QString(m_reportAudio.trimmed()));
    }
    if (report->getLanguage()) {
        *report->getLanguage() = m_reportLanguage.trimmed();
    } else {
        report->setLanguage(new QString(m_reportLanguage.trimmed()));
    }
    if (report->getProgramType()) {
        *report->getProgramType() = m_reportProgramType.trimmed();
    } else {
        report->setProgramType(new QString(m_reportProgramType.trimmed()));
    }
    report->setFrameQuality(m_reportFrameQuality);
    report->setReedSolomonQuality(m_reportReedSolomonQuality);
    report->setAacQuality(m_reportAACQuality);
    report->setFibQuality(m_reportFIBQuality);
    report->setTii(m_reportTII);
    if (m_reportQuality.measured)
    {
        report->setCn(m_reportQuality.cn);
        report->setNullDepth(m_reportQuality.nullDepth);
        report->setMscBer(m_reportQuality.mscBER);
        report->setFicBer(m_reportQuality.ficBER);
        report->setRsCorrectedRate(m_reportQuality.rsCorrectedRate);
        report->setFramesLost(m_reportQuality.framesLost);
        report->setReacquisitions(m_reportQuality.reacquisitions);
    }
    report->setInputLevel(m_reportQuality.inputLevel);
    report->setClipping(m_reportQuality.clipping);
    report->setGainStatus(m_reportQuality.gainStatus);
    if (report->getGainAdvice()) {
        *report->getGainAdvice() = m_reportQuality.gainAdvice;
    } else {
        report->setGainAdvice(new QString(m_reportQuality.gainAdvice));
    }
    if (report->getData()) {
        *report->getData() = m_reportData.trimmed();
    } else {
        report->setData(new QString(m_reportData.trimmed()));
    }
    QString artist;
    QString title;
    for (const auto& tag : m_reportDLPlus)
    {
        if (tag.first == "ITEM.ARTIST") {
            artist = tag.second;
        } else if (tag.first == "ITEM.TITLE") {
            title = tag.second;
        }
    }
    const auto setString = [](QString *existing, const QString& value, auto setter) {
        if (existing) {
            *existing = value;
        } else {
            setter(new QString(value));
        }
    };
    setString(report->getNowPlayingArtist(), artist, [report](QString *s) { report->setNowPlayingArtist(s); });
    setString(report->getNowPlayingTitle(), title, [report](QString *s) { report->setNowPlayingTitle(s); });
    setString(report->getAnnouncement(), m_reportAnnouncement, [report](QString *s) { report->setAnnouncement(s); });
    setString(report->getProgrammeNow(), m_reportProgrammeNow, [report](QString *s) { report->setProgrammeNow(s); });
}

void DABDemod::clearProgramReport()
{
    QMutexLocker locker(&m_reportMutex);
    m_reportAudioActive = false;
    m_reportBitrate = 0;
    m_reportAudio.clear();
    m_reportLanguage.clear();
    m_reportProgramType.clear();
    m_reportFrameQuality = 0;
    m_reportReedSolomonQuality = 0;
    m_reportAACQuality = 0;
    m_reportData.clear();
    m_reportDLPlus.clear();
    m_reportAnnouncement.clear();
    m_reportProgrammeNow.clear();
}

// The report and the receiver start again together: the sink sends the ensemble, programs,
// TII and SPI only when they change, so a report cleared on its own would stay empty
void DABDemod::resetReceiver()
{
    clearReport();
    m_basebandSink->getInputMessageQueue()->push(MsgDABReset::create());
}

void DABDemod::clearReport()
{
    QMutexLocker locker(&m_reportMutex);
    m_reportSync = false;
    m_reportSNR = 0;
    m_reportFrequencyOffset = 0;
    m_reportEnsembleName.clear();
    m_reportEnsembleId = 0;
    m_reportPrograms.clear();
    m_reportAudioActive = false;
    m_reportBitrate = 0;
    m_reportAudio.clear();
    m_reportLanguage.clear();
    m_reportProgramType.clear();
    m_reportFrameQuality = 0;
    m_reportReedSolomonQuality = 0;
    m_reportAACQuality = 0;
    m_reportFIBQuality = 0;
    m_reportTII = TIINone;
    m_reportData.clear();
    m_reportDLPlus.clear();
    m_reportAnnouncement.clear();
    m_reportProgrammeNow.clear();
    m_reportQuality = SignalQuality();
    // The receiver starts again: its SPI cache too
    m_spiCacheEnsemble = -1;
}

int DABDemod::gainAdvice(const SignalQuality& quality, bool sync, float snr, QString& advice)
{
    // Clipping, of an 8-bit ADC especially, spreads distortion over every carrier. OFDM
    // peaks are 10 dB or more above the mean, so a mean above -12 dBFS clips; an RTL-SDR's
    // tuner already compresses above about -15 dBFS.
    const float clippingLimit = 0.1f;      // % of samples
    const float levelHigh = -15.0f;        // dBFS
    const float levelLow = -45.0f;         // dBFS: near an 8-bit ADC's quantization
    const float levelModerate = -30.0f;    // dBFS
    const float cnPoor = 10.0f;            // dB
    // The MER from the symbols sees overload of the receiver by the ensemble, which the
    // null symbol does not; but also multipath, Doppler and interference
    const float merGap = 8.0f;             // dB
    const float merCheckCN = 15.0f;        // dB

    if (quality.clipping >= clippingLimit)
    {
        advice = QString("Reduce gain or add an attenuator: %1% of samples clip").arg(quality.clipping, 0, 'f', 2);
        return GainReduce;
    }
    if (quality.inputLevel > levelHigh)
    {
        advice = "Reduce gain or add an attenuator: input level high";
        return GainReduce;
    }
    if (quality.inputLevel < levelLow)
    {
        advice = "Increase gain: input level low";
        return GainIncrease;
    }
    // A low level with a poor signal: the receiver's own noise may be limiting it
    if ((quality.inputLevel < levelModerate) && (!sync || (quality.measured && (quality.cn < cnPoor))))
    {
        advice = "Increase gain: input level low for the C/N";
        return GainIncrease;
    }
    if (!sync)
    {
        advice = "Level OK, no signal";
        return GainOK;
    }
    if (quality.measured && (quality.cn >= merCheckCN) && (quality.cn - snr >= merGap))
    {
        advice = "Check: SNR well below C/N. Try less gain (overload), else multipath or interference";
        return GainCheck;
    }
    advice = "Gain OK";
    return GainOK;
}

int DABDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int DABDemod::webapiSettingsPutPatch(
        bool force,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response,
        QString& errorMessage)
{
    (void) errorMessage;
    DABDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureDABDemod *msg = MsgConfigureDABDemod::create(channelSettingsKeys, settings, force);
    m_inputMessageQueue.push(msg);

    qDebug("DABDemod::webapiSettingsPutPatch: forward to GUI: %p", m_guiMessageQueue);
    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureDABDemod *msgToGUI = MsgConfigureDABDemod::create(channelSettingsKeys, settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void DABDemod::webapiUpdateChannelSettings(
        DABDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getDabDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("rfBandwidth")) {
        settings.m_rfBandwidth = response.getDabDemodSettings()->getRfBandwidth();
    }
    if (channelSettingsKeys.contains("program")) {
        settings.m_program = *response.getDabDemodSettings()->getProgram();
    }
    if (channelSettingsKeys.contains("volume")) {
        settings.m_volume = response.getDabDemodSettings()->getVolume();
    }
    if (channelSettingsKeys.contains("audioMute")) {
        settings.m_audioMute = response.getDabDemodSettings()->getAudioMute();
    }
    if (channelSettingsKeys.contains("audioDeviceName")) {
        settings.m_audioDeviceName = *response.getDabDemodSettings()->getAudioDeviceName();
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getDabDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getDabDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getDabDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("coherent")) {
        settings.m_coherent = response.getDabDemodSettings()->getCoherent() != 0;
    }
    if (channelSettingsKeys.contains("announcementTypes")) {
        settings.m_announcementTypes = response.getDabDemodSettings()->getAnnouncementTypes();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getDabDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getDabDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getDabDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getDabDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getDabDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getDabDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getDabDemodSettings()->getRollupState());
    }
}

void DABDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const DABDemodSettings& settings)
{
    response.getDabDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getDabDemodSettings()->setRfBandwidth(settings.m_rfBandwidth);
    if (response.getDabDemodSettings()->getProgram()) {
        *response.getDabDemodSettings()->getProgram() = settings.m_program;
    } else {
        response.getDabDemodSettings()->setProgram(new QString(settings.m_program));
    }
    response.getDabDemodSettings()->setVolume(settings.m_volume);
    response.getDabDemodSettings()->setAudioMute(settings.m_audioMute);
    if (response.getDabDemodSettings()->getAudioDeviceName()) {
        *response.getDabDemodSettings()->getAudioDeviceName() = settings.m_audioDeviceName;
    } else {
        response.getDabDemodSettings()->setAudioDeviceName(new QString(settings.m_audioDeviceName));
    }

    response.getDabDemodSettings()->setRgbColor(settings.m_rgbColor);
    if (response.getDabDemodSettings()->getTitle()) {
        *response.getDabDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getDabDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getDabDemodSettings()->setStreamIndex(settings.m_streamIndex);
    response.getDabDemodSettings()->setCoherent(settings.m_coherent ? 1 : 0);
    response.getDabDemodSettings()->setAnnouncementTypes(settings.m_announcementTypes);
    response.getDabDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getDabDemodSettings()->getReverseApiAddress()) {
        *response.getDabDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getDabDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getDabDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getDabDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getDabDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_channelMarker)
    {
        if (response.getDabDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getDabDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getDabDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getDabDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getDabDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getDabDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void DABDemod::webapiReverseSendSettings(const QList<QString>& channelSettingsKeys, const DABDemodSettings& settings, bool force)
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

void DABDemod::webapiFormatChannelSettings(
        const QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const DABDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    if (swgChannelSettings->getChannelType()) {
        *swgChannelSettings->getChannelType() = "DABDemod";
    } else {
        swgChannelSettings->setChannelType(new QString("DABDemod"));
    }
    swgChannelSettings->setDabDemodSettings(new SWGSDRangel::SWGDABDemodSettings());
    SWGSDRangel::SWGDABDemodSettings *swgDABDemodSettings = swgChannelSettings->getDabDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgDABDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("rfBandwidth") || force) {
        swgDABDemodSettings->setRfBandwidth(settings.m_rfBandwidth);
    }
    if (channelSettingsKeys.contains("program") || force) {
        if (swgDABDemodSettings->getProgram()) {
            *swgDABDemodSettings->getProgram() = settings.m_program;
        } else {
            swgDABDemodSettings->setProgram(new QString(settings.m_program));
        }
    }
    if (channelSettingsKeys.contains("volume") || force) {
        swgDABDemodSettings->setVolume(settings.m_volume);
    }
    if (channelSettingsKeys.contains("audioMute") || force) {
        swgDABDemodSettings->setAudioMute(settings.m_audioMute);
    }
    if (channelSettingsKeys.contains("audioDeviceName") || force) {
        if (swgDABDemodSettings->getAudioDeviceName()) {
            *swgDABDemodSettings->getAudioDeviceName() = settings.m_audioDeviceName;
        } else {
            swgDABDemodSettings->setAudioDeviceName(new QString(settings.m_audioDeviceName));
        }
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgDABDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgDABDemodSettings->getTitle()) {
            *swgDABDemodSettings->getTitle() = settings.m_title;
        } else {
            swgDABDemodSettings->setTitle(new QString(settings.m_title));
        }
    }
    if (channelSettingsKeys.contains("streamIndex") || force) {
        swgDABDemodSettings->setStreamIndex(settings.m_streamIndex);
    }
    if (channelSettingsKeys.contains("coherent") || force) {
        swgDABDemodSettings->setCoherent(settings.m_coherent ? 1 : 0);
    }
    if (channelSettingsKeys.contains("announcementTypes") || force) {
        swgDABDemodSettings->setAnnouncementTypes(settings.m_announcementTypes);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgDABDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgDABDemodSettings->setRollupState(swgRollupState);
    }
}

void DABDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "DABDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("DABDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

void DABDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
    m_basebandSink->setAudioFifoLabel(fifoLabel);
}

int DABDemod::getChannelSampleRate() const
{
    return m_basebandSink ? m_basebandSink->getChannelSampleRate() : 0;
}

// A directory for each ensemble, with an index of its files: their names (which can be paths
// or URLs) are hashed for the files' own names
QString DABDemod::spiCacheDirectory(int ensembleId)
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QString("/dabdemod_spi/%1").arg(ensembleId, 4, 16, QLatin1Char('0')).toUpper();
}

static QString spiCacheFileName(const QString& name)
{
    return QString(QCryptographicHash::hash(name.toUtf8(), QCryptographicHash::Sha1).toHex()) + ".bin";
}

void DABDemod::loadSPICache(int ensembleId)
{
    m_spiCacheEnsemble = ensembleId;
    m_spiSaved.clear();
    m_spiLoaded.clear();

    const QDir dir(spiCacheDirectory(ensembleId));
    QFile indexFile(dir.filePath("index.json"));
    if (!indexFile.open(QIODevice::ReadOnly))
    {
        // Nothing cached: the receiver must not keep another ensemble's files
        m_basebandSink->getInputMessageQueue()->push(MsgDABSPICache::create({}));
        return;
    }
    const QJsonArray entries = QJsonDocument::fromJson(indexFile.readAll()).object().value("files").toArray();
    const QDateTime oldest = QDateTime::currentDateTimeUtc().addDays(-SPICacheDays);
    std::vector<modemdr::MOTFile> files;

    for (const QJsonValue& value : entries)
    {
        const QJsonObject entry = value.toObject();
        const QString name = entry.value("name").toString();
        const QString fileName = spiCacheFileName(name);

        // Gone from the carousel a while ago
        if (QDateTime::fromString(entry.value("saved").toString(), Qt::ISODate) < oldest)
        {
            QFile::remove(dir.filePath(fileName));
            continue;
        }
        QFile file(dir.filePath(fileName));
        if (name.isEmpty() || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray bytes = file.readAll();

        modemdr::MOTFile motFile;
        motFile.name = name.toStdString();
        motFile.contentType = entry.value("contentType").toInt(-1);
        motFile.contentSubtype = entry.value("contentSubtype").toInt(-1);
        motFile.mimeType = entry.value("mimeType").toString().toStdString();
        const QJsonObject parameters = entry.value("parameters").toObject();
        for (auto it = parameters.begin(); it != parameters.end(); ++it)
        {
            const QByteArray parameter = QByteArray::fromHex(it.value().toString().toLatin1());
            motFile.parameters[it.key().toInt()].assign(parameter.begin(), parameter.end());
        }
        auto data = std::make_shared<std::vector<uint8_t>>(bytes.begin(), bytes.end());
        motFile.data = data;
        m_spiSaved.insert(name, data.get());
        m_spiLoaded.insert(data.get());
        files.push_back(motFile);
    }

    qDebug() << "DABDemod::loadSPICache:" << files.size() << "files for ensemble" << QString::number(ensembleId, 16);
    m_basebandSink->getInputMessageQueue()->push(MsgDABSPICache::create(files));
}

// New and changed files, as they arrive
void DABDemod::saveSPICache(const modemdr::SPIData& data, int ensembleId)
{
    if (ensembleId != m_spiCacheEnsemble) {
        return;     // Before the cache was loaded for the ensemble: the index would lose its files
    }

    const QDir dir(spiCacheDirectory(ensembleId));
    bool changed = false;

    for (const auto *files : {&data.images, &data.objects})
    {
        for (const auto& entry : *files)
        {
            const modemdr::MOTFile& motFile = entry.second;
            const QString name = QString::fromStdString(motFile.name);
            if (!motFile.data || (m_spiSaved.value(name) == motFile.data.get())) {
                continue;
            }
            if (!dir.exists() && !QDir().mkpath(dir.path())) {
                return;
            }
            QFile file(dir.filePath(spiCacheFileName(name)));
            if (file.open(QIODevice::WriteOnly))
            {
                file.write(reinterpret_cast<const char *>(motFile.data->data()), (qint64) motFile.data->size());
                m_spiSaved.insert(name, motFile.data.get());
                changed = true;
            }
        }
    }
    if (!changed) {
        return;
    }

    // The index: the files of the data, and those of the cache it does not have
    QJsonObject index;
    QFile indexFile(dir.filePath("index.json"));
    if (indexFile.open(QIODevice::ReadOnly))
    {
        index = QJsonDocument::fromJson(indexFile.readAll()).object();
        indexFile.close();
    }
    QMap<QString, QJsonObject> entries;
    for (const QJsonValue& value : index.value("files").toArray()) {
        entries.insert(value.toObject().value("name").toString(), value.toObject());
    }
    const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    for (const auto *files : {&data.images, &data.objects})
    {
        for (const auto& entry : *files)
        {
            const modemdr::MOTFile& motFile = entry.second;
            // A file from the cache, not received again: as it was
            if (m_spiLoaded.contains(motFile.data.get()) && entries.contains(QString::fromStdString(motFile.name))) {
                continue;
            }
            QJsonObject object;
            object.insert("name", QString::fromStdString(motFile.name));
            object.insert("contentType", motFile.contentType);
            object.insert("contentSubtype", motFile.contentSubtype);
            object.insert("mimeType", QString::fromStdString(motFile.mimeType));
            QJsonObject parameters;
            for (const auto& parameter : motFile.parameters)
            {
                parameters.insert(QString::number(parameter.first),
                    QString(QByteArray(reinterpret_cast<const char *>(parameter.second.data()), (int) parameter.second.size()).toHex()));
            }
            object.insert("parameters", parameters);
            object.insert("saved", now);
            entries.insert(QString::fromStdString(motFile.name), object);
        }
    }
    QJsonArray array;
    for (const QJsonObject& object : entries) {
        array.append(object);
    }
    index.insert("files", array);
    if (indexFile.open(QIODevice::WriteOnly)) {
        indexFile.write(QJsonDocument(index).toJson(QJsonDocument::Compact));
    }
}
