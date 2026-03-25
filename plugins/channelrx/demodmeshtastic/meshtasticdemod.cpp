///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 maintech GmbH, Otto-Hahn-Str. 15, 97204 Hoechberg, Germany //
// written by Christian Daniel                                                   //
// Copyright (C) 2015-2023 Edouard Griffiths, F4EXB <f4exb06@gmail.com>          //
// Copyright (C) 2015 John Greb <hexameron@spam.no>                              //
// Copyright (C) 2020 Kacper Michajłow <kasper93@gmail.com>                      //
// Copyright (C) 2021 Jon Beniston, M7RCE <jon@beniston.com>                     //
// (c) 2015 John Greb                                                            //
// (c) 2020 Edouard Griffiths, F4EXB                                             //
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

#include <QTime>
#include <QDebug>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QBuffer>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGMeshtasticDemodReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/ax25.h"
#include "util/db.h"
#include "maincore.h"
#include "channel/channelwebapiutils.h"

#include "meshtasticdemodmsg.h"
#include "meshtasticdemoddecoder.h"
#include "meshtasticdemod.h"
#include "meshtasticpacket.h"

MESSAGE_CLASS_DEFINITION(MeshtasticDemod::MsgConfigureMeshtasticDemod, Message)

const char* const MeshtasticDemod::m_channelIdURI = "sdrangel.channel.meshtasticdemod";
const char* const MeshtasticDemod::m_channelId = "MeshtasticDemod";

MeshtasticDemod::MeshtasticDemod(DeviceAPI* deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_running(false),
        m_spectrumVis(SDR_RX_SCALEF),
        m_basebandSampleRate(0),
        m_basebandCenterFrequency(0),
        m_haveBasebandCenterFrequency(false),
        m_lastMsgSignalDb(0.0),
        m_lastMsgNoiseDb(0.0),
        m_lastMsgSyncWord(0),
        m_lastMsgPacketLength(0),
        m_lastMsgNbParityBits(0),
        m_lastMsgHasCRC(false),
        m_lastMsgNbSymbols(0),
        m_lastMsgNbCodewords(0),
        m_lastMsgEarlyEOM(false),
        m_lastMsgHeaderCRC(false),
        m_lastMsgHeaderParityStatus(0),
        m_lastMsgPayloadCRC(false),
        m_lastMsgPayloadParityStatus(0),
        m_udpSink(this, 256)
{
	setObjectName(m_channelId);
	applySettings(m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);
    m_networkManager = new QNetworkAccessManager();

    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &MeshtasticDemod::handleIndexInDeviceSetChanged
    );

    start();
}

MeshtasticDemod::~MeshtasticDemod()
{
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);
    stop();
}

void MeshtasticDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t MeshtasticDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void MeshtasticDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO)
{
    (void) pO;

    if (!m_running) {
        return;
    }

    for (PipelineRuntime& runtime : m_pipelines)
    {
        if (runtime.basebandSink) {
            runtime.basebandSink->feed(begin, end);
        }
    }
}

int MeshtasticDemod::findBandwidthIndexForHz(int bandwidthHz) const
{
    if (bandwidthHz <= 0) {
        return -1;
    }

    int exactIndex = -1;
    int nearestIndex = -1;
    qint64 nearestDelta = std::numeric_limits<qint64>::max();

    for (int i = 0; i < MeshtasticDemodSettings::nbBandwidths; ++i)
    {
        const int bw = MeshtasticDemodSettings::bandwidths[i];

        if (bw == bandwidthHz) {
            exactIndex = i;
            break;
        }

        const qint64 delta = std::abs(static_cast<qint64>(bw) - static_cast<qint64>(bandwidthHz));

        if (delta < nearestDelta)
        {
            nearestDelta = delta;
            nearestIndex = i;
        }
    }

    return exactIndex >= 0 ? exactIndex : nearestIndex;
}

MeshtasticDemodSettings MeshtasticDemod::makePipelineSettingsFromMeshRadio(
    const MeshtasticDemodSettings& baseSettings,
    const QString& presetName,
    const modemmeshtastic::TxRadioSettings& meshRadio,
    qint64 selectedPresetFrequencyHz,
    bool haveSelectedPresetFrequency
) const
{
    MeshtasticDemodSettings out = baseSettings;
    out.m_spreadFactor = meshRadio.spreadFactor;
    out.m_deBits = meshRadio.deBits;
    out.m_nbParityBits = meshRadio.parityBits;
    out.m_meshtasticPresetName = presetName;
    out.m_preambleChirps = meshRadio.preambleChirps;

    const int bandwidthIndex = findBandwidthIndexForHz(meshRadio.bandwidthHz);

    if (bandwidthIndex >= 0) {
        out.m_bandwidthIndex = bandwidthIndex;
    }

    if (meshRadio.hasCenterFrequency)
    {
        if (m_haveBasebandCenterFrequency)
        {
            out.m_inputFrequencyOffset = static_cast<int>(meshRadio.centerFrequencyHz - m_basebandCenterFrequency);
        }
        else if (haveSelectedPresetFrequency)
        {
            out.m_inputFrequencyOffset = baseSettings.m_inputFrequencyOffset
                + static_cast<int>(meshRadio.centerFrequencyHz - selectedPresetFrequencyHz);
        }
        else
        {
            out.m_inputFrequencyOffset = baseSettings.m_inputFrequencyOffset;
        }
    }

    return out;
}

