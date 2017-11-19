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

#include <QApplication>

#include "mainwindow.h"
#include "loggerwithfile.h"
#include "device/devicesourceapi.h"
#include "device/devicesinkapi.h"
#include "device/deviceuiset.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "channel/channelsinkapi.h"
#include "channel/channelsourceapi.h"

#include "SWGInstanceSummaryResponse.h"
#include "SWGErrorResponse.h"

#include "webapiadaptergui.h"

WebAPIAdapterGUI::WebAPIAdapterGUI(MainWindow& mainWindow) :
    m_mainWindow(mainWindow)
{
}

WebAPIAdapterGUI::~WebAPIAdapterGUI()
{
}

int WebAPIAdapterGUI::instanceSummary(
            Swagger::SWGInstanceSummaryResponse& response,
            Swagger::SWGErrorResponse& error __attribute__((unused)))
{

    *response.getVersion() = qApp->applicationVersion();

    Swagger::SWGLoggingInfo *logging = response.getLogging();
    logging->init();
    logging->setDumpToFile(m_mainWindow.m_logger->getUseFileLogger());
    if (logging->getDumpToFile()) {
        m_mainWindow.m_logger->getLogFileName(*logging->getFileName());
        m_mainWindow.m_logger->getFileMinMessageLevelStr(*logging->getFileLevel());
    }
    m_mainWindow.m_logger->getConsoleMinMessageLevelStr(*logging->getConsoleLevel());

    Swagger::SWGDeviceSetList *deviceSetList = response.getDevicesetlist();
    deviceSetList->init();
    deviceSetList->setDevicesetcount((int) m_mainWindow.m_deviceUIs.size());

    std::vector<DeviceUISet*>::const_iterator it = m_mainWindow.m_deviceUIs.begin();

    for (int i = 0; it != m_mainWindow.m_deviceUIs.end(); ++it, i++)
    {
        QList<Swagger::SWGDeviceSet*> *deviceSet = deviceSetList->getDeviceSets();
        deviceSet->append(new Swagger::SWGDeviceSet());
        Swagger::SWGSamplingDevice *samplingDevice = deviceSet->back()->getSamplingDevice();
        samplingDevice->init();
        samplingDevice->setIndex(i);
        samplingDevice->setTx((*it)->m_deviceSinkEngine != 0);

        if ((*it)->m_deviceSinkEngine) // Tx data
        {
            *samplingDevice->getHwType() = (*it)->m_deviceSinkAPI->getHardwareId();
            *samplingDevice->getSerial() = (*it)->m_deviceSinkAPI->getSampleSinkSerial();
            samplingDevice->setSequence((*it)->m_deviceSinkAPI->getSampleSinkSequence());
            samplingDevice->setNbStreams((*it)->m_deviceSinkAPI->getNbItems());
            samplingDevice->setStreamIndex((*it)->m_deviceSinkAPI->getItemIndex());
            (*it)->m_deviceSinkAPI->getDeviceEngineStateStr(*samplingDevice->getState());
            DeviceSampleSink *sampleSink = (*it)->m_deviceSinkEngine->getSink();

            if (sampleSink) {
                samplingDevice->setCenterFrequency(sampleSink->getCenterFrequency());
                samplingDevice->setBandwidth(sampleSink->getSampleRate());
            }

            deviceSet->back()->setChannelcount((*it)->m_deviceSinkAPI->getNbChannels());
            QList<Swagger::SWGChannel*> *channels = deviceSet->back()->getChannels();

            for (int i = 0; i <  deviceSet->back()->getChannelcount(); i++)
            {
                channels->append(new Swagger::SWGChannel);
                ChannelSourceAPI *channel = (*it)->m_deviceSinkAPI->getChanelAPIAt(i);
                channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
                channels->back()->setIndex(channel->getIndexInDeviceSet());
                channels->back()->setUid(channel->getUID());
                channel->getIdentifier(*channels->back()->getId());
                channel->getTitle(*channels->back()->getTitle());
            }
        }

        if ((*it)->m_deviceSourceEngine) // Rx data
        {
            *samplingDevice->getHwType() = (*it)->m_deviceSourceAPI->getHardwareId();
            *samplingDevice->getSerial() = (*it)->m_deviceSourceAPI->getSampleSourceSerial();
            samplingDevice->setSequence((*it)->m_deviceSourceAPI->getSampleSourceSequence());
            samplingDevice->setNbStreams((*it)->m_deviceSourceAPI->getNbItems());
            samplingDevice->setStreamIndex((*it)->m_deviceSourceAPI->getItemIndex());
            (*it)->m_deviceSourceAPI->getDeviceEngineStateStr(*samplingDevice->getState());
            DeviceSampleSource *sampleSource = (*it)->m_deviceSourceEngine->getSource();

            if (sampleSource) {
                samplingDevice->setCenterFrequency(sampleSource->getCenterFrequency());
                samplingDevice->setBandwidth(sampleSource->getSampleRate());
            }

            deviceSet->back()->setChannelcount((*it)->m_deviceSourceAPI->getNbChannels());
            QList<Swagger::SWGChannel*> *channels = deviceSet->back()->getChannels();

            for (int i = 0; i <  deviceSet->back()->getChannelcount(); i++)
            {
                channels->append(new Swagger::SWGChannel);
                ChannelSinkAPI *channel = (*it)->m_deviceSourceAPI->getChanelAPIAt(i);
                channels->back()->setDeltaFrequency(channel->getDeltaFrequency());
                channels->back()->setIndex(channel->getIndexInDeviceSet());
                channels->back()->setUid(channel->getUID());
                channel->getIdentifier(*channels->back()->getId());
                channel->getTitle(*channels->back()->getTitle());
            }
        }
    }

    return 200;
}


