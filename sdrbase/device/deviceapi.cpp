///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2016, 2019-2020, 2022 Edouard Griffiths, F4EXB <f4exb06@gmail.com> //
// Copyright (C) 2022-2023 Jon Beniston, M7RCE <jon@beniston.com>                //
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

#include "plugin/plugininterface.h"
#include "dsp/dspdevicesourceengine.h"
#include "dsp/dspdevicesinkengine.h"
#include "dsp/dspdevicemimoengine.h"
#include "dsp/dspengine.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "settings/preset.h"
#include "channel/channelapi.h"
#include "util/simpleserializer.h"

#include "deviceapi.h"
#include <algorithm>

DeviceAPI::DeviceAPI(
        StreamType streamType,
        int deviceTabIndex,
        DSPDeviceSourceEngine *deviceSourceEngine,
        DSPDeviceSinkEngine *deviceSinkEngine,
        DSPDeviceMIMOEngine *deviceMIMOEngine
) :
    m_streamType(streamType),
    m_deviceTabIndex(deviceTabIndex),
    m_deviceNbItems(1),
    m_deviceItemIndex(0),
    m_nbSourceStreams(0),
    m_nbSinkStreams(0),
    m_pluginInterface(nullptr),
    m_masterTimer(DSPEngine::instance()->getMasterTimer()),
    m_samplingDeviceSequence(0),
    m_workspaceIndex(0),
    m_buddySharedPtr(nullptr),
    m_isBuddyLeader(false),
    m_deviceSourceEngine(deviceSourceEngine),
    m_deviceSinkEngine(deviceSinkEngine),
    m_deviceMIMOEngine(deviceMIMOEngine)
{
    if (m_deviceSourceEngine) {
        QObject::connect(m_deviceSourceEngine, &DSPDeviceSourceEngine::stateChanged, this, &DeviceAPI::engineStateChanged);
    }
    if (m_deviceSinkEngine) {
        QObject::connect(m_deviceSinkEngine, &DSPDeviceSinkEngine::stateChanged, this, &DeviceAPI::engineStateChanged);
    }
    if (m_deviceMIMOEngine) {
        QObject::connect(m_deviceMIMOEngine, &DSPDeviceMIMOEngine::stateChanged, this, &DeviceAPI::engineStateChanged);
    }
}

DeviceAPI::~DeviceAPI() = default;

void DeviceAPI::setSpectrumSinkInput(bool sourceElseSink, unsigned int index)
{
    if (m_deviceMIMOEngine) { // In practice this is only used in the MIMO case
        m_deviceMIMOEngine->setSpectrumSinkInput(sourceElseSink, index);
    }
}

void DeviceAPI::addChannelSink(BasebandSampleSink* sink, int streamIndex)
{
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->addSink(sink);
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->addChannelSink(sink, streamIndex);
    }
}

void DeviceAPI::removeChannelSink(BasebandSampleSink* sink, int streamIndex)
{
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->removeSink(sink);
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->removeChannelSink(sink, streamIndex);
    }
}

void DeviceAPI::addChannelSource(BasebandSampleSource* source, int streamIndex)
{
    if (m_deviceSinkEngine) {
        m_deviceSinkEngine->addChannelSource(source);
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->addChannelSource(source, streamIndex);
    }
}

void DeviceAPI::removeChannelSource(BasebandSampleSource* source, int streamIndex)
{
    if (m_deviceSinkEngine) {
        m_deviceSinkEngine->removeChannelSource(source);
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->removeChannelSource(source, streamIndex);
    }
}

void DeviceAPI::addMIMOChannel(MIMOChannel* channel)
{
    if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->addMIMOChannel(channel);
    }
}

void DeviceAPI::removeMIMOChannel(MIMOChannel* channel)
{
    if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->removeMIMOChannel(channel);
    }
}

void DeviceAPI::addChannelSinkAPI(ChannelAPI* channelAPI)
{
    m_channelSinkAPIs.append(channelAPI);
    renumerateChannels();
}