std::vector<MeshtasticDemod::PipelineConfig> MeshtasticDemod::buildPipelineConfigs(const MeshtasticDemodSettings& settings) const
{
    std::vector<PipelineConfig> configs;

    if (settings.m_codingScheme != MeshtasticDemodSettings::CodingLoRa)
    {
        PipelineConfig config;
        config.id = 0;
        config.name = "Main";
        config.presetName = "MAIN";
        config.settings = settings;
        configs.push_back(config);
        return configs;
    }

    static const std::array<const char*, 9> kPresetOrder = {
        "LONG_FAST",
        "LONG_SLOW",
        "LONG_MODERATE",
        "LONG_TURBO",
        "MEDIUM_FAST",
        "MEDIUM_SLOW",
        "SHORT_FAST",
        "SHORT_SLOW",
        "SHORT_TURBO"
    };

    const QString selectedPreset = settings.m_meshtasticPresetName.trimmed().isEmpty()
        ? QString("LONG_FAST")
        : settings.m_meshtasticPresetName.trimmed().toUpper();

    QStringList orderedPresets;
    orderedPresets.append(selectedPreset);

    for (const char *preset : kPresetOrder)
    {
        const QString p(preset);

        if (!orderedPresets.contains(p)) {
            orderedPresets.append(p);
        }
    }

    const QString region = settings.m_meshtasticRegionCode.trimmed().isEmpty()
        ? QString("US")
        : settings.m_meshtasticRegionCode.trimmed();
    const int channelNum = std::max(1, settings.m_meshtasticChannelIndex + 1);

    qint64 selectedPresetFrequencyHz = 0;
    bool haveSelectedPresetFrequency = false;
    {
        modemmeshtastic::TxRadioSettings selectedMeshRadio;
        QString error;
        const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3")
            .arg(selectedPreset)
            .arg(region)
            .arg(channelNum);

        if (modemmeshtastic::Packet::deriveTxRadioSettings(command, selectedMeshRadio, error) && selectedMeshRadio.hasCenterFrequency)
        {
            selectedPresetFrequencyHz = selectedMeshRadio.centerFrequencyHz;
            haveSelectedPresetFrequency = true;
        }
    }

    int id = 0;

    for (const QString& presetName : orderedPresets)
    {
        modemmeshtastic::TxRadioSettings meshRadio;
        QString error;
        const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3")
            .arg(presetName)
            .arg(region)
            .arg(channelNum);

        if (!modemmeshtastic::Packet::deriveTxRadioSettings(command, meshRadio, error))
        {
            qDebug() << "MeshtasticDemod::buildPipelineConfigs: skip preset" << presetName << ":" << error;
            continue;
        }

        if (!meshRadio.hasLoRaParams) {
            continue;
        }

        PipelineConfig config;
        config.id = id++;
        config.name = presetName;
        config.presetName = presetName;
        config.settings = makePipelineSettingsFromMeshRadio(
            settings,
            presetName,
            meshRadio,
            selectedPresetFrequencyHz,
            haveSelectedPresetFrequency
        );
        configs.push_back(config);
    }

    if (configs.empty())
    {
        PipelineConfig config;
        config.id = 0;
        config.name = "Main";
        config.presetName = "MAIN";
        config.settings = settings;
        configs.push_back(config);
    }

    return configs;
}

void MeshtasticDemod::makePipelineConfigFromSettings(int configId, PipelineConfig& config, const MeshtasticDemodSettings& settings) const
{
    // USER preset: all LoRa parameters are user-controlled; skip derivation from the mesh radio table.
    if (settings.m_meshtasticPresetName.trimmed().compare("USER", Qt::CaseInsensitive) == 0)
    {
        config.id = configId;
        config.name = QString("CONF%1").arg(configId);
        config.presetName = settings.m_meshtasticPresetName;
        config.settings = settings;
        return;
    }

    const QString region = settings.m_meshtasticRegionCode.trimmed().isEmpty()
        ? QString("US")
        : settings.m_meshtasticRegionCode.trimmed();
    const int channelNum = std::max(1, settings.m_meshtasticChannelIndex + 1);

    modemmeshtastic::TxRadioSettings meshRadio;
    QString error;
    const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3")
        .arg(settings.m_meshtasticPresetName.trimmed().isEmpty() ? QString("LONG_FAST") : settings.m_meshtasticPresetName.trimmed().toUpper())
        .arg(region)
        .arg(channelNum);
    qint64 selectedPresetFrequencyHz = 0;
    bool haveSelectedPresetFrequency = false;

    if (modemmeshtastic::Packet::deriveTxRadioSettings(command, meshRadio, error) && meshRadio.hasCenterFrequency)
    {
        selectedPresetFrequencyHz = meshRadio.centerFrequencyHz;
        haveSelectedPresetFrequency = true;
    }

    config.id = configId;
    config.name = QString("CONF%1").arg(configId);
    config.presetName = settings.m_meshtasticPresetName;
    config.settings = makePipelineSettingsFromMeshRadio(
        settings,
        config.presetName,
        meshRadio,
        selectedPresetFrequencyHz,
        haveSelectedPresetFrequency
    );
}

void MeshtasticDemod::applyPipelineRuntimeSettings(PipelineRuntime& runtime, const MeshtasticDemodSettings& settings, bool force)
{
    runtime.settings = settings;

    if (runtime.decoder)
    {
        runtime.decoder->setCodingScheme(settings.m_codingScheme);
        runtime.decoder->setNbSymbolBits(settings.m_spreadFactor, settings.m_deBits);
        runtime.decoder->setLoRaHasHeader(settings.m_hasHeader);
        runtime.decoder->setLoRaHasCRC(settings.m_hasCRC);
        runtime.decoder->setLoRaParityBits(settings.m_nbParityBits);
        runtime.decoder->setLoRaPacketLength(settings.m_packetLength);
        runtime.decoder->setLoRaBandwidth(MeshtasticDemodSettings::bandwidths[settings.m_bandwidthIndex]);
    }

    if (runtime.basebandSink)
    {
        MeshtasticDemodBaseband::MsgConfigureMeshtasticDemodBaseband *msg =
            MeshtasticDemodBaseband::MsgConfigureMeshtasticDemodBaseband::create(settings, force);
        runtime.basebandSink->getInputMessageQueue()->push(msg);
    }
}

