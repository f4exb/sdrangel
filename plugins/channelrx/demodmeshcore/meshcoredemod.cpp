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
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "SWGChannelSettings.h"
#include "SWGWorkspaceInfo.h"
#include "SWGChannelReport.h"
#include "SWGMeshcoreDemodReport.h"

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "util/ax25.h"
#include "util/db.h"
#include "maincore.h"
#include "channel/channelwebapiutils.h"

#include "meshcoredemodmsg.h"
#include "meshcoredemoddecoder.h"
#include "meshcoredemod.h"
#include "meshcorepacket.h"
#include "meshcore_identity.h"

MESSAGE_CLASS_DEFINITION(MeshcoreDemod::MsgConfigureMeshcoreDemod, Message)
MESSAGE_CLASS_DEFINITION(MeshcoreDemod::MsgSetExtraPipelineSettings, Message)

const char* const MeshcoreDemod::m_channelIdURI = "sdrangel.channel.meshcoredemod";
const char* const MeshcoreDemod::m_channelId = "MeshcoreDemod";

MeshcoreDemod::MeshcoreDemod(DeviceAPI* deviceAPI) :
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
        &MeshcoreDemod::handleIndexInDeviceSetChanged
    );

    start();
}

MeshcoreDemod::~MeshcoreDemod()
{
    delete m_networkManager;
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);
    stop();
}

void MeshcoreDemod::setDeviceAPI(DeviceAPI *deviceAPI)
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

uint32_t MeshcoreDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void MeshcoreDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool pO)
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