void DeviceAPI::removeChannelSinkAPI(ChannelAPI* channelAPI)
{
    if (m_channelSinkAPIs.removeOne(channelAPI)) {
        renumerateChannels();
    }

    channelAPI->setIndexInDeviceSet(-1);
}

void DeviceAPI::addChannelSourceAPI(ChannelAPI* channelAPI)
{
    m_channelSourceAPIs.append(channelAPI);
    renumerateChannels();
}

void DeviceAPI::removeChannelSourceAPI(ChannelAPI* channelAPI)
{
    if (m_channelSourceAPIs.removeOne(channelAPI)) {
        renumerateChannels();
    }

    channelAPI->setIndexInDeviceSet(-1);
}

void DeviceAPI::addMIMOChannelAPI(ChannelAPI* channelAPI)
{
    m_mimoChannelAPIs.append(channelAPI);
    renumerateChannels();
}

void DeviceAPI::removeMIMOChannelAPI(ChannelAPI *channelAPI)
{
    if (m_mimoChannelAPIs.removeOne(channelAPI)) {
        renumerateChannels();
    }

    channelAPI->setIndexInDeviceSet(-1);
}

void DeviceAPI::setSampleSource(DeviceSampleSource* source)
{
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->setSource(source);
    }
}

void DeviceAPI::setSampleSink(DeviceSampleSink* sink)
{
    if (m_deviceSinkEngine) {
        m_deviceSinkEngine->setSink(sink);
    }
}

void DeviceAPI::setSampleMIMO(DeviceSampleMIMO* mimo)
{
    if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->setMIMO(mimo);
    }
}

DeviceSampleSource *DeviceAPI::getSampleSource()
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->getSource();
    } else {
        return nullptr;
    }
}

DeviceSampleSink *DeviceAPI::getSampleSink()
{
    if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->getSink();
    } else {
        return nullptr;
    }
}

DeviceSampleMIMO *DeviceAPI::getSampleMIMO()
{
    if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->getMIMO();
    } else {
        return nullptr;
    }
}

bool DeviceAPI::initDeviceEngine(int subsystemIndex)
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->initAcquisition();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->initGeneration();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->initProcess(subsystemIndex);
    } else {
        return false;
    }
}

bool DeviceAPI::startDeviceEngine(int subsystemIndex)
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->startAcquisition();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->startGeneration();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->startProcess(subsystemIndex);
    } else {
        return false;
    }
}

void DeviceAPI::stopDeviceEngine(int subsystemIndex)
{
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->stopAcquistion();
    } else if (m_deviceSinkEngine) {
        m_deviceSinkEngine->stopGeneration();
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->stopProcess(subsystemIndex);
    }
}

DeviceAPI::EngineState DeviceAPI::state(int subsystemIndex) const
{
    if (m_deviceSourceEngine) {
        return (DeviceAPI::EngineState) m_deviceSourceEngine->state();
    } else if (m_deviceSinkEngine) {
        return (DeviceAPI::EngineState) m_deviceSinkEngine->state();
    } else if (m_deviceMIMOEngine) {
        return (DeviceAPI::EngineState) m_deviceMIMOEngine->state(subsystemIndex);
    } else {
        return StError;
    }
}

QString DeviceAPI::errorMessage(int subsystemIndex) const
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->errorMessage();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->errorMessage();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->errorMessage(subsystemIndex);
    } else {
        return "Not implemented";
    }
}

uint DeviceAPI::getDeviceUID() const
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->getUID();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->getUID();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->getUID();
    } else {
        return 0;
    }
}

MessageQueue *DeviceAPI::getDeviceEngineInputMessageQueue()
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->getInputMessageQueue();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->getInputMessageQueue();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->getInputMessageQueue();
    } else {
        return nullptr;
    }
}