void MeshtasticDemod::startPipelines(const std::vector<PipelineConfig>& configs)
{
    m_pipelines.clear();
    m_pipelines.reserve(configs.size());

    for (const PipelineConfig& config : configs)
    {
        PipelineRuntime runtime;
        runtime.id = config.id;
        runtime.name = config.name;
        runtime.presetName = config.presetName;
        runtime.settings = config.settings;

        runtime.decoderThread = new QThread();
        runtime.decoder = new MeshtasticDemodDecoder();
        runtime.decoder->setOutputMessageQueue(getInputMessageQueue());
        runtime.decoder->setPipelineMetadata(runtime.id, runtime.name, runtime.presetName);
        runtime.decoder->moveToThread(runtime.decoderThread);

        QObject::connect(runtime.decoderThread, &QThread::finished, runtime.decoder, &QObject::deleteLater);
        runtime.decoderThread->start();

        runtime.basebandThread = new QThread();
        runtime.basebandSink = new MeshtasticDemodBaseband();

        if (config.id == 0) {
            runtime.basebandSink->setSpectrumSink(&m_spectrumVis);
        }

        runtime.basebandSink->setDecoderMessageQueue(runtime.decoder->getInputMessageQueue());
        runtime.decoder->setHeaderFeedbackMessageQueue(runtime.basebandSink->getInputMessageQueue());
        runtime.basebandSink->moveToThread(runtime.basebandThread);

        QObject::connect(runtime.basebandThread, &QThread::finished, runtime.basebandSink, &QObject::deleteLater);

        if (m_basebandSampleRate != 0) {
            runtime.basebandSink->setBasebandSampleRate(m_basebandSampleRate);
        }

        runtime.basebandSink->reset();
        runtime.basebandSink->setFifoLabel(QString("%1[%2]").arg(m_channelId).arg(config.name));
        runtime.basebandThread->start();

        applyPipelineRuntimeSettings(runtime, runtime.settings, true);
        m_pipelines.push_back(runtime);
    }
}

void MeshtasticDemod::stopPipelines()
{
    for (PipelineRuntime& runtime : m_pipelines)
    {
        if (runtime.basebandThread)
        {
            runtime.basebandThread->exit();
            runtime.basebandThread->wait();
            delete runtime.basebandThread;
            runtime.basebandThread = nullptr;
        }

        if (runtime.decoderThread)
        {
            runtime.decoderThread->exit();
            runtime.decoderThread->wait();
            delete runtime.decoderThread;
            runtime.decoderThread = nullptr;
        }

        runtime.basebandSink = nullptr;
        runtime.decoder = nullptr;
    }

    m_pipelines.clear();
}

bool MeshtasticDemod::pipelineLayoutMatches(const std::vector<PipelineConfig>& configs) const
{
    if (configs.size() != m_pipelines.size()) {
        return false;
    }

    for (size_t i = 0; i < configs.size(); ++i)
    {
        if ((configs[i].id != m_pipelines[i].id) || (configs[i].presetName != m_pipelines[i].presetName)) {
            return false;
        }
    }

    return true;
}

void MeshtasticDemod::syncPipelinesWithSettings(const MeshtasticDemodSettings& settings, bool force)
{
    if (!m_running) {
        return;
    }

    // Rebuild pipeline configs from the new settings so that region/preset/channel
    // changes are reflected in the single pipeline entry.
    for (size_t i = 0; i < m_pipelineConfigs.size(); ++i) {
        makePipelineConfigFromSettings(m_pipelineConfigs[i].id, m_pipelineConfigs[i], settings);
    }

    for (size_t i = 0; i < m_pipelineConfigs.size(); ++i)
    {
        m_pipelines[i].id = m_pipelineConfigs[i].id;
        m_pipelines[i].name = m_pipelineConfigs[i].name;
        m_pipelines[i].presetName = m_pipelineConfigs[i].presetName;

        if (m_pipelines[i].decoder) {
            m_pipelines[i].decoder->setPipelineMetadata(m_pipelineConfigs[i].id, m_pipelineConfigs[i].name, m_pipelineConfigs[i].presetName);
        }

        applyPipelineRuntimeSettings(m_pipelines[i], m_pipelineConfigs[i].settings, force);
    }

    // const std::vector<PipelineConfig> configs = buildPipelineConfigs(settings);

    // if (!pipelineLayoutMatches(configs))
    // {
    //     stopPipelines();
    //     startPipelines(configs);
    //     return;
    // }

    // for (size_t i = 0; i < configs.size(); ++i)
    // {
    //     m_pipelines[i].id = configs[i].id;
    //     m_pipelines[i].name = configs[i].name;
    //     m_pipelines[i].presetName = configs[i].presetName;

    //     if (m_pipelines[i].decoder) {
    //         m_pipelines[i].decoder->setPipelineMetadata(configs[i].id, configs[i].name, configs[i].presetName);
    //     }

    //     applyPipelineRuntimeSettings(m_pipelines[i], configs[i].settings, force);
    // }
}

