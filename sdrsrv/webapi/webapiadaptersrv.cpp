///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2017 Edouard Griffiths, F4EXB.                                  //
//                                                                               //
// Swagger server adapter interface                                              //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QCoreApplication>
#include <QList>

#include <unistd.h>

#include "SWGInstanceSummaryResponse.h"
#include "SWGInstanceDevicesResponse.h"
#include "SWGInstanceChannelsResponse.h"
#include "SWGLoggingInfo.h"
#include "SWGAudioDevices.h"
#include "SWGAudioDevicesSelect.h"
#include "SWGLocationInformation.h"
#include "SWGErrorResponse.h"

#include "maincore.h"
#include "loggerwithfile.h"
#include "device/deviceset.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplesource.h"
#include "dsp/dspengine.h"
#include "channel/channelsourceapi.h"
#include "channel/channelsinkapi.h"
#include "plugin/pluginapi.h"
#include "plugin/pluginmanager.h"
#include "webapiadaptersrv.h"

WebAPIAdapterSrv::WebAPIAdapterSrv(MainCore& mainCore) :
    m_mainCore(mainCore)
{
}

WebAPIAdapterSrv::~WebAPIAdapterSrv()
{
}

int WebAPIAdapterSrv::instanceSummary(
        SWGSDRangel::SWGInstanceSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{

    *response.getVersion() = QCoreApplication::instance()->applicationVersion();
    *response.getQtVersion() = QString(QT_VERSION_STR);

    SWGSDRangel::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainCore.m_logger->getUseFileLogger() ? 1 : 0);

    if (logging->getDumpToFile()) {
        m_mainCore.m_logger->getLogFileName(*logging->getFileName());
        m_mainCore.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }

    m_mainCore.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    SWGSDRangel::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    getDeviceSetList(deviceSetList);

    return 200;
}

int WebAPIAdapterSrv::instanceDelete(
        SWGSDRangel::SWGInstanceSummaryResponse& response,
        SWGSDRangel::SWGErrorResponse& error)
{
    instanceSummary(response, error);

    MainCore::MsgDeleteInstance *msg = MainCore::MsgDeleteInstance::create();
    m_mainCore.getInputMessageQueue()->push(msg);

    return 200;
}