MessageQueue *DeviceAPI::getSamplingDeviceInputMessageQueue()
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->getSource()->getInputMessageQueue();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->getSink()->getInputMessageQueue();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->getMIMO()->getInputMessageQueue();
    } else {
        return nullptr;
    }
}

MessageQueue *DeviceAPI::getSamplingDeviceGUIMessageQueue()
{
    if (m_deviceSourceEngine) {
        return m_deviceSourceEngine->getSource()->getMessageQueueToGUI();
    } else if (m_deviceSinkEngine) {
        return m_deviceSinkEngine->getSink()->getMessageQueueToGUI();
    } else if (m_deviceMIMOEngine) {
        return m_deviceMIMOEngine->getMIMO()->getMessageQueueToGUI();
    } else {
        return nullptr;
    }
}

void DeviceAPI::configureCorrections(bool dcOffsetCorrection, bool iqImbalanceCorrection, int streamIndex)
{
    if (m_deviceSourceEngine) {
        m_deviceSourceEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection);
    } else if (m_deviceMIMOEngine) {
        m_deviceMIMOEngine->configureCorrections(dcOffsetCorrection, iqImbalanceCorrection, streamIndex);
    }
}

void DeviceAPI::setHardwareId(const QString& id)
{
    m_hardwareId = id;
}

void DeviceAPI::setDeviceNbItems(uint32_t nbItems)
{
    m_deviceNbItems = nbItems;
}

void DeviceAPI::setDeviceItemIndex(uint32_t index)
{
    m_deviceItemIndex = index;
}

void DeviceAPI::setSamplingDevicePluginInterface(PluginInterface *iface)
{
    m_pluginInterface = iface;
}

void DeviceAPI::getDeviceEngineStateStr(QString& state, int subsystemIndex) const
{
    if (m_deviceSourceEngine)
    {
        switch(m_deviceSourceEngine->state())
        {
        case DSPDeviceSourceEngine::State::StNotStarted:
            state = "notStarted";
            break;
        case DSPDeviceSourceEngine::State::StIdle:
            state = "idle";
            break;
        case DSPDeviceSourceEngine::State::StReady:
            state = "ready";
            break;
        case DSPDeviceSourceEngine::State::StRunning:
            state = "running";
            break;
        case DSPDeviceSourceEngine::State::StError:
            state = "error";
            break;
        default:
            state = "notStarted";
            break;
        }
    }
    else if (m_deviceSinkEngine)
    {
        switch(m_deviceSinkEngine->state())
        {
        case DSPDeviceSinkEngine::State::StNotStarted:
            state = "notStarted";
            break;
        case DSPDeviceSinkEngine::State::StIdle:
            state = "idle";
            break;
        case DSPDeviceSinkEngine::State::StReady:
            state = "ready";
            break;
        case DSPDeviceSinkEngine::State::StRunning:
            state = "running";
            break;
        case DSPDeviceSinkEngine::State::StError:
            state = "error";
            break;
        default:
            state = "notStarted";
            break;
        }
    }
    else if (m_deviceMIMOEngine)
    {
        switch(m_deviceMIMOEngine->state(subsystemIndex))
        {
        case DSPDeviceMIMOEngine::State::StNotStarted:
            state = "notStarted";
            break;
        case DSPDeviceMIMOEngine::State::StIdle:
            state = "idle";
            break;
        case DSPDeviceMIMOEngine::State::StReady:
            state = "ready";
            break;
        case DSPDeviceMIMOEngine::State::StRunning:
            state = "running";
            break;
        case DSPDeviceMIMOEngine::State::StError:
            state = "error";
            break;
        default:
            state = "notStarted";
            break;
        }
    }
    else
    {
        state = "notStarted";
    }
}

ChannelAPI *DeviceAPI::getChanelSinkAPIAt(int index)
{
    if (index < m_channelSinkAPIs.size()) {
        return m_channelSinkAPIs.at(index);
    } else {
        return nullptr;
    }
}