void MeshtasticDemod::start()
{
    if (m_running) {
        return;
    }

    qDebug() << "MeshtasticDemod::start";
    m_pipelineConfigs.push_back(PipelineConfig());
    m_currentPipelineId = 0;
    makePipelineConfigFromSettings(m_currentPipelineId, m_pipelineConfigs.back(), m_settings);
    startPipelines(m_pipelineConfigs);
    // const std::vector<PipelineConfig> configs = buildPipelineConfigs(m_settings);
    // startPipelines(configs);

    SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis.getInputMessageQueue()->push(msg);

    m_running = true;
}

void MeshtasticDemod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug() << "MeshtasticDemod::stop";
    m_running = false;
    stopPipelines();
    m_pipelineConfigs.clear();
}

bool MeshtasticDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureMeshtasticDemod::match(cmd))
	{
		qDebug() << "MeshtasticDemod::handleMessage: MsgConfigureMeshtasticDemod";
		MsgConfigureMeshtasticDemod& cfg = (MsgConfigureMeshtasticDemod&) cmd;
		MeshtasticDemodSettings settings = cfg.getSettings();
		applySettings(settings, cfg.getForce());

		return true;
	}
    else if (MeshtasticDemodMsg::MsgReportDecodeBytes::match(cmd))
    {
        qDebug() << "MeshtasticDemod::handleMessage: MsgReportDecodeBytes";
        MeshtasticDemodMsg::MsgReportDecodeBytes& msg = (MeshtasticDemodMsg::MsgReportDecodeBytes&) cmd;

        m_lastMsgSignalDb = msg.getSingalDb();
        m_lastMsgNoiseDb = msg.getNoiseDb();
        m_lastMsgSyncWord = msg.getSyncWord();
        m_lastMsgTimestamp = msg.getMsgTimestamp();

        if (m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa)
        {
            m_lastMsgBytes = msg.getBytes();
            m_lastMsgPacketLength = msg.getPacketSize();
            m_lastMsgNbParityBits = msg.getNbParityBits();
            m_lastMsgHasCRC = msg.getHasCRC();
            m_lastMsgNbSymbols = msg.getNbSymbols();
            m_lastMsgNbCodewords = msg.getNbCodewords();
            m_lastMsgEarlyEOM = msg.getEarlyEOM();
            m_lastMsgHeaderCRC = msg.getHeaderCRCStatus();
            m_lastMsgHeaderParityStatus = msg.getHeaderParityStatus();
            m_lastMsgPayloadCRC = msg.getPayloadCRCStatus();
            m_lastMsgPayloadParityStatus = msg.getPayloadParityStatus();
            m_lastMsgPipelineName = msg.getPipelineName();
            m_lastFrameType = QStringLiteral("LORA_FRAME");

            QByteArray bytesCopy(m_lastMsgBytes);
            bytesCopy.truncate(m_lastMsgPacketLength);
            bytesCopy.replace('\0', " ");
            m_lastMsgString = QString(bytesCopy.toStdString().c_str());

            if (m_settings.m_sendViaUDP)
            {
                uint8_t *bytes = reinterpret_cast<uint8_t*>(m_lastMsgBytes.data());
                m_udpSink.writeUnbuffered(bytes, m_lastMsgPacketLength);
            }

            if (getMessageQueueToGUI()) {
                getMessageQueueToGUI()->push(new MeshtasticDemodMsg::MsgReportDecodeBytes(msg)); // make a copy
            }

            modemmeshtastic::DecodeResult meshResult;

            if (modemmeshtastic::Packet::decodeFrame(m_lastMsgBytes, meshResult, m_settings.m_meshtasticKeySpecList))
            {
                m_lastMsgString = meshResult.summary;

                for (const modemmeshtastic::DecodeResult::Field& field : meshResult.fields)
                {
                    if (field.path == QStringLiteral("data.port_name"))
                    {
                        m_lastFrameType = field.value;
                        break;
                    }
                }

                qInfo() << "MeshtasticDemod::handleMessage:" << meshResult.summary;

                if (meshResult.dataDecoded && getMessageQueueToGUI())
                {
                    MeshtasticDemodMsg::MsgReportDecodeString *meshMsg = MeshtasticDemodMsg::MsgReportDecodeString::create(meshResult.summary);
                    meshMsg->setFrameId(msg.getFrameId());
                    meshMsg->setSyncWord(msg.getSyncWord());
                    meshMsg->setSignalDb(msg.getSingalDb());
                    meshMsg->setNoiseDb(msg.getNoiseDb());
                    meshMsg->setMsgTimestamp(msg.getMsgTimestamp());
                    meshMsg->setPipelineMetadata(msg.getPipelineId(), msg.getPipelineName(), msg.getPipelinePreset());
                    QVector<QPair<QString, QString>> structuredFields;
                    structuredFields.reserve(meshResult.fields.size());

                    for (const modemmeshtastic::DecodeResult::Field& field : meshResult.fields) {
                        structuredFields.append(qMakePair(field.path, field.value));
                    }

                    meshMsg->setStructuredFields(structuredFields);
                    getMessageQueueToGUI()->push(meshMsg);
                }
            }

            // Is this an APRS packet?
            // As per: https://github.com/oe3cjb/TTGO-T-Beam-LoRa-APRS/blob/master/lib/BG_RF95/BG_RF95.cpp
            // There is a 3 byte header for LoRa APRS packets. Addressing follows in ASCII: srccall>dst:
            int colonIdx = m_lastMsgBytes.indexOf(':');
            int greaterThanIdx =  m_lastMsgBytes.indexOf('>');
            if (   (m_lastMsgBytes[0] == '<')
                && (greaterThanIdx != -1)
                && (colonIdx != -1)
                && ((m_lastMsgHasCRC && m_lastMsgPayloadCRC) || !m_lastMsgHasCRC)
                )
            {
                QByteArray packet;

                // Extract addresses
                const char *d = m_lastMsgBytes.data();
                QString srcString = QString::fromLatin1(d + 3, greaterThanIdx - 3);
                QString dstString = QString::fromLatin1(d + greaterThanIdx + 1, colonIdx - greaterThanIdx - 1);

                // Convert to AX.25 format
                packet.append(AX25Packet::encodeAddress(dstString));
                packet.append(AX25Packet::encodeAddress(srcString, 1));
                packet.append(3);
                packet.append(-16); // 0xf0
                packet.append(m_lastMsgBytes.mid(colonIdx+1));
                if (!m_lastMsgHasCRC)
                {
                    packet.append((char)0); // dummy crc
                    packet.append((char)0);
                }

                // Forward to APRS and other packet features
                QList<ObjectPipe*> packetsPipes;
                MainCore::instance()->getMessagePipes().getMessagePipes(this, "packets", packetsPipes);

                for (const auto& pipe : packetsPipes)
                {
                    MessageQueue *messageQueue = qobject_cast<MessageQueue*>(pipe->m_element);
                    MainCore::MsgPacket *msg = MainCore::MsgPacket::create(this, packet, QDateTime::currentDateTime());
                    messageQueue->push(msg);
                }
            }

            // In explicit-header LoRa mode, frame length is already derived from header
            // and may legitimately vary across packets. Auto-clamping nbSymbolsMax to the
            // first short frame breaks subsequent longer frames.
            if (m_settings.m_autoNbSymbolsMax
                && !((m_settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa) && m_settings.m_hasHeader))
            {
                MeshtasticDemodSettings settings = m_settings;
                settings.m_nbSymbolsMax = m_lastMsgNbSymbols;
                applySettings(settings);

                if (getMessageQueueToGUI()) // forward to GUI if any
                {
                    MsgConfigureMeshtasticDemod *msgToGUI = MsgConfigureMeshtasticDemod::create(settings, false);
                    getMessageQueueToGUI()->push(msgToGUI);
                }
            }
        }

        return true;
    }
    else if (MeshtasticDemodMsg::MsgReportDecodeString::match(cmd))
    {
        qDebug() << "MeshtasticDemod::handleMessage: MsgReportDecodeString";
        MeshtasticDemodMsg::MsgReportDecodeString& msg = (MeshtasticDemodMsg::MsgReportDecodeString&) cmd;
        m_lastMsgSignalDb = msg.getSingalDb();
        m_lastMsgNoiseDb = msg.getNoiseDb();
        m_lastMsgSyncWord = msg.getSyncWord();
        m_lastMsgTimestamp = msg.getMsgTimestamp();
        m_lastMsgString = msg.getString();

        if (m_settings.m_sendViaUDP)
        {
            const QByteArray& byteArray = m_lastMsgString.toUtf8();
            const uint8_t *bytes = reinterpret_cast<const uint8_t*>(byteArray.data());
            m_udpSink.writeUnbuffered(bytes, byteArray.size());
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new MeshtasticDemodMsg::MsgReportDecodeString(msg)); // make a copy
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_basebandCenterFrequency = notif.getCenterFrequency();
        m_haveBasebandCenterFrequency = true;
        qDebug() << "MeshtasticDemod::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << m_basebandSampleRate;

        // Forward to the sink
        if (m_running)
        {
            for (PipelineRuntime& runtime : m_pipelines)
            {
                if (runtime.basebandSink)
                {
                    DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
                    runtime.basebandSink->getInputMessageQueue()->push(rep);
                }
            }

            // Frequency-dependent offsets may need update when source center changes.
            syncPipelinesWithSettings(m_settings, true);
        }

        if (getMessageQueueToGUI()) {
            getMessageQueueToGUI()->push(new DSPSignalNotification(notif)); // make a copy
        }

        return true;
    }
	else
	{
		return false;
	}
}