int MeshcoreDemod::findBandwidthIndexForHz(int bandwidthHz) const
{
    if (bandwidthHz <= 0) {
        return -1;
    }

    int exactIndex = -1;
    int nearestIndex = -1;
    qint64 nearestDelta = std::numeric_limits<qint64>::max();

    for (int i = 0; i < MeshcoreDemodSettings::nbBandwidths; ++i)
    {
        const int bw = MeshcoreDemodSettings::bandwidths[i];

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

MeshcoreDemodSettings MeshcoreDemod::makePipelineSettingsFromMeshRadio(
    const MeshcoreDemodSettings& baseSettings,
    const QString& presetName,
    const modemmeshcore::TxRadioSettings& meshRadio,
    qint64 selectedPresetFrequencyHz,
    bool haveSelectedPresetFrequency
) const
{
    MeshcoreDemodSettings out = baseSettings;
    out.m_spreadFactor = meshRadio.spreadFactor;
    out.m_deBits = meshRadio.deBits;
    out.m_nbParityBits = meshRadio.parityBits;
    out.m_meshcorePresetName = presetName;
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

void MeshcoreDemod::makePipelineConfigFromSettings(int configId, PipelineConfig& config, const MeshcoreDemodSettings& settings) const
{
    // USER preset: all LoRa parameters are user-controlled; skip derivation from the mesh radio table.
    if (settings.m_meshcorePresetName.trimmed().compare("USER", Qt::CaseInsensitive) == 0)
    {
        config.id = configId;
        config.name = QString("Cnf_%1").arg(configId);
        config.presetName = settings.m_meshcorePresetName;
        config.settings = settings;
        return;
    }

    const QString region = settings.m_meshcoreRegionCode.trimmed().isEmpty()
        ? QString("EU_868")
        : settings.m_meshcoreRegionCode.trimmed();
    const int channelNum = std::max(1, settings.m_meshcoreChannelIndex + 1);

    modemmeshcore::TxRadioSettings meshRadio;
    QString error;
    const QString command = QString("MESH:preset=%1;region=%2;channel_num=%3")
        .arg(settings.m_meshcorePresetName.trimmed().isEmpty() ? QString("EU_NARROW") : settings.m_meshcorePresetName.trimmed().toUpper())
        .arg(region)
        .arg(channelNum);
    qint64 selectedPresetFrequencyHz = 0;
    bool haveSelectedPresetFrequency = false;

    if (modemmeshcore::Packet::deriveTxRadioSettings(command, meshRadio, error) && meshRadio.hasCenterFrequency)
    {
        selectedPresetFrequencyHz = meshRadio.centerFrequencyHz;
        haveSelectedPresetFrequency = true;
    }

    config.id = configId;
    config.name = QString("Cnf_%1").arg(configId);
    config.presetName = settings.m_meshcorePresetName;
    config.settings = makePipelineSettingsFromMeshRadio(
        settings,
        config.presetName,
        meshRadio,
        selectedPresetFrequencyHz,
        haveSelectedPresetFrequency
    );
}

void MeshcoreDemod::applyPipelineRuntimeSettings(PipelineRuntime& runtime, const MeshcoreDemodSettings& settings, bool force)
{
    runtime.settings = settings;

    if (runtime.decoder)
    {
        runtime.decoder->setCodingScheme(MeshcoreDemodSettings::m_codingScheme);
        runtime.decoder->setNbSymbolBits(settings.m_spreadFactor, settings.m_deBits);
        runtime.decoder->setLoRaHasHeader(MeshcoreDemodSettings::m_hasHeader);
        runtime.decoder->setLoRaHasCRC(MeshcoreDemodSettings::m_hasCRC);
        runtime.decoder->setLoRaParityBits(settings.m_nbParityBits);
        runtime.decoder->setLoRaPacketLength(settings.m_packetLength);
        runtime.decoder->setLoRaBandwidth(MeshcoreDemodSettings::bandwidths[settings.m_bandwidthIndex]);
    }

    if (runtime.basebandSink)
    {
        MeshcoreDemodBaseband::MsgConfigureMeshcoreDemodBaseband *msg =
            MeshcoreDemodBaseband::MsgConfigureMeshcoreDemodBaseband::create(settings, force);
        runtime.basebandSink->getInputMessageQueue()->push(msg);
    }
}

void MeshcoreDemod::startPipelines(const std::vector<PipelineConfig>& configs)
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
        runtime.decoder = new MeshcoreDemodDecoder();
        runtime.decoder->setOutputMessageQueue(getInputMessageQueue());
        runtime.decoder->setPipelineMetadata(runtime.id, runtime.name, runtime.presetName);
        runtime.decoder->moveToThread(runtime.decoderThread);

        QObject::connect(runtime.decoderThread, &QThread::finished, runtime.decoder, &QObject::deleteLater);
        runtime.decoderThread->start();

        runtime.basebandThread = new QThread();
        runtime.basebandSink = new MeshcoreDemodBaseband();

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

void MeshcoreDemod::stopPipelines()
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

bool MeshcoreDemod::pipelineLayoutMatches(const std::vector<PipelineConfig>& configs) const
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

void MeshcoreDemod::syncPipelinesWithSettings(const MeshcoreDemodSettings& settings, bool force)
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
}

void MeshcoreDemod::applyExtraPipelineSettings(const QVector<MeshcoreDemodSettings>& settingsList, bool force)
{
    if (!m_running) {
        return;
    }

    // Remove all secondary configs (indices 1+), keeping only the primary (index 0).
    while (m_pipelineConfigs.size() > 1) {
        m_pipelineConfigs.pop_back();
    }

    // Build new secondary configs from the provided settings list.
    // Settings are used as-is (the GUI has already applied preset derivation and frequency offsets).
    for (int i = 0; i < settingsList.size(); ++i)
    {
        PipelineConfig config;
        const int configId = i + 1;
        config.id = configId;
        config.name = QString("Cnf_%1").arg(configId);
        config.presetName = settingsList[i].m_meshcorePresetName;
        config.settings = settingsList[i];
        m_pipelineConfigs.push_back(config);
    }

    // Determine if the layout (pipeline count / IDs) has changed.
    const bool layoutChanged = !pipelineLayoutMatches(m_pipelineConfigs);

    if (layoutChanged)
    {
        // Stop and destroy all secondary runtimes (primary at index 0 stays).
        for (int i = static_cast<int>(m_pipelines.size()) - 1; i >= 1; --i)
        {
            PipelineRuntime& rt = m_pipelines[i];
            if (rt.basebandThread)
            {
                rt.basebandThread->exit();
                rt.basebandThread->wait();
                delete rt.basebandThread;
                rt.basebandThread = nullptr;
            }
            if (rt.decoderThread)
            {
                rt.decoderThread->exit();
                rt.decoderThread->wait();
                delete rt.decoderThread;
                rt.decoderThread = nullptr;
            }
            rt.basebandSink = nullptr;
            rt.decoder = nullptr;
        }
        while (m_pipelines.size() > 1) {
            m_pipelines.pop_back();
        }

        // Start fresh secondary runtimes.
        for (int i = 1; i < static_cast<int>(m_pipelineConfigs.size()); ++i)
        {
            const PipelineConfig& config = m_pipelineConfigs[i];
            PipelineRuntime runtime;
            runtime.id = config.id;
            runtime.name = config.name;
            runtime.presetName = config.presetName;
            runtime.settings = config.settings;

            runtime.decoderThread = new QThread();
            runtime.decoder = new MeshcoreDemodDecoder();
            runtime.decoder->setOutputMessageQueue(getInputMessageQueue());
            runtime.decoder->setPipelineMetadata(runtime.id, runtime.name, runtime.presetName);
            runtime.decoder->moveToThread(runtime.decoderThread);
            QObject::connect(runtime.decoderThread, &QThread::finished, runtime.decoder, &QObject::deleteLater);
            runtime.decoderThread->start();

            runtime.basebandThread = new QThread();
            runtime.basebandSink = new MeshcoreDemodBaseband();
            // Secondary pipelines (id != 0) do not own the spectrum visualiser.
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
    else
    {
        // Layout unchanged — just update settings for each secondary pipeline.
        for (int i = 1; i < static_cast<int>(m_pipelineConfigs.size()); ++i)
        {
            m_pipelines[i].id = m_pipelineConfigs[i].id;
            m_pipelines[i].name = m_pipelineConfigs[i].name;
            m_pipelines[i].presetName = m_pipelineConfigs[i].presetName;
            if (m_pipelines[i].decoder) {
                m_pipelines[i].decoder->setPipelineMetadata(m_pipelineConfigs[i].id, m_pipelineConfigs[i].name, m_pipelineConfigs[i].presetName);
            }
            applyPipelineRuntimeSettings(m_pipelines[i], m_pipelineConfigs[i].settings, force);
        }
    }

    qDebug() << "MeshcoreDemod::applyExtraPipelineSettings: total pipelines=" << m_pipelines.size();
}

void MeshcoreDemod::start()
{
    if (m_running) {
        return;
    }

    qDebug() << "MeshcoreDemod::start";
    m_pipelineConfigs.emplace_back();
    m_currentPipelineId = 0;
    makePipelineConfigFromSettings(m_currentPipelineId, m_pipelineConfigs.back(), m_settings);
    startPipelines(m_pipelineConfigs);

    SpectrumSettings spectrumSettings = m_spectrumVis.getSettings();
    spectrumSettings.m_ssb = true;
    SpectrumVis::MsgConfigureSpectrumVis *msg = SpectrumVis::MsgConfigureSpectrumVis::create(spectrumSettings, false);
    m_spectrumVis.getInputMessageQueue()->push(msg);

    m_running = true;
}

void MeshcoreDemod::stop()
{
    if (!m_running) {
        return;
    }

    qDebug() << "MeshcoreDemod::stop";
    m_running = false;
    stopPipelines();
    m_pipelineConfigs.clear();
}

namespace {

QString syncWordToPacketType(uint8_t syncWord)
{
    switch (syncWord)
    {
    case 0x2B: return QStringLiteral("chirpchat");
    case 0x12: return QStringLiteral("meshcore");
    case 0x43: return QStringLiteral("helium");
    case 0x00: return QStringLiteral("unset");
    default:   return QStringLiteral("custom");
    }
}

QString parityStatusToStr(int status)
{
    switch (status)
    {
    case (int) MeshcoreDemodSettings::ParityOK:        return QStringLiteral("ok");
    case (int) MeshcoreDemodSettings::ParityCorrected: return QStringLiteral("fix");
    case (int) MeshcoreDemodSettings::ParityError:     return QStringLiteral("err");
    default:                                              return QStringLiteral("n/a");
    }
}

QString getMeshField(const modemmeshcore::DecodeResult& result, const QString& path)
{
    for (const auto& field : result.fields)
    {
        if (field.path == path) {
            return field.value;
        }
    }
    return QString();
}

} // namespace

QString MeshcoreDemod::buildMeshcoreJsonPacket(
    const MeshcoreDemodMsg::MsgReportDecodeBytes& msg,
    const modemmeshcore::DecodeResult& meshResult) const
{
    QJsonObject root;

    // Timestamps
    const QDateTime now = QDateTime::currentDateTime();
    root["timestamp"]      = now.toString("yyyy-MM-ddTHH:mm:ss.zzz");
    root["timestamp_unix"] = now.toMSecsSinceEpoch();

    // RF
    QJsonObject rf;
    rf["center_freq_hz"]  = static_cast<qint64>(
        m_basebandCenterFrequency + m_settings.m_inputFrequencyOffset);
    rf["bandwidth_hz"]    = MeshcoreDemodSettings::bandwidths[m_settings.m_bandwidthIndex];
    rf["spreading_factor"] = m_settings.m_spreadFactor;
    rf["signal_db"]       = msg.getSingalDb();
    rf["noise_db"]        = msg.getNoiseDb();
    rf["snr_db"]          = msg.getSingalDb() - msg.getNoiseDb();
    root["rf"] = rf;

    // LoRa
    const uint8_t syncWord  = static_cast<uint8_t>(msg.getSyncWord());
    const QString packetType = syncWordToPacketType(syncWord);

    QJsonObject lora;
    lora["packet_type"]  = packetType;
    lora["sync_word"]    = QString("0x%1").arg(syncWord, 2, 16, QChar('0'));
    lora["header_fec"]   = parityStatusToStr(msg.getHeaderParityStatus());
    lora["header_crc"]   = msg.getHeaderCRCStatus() ? QStringLiteral("ok") : QStringLiteral("err");

    if (msg.getEarlyEOM())
    {
        lora["payload_fec"] = QStringLiteral("n/a");
        lora["payload_crc"] = QStringLiteral("n/a");
    }
    else
    {
        lora["payload_fec"] = parityStatusToStr(msg.getPayloadParityStatus());
        lora["payload_crc"] = msg.getPayloadCRCStatus() ? QStringLiteral("ok") : QStringLiteral("err");
    }

    lora["early_eom"]     = msg.getEarlyEOM();
    lora["packet_length"] = static_cast<int>(msg.getPacketSize());
    lora["nb_symbols"]    = static_cast<int>(msg.getNbSymbols());
    lora["nb_codewords"]  = static_cast<int>(msg.getNbCodewords());
    lora["payload_hex"]   = QString(msg.getBytes().left(
        static_cast<int>(msg.getPacketSize())).toHex());
    root["lora"] = lora;

    // MeshCore section — sync word 0x12. Emits decoded fields from
    // modemmeshcore::Packet::decodeFrame: payload type, decrypted plaintext
    // when our identity / channel PSK was the recipient, ADVERT name+pubkey,
    // etc. Operators reading the UDP JSON now see *what* was sent on-air,
    // not just the raw ciphertext under `lora.payload_hex`.
    if (syncWord == 0x12)
    {
        QJsonObject mesh;
        mesh["decoded"]   = meshResult.isFrame;
        mesh["decrypted"] = meshResult.decrypted;
        if (!meshResult.summary.isEmpty()) {
            mesh["summary"] = meshResult.summary;
        }
        if (!meshResult.keyLabel.isEmpty()) {
            mesh["key_label"] = meshResult.keyLabel;
        }
        QJsonObject fields;
        for (const auto& field : meshResult.fields) {
            fields[field.path] = field.value;
        }
        mesh["fields"] = fields;
        root["meshcore"] = mesh;
    }
    // Meshtastic-format report (sync word 0x2B) — emitted only when the
    // demodulator is fed Meshtastic traffic on a non-MeshCore sync word.
    // Useful for diagnostic / cross-protocol debugging.
    else if (syncWord == 0x2B)
    {
        QJsonObject mesh;
        const bool headerCrcOk = msg.getHeaderCRCStatus();

        const QString channelHash = getMeshField(meshResult, "header.channel_hash");
        if (headerCrcOk && !channelHash.isEmpty()) {
            mesh["channel_hash"] = channelHash;
        } else {
            mesh["channel_hash"] = QStringLiteral("unknown");
        }

        const QString packetId = getMeshField(meshResult, "header.id");
        if (headerCrcOk && !packetId.isEmpty()) {
            mesh["packet_id"] = packetId;
        } else {
            mesh["packet_id"] = QStringLiteral("unknown");
        }

        const QString hopStart  = getMeshField(meshResult, "header.hop_start");
        const QString hopLimit  = getMeshField(meshResult, "header.hop_limit");
        const QString relayNode = getMeshField(meshResult, "header.relay_node");

        if (headerCrcOk)
        {
            if (!hopStart.isEmpty()) {
                mesh["hop_start"] = hopStart.toInt();
            } else {
                mesh["hop_start"] = QStringLiteral("unknown");
            }

            if (!hopLimit.isEmpty()) {
                mesh["hop_limit"] = hopLimit.toInt();
            } else {
                mesh["hop_limit"] = QStringLiteral("unknown");
            }

            if (!hopStart.isEmpty() && !hopLimit.isEmpty()) {
                mesh["hops_consumed"] = hopStart.toInt() - hopLimit.toInt();
            } else {
                mesh["hops_consumed"] = QStringLiteral("unknown");
            }

            if (!relayNode.isEmpty()) {
                mesh["relay_node"] = relayNode.toInt();
            } else {
                mesh["relay_node"] = QStringLiteral("unknown");
            }
        }
        else
        {
            mesh["hop_start"] = QStringLiteral("unknown");
            mesh["hop_limit"] = QStringLiteral("unknown");
            mesh["hops_consumed"] = QStringLiteral("unknown");
            mesh["relay_node"] = QStringLiteral("unknown");
        }

        const QString decodePath = getMeshField(meshResult, "decode.path");
        const QString keyLabel   = getMeshField(meshResult, "decode.key_label");

        QString decryption;
        QString keyLabelOut;

        if (decodePath == QStringLiteral("plain"))
        {
            decryption  = QStringLiteral("plaintext");
            keyLabelOut = QStringLiteral("no_key");
            mesh["hash_matching_index"] = QStringLiteral("none");
        }
        else if (decodePath == QStringLiteral("aes_ctr_be"))
        {
            decryption  = QStringLiteral("decrypted");
            keyLabelOut = keyLabel.isEmpty() ? QStringLiteral("unknown_key") : keyLabel;
            mesh["hash_matching_index"] = 0;
        }
        else
        {
            decryption  = QStringLiteral("not_decrypted");
            keyLabelOut = QStringLiteral("unknown_key");
            mesh["hash_matching_index"] = QStringLiteral("none");
        }

        mesh["decryption"] = decryption;
        mesh["key_label"]  = keyLabelOut;
        mesh["parsed"]     = meshResult.dataDecoded;

        if (meshResult.dataDecoded)
        {
            mesh["channel_type"] = m_settings.m_meshcorePresetName;

            QJsonObject fields;
            for (const auto& field : meshResult.fields)
            {
                if (field.path.startsWith(QStringLiteral("decode."))) {
                    continue;
                }
                fields[field.path] = field.value;
            }
            mesh["fields"] = fields;
        }

        root["meshcore"] = mesh;
    }

    return QString(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

bool MeshcoreDemod::handleMessage(const Message& cmd)
{
	if (MsgConfigureMeshcoreDemod::match(cmd))
	{
		qDebug() << "MeshcoreDemod::handleMessage: MsgConfigureMeshcoreDemod";
		MsgConfigureMeshcoreDemod& cfg = (MsgConfigureMeshcoreDemod&) cmd;
		MeshcoreDemodSettings settings = cfg.getSettings();
		applySettings(settings, cfg.getForce());

		return true;
	}
    else if (MsgSetExtraPipelineSettings::match(cmd))
    {
        qDebug() << "MeshcoreDemod::handleMessage: MsgSetExtraPipelineSettings";
        const auto& msg = static_cast<const MsgSetExtraPipelineSettings&>(cmd);
        applyExtraPipelineSettings(msg.getSettingsList(), false);
        return true;
    }
    else if (MeshcoreDemodMsg::MsgReportDecodeBytes::match(cmd))
    {
        qDebug() << "MeshcoreDemod::handleMessage: MsgReportDecodeBytes";
        MeshcoreDemodMsg::MsgReportDecodeBytes& msg = (MeshcoreDemodMsg::MsgReportDecodeBytes&) cmd;

        m_lastMsgSignalDb = msg.getSingalDb();
        m_lastMsgNoiseDb = msg.getNoiseDb();
        m_lastMsgSyncWord = msg.getSyncWord();
        m_lastMsgTimestamp = msg.getMsgTimestamp();

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
            getMessageQueueToGUI()->push(new MeshcoreDemodMsg::MsgReportDecodeBytes(msg)); // make a copy
        }

        modemmeshcore::DecodeResult meshResult;

        // Augment the user-provided key spec list with sensible defaults so
        // common-case decoding works out-of-the-box:
        //   identity=<our seed>           — trial-decrypts TXT_MSG / ANON_REQ
        //                                   addressed to our pubkey
        //   channel:public=8b3387e9...    — fixed MeshCore Public channel PSK
        //                                   per firmware convention; lets
        //                                   GRP_TXT on the public group decode
        //                                   without the operator typing it
        //                                   in the Keys dialog.
        // Both are no-ops if the operator already supplied them.
        //
        // Environment override: SDRANGEL_MESHCORE_KEYS prepends key specs,
        // useful for CI/headless testing. GUI-specified keys come next so
        // they can still contribute (both env and GUI keys are tried).
        QString envKeys = modemmeshcore::Packet::defaultKeysFromEnv();
        QString effectiveKeys = envKeys.isEmpty()
            ? m_settings.m_meshcoreKeySpecList
            : (m_settings.m_meshcoreKeySpecList.isEmpty()
                ? envKeys
                : envKeys + QStringLiteral("; ") + m_settings.m_meshcoreKeySpecList);
        auto appendSep = [&](){
            if (!effectiveKeys.isEmpty()
                && !effectiveKeys.endsWith(QChar(';'))) {
                effectiveKeys.append(QStringLiteral("; "));
            }
        };
        if (!effectiveKeys.contains(QStringLiteral("identity="), Qt::CaseInsensitive))
        {
            modemmeshcore::identity::Identity self =
                modemmeshcore::identity::loadOrCreateIdentity(
                    modemmeshcore::identity::defaultIdentityPath());
            if (self.isValid())
            {
                appendSep();
                effectiveKeys.append(QStringLiteral("identity="));
                effectiveKeys.append(self.seedHex());
            }
        }
        if (!effectiveKeys.contains(QStringLiteral("channel:public"), Qt::CaseInsensitive))
        {
            appendSep();
            effectiveKeys.append(QStringLiteral("channel:public=8b3387e9c5cdea6ac9e5edbaa115cd72"));
        }

        if (modemmeshcore::Packet::decodeFrame(m_lastMsgBytes, meshResult, effectiveKeys))
        {
            m_lastMsgString = meshResult.summary;

            for (const modemmeshcore::DecodeResult::Field& field : meshResult.fields)
            {
                if (field.path == QStringLiteral("data.port_name"))
                {
                    m_lastFrameType = field.value;
                    break;
                }
            }

            qInfo() << "MeshcoreDemod::handleMessage:" << meshResult.summary;

            if (meshResult.dataDecoded && getMessageQueueToGUI())
            {
                MeshcoreDemodMsg::MsgReportDecodeString *meshMsg = MeshcoreDemodMsg::MsgReportDecodeString::create(meshResult.summary);
                meshMsg->setFrameId(msg.getFrameId());
                meshMsg->setSyncWord(msg.getSyncWord());
                meshMsg->setSignalDb(msg.getSingalDb());
                meshMsg->setNoiseDb(msg.getNoiseDb());
                meshMsg->setMsgTimestamp(msg.getMsgTimestamp());
                meshMsg->setPipelineMetadata(msg.getPipelineId(), msg.getPipelineName(), msg.getPipelinePreset());
                QVector<QPair<QString, QString>> structuredFields;
                structuredFields.reserve(meshResult.fields.size());

                for (const modemmeshcore::DecodeResult::Field& field : meshResult.fields) {
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

        if (m_settings.m_sendJsonViaUDP)
        {
            const QString json = buildMeshcoreJsonPacket(msg, meshResult);
            const QByteArray jsonBytes = json.toUtf8();
            m_udpSink.writeUnbuffered(
                reinterpret_cast<const uint8_t*>(jsonBytes.constData()),
                jsonBytes.size());
        }

        return true;
    }
    else if (MeshcoreDemodMsg::MsgReportDecodeString::match(cmd))
    {
        qDebug() << "MeshcoreDemod::handleMessage: MsgReportDecodeString";
        MeshcoreDemodMsg::MsgReportDecodeString& msg = (MeshcoreDemodMsg::MsgReportDecodeString&) cmd;
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
            getMessageQueueToGUI()->push(new MeshcoreDemodMsg::MsgReportDecodeString(msg)); // make a copy
        }

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_basebandCenterFrequency = notif.getCenterFrequency();
        m_haveBasebandCenterFrequency = true;
        qDebug() << "MeshcoreDemod::handleMessage: DSPSignalNotification: m_basebandSampleRate: " << m_basebandSampleRate;

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

void MeshcoreDemod::setCenterFrequency(qint64 frequency)
{
    MeshcoreDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshcoreDemod *msgToGUI = MsgConfigureMeshcoreDemod::create(settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

QByteArray MeshcoreDemod::serialize() const
{
    return m_settings.serialize();
}

bool MeshcoreDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureMeshcoreDemod *msg = MsgConfigureMeshcoreDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureMeshcoreDemod *msg = MsgConfigureMeshcoreDemod::create(m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void MeshcoreDemod::applySettings(MeshcoreDemodSettings settings, bool force)
{
    qDebug() << "MeshcoreDemod::applySettings:"
            << " m_inputFrequencyOffset: " << settings.m_inputFrequencyOffset
            << " m_bandwidthIndex: " << settings.m_bandwidthIndex
            << " m_spreadFactor: " << settings.m_spreadFactor
            << " m_deBits: " << settings.m_deBits
            << " m_codingScheme: " << MeshcoreDemodSettings::m_codingScheme
            << " m_hasHeader: " << MeshcoreDemodSettings::m_hasHeader
            << " m_hasCRC: " << MeshcoreDemodSettings::m_hasCRC
            << " m_nbParityBits: " << settings.m_nbParityBits
            << " m_packetLength: " << settings.m_packetLength
            << " m_autoNbSymbolsMax: " << MeshcoreDemodSettings::m_autoNbSymbolsMax
            << " m_sendViaUDP: " << settings.m_sendViaUDP
            << " m_sendJsonViaUDP: " << settings.m_sendJsonViaUDP
            << " m_udpAddress: " << settings.m_udpAddress
            << " m_udpPort: " << settings.m_udpPort
            << " m_meshcoreKeySpecList: " << settings.m_meshcoreKeySpecList
            << " m_decodeActive: " << settings.m_decodeActive
            << " m_eomSquelchTenths: " << settings.m_eomSquelchTenths
            << " m_nbSymbolsMax: " << settings.m_nbSymbolsMax
            << " m_preambleChirps: " << settings.m_preambleChirps
            << " m_streamIndex: " << settings.m_streamIndex
            << " m_useReverseAPI: " << settings.m_useReverseAPI
            << " m_invertRamps: " << settings.m_invertRamps
            << " m_rgbColor: " << settings.m_rgbColor
            << " m_title: " << settings.m_title
            << " m_meshcorePresetName: " << settings.m_meshcorePresetName
            << " force: " << force;

    QList<QString> reverseAPIKeys;

    if ((settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset) || force) {
        reverseAPIKeys.append("inputFrequencyOffset");
    }

    if ((settings.m_bandwidthIndex != m_settings.m_bandwidthIndex) || force)
    {
        reverseAPIKeys.append("bandwidthIndex");
        DSPSignalNotification *msg = new DSPSignalNotification(
            MeshcoreDemodSettings::bandwidths[settings.m_bandwidthIndex],
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
    if ((settings.m_sendJsonViaUDP != m_settings.m_sendJsonViaUDP) || force) {
        reverseAPIKeys.append("sendJsonViaUDP");
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

    if ((settings.m_meshcoreKeySpecList != m_settings.m_meshcoreKeySpecList) || force) {
        reverseAPIKeys.append("meshcoreKeySpecList");
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
        settings.m_meshcorePresetName.trimmed().compare("USER", Qt::CaseInsensitive) != 0)
    {
        const MeshcoreDemodSettings& derived = m_pipelineConfigs[0].settings;
        const bool bwChanged = (settings.m_bandwidthIndex != derived.m_bandwidthIndex);

        settings.m_spreadFactor           = derived.m_spreadFactor;
        settings.m_deBits                 = derived.m_deBits;
        settings.m_nbParityBits           = derived.m_nbParityBits;
        settings.m_preambleChirps         = derived.m_preambleChirps;
        settings.m_bandwidthIndex         = derived.m_bandwidthIndex;
        settings.m_inputFrequencyOffset   = derived.m_inputFrequencyOffset;

        if (bwChanged)
        {
            auto *bwMsg = new DSPSignalNotification(
                MeshcoreDemodSettings::bandwidths[settings.m_bandwidthIndex], 0);
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
    // (GUI apply → demod echo → GUI displaySettings → rebuildMeshcoreChannelOptions
    // → queued apply → …).
    const bool isUserPreset = m_settings.m_meshcorePresetName.trimmed().compare("USER", Qt::CaseInsensitive) == 0;
    if (!isUserPreset && getMessageQueueToGUI())
    {
        MsgConfigureMeshcoreDemod *msgToGUI = MsgConfigureMeshcoreDemod::create(m_settings, false);
        getMessageQueueToGUI()->push(msgToGUI);
    }
}

int MeshcoreDemod::webapiSettingsGet(
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshcoreDemodSettings(new SWGSDRangel::SWGMeshcoreDemodSettings());
    response.getMeshcoreDemodSettings()->init();
    webapiFormatChannelSettings(response, m_settings);

    return 200;
}

int MeshcoreDemod::webapiWorkspaceGet(
        SWGSDRangel::SWGWorkspaceInfo& response,
        QString& errorMessage)
{
    (void) errorMessage;
    response.setIndex(m_settings.m_workspaceIndex);
    return 200;
}

int MeshcoreDemod::webapiSettingsPutPatch(
    bool force,
    const QStringList& channelSettingsKeys,
    SWGSDRangel::SWGChannelSettings& response,
    QString& errorMessage)
{
    (void) errorMessage;
    MeshcoreDemodSettings settings = m_settings;
    webapiUpdateChannelSettings(settings, channelSettingsKeys, response);

    MsgConfigureMeshcoreDemod *msg = MsgConfigureMeshcoreDemod::create(settings, force);
    m_inputMessageQueue.push(msg);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureMeshcoreDemod *msgToGUI = MsgConfigureMeshcoreDemod::create(settings, force);
        m_guiMessageQueue->push(msgToGUI);
    }

    webapiFormatChannelSettings(response, settings);

    return 200;
}

void MeshcoreDemod::webapiUpdateChannelSettings(
        MeshcoreDemodSettings& settings,
        const QStringList& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings& response)
{
    if (channelSettingsKeys.contains("inputFrequencyOffset")) {
        settings.m_inputFrequencyOffset = response.getMeshcoreDemodSettings()->getInputFrequencyOffset();
    }
    if (channelSettingsKeys.contains("bandwidthIndex")) {
        settings.m_bandwidthIndex = response.getMeshcoreDemodSettings()->getBandwidthIndex();
    }
    if (channelSettingsKeys.contains("spreadFactor")) {
        settings.m_spreadFactor = response.getMeshcoreDemodSettings()->getSpreadFactor();
    }
    if (channelSettingsKeys.contains("deBits")) {
        settings.m_deBits = response.getMeshcoreDemodSettings()->getDeBits();
    }
    if (channelSettingsKeys.contains("decodeActive")) {
        settings.m_decodeActive = response.getMeshcoreDemodSettings()->getDecodeActive() != 0;
    }
    if (channelSettingsKeys.contains("eomSquelchTenths")) {
        settings.m_eomSquelchTenths = response.getMeshcoreDemodSettings()->getEomSquelchTenths();
    }
    if (channelSettingsKeys.contains("nbSymbolsMax")) {
        settings.m_nbSymbolsMax = response.getMeshcoreDemodSettings()->getNbSymbolsMax();
    }
    if (channelSettingsKeys.contains("preambleChirps")) {
        settings.m_preambleChirps = response.getMeshcoreDemodSettings()->getPreambleChirps();
    }
    if (channelSettingsKeys.contains("nbParityBits")) {
        settings.m_nbParityBits = response.getMeshcoreDemodSettings()->getNbParityBits();
    }
    if (channelSettingsKeys.contains("packetLength")) {
        settings.m_packetLength = response.getMeshcoreDemodSettings()->getPacketLength();
    }
    if (channelSettingsKeys.contains("sendViaUDP")) {
        settings.m_sendViaUDP = response.getMeshcoreDemodSettings()->getSendViaUdp() != 0;
    }
    if (channelSettingsKeys.contains("sendJsonViaUDP")) {
        settings.m_sendJsonViaUDP = response.getMeshcoreDemodSettings()->getSendJsonViaUdp() != 0;
    }
    if (channelSettingsKeys.contains("udpAddress")) {
        settings.m_udpAddress = *response.getMeshcoreDemodSettings()->getUdpAddress();
    }
    if (channelSettingsKeys.contains("udpPort"))
    {
        uint16_t port = response.getMeshcoreDemodSettings()->getUdpPort();
        settings.m_udpPort = port < 1024 ? 1024 : port;
    }
    if (channelSettingsKeys.contains("invertRamps")) {
        settings.m_invertRamps = response.getMeshcoreDemodSettings()->getInvertRamps() != 0;
    }
    if (channelSettingsKeys.contains("rgbColor")) {
        settings.m_rgbColor = response.getMeshcoreDemodSettings()->getRgbColor();
    }
    if (channelSettingsKeys.contains("title")) {
        settings.m_title = *response.getMeshcoreDemodSettings()->getTitle();
    }
    if (channelSettingsKeys.contains("streamIndex")) {
        settings.m_streamIndex = response.getMeshcoreDemodSettings()->getStreamIndex();
    }
    if (channelSettingsKeys.contains("useReverseAPI")) {
        settings.m_useReverseAPI = response.getMeshcoreDemodSettings()->getUseReverseApi() != 0;
    }
    if (channelSettingsKeys.contains("reverseAPIAddress")) {
        settings.m_reverseAPIAddress = *response.getMeshcoreDemodSettings()->getReverseApiAddress();
    }
    if (channelSettingsKeys.contains("reverseAPIPort")) {
        settings.m_reverseAPIPort = response.getMeshcoreDemodSettings()->getReverseApiPort();
    }
    if (channelSettingsKeys.contains("reverseAPIDeviceIndex")) {
        settings.m_reverseAPIDeviceIndex = response.getMeshcoreDemodSettings()->getReverseApiDeviceIndex();
    }
    if (channelSettingsKeys.contains("reverseAPIChannelIndex")) {
        settings.m_reverseAPIChannelIndex = response.getMeshcoreDemodSettings()->getReverseApiChannelIndex();
    }
    if (settings.m_spectrumGUI && channelSettingsKeys.contains("spectrumConfig")) {
        settings.m_spectrumGUI->updateFrom(channelSettingsKeys, response.getMeshcoreDemodSettings()->getSpectrumConfig());
    }
    if (settings.m_channelMarker && channelSettingsKeys.contains("channelMarker")) {
        settings.m_channelMarker->updateFrom(channelSettingsKeys, response.getMeshcoreDemodSettings()->getChannelMarker());
    }
    if (settings.m_rollupState && channelSettingsKeys.contains("rollupState")) {
        settings.m_rollupState->updateFrom(channelSettingsKeys, response.getMeshcoreDemodSettings()->getRollupState());
    }
}

int MeshcoreDemod::webapiReportGet(
    SWGSDRangel::SWGChannelReport& response,
    QString& errorMessage)
{
    (void) errorMessage;
    response.setMeshcoreDemodReport(new SWGSDRangel::SWGMeshcoreDemodReport());
    response.getMeshcoreDemodReport()->init();
    webapiFormatChannelReport(response);
    return 200;
}

void MeshcoreDemod::webapiFormatChannelSettings(SWGSDRangel::SWGChannelSettings& response, const MeshcoreDemodSettings& settings)
{
    response.getMeshcoreDemodSettings()->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    response.getMeshcoreDemodSettings()->setBandwidthIndex(settings.m_bandwidthIndex);
    response.getMeshcoreDemodSettings()->setSpreadFactor(settings.m_spreadFactor);
    response.getMeshcoreDemodSettings()->setDeBits(settings.m_deBits);
    response.getMeshcoreDemodSettings()->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    response.getMeshcoreDemodSettings()->setEomSquelchTenths(settings.m_eomSquelchTenths);
    response.getMeshcoreDemodSettings()->setNbSymbolsMax(settings.m_nbSymbolsMax);
    response.getMeshcoreDemodSettings()->setPreambleChirps(settings.m_preambleChirps);
    response.getMeshcoreDemodSettings()->setNbParityBits(settings.m_nbParityBits);
    response.getMeshcoreDemodSettings()->setPacketLength(settings.m_packetLength);
    response.getMeshcoreDemodSettings()->setSendViaUdp(settings.m_sendViaUDP ? 1 : 0);
    response.getMeshcoreDemodSettings()->setSendJsonViaUdp(settings.m_sendJsonViaUDP ? 1 : 0);
    response.getMeshcoreDemodSettings()->setInvertRamps(settings.m_invertRamps ? 1 : 0);

    if (response.getMeshcoreDemodSettings()->getUdpAddress()) {
        *response.getMeshcoreDemodSettings()->getUdpAddress() = settings.m_udpAddress;
    } else {
        response.getMeshcoreDemodSettings()->setUdpAddress(new QString(settings.m_udpAddress));
    }

    response.getMeshcoreDemodSettings()->setUdpPort(settings.m_udpPort);
    response.getMeshcoreDemodSettings()->setRgbColor(settings.m_rgbColor);

    if (response.getMeshcoreDemodSettings()->getTitle()) {
        *response.getMeshcoreDemodSettings()->getTitle() = settings.m_title;
    } else {
        response.getMeshcoreDemodSettings()->setTitle(new QString(settings.m_title));
    }

    response.getMeshcoreDemodSettings()->setUseReverseApi(settings.m_useReverseAPI ? 1 : 0);

    if (response.getMeshcoreDemodSettings()->getReverseApiAddress()) {
        *response.getMeshcoreDemodSettings()->getReverseApiAddress() = settings.m_reverseAPIAddress;
    } else {
        response.getMeshcoreDemodSettings()->setReverseApiAddress(new QString(settings.m_reverseAPIAddress));
    }

    response.getMeshcoreDemodSettings()->setReverseApiPort(settings.m_reverseAPIPort);
    response.getMeshcoreDemodSettings()->setReverseApiDeviceIndex(settings.m_reverseAPIDeviceIndex);
    response.getMeshcoreDemodSettings()->setReverseApiChannelIndex(settings.m_reverseAPIChannelIndex);

    if (settings.m_spectrumGUI)
    {
        if (response.getMeshcoreDemodSettings()->getSpectrumConfig())
        {
            settings.m_spectrumGUI->formatTo(response.getMeshcoreDemodSettings()->getSpectrumConfig());
        }
        else
        {
            SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
            settings.m_spectrumGUI->formatTo(swgGLSpectrum);
            response.getMeshcoreDemodSettings()->setSpectrumConfig(swgGLSpectrum);
        }
    }

    if (settings.m_channelMarker)
    {
        if (response.getMeshcoreDemodSettings()->getChannelMarker())
        {
            settings.m_channelMarker->formatTo(response.getMeshcoreDemodSettings()->getChannelMarker());
        }
        else
        {
            SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
            settings.m_channelMarker->formatTo(swgChannelMarker);
            response.getMeshcoreDemodSettings()->setChannelMarker(swgChannelMarker);
        }
    }

    if (settings.m_rollupState)
    {
        if (response.getMeshcoreDemodSettings()->getRollupState())
        {
            settings.m_rollupState->formatTo(response.getMeshcoreDemodSettings()->getRollupState());
        }
        else
        {
            SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
            settings.m_rollupState->formatTo(swgRollupState);
            response.getMeshcoreDemodSettings()->setRollupState(swgRollupState);
        }
    }
}

void MeshcoreDemod::webapiFormatChannelReport(SWGSDRangel::SWGChannelReport& response)
{
    if (m_running && !m_pipelines.empty() && m_pipelines[0].basebandSink) {
        response.getMeshcoreDemodReport()->setChannelSampleRate(m_pipelines[0].basebandSink->getChannelSampleRate());
    }

    response.getMeshcoreDemodReport()->setChannelPowerDb(CalcDb::dbPower(getTotalPower()));
    response.getMeshcoreDemodReport()->setSignalPowerDb(m_lastMsgSignalDb);
    response.getMeshcoreDemodReport()->setNoisePowerDb(CalcDb::dbPower(getCurrentNoiseLevel()));
    response.getMeshcoreDemodReport()->setSnrPowerDb(m_lastMsgSignalDb - m_lastMsgNoiseDb);
    response.getMeshcoreDemodReport()->setNbParityBits(m_lastMsgNbParityBits);
    response.getMeshcoreDemodReport()->setPacketLength(m_lastMsgPacketLength);
    response.getMeshcoreDemodReport()->setNbSymbols(m_lastMsgNbSymbols);
    response.getMeshcoreDemodReport()->setNbCodewords(m_lastMsgNbCodewords);
    response.getMeshcoreDemodReport()->setHeaderParityStatus(m_lastMsgHeaderParityStatus);
    response.getMeshcoreDemodReport()->setHeaderCrcStatus(m_lastMsgHeaderCRC);
    response.getMeshcoreDemodReport()->setPayloadParityStatus(m_lastMsgPayloadParityStatus);
    response.getMeshcoreDemodReport()->setPayloadCrcStatus(m_lastMsgPayloadCRC);
    if (response.getMeshcoreDemodReport()->getMessageTimestamp()) {
        *response.getMeshcoreDemodReport()->getMessageTimestamp() = m_lastMsgTimestamp;
    } else {
        response.getMeshcoreDemodReport()->setMessageTimestamp(new QString(m_lastMsgTimestamp));
    }
    if (response.getMeshcoreDemodReport()->getMessageString()) {
        *response.getMeshcoreDemodReport()->getMessageString() = m_lastMsgString;
    } else {
        response.getMeshcoreDemodReport()->setMessageString(new QString(m_lastMsgString));
    }
    if (response.getMeshcoreDemodReport()->getFrameType()) {
        *response.getMeshcoreDemodReport()->getFrameType() = m_lastFrameType;
    } else {
        response.getMeshcoreDemodReport()->setFrameType(new QString(m_lastFrameType));
    }
    if (response.getMeshcoreDemodReport()->getChannelType()) {
        *response.getMeshcoreDemodReport()->getChannelType() = m_lastMsgPipelineName;
    } else {
        response.getMeshcoreDemodReport()->setChannelType(new QString(m_lastMsgPipelineName));
    }
    response.getMeshcoreDemodReport()->setDecoding(getDemodActive() ? 1 : 0);

    response.getMeshcoreDemodReport()->setMessageBytes(new QList<QString *>);
    QList<QString *> *bytesStr = response.getMeshcoreDemodReport()->getMessageBytes();

    for (QByteArray::const_iterator it = m_lastMsgBytes.begin(); it != m_lastMsgBytes.end(); ++it)
    {
        unsigned char b = *it;
        bytesStr->push_back(new QString(tr("%1").arg(b, 2, 16, QChar('0'))));
    }
}

void MeshcoreDemod::webapiReverseSendSettings(QList<QString>& channelSettingsKeys, const MeshcoreDemodSettings& settings, bool force)
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

void MeshcoreDemod::sendChannelSettings(
    const QList<ObjectPipe*>& pipes,
    QList<QString>& channelSettingsKeys,
    const MeshcoreDemodSettings& settings,
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

void MeshcoreDemod::webapiFormatChannelSettings(
        QList<QString>& channelSettingsKeys,
        SWGSDRangel::SWGChannelSettings *swgChannelSettings,
        const MeshcoreDemodSettings& settings,
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
    swgChannelSettings->setMeshcoreDemodSettings(new SWGSDRangel::SWGMeshcoreDemodSettings());
    SWGSDRangel::SWGMeshcoreDemodSettings *swgMeshcoreDemodSettings = swgChannelSettings->getMeshcoreDemodSettings();

    // transfer data that has been modified. When force is on transfer all data except reverse API data

    if (channelSettingsKeys.contains("inputFrequencyOffset") || force) {
        swgMeshcoreDemodSettings->setInputFrequencyOffset(settings.m_inputFrequencyOffset);
    }
    if (channelSettingsKeys.contains("bandwidthIndex") || force) {
        swgMeshcoreDemodSettings->setBandwidthIndex(settings.m_bandwidthIndex);
    }
    if (channelSettingsKeys.contains("spreadFactor") || force) {
        swgMeshcoreDemodSettings->setSpreadFactor(settings.m_spreadFactor);
    }
    if (channelSettingsKeys.contains("deBits") || force) {
        swgMeshcoreDemodSettings->setDeBits(settings.m_deBits);
    }
    if (channelSettingsKeys.contains("decodeActive") || force) {
        swgMeshcoreDemodSettings->setDecodeActive(settings.m_decodeActive ? 1 : 0);
    }
    if (channelSettingsKeys.contains("eomSquelchTenths") || force) {
        swgMeshcoreDemodSettings->setEomSquelchTenths(settings.m_eomSquelchTenths);
    }
    if (channelSettingsKeys.contains("nbSymbolsMax") || force) {
        swgMeshcoreDemodSettings->setNbSymbolsMax(settings.m_nbSymbolsMax);
    }
    if (channelSettingsKeys.contains("preambleChirps") || force) {
        swgMeshcoreDemodSettings->setPreambleChirps(settings.m_preambleChirps);
    }
    if (channelSettingsKeys.contains("nbParityBits") || force) {
        swgMeshcoreDemodSettings->setNbParityBits(settings.m_nbParityBits);
    }
    if (channelSettingsKeys.contains("packetLength") || force) {
        swgMeshcoreDemodSettings->setPacketLength(settings.m_packetLength);
    }
    if (channelSettingsKeys.contains("sendViaUDP") || force) {
        swgMeshcoreDemodSettings->setSendViaUdp(settings.m_sendViaUDP ? 1 : 0);
    }
    if (channelSettingsKeys.contains("sendJsonViaUDP") || force) {
        swgMeshcoreDemodSettings->setSendJsonViaUdp(settings.m_sendJsonViaUDP ? 1 : 0);
    }
    if (channelSettingsKeys.contains("udpAddress") || force) {
        if (swgMeshcoreDemodSettings->getUdpAddress()) {
            *swgMeshcoreDemodSettings->getUdpAddress() = settings.m_udpAddress;
        } else {
            swgMeshcoreDemodSettings->setUdpAddress(new QString(settings.m_udpAddress));
        }
    }
    if (channelSettingsKeys.contains("udpPort") || force) {
        swgMeshcoreDemodSettings->setUdpPort(settings.m_udpPort);
    }
    if (channelSettingsKeys.contains("invertRamps") || force) {
        swgMeshcoreDemodSettings->setInvertRamps(settings.m_invertRamps ? 1 : 0);
    }
    if (channelSettingsKeys.contains("rgbColor") || force) {
        swgMeshcoreDemodSettings->setRgbColor(settings.m_rgbColor);
    }
    if (channelSettingsKeys.contains("title") || force) {
        if (swgMeshcoreDemodSettings->getTitle()) {
            *swgMeshcoreDemodSettings->getTitle() = settings.m_title;
        } else {
            swgMeshcoreDemodSettings->setTitle(new QString(settings.m_title));
        }
    }

    if (settings.m_spectrumGUI && (channelSettingsKeys.contains("spectrumConfig") || force))
    {
        SWGSDRangel::SWGGLSpectrum *swgGLSpectrum = new SWGSDRangel::SWGGLSpectrum();
        settings.m_spectrumGUI->formatTo(swgGLSpectrum);
        swgMeshcoreDemodSettings->setSpectrumConfig(swgGLSpectrum);
    }

    if (settings.m_channelMarker && (channelSettingsKeys.contains("channelMarker") || force))
    {
        SWGSDRangel::SWGChannelMarker *swgChannelMarker = new SWGSDRangel::SWGChannelMarker();
        settings.m_channelMarker->formatTo(swgChannelMarker);
        swgMeshcoreDemodSettings->setChannelMarker(swgChannelMarker);
    }

    if (settings.m_rollupState && (channelSettingsKeys.contains("rollupState") || force))
    {
        SWGSDRangel::SWGRollupState *swgRollupState = new SWGSDRangel::SWGRollupState();
        settings.m_rollupState->formatTo(swgRollupState);
        swgMeshcoreDemodSettings->setRollupState(swgRollupState);
    }
}

void MeshcoreDemod::networkManagerFinished(QNetworkReply *reply)
{
    QNetworkReply::NetworkError replyError = reply->error();

    if (replyError)
    {
        qWarning() << "MeshcoreDemod::networkManagerFinished:"
                << " error(" << (int) replyError
                << "): " << replyError
                << ": " << reply->errorString();
    }
    else
    {
        QString answer = reply->readAll();
        answer.chop(1); // remove last \n
        qDebug("MeshcoreDemod::networkManagerFinished: reply:\n%s", answer.toStdString().c_str());
    }

    reply->deleteLater();
}

bool MeshcoreDemod::getDemodActive() const
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

double MeshcoreDemod::getCurrentNoiseLevel() const
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

double MeshcoreDemod::getTotalPower() const
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

void MeshcoreDemod::handleIndexInDeviceSetChanged(int index)
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