ChannelAPI *DeviceAPI::getChanelSourceAPIAt(int index)
{
    if (index < m_channelSourceAPIs.size()) {
        return m_channelSourceAPIs.at(index);
    } else {
        return nullptr;
    }
}

ChannelAPI *DeviceAPI::getMIMOChannelAPIAt(int index)
{
    if (index < m_mimoChannelAPIs.size()) {
        return m_mimoChannelAPIs.at(index);
    } else {
        return nullptr;
    }
}

QList<quint64> DeviceAPI::getCenterFrequency() const
{
    QList<quint64> frequencies;

    if (m_deviceSourceEngine && m_deviceSourceEngine->getSource())
    {
        frequencies.append(m_deviceSourceEngine->getSource()->getCenterFrequency());
    }
    else if (m_deviceSinkEngine && m_deviceSinkEngine->getSink())
    {
        frequencies.append(m_deviceSinkEngine->getSink()->getCenterFrequency());
    }
    else if (m_deviceMIMOEngine && m_deviceMIMOEngine->getMIMO())
    {
        for (uint32_t i = 0; i < m_deviceMIMOEngine->getMIMO()->getNbSourceStreams(); i++) {
            frequencies.append(m_deviceMIMOEngine->getMIMO()->getSourceCenterFrequency(i));
        }
        for (uint32_t i = 0; i < m_deviceMIMOEngine->getMIMO()->getNbSinkStreams(); i++) {
            frequencies.append(m_deviceMIMOEngine->getMIMO()->getSinkCenterFrequency(i));
        }
    }
    return frequencies;
}

void DeviceAPI::setCenterFrequency(QList<quint64> centerFrequency)
{
    if (m_deviceSourceEngine && m_deviceSourceEngine->getSource())
    {
        m_deviceSourceEngine->getSource()->setCenterFrequency(centerFrequency[0]);
    }
    else if (m_deviceSinkEngine && m_deviceSinkEngine->getSink())
    {
        m_deviceSinkEngine->getSink()->setCenterFrequency(centerFrequency[0]);
    }
    else if (m_deviceMIMOEngine && m_deviceMIMOEngine->getMIMO())
    {
        int idx = 0;
        for (uint32_t i = 0; i < m_deviceMIMOEngine->getMIMO()->getNbSourceStreams(); i++, idx++) {
            m_deviceMIMOEngine->getMIMO()->setSourceCenterFrequency(centerFrequency[idx], i);
        }
        for (uint32_t i = 0; i < m_deviceMIMOEngine->getMIMO()->getNbSinkStreams(); i++, idx++) {
            m_deviceMIMOEngine->getMIMO()->setSinkCenterFrequency(centerFrequency[idx], i);
        }
    }
}

// Serialization is only currently used for saving device settings as part of a Device preset
// loadSamplingDeviceSettings/saveSamplingDeviceSettings is used for Device Set presets (which includes channel settings)

QByteArray DeviceAPI::serialize() const
{
    SimpleSerializer s(1);

    if (m_deviceSourceEngine && m_deviceSourceEngine->getSource()) {
        s.writeBlob(1, m_deviceSourceEngine->getSource()->serialize());
    }
    if (m_deviceSinkEngine && m_deviceSinkEngine->getSink()) {
        s.writeBlob(2, m_deviceSinkEngine->getSink()->serialize());
    }
    if (m_deviceMIMOEngine && m_deviceMIMOEngine->getMIMO()) {
        s.writeBlob(3, m_deviceMIMOEngine->getMIMO()->serialize());
    }
    s.writeList<quint64>(4, getCenterFrequency());
    return s.final();
}