void MeshtasticDemod::setCenterFrequency(qint64 frequency)
{
    MeshtasticDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshtasticDemod *msgToGUI = MsgConfigureMeshtasticDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

QByteArray MeshtasticDemod::serialize() const
{
    return m_settings.serialize();
}

bool MeshtasticDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMeshtasticDemod *msg = MsgConfigureMeshtasticDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMeshtasticDemod *msg = MsgConfigureMeshtasticDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void MeshtasticDemod::applySettings(MeshtasticDemodSettings settings, bool force)
{
    qDebug() << "MeshtasticDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_deBits: " << settings.m_deBits
            << " m_codingScheme: " << settings.m_codingScheme
            << " m_hasHeader: " << settings.m_hasHeader
            << " m_hasCRC: " << settings.m_hasCRC
            << " m_nbParityBits: " << settings.m_nbParityBits
            << " m_packetLength: " << settings.m_packetLength
            << " m_autoNbSymbolsMax: " << settings.m_autoNbSymbolsMax
            << " m_sendViaUDP: " << settings.m_sendViaUDP
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_meshtasticKeySpecList: " << settings.m_meshtasticKeySpecList
            << " m_decodeActive: " << settings.m_decodeActive
            << " m_eomSquelchTenths: " << settings.m_eomSquelchTenths
            << " m_nbSymbolsMax: " << settings.m_nbSymbolsMax
            << " m_preambleChirps: " << settings.m_preambleChirps
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_invertRamps: " << settings.m_invertRamps
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " m_meshtasticPresetName: " << settings.m_meshtasticPresetName
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }

    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex) || force)
    {
        reverseAPIKeys.append("bandwidthIndex");
        DSPSignalNotification *msg = new DSPSignalNotification(
            MeshtasticDemodSettings::bandwidths[settings.m_bandwidthIndex],
            0);
        m_spectrumVis.getInputMessageQueue()->push(msg);
    }

    if ((settings.m_spreadFactor != m_settings.m_spreadFactor) || force) {
        reverseAPIKeys.append("spreadFactor");
    }
    if ((settings.m_deBits != m_settings.m_deBits) || force) {
        reverseAPIKeys.append("deBits");
    }

    if ((settings.m_nbParityBits != m_settings.m_nbParityBits) || force)
    {
        reverseAPIKeys.append("nbParityBits");
    }

    if ((settings.m_packetLength != m_settings.m_packetLength) || force)
    {
        reverseAPIKeys.append("packetLength");
    }

    if ((settings.m_decodeActive != m_settings.m_decodeActive) || force) {
        reverseAPIKeys.append("decodeActive");
    }
    if ((settings.m_eomSquelchTenths != m_settings.m_eomSquelchTenths) || force) {
        reverseAPIKeys.append("eomSquelchTenths");
    }
    if ((settings.m_nbSymbolsMax != m_settings.m_nbSymbolsMax) || force) {
        reverseAPIKeys.append("nbSymbolsMax");
    }
    if ((settings.m_preambleChirps != m_settings.m_preambleChirps) || force) {
        reverseAPIKeys.append("preambleChirps");
    }
    if ((settings.m_rgbColor != m_settings.m_rgbColor) || force) {
        reverseAPIKeys.append("rgbColor");
    }
    if ((settings.m_title != m_settings.m_title) || force) {
        reverseAPIKeys.append("title");
    }
    if ((settings.m_sendViaUDP != m_settings.m_sendViaUDP) || force) {
        reverseAPIKeys.append("sendViaUDP");
    }
    if ((settings.m_invertRamps != m_settings.m_invertRamps) || force) {
        reverseAPIKeys.append("invertRamps");
    }

    if ((settings.m_udpAddress != m_settings.m_udpAddress) || force)
    {
        reverseAPIKeys.append("udpAddress");
        m_udpSink.setAddress(settings.m_udpAddress);
    }

    if ((settings.m_udpPort != m_settings.m_udpPort) || force)
    {
        reverseAPIKeys.append("udpPort");
        m_udpSink.setPort(settings.m_udpPort);
    }

    if ((settings.m_meshtasticKeySpecList != m_settings.m_meshtasticKeySpecList) || force) {
        reverseAPIKeys.append("meshtasticKeySpecList");
    }

    if (m_settings.m_streamIndex != settings.m_streamIndex)
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

    if (m_running) {
        syncPipelinesWithSettings(settings, force);
    }

    // Copy LoRa params derived from the preset (bandwidth, spread factor, etc.) back into
    // settings so that m_settings and the GUI stay in sync with what was actually applied.
    // Skip for USER preset: those parameters are controlled entirely by the user via the GUI.
    if (m_running && !m_pipelineConfigs.empty() &&
        settings.m_codingScheme == MeshtasticDemodSettings::CodingLoRa &&
        settings.m_meshtasticPresetName.trimmed().compare("USER", Qt::CaseInsensitive) != 0)
    {
        const MeshtasticDemodSettings& derived = m_pipelineConfigs[0].settings;
        const bool bwChanged = (settings.m_bandwidthIndex != derived.m_bandwidthIndex);

        settings.m_spreadFactor           = derived.m_spreadFactor;
        settings.m_deBits                 = derived.m_deBits;
        settings.m_nbParityBits           = derived.m_nbParityBits;
        settings.m_preambleChirps         = derived.m_preambleChirps;
        settings.m_bandwidthIndex         = derived.m_bandwidthIndex;
        settings.m_inputFrequencyOffset   = derived.m_inputFrequencyOffset;

        if (bwChanged)
        {
            DSPSignalNotification *bwMsg = new DSPSignalNotification(
                MeshtasticDemodSettings::bandwidths[settings.m_bandwidthIndex], 0);
            m_spectrumVis.getInputMessageQueue()->push(bwMsg);
        }
    }

    if (settings.m_useReverseAPI)
    {
        bool fullUpdate = ((m_settings.m_useReverseAPI != settings.m_useReverseAPI) && settings.m_useReverseAPI) ||
                (m_settings.m_reverseAPIAddress != settings.m_reverseAPIAddress) ||
                (m_settings.m_reverseAPIPort != settings.m_reverseAPIPort) ||
                (m_settings.m_reverseAPIDeviceIndex != settings.m_reverseAPIDeviceIndex) ||
                (m_settings.m_reverseAPIChannelIndex != settings.m_reverseAPIChannelIndex);
        webapiReverseSendSettings(reverseAPIKeys, settings, fullUpdate || force);
    }

    QList<ObjectPipe*> pipes;
    MainCore::instance()->getMessagePipes().getMessagePipes(this, "settings", pipes);

    if (pipes.size() > 0) {
        sendChannelSettings(pipes, reverseAPIKeys, settings, force);
    }

    m_settings = settings;

    // Forward preset-derived settings back to GUI so controls (e.g. BW slider) reflect
    // the values actually applied. Skip for USER preset: no parameters were derived, so
    // there is nothing to sync back — and echoing would trigger an infinite apply loop
    // (GUI apply → demod echo → GUI displaySettings → rebuildMeshtasticChannelOptions
    // → queued apply → …).
    const bool isUserPreset = m_settings.m_meshtasticPresetName.trimmed().compare("USER", Qt::CaseInsensitive) == 0;
    if (!isUserPreset && getMessageQueueToGUI())
    {
        MsgConfigureMeshtasticDemod *msgToGUI = MsgConfigureMeshtasticDemod::create(m_settings, false);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

int MeshtasticDemod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshtasticDemodSettings(new SWGSDRangel::SWGMeshtasticDemodSettings());
    response.getMeshtasticDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int MeshtasticDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MeshtasticDemod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MeshtasticDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureMeshtasticDemod *msg = MsgConfigureMeshtasticDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshtasticDemod *msgToGUI = MsgConfigureMeshtasticDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void MeshtasticDemod::webapiUpdateChannelSettings(
        MeshtasticDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getMeshtasticDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getMeshtasticDemodSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getMeshtasticDemodSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getMeshtasticDemodSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("decodeActive")) {
        settings.m_decodeActive = response.getMeshtasticDemodSettings()->getDecodeActive() != 0;
    }
    if (channelSettingsKeys.contains("eomSquelchTenths")) {
        settings.m_eomSquelchTenths = response.getMeshtasticDemodSettings()->getEomSquelchTenths();
    }
    if (channelSettingsKeys.contains("nbSymbolsMax")) {
        settings.m_nbSymbolsMax = response.getMeshtasticDemodSettings()->getNbSymbolsMax();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getMeshtasticDemodSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getMeshtasticDemodSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("packetLength")) {
        settings.m_packetLength = response.getMeshtasticDemodSettings()->getPacketLength();
    }
    if (channelSettingsKeys.contains("sendViaUDP")) {
        settings.m_sendViaUDP = response.getMeshtasticDemodSettings()->getSendViaUdp() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getMeshtasticDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort"))
    {
        uint16_t port = response.getMeshtasticDemodSettings()->getUdpPort();
        settings.m_udpPort = port < 1024 ? 1024 : port;
    }
    if (channelSettingsKeys.contains("invertRamps")) {
        settings.m_invertRamps = response.getMeshtasticDemodSettings()->getInvertRamps() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMeshtasticDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getMeshtasticDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getMeshtasticDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMeshtasticDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMeshtasticDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMeshtasticDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getMeshtasticDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getMeshtasticDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getMeshtasticDemodSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getMeshtasticDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getMeshtasticDemodSettings()->getRollupState());
    }
}

