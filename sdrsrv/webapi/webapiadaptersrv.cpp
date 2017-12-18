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
#include "SWGErrorResponse.h"

#include "maincore.h"
#include "loggerwithfile.h"
#include "device/deviceset.h"
#include "device/devicesinkapi.h"
#include "device/devicesourceapi.h"
#include "device/deviceenumerator.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplesource.h"
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