bool DeviceAPI::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid()) {
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bdata;
        QList<quint64> centerFrequency;

        if (m_deviceSourceEngine && m_deviceSourceEngine->getSource())
        {
            d.readBlob(1, &bdata);
            if (bdata.size() > 0) {
                m_deviceSourceEngine->getSource()->deserialize(bdata);
            }
        }
        if (m_deviceSinkEngine && m_deviceSinkEngine->getSink())
        {
            d.readBlob(2, &bdata);
            if (bdata.size() > 0) {
                m_deviceSinkEngine->getSink()->deserialize(bdata);
            }
        }
        if (m_deviceMIMOEngine && m_deviceMIMOEngine->getMIMO())
        {
            d.readBlob(3, &bdata);
            if (bdata.size() > 0) {
                m_deviceMIMOEngine->getMIMO()->deserialize(bdata);
            }
        }
        d.readList<quint64>(4, &centerFrequency);
        setCenterFrequency(centerFrequency);

        return true;
    }
    else
    {
        return false;
    }
}

void DeviceAPI::loadSamplingDeviceSettings(const Preset* preset)
{
    if (m_deviceSourceEngine && (preset->isSourcePreset()))
    {
        qDebug("DeviceAPI::loadSamplingDeviceSettings: Loading Rx preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        const QByteArray* sourceConfig = preset->findBestDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence);
        qint64 centerFrequency = preset->getCenterFrequency();
        qDebug("DeviceAPI::loadSamplingDeviceSettings: source center frequency: %llu Hz", centerFrequency);

        if (sourceConfig)
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: deserializing source %s[%d]: %s",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

            if (m_deviceSourceEngine->getSource() != nullptr) // Server flavor
            {
                m_deviceSourceEngine->getSource()->deserialize(*sourceConfig);
            }
            else
            {
                qDebug("DeviceAPI::loadSamplingDeviceSettings: deserializing no source");
            }
        }
        else
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: source %s[%d]: %s not found",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));
        }

        // set center frequency anyway
        if (m_deviceSourceEngine->getSource())
        {
            m_deviceSourceEngine->getSource()->setCenterFrequency(centerFrequency);
        }
        else
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: no source");
        }
    }
    else if (m_deviceSinkEngine && preset->isSinkPreset())
    {
        qDebug("DeviceAPI::loadSamplingDeviceSettings: Loading Tx preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        const QByteArray* sinkConfig = preset->findBestDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence);
        qint64 centerFrequency = preset->getCenterFrequency();
        qDebug("DeviceAPI::loadSamplingDeviceSettings: sink center frequency: %llu Hz", centerFrequency);

        if (sinkConfig)
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: deserializing sink %s[%d]: %s",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

            if (m_deviceSinkEngine->getSink())
            {
                m_deviceSinkEngine->getSink()->deserialize(*sinkConfig);
                m_deviceSinkEngine->getSink()->setCenterFrequency(centerFrequency);
            }
            else
            {
                qDebug("DeviceAPI::loadSamplingDeviceSettings: no sink");
            }
        }
        else
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: sink %s[%d]: %s not found",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));
        }
    }
    else if (m_deviceMIMOEngine && preset->isMIMOPreset())
    {
        qDebug("DeviceAPI::loadSamplingDeviceSettings: Loading MIMO preset [%s | %s]", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));

        const QByteArray* mimoConfig = preset->findBestDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence);
        qint64 centerFrequency = preset->getCenterFrequency();
        qDebug("DeviceAPI::loadSamplingDeviceSettings: MIMO center frequency: %llu Hz", centerFrequency);

        if (mimoConfig)
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: deserializing MIMO %s[%d]: %s",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

            if (m_deviceMIMOEngine->getMIMO())
            {
                m_deviceMIMOEngine->getMIMO()->deserialize(*mimoConfig);
                m_deviceMIMOEngine->getMIMO()->setSourceCenterFrequency(centerFrequency, 0);
                m_deviceMIMOEngine->getMIMO()->setSinkCenterFrequency(centerFrequency, 0);
            }
            else
            {
                qDebug("DeviceAPI::loadSamplingDeviceSettings: no MIMO");
            }
        }
        else
        {
            qDebug("DeviceAPI::loadSamplingDeviceSettings: MIMO %s[%d]: %s not found",
                qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));
        }
    }
    else
    {
        qDebug("DeviceAPI::loadSamplingDeviceSettings: Loading preset [%s | %s] is not a suitable preset", qPrintable(preset->getGroup()), qPrintable(preset->getDescription()));
    }
}