int MeshtasticDemod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshtasticDemodReport(new SWGSDRangel::SWGMeshtasticDemodReport());
    response.getMeshtasticDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void MeshtasticDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const MeshtasticDemodSettings& settings)
{
    response.getMeshtasticDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getMeshtasticDemodSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getMeshtasticDemodSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getMeshtasticDemodSettings()->setDeBits(settings.m_deBits);
    response.getMeshtasticDemodSettings()->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    response.getMeshtasticDemodSettings()->setEomSquelchTenths(settings.m_eomSquelchTenths);
    response.getMeshtasticDemodSettings()->setNbSymbolsMax(settings.m_nbSymbolsMax);
    response.getMeshtasticDemodSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getMeshtasticDemodSettings()->setNbParityBits(settings.m_nbParityBits);
    response.getMeshtasticDemodSettings()->setPacketLength(settings.m_packetLength);
    response.getMeshtasticDemodSettings()->setSendViaUdp(settings.m_sendViaUDP ? 1 : 0);
    response.getMeshtasticDemodSettings()->setInvertRamps(settings.m_invertRamps ? 1 : 0);

    if (response.getMeshtasticDemodSettings()->getUdpAddress()) {
        *response.getMeshtasticDemodSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getMeshtasticDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getMeshtasticDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getMeshtasticDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getMeshtasticDemodSettings()->getTitle()) {
        *response.getMeshtasticDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getMeshtasticDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMeshtasticDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMeshtasticDemodSettings()->getReverseApiAddress()) {
        *response.getMeshtasticDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMeshtasticDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMeshtasticDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMeshtasticDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getMeshtasticDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getMeshtasticDemodSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getMeshtasticDemodSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getMeshtasticDemodSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getMeshtasticDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getMeshtasticDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getMeshtasticDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getMeshtasticDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMeshtasticDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMeshtasticDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void MeshtasticDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    if (m_running && !m_pipelines.empty() && m_pipelines[0].basebandSink) {
        response.getMeshtasticDemodReport()->setChannelSampleRate(m_pipelines[0].basebandSink->getChannelSampleRate());
    }

    response.getMeshtasticDemodReport()->setChannelPowerDb(CalcDb::dbPower(getTotalPower()));
    response.getMeshtasticDemodReport()->setSignalPowerDb(m_lastMsgSignalDb);
    response.getMeshtasticDemodReport()->setNoisePowerDb(CalcDb::dbPower(getCurrentNoiseLevel()));
    response.getMeshtasticDemodReport()->setSnrPowerDb(m_lastMsgSignalDb - m_lastMsgNoiseDb);
    response.getMeshtasticDemodReport()->setNbParityBits(m_lastMsgNbParityBits);
    response.getMeshtasticDemodReport()->setPacketLength(m_lastMsgPacketLength);
    response.getMeshtasticDemodReport()->setNbSymbols(m_lastMsgNbSymbols);
    response.getMeshtasticDemodReport()->setNbCodewords(m_lastMsgNbCodewords);
    response.getMeshtasticDemodReport()->setHeaderParityStatus(m_lastMsgHeaderParityStatus);
    response.getMeshtasticDemodReport()->setHeaderCrcStatus(m_lastMsgHeaderCRC);
    response.getMeshtasticDemodReport()->setPayloadParityStatus(m_lastMsgPayloadParityStatus);
    response.getMeshtasticDemodReport()->setPayloadCrcStatus(m_lastMsgPayloadCRC);
    response.getMeshtasticDemodReport()->setMessageTimestamp(new QString(m_lastMsgTimestamp));
    response.getMeshtasticDemodReport()->setMessageString(new QString(m_lastMsgString));
    response.getMeshtasticDemodReport()->setFrameType(new QString(m_lastFrameType));
    response.getMeshtasticDemodReport()->setChannelType(new QString(m_lastMsgPipelineName));
    response.getMeshtasticDemodReport()->setDecoding(getDemodActive() ? 1 : 0);

    response.getMeshtasticDemodReport()->setMessageBytes(new QList<QString *>);
    QList<QString *> *bytesStr = response.getMeshtasticDemodReport()->getMessageBytes();

    for (QByteArray::const_iterator it = m_lastMsgBytes.begin(); it != m_lastMsgBytes.end(); ++it)
    {
        unsigned char b = *it;
        bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
    }
}

void MeshtasticDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const MeshtasticDemodSettings& settings, bool force)
{
    SWGSDRangel::SWGChannelSettings *swgChannelSettings = new SWGSDRangel::SWGChannelSettings();
    webapiFormatChannelSettings(channelSettingsKeys, swgChannelSettings, settings, force);

    const QUrl channelSettingsURL = ChannelWebAPIUtils::buildChannelSettingsURL(
        settings.m_reverseAPIAddress,
        settings.m_reverseAPIPort,
        settings.m_reverseAPIDeviceIndex,
        settings.m_reverseAPIChannelIndex);
    m_networkRequest.setUrl(channelSettingsURL);
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

void MeshtasticDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const MeshtasticDemodSettings& settings,
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

void MeshtasticDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const MeshtasticDemodSettings& settings,
        bool force
)
{
    swgChannelSettings->setDirection(0); // Single sink (Rx)
    swgChannelSettings->setOriginatorChannelIndex(getIndexInDeviceSet());
    swgChannelSettings->setOriginatorDeviceSetIndex(getDeviceSetIndex());
    swgChannelSettings->setChannelType(new QString(m_channelId));
    swgChannelSettings->setMeshtasticDemodSettings(new SWGSDRangel::SWGMeshtasticDemodSettings());
    SWGSDRangel::SWGMeshtasticDemodSettings *swgMeshtasticDemodSettings = swgChannelSettings->getMeshtasticDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgMeshtasticDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgMeshtasticDemodSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgMeshtasticDemodSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgMeshtasticDemodSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("decodeActive") || force) {
        swgMeshtasticDemodSettings->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("eomSquelchTenths") || force) {
        swgMeshtasticDemodSettings->setEomSquelchTenths(settings.m_eomSquelchTenths);
    }
    if (channelSettingsKeys.contains("nbSymbolsMax") || force) {
        swgMeshtasticDemodSettings->setNbSymbolsMax(settings.m_nbSymbolsMax);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgMeshtasticDemodSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgMeshtasticDemodSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("packetLength") || force) {
        swgMeshtasticDemodSettings->setPacketLength(settings.m_packetLength);
    }
    if (channelSettingsKeys.contains("sendViaUDP") || force) {
        swgMeshtasticDemodSettings->setSendViaUdp(settings.m_sendViaUDP ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        swgMeshtasticDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgMeshtasticDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("invertRamps") || force) {
        swgMeshtasticDemodSettings->setInvertRamps(settings.m_invertRamps ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgMeshtasticDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        swgMeshtasticDemodSettings->setTitle(new QString(settings.m_title));
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrumConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgMeshtasticDemodSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgMeshtasticDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgMeshtasticDemodSettings->setRollupState(swgRollupState);
    }
}

void MeshtasticDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MeshtasticDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MeshtasticDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

bool MeshtasticDemod::getDemodActive() const
{
    if (!m_running) {
        return false;
    }

    for (const PipelineRuntime& runtime : m_pipelines)
    {
        if (runtime.basebandSink && runtime.basebandSink->getDemodActive()) {
            return true;
        }
    }

    return false;
}

double MeshtasticDemod::getCurrentNoiseLevel() const
{
    if (!m_running) {
        return 0.0;
    }

    double level = 0.0;

    for (const PipelineRuntime& runtime : m_pipelines)
    {
        if (runtime.basebandSink) {
            level = std::max(level, runtime.basebandSink->getCurrentNoiseLevel());
        }
    }

    return level;
}

double MeshtasticDemod::getTotalPower() const
{
    if (!m_running) {
        return 0.0;
    }

    double level = 0.0;

    for (const PipelineRuntime& runtime : m_pipelines)
    {
        if (runtime.basebandSink) {
            level = std::max(level, runtime.basebandSink->getTotalPower());
        }
    }

    return level;
}

void MeshtasticDemod::handleIndexInDeviceSetChanged(int index)
{
    if (!m_running || (index < 0)) {
        return;
    }

    for (const PipelineRuntime& runtime : m_pipelines)
    {
        if (!runtime.basebandSink) {
            continue;
        }

        QString fifoLabel = QString("%1 [%2:%3 %4]")
            .arg(m_channelId)
            .arg(m_deviceAPI->getDeviceSetIndex())
            .arg(index)
            .arg(runtime.name);
        runtime.basebandSink->setFifoLabel(fifoLabel);
    }
}