int WebAPIAdapterSrv::instanceDevices(
            bool tx,
            SWGSDRangel::SWGInstanceDevicesResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    int nbSamplingDevices = tx ? DeviceEnumerator::instance()->getNbTxSamplingDevices() : DeviceEnumerator::instance()->getNbRxSamplingDevices();
    response.setDevicecount(nbSamplingDevices);
    QList<SWGSDRangel::SWGDeviceListItem*> *devices = response.getDevices();

    for (int i = 0; i < nbSamplingDevices; i++)
    {
        PluginInterface::SamplingDevice samplingDevice = tx ? DeviceEnumerator::instance()->getTxSamplingDevice(i) : DeviceEnumerator::instance()->getRxSamplingDevice(i);
        devices->append(new SWGSDRangel::SWGDeviceListItem);
        *devices->back()->getDisplayedName() = samplingDevice.displayedName;
        *devices->back()->getHwType() = samplingDevice.hardwareId;
        *devices->back()->getSerial() = samplingDevice.serial;
        devices->back()->setSequence(samplingDevice.sequence);
        devices->back()->setTx(!samplingDevice.rxElseTx);
        devices->back()->setNbStreams(samplingDevice.deviceNbItems);
        devices->back()->setDeviceSetIndex(samplingDevice.claimed);
        devices->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterSrv::instanceChannels(
            bool tx,
            SWGSDRangel::SWGInstanceChannelsResponse& response,
            SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    PluginAPI::ChannelRegistrations *channelRegistrations = tx ? m_mainCore.m_pluginManager->getTxChannelRegistrations() : m_mainCore.m_pluginManager->getRxChannelRegistrations();
    int nbChannelDevices = channelRegistrations->size();
    response.setChannelcount(nbChannelDevices);
    QList<SWGSDRangel::SWGChannelListItem*> *channels = response.getChannels();

    for (int i = 0; i < nbChannelDevices; i++)
    {
        channels->append(new SWGSDRangel::SWGChannelListItem);
        PluginInterface *channelInterface = channelRegistrations->at(i).m_plugin;
        const PluginDescriptor& pluginDescriptor = channelInterface->getPluginDescriptor();
        *channels->back()->getVersion() = pluginDescriptor.version;
        *channels->back()->getName() = pluginDescriptor.displayedName;
        channels->back()->setTx(tx);
        *channels->back()->getIdUri() = channelRegistrations->at(i).m_channelIdURI;
        *channels->back()->getId() = channelRegistrations->at(i).m_channelId;
        channels->back()->setIndex(i);
    }

    return 200;
}

int WebAPIAdapterSrv::instanceLoggingGet(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.setDumpToFile(m_mainCore.m_logger->getUseFileLogger() ? 1 : 0);

    if (response.getDumpToFile()) {
    	m_mainCore.m_logger->getLogFileName(*response.getFileName());
    	m_mainCore.m_logger->getFileMinMessageLevelStr(*response.getFileLevel());
    }

    m_mainCore.m_logger->getConsoleMinMessageLevelStr(*response.getConsoleLevel());

    return 200;
}

int WebAPIAdapterSrv::instanceLoggingPut(
        SWGSDRangel::SWGLoggingInfo& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    bool dumpToFile = (response.getDumpToFile() != 0);
    QString* consoleLevel = response.getConsoleLevel();
    QString* fileLevel = response.getFileLevel();
    QString* fileName = response.getFileName();

    // perform actions
    if (consoleLevel) {
    	m_mainCore.m_settings.setConsoleMinLogLevel(getMsgTypeFromString(*consoleLevel));
    }

    if (fileLevel) {
    	m_mainCore.m_settings.setFileMinLogLevel(getMsgTypeFromString(*fileLevel));
    }

    m_mainCore.m_settings.setUseLogFile(dumpToFile);

    if (fileName) {
    	m_mainCore.m_settings.setLogFileName(*fileName);
    }

    m_mainCore.setLoggingOptions();

    // build response
    response.init();
    getMsgTypeString(m_mainCore.m_settings.getConsoleMinLogLevel(), *response.getConsoleLevel());
    response.setDumpToFile(m_mainCore.m_settings.getUseLogFile() ? 1 : 0);
    getMsgTypeString(m_mainCore.m_settings.getFileMinLogLevel(), *response.getFileLevel());
    *response.getFileName() = m_mainCore.m_settings.getLogFileName();

    return 200;
}

int WebAPIAdapterSrv::instanceAudioGet(
        SWGSDRangel::SWGAudioDevices& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainCore.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainCore.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    response.init();
    response.setNbInputDevices(nbInputDevices);
    response.setInputDeviceSelectedIndex(m_mainCore.m_audioDeviceInfo.getInputDeviceIndex());
    response.setNbOutputDevices(nbOutputDevices);
    response.setOutputDeviceSelectedIndex(m_mainCore.m_audioDeviceInfo.getOutputDeviceIndex());
    response.setInputVolume(m_mainCore.m_audioDeviceInfo.getInputVolume());
    QList<SWGSDRangel::SWGAudioDevice*> *inputDevices = response.getInputDevices();
    QList<SWGSDRangel::SWGAudioDevice*> *outputDevices = response.getOutputDevices();

    for (int i = 0; i < nbInputDevices; i++)
    {
        inputDevices->append(new SWGSDRangel::SWGAudioDevice);
        *inputDevices->back()->getName() = audioInputDevices.at(i).deviceName();
    }

    for (int i = 0; i < nbOutputDevices; i++)
    {
        outputDevices->append(new SWGSDRangel::SWGAudioDevice);
        *outputDevices->back()->getName() = audioOutputDevices.at(i).deviceName();
    }

    return 200;
}

int WebAPIAdapterSrv::instanceAudioPatch(
        SWGSDRangel::SWGAudioDevicesSelect& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    // response input is the query actually
    float inputVolume = response.getInputVolume();
    int inputIndex = response.getInputIndex();
    int outputIndex = response.getOutputIndex();

    const QList<QAudioDeviceInfo>& audioInputDevices = m_mainCore.m_audioDeviceInfo.getInputDevices();
    const QList<QAudioDeviceInfo>& audioOutputDevices = m_mainCore.m_audioDeviceInfo.getOutputDevices();
    int nbInputDevices = audioInputDevices.size();
    int nbOutputDevices = audioOutputDevices.size();

    inputVolume = inputVolume < 0.0 ? 0.0 : inputVolume > 1.0 ? 1.0 : inputVolume;
    inputIndex = inputIndex < -1 ? -1 : inputIndex > nbInputDevices ? nbInputDevices-1 : inputIndex;
    outputIndex = outputIndex < -1 ? -1 : outputIndex > nbOutputDevices ? nbOutputDevices-1 : outputIndex;

    m_mainCore.m_audioDeviceInfo.setInputVolume(inputVolume);
    m_mainCore.m_audioDeviceInfo.setInputDeviceIndex(inputIndex);
    m_mainCore.m_audioDeviceInfo.setOutputDeviceIndex(outputIndex);

    m_mainCore.m_dspEngine->setAudioInputVolume(inputVolume);
    m_mainCore.m_dspEngine->setAudioInputDeviceIndex(inputIndex);
    m_mainCore.m_dspEngine->setAudioOutputDeviceIndex(outputIndex);

    response.setInputVolume(m_mainCore.m_audioDeviceInfo.getInputVolume());
    response.setInputIndex(m_mainCore.m_audioDeviceInfo.getInputDeviceIndex());
    response.setOutputIndex(m_mainCore.m_audioDeviceInfo.getOutputDeviceIndex());

    return 200;
}

int WebAPIAdapterSrv::instanceLocationGet(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    response.setLatitude(m_mainCore.m_settings.getLatitude());
    response.setLongitude(m_mainCore.m_settings.getLongitude());

    return 200;
}

int WebAPIAdapterSrv::instanceLocationPut(
        SWGSDRangel::SWGLocationInformation& response,
        SWGSDRangel::SWGErrorResponse& error __attribute__((unused)))
{
    float latitude = response.getLatitude();
    float longitude = response.getLongitude();

    latitude = latitude < -90.0 ? -90.0 : latitude > 90.0 ? 90.0 : latitude;
    longitude = longitude < -180.0 ? -180.0 : longitude > 180.0 ? 180.0 : longitude;

    m_mainCore.m_settings.setLatitude(latitude);
    m_mainCore.m_settings.setLongitude(longitude);

    response.setLatitude(m_mainCore.m_settings.getLatitude());
    response.setLongitude(m_mainCore.m_settings.getLongitude());

    return 200;
}

void WebAPIAdapterSrv::getDeviceSetList(SWGSDRangel::SWGDeviceSetList* deviceSetList)
{
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainCore.m_deviceSets.size());

    std::vector<DeviceSet*>::const_iterator it = m_mainCore.m_deviceSets.begin();

    for (int i = 0; it != m_mainCore.m_deviceSets.end(); ++it, i++)
    {
        QList<SWGSDRangel::SWGDeviceSet*> *deviceSets = deviceSetList->getDeviceSets();
        deviceSets->append(new SWGSDRangel::SWGDeviceSet());

        getDeviceSet(deviceSets->back(), *it, i);
    }
}

void WebAPIAdapterSrv::getDeviceSet(SWGSDRangel::SWGDeviceSet *swgDeviceSet, const DeviceSet* deviceSet, int deviceUISetIndex)
{
    SWGSDRangel::SWGSamplingDevice *samplingDevice = swgDeviceSet->getSamplingDevice();
    samplingDevice->init();
    samplingDevice->setIndex(deviceUISetIndex);
    samplingDevice->setTx(deviceSet->m_deviceSinkEngine != 0);

    if (deviceSet->m_deviceSinkEngine) // Tx data
    {
        *samplingDevice->getHwType() = deviceSet->m_deviceSinkAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceSinkAPI->getSampleSinkSerial();
        samplingDevice->setSequence(deviceSet->m_deviceSinkAPI->getSampleSinkSequence());
        samplingDevice->setNbStreams(deviceSet->m_deviceSinkAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceSet->m_deviceSinkAPI->getItemIndex());
        deviceSet->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSink *sampleSink = deviceSet->m_deviceSinkEngine->getSink();

        if (sampleSink) {
            samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSink->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceSinkAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            ChannelSourceAPI *channel = deviceSet->m_deviceSinkAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }

    if (deviceSet->m_deviceSourceEngine) // Rx data
    {
        *samplingDevice->getHwType() = deviceSet->m_deviceSourceAPI->getHardwareId();
        *samplingDevice->getSerial() = deviceSet->m_deviceSourceAPI->getSampleSourceSerial();
        samplingDevice->setSequence(deviceSet->m_deviceSourceAPI->getSampleSourceSequence());
        samplingDevice->setNbStreams(deviceSet->m_deviceSourceAPI->getNbItems());
        samplingDevice->setStreamIndex(deviceSet->m_deviceSourceAPI->getItemIndex());
        deviceSet->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
        DeviceSampleSource *sampleSource = deviceSet->m_deviceSourceEngine->getSource();

        if (sampleSource) {
            samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
            samplingDevice->setBandwidth(sampleSource->getSampleRate());
        }

        swgDeviceSet->setChannelcount(deviceSet->m_deviceSourceAPI->getNbChannels());
        QList<SWGSDRangel::SWGChannel*> *channels = swgDeviceSet->getChannels();

        for (int i = 0; i <  swgDeviceSet->getChannelcount(); i++)
        {
            channels->append(new SWGSDRangel::SWGChannel);
            ChannelSinkAPI *channel = deviceSet->m_deviceSourceAPI->getChanelAPIAt(i);
            channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
            channels->back()->setIndex(channel->getIndexInDeviceSet());
            channels->back()->setUid(channel->getUID());
            channel->getIdentifier(*channels->back()->getId());
            channel->getTitle(*channels->back()->getTitle());
        }
    }
}

QtMsgType WebAPIAdapterSrv::getMsgTypeFromString(const QString& msgTypeString)
{
    if (msgTypeString == "debug") {
        return QtDebugMsg;
    } else if (msgTypeString == "info") {
        return QtInfoMsg;
    } else if (msgTypeString == "warning") {
        return QtWarningMsg;
    } else if (msgTypeString == "error") {
        return QtCriticalMsg;
    } else {
        return QtDebugMsg;
    }
}

void WebAPIAdapterSrv::getMsgTypeString(const QtMsgType& msgType, QString& levelStr)
{
    switch (msgType)
    {
    case QtDebugMsg:
        levelStr = "debug";
        break;
    case QtInfoMsg:
        levelStr = "info";
        break;
    case QtWarningMsg:
        levelStr = "warning";
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        levelStr = "error";
        break;
    default:
        levelStr = "debug";
        break;
    }
}