void DeviceAPI::saveSamplingDeviceSettings(Preset* preset)
{
    if (m_deviceSourceEngine && (preset->isSourcePreset()))
    {
        qDebug("DeviceAPI::saveSamplingDeviceSettings: serializing source %s[%d]: %s",
            qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

        if (m_deviceSourceEngine->getSource()) // Server flavor
        {
            preset->addOrUpdateDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence, m_deviceSourceEngine->getSource()->serialize());
            preset->setCenterFrequency(m_deviceSourceEngine->getSource()->getCenterFrequency());
        }
        else
        {
            qDebug("DeviceAPI::saveSamplingDeviceSettings: no source");
        }
    }
    else if (m_deviceSinkEngine && preset->isSinkPreset())
    {
        qDebug("DeviceAPI::saveSamplingDeviceSettings: serializing sink %s[%d]: %s",
            qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

        if (m_deviceSinkEngine->getSink()) // Server flavor
        {
            preset->addOrUpdateDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence, m_deviceSinkEngine->getSink()->serialize());
            preset->setCenterFrequency(m_deviceSinkEngine->getSink()->getCenterFrequency());
        }
        else
        {
            qDebug("DeviceAPI::saveSamplingDeviceSettings: no sink");
        }
    }
    else if (m_deviceMIMOEngine && preset->isMIMOPreset())
    {
        qDebug("DeviceAPI::saveSamplingDeviceSettings: serializing MIMO %s[%d]: %s",
            qPrintable(m_samplingDeviceId), m_samplingDeviceSequence, qPrintable(m_samplingDeviceSerial));

        if (m_deviceMIMOEngine->getMIMO()) // Server flavor
        {
            preset->addOrUpdateDeviceConfig(m_samplingDeviceId, m_samplingDeviceSerial, m_samplingDeviceSequence, m_deviceMIMOEngine->getMIMO()->serialize());
            preset->setCenterFrequency(m_deviceMIMOEngine->getMIMO()->getMIMOCenterFrequency());
        }
        else
        {
            qDebug("DeviceAPI::saveSamplingDeviceSettings: no MIMO");
        }
    }
    else
    {
        qDebug("DeviceAPI::saveSamplingDeviceSettings: not a suitable preset");
    }
}

void DeviceAPI::addBuddy(DeviceAPI* buddy)
{
    if (buddy->m_streamType == StreamSingleRx)
    {
        m_sourceBuddies.push_back(buddy); // this is a source
    }
    else if (buddy->m_streamType == StreamSingleTx)
    {
        m_sinkBuddies.push_back(buddy); // this is a sink
    }
    else
    {
        qDebug("DeviceAPI::addBuddy: not relevant if buddy is not a single Rx or Tx");
        return;
    }

    if (m_streamType == StreamSingleRx) {
        buddy->m_sourceBuddies.push_back(this); // this is a source
    } else if (m_streamType == StreamSingleTx) {
        buddy->m_sinkBuddies.push_back(this); // this is a sink
    } else {
        qDebug("DeviceAPI::addBuddy: not relevant if this is not a single Rx or Tx");
        return;
    }

    qDebug("DeviceAPI::addBuddy: added buddy %s(%s) [%llu] <-> [%llu]",
            qPrintable(buddy->getHardwareId()),
            qPrintable(buddy->getSamplingDeviceSerial()),
            (quint64) buddy,
            (quint64) this);
}


void DeviceAPI::removeBuddy(DeviceAPI* buddy)
{
    switch(buddy->m_streamType) {
    case StreamSingleRx:
        m_sourceBuddies.erase(std::find(m_sourceBuddies.begin(), m_sourceBuddies.end(), buddy));
        break;
    case StreamSingleTx:
        m_sinkBuddies.erase(std::find(m_sinkBuddies.begin(), m_sinkBuddies.end(), buddy));
        break;
    default:
        qDebug("DeviceAPI::removeSourceBuddy: buddy %s(%s) is not of single Rx or Tx type",
                qPrintable(buddy->getHardwareId()),
                qPrintable(buddy->getSamplingDeviceSerial()));
        return;
    }
}

void DeviceAPI::clearBuddiesLists()
{
    auto itSource = m_sourceBuddies.begin();
    auto itSink = m_sinkBuddies.begin();
    bool leaderElected = false;

    for (;itSource != m_sourceBuddies.end(); ++itSource)
    {
        if (isBuddyLeader() && !leaderElected)
        {
            (*itSource)->setBuddyLeader(true);
            leaderElected = true;
        }

        (*itSource)->removeBuddy(this);
    }

    m_sourceBuddies.clear();

    for (;itSink != m_sinkBuddies.end(); ++itSink)
    {
        if (isBuddyLeader() && !leaderElected)
        {
            (*itSink)->setBuddyLeader(true);
            leaderElected = true;
        }

        (*itSink)->removeBuddy(this);
    }

    m_sinkBuddies.clear();
}

void DeviceAPI::renumerateChannels()
{
    if (m_streamType == StreamSingleRx)
    {
        for (int i = 0; i < m_channelSinkAPIs.size(); ++i)
        {
            m_channelSinkAPIs.at(i)->setIndexInDeviceSet(i);
            m_channelSinkAPIs.at(i)->setDeviceSetIndex(m_deviceTabIndex);
            m_channelSinkAPIs.at(i)->setDeviceAPI(this);
        }
    }
    else if (m_streamType == StreamSingleTx)
    {
        for (int i = 0; i < m_channelSourceAPIs.size(); ++i)
        {
            m_channelSourceAPIs.at(i)->setIndexInDeviceSet(i);
            m_channelSourceAPIs.at(i)->setDeviceSetIndex(m_deviceTabIndex);
            m_channelSourceAPIs.at(i)->setDeviceAPI(this);
        }
    }
    else if (m_streamType == StreamMIMO)
    {
        int index = 0;

        for (; index < m_channelSinkAPIs.size(); ++index)
        {
            m_channelSinkAPIs.at(index)->setIndexInDeviceSet(index);
            m_channelSinkAPIs.at(index)->setDeviceSetIndex(m_deviceTabIndex);
            m_channelSinkAPIs.at(index)->setDeviceAPI(this);
        }

        for (; index < m_channelSourceAPIs.size() + m_channelSinkAPIs.size(); ++index)
        {
            int sourceIndex = index - m_channelSinkAPIs.size();
            m_channelSourceAPIs.at(sourceIndex)->setIndexInDeviceSet(index);
            m_channelSourceAPIs.at(sourceIndex)->setDeviceSetIndex(m_deviceTabIndex);
            m_channelSourceAPIs.at(sourceIndex)->setDeviceAPI(this);
        }

        for (; index < m_mimoChannelAPIs.size() + m_channelSourceAPIs.size() + m_channelSinkAPIs.size(); ++index)
        {
            int mimoIndex = index - m_channelSourceAPIs.size() - m_channelSinkAPIs.size();
            m_mimoChannelAPIs.at(mimoIndex)->setIndexInDeviceSet(index);
            m_mimoChannelAPIs.at(mimoIndex)->setDeviceSetIndex(m_deviceTabIndex);
            m_mimoChannelAPIs.at(mimoIndex)->setDeviceAPI(this);
        }
    }
}

void DeviceAPI::setDeviceSetIndex(int deviceSetIndex)
{
    m_deviceTabIndex = deviceSetIndex;
    renumerateChannels();
}

void DeviceAPI::engineStateChanged()
{
    emit stateChanged(this);
}
