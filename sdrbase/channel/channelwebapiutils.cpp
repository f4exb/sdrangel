///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2020 Jon Beniston, M7RCE                                        //
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

#include "channelwebapiutils.h"

#include "SWGDeviceState.h"
#include "SWGSuccessResponse.h"
#include "SWGErrorResponse.h"
#include "SWGDeviceSettings.h"
#include "SWGChannelSettings.h"
#include "SWGDeviceSet.h"
#include "SWGChannelActions.h"
#include "SWGFileSinkActions.h"

#include "maincore.h"
#include "device/deviceset.h"
#include "device/deviceapi.h"
#include "channel/channelutils.h"
#include "dsp/devicesamplesource.h"
#include "dsp/devicesamplesink.h"
#include "dsp/devicesamplemimo.h"
#include "webapi/webapiadapterinterface.h"
#include "webapi/webapiutils.h"

// Get device center frequency
bool ChannelWebAPIUtils::getCenterFrequency(unsigned int deviceIndex, double &frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::getCenterFrequency - not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::getCenterFrequency - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::getCenterFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
    return WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
}

// Set device center frequency
bool ChannelWebAPIUtils::setCenterFrequency(unsigned int deviceIndex, double frequencyInHz)
{
    SWGSDRangel::SWGDeviceSettings deviceSettingsResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    // Get current device settings
    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(0);
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(1);
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            deviceSettingsResponse.setDeviceHwType(new QString(deviceSet->m_deviceAPI->getHardwareId()));
            deviceSettingsResponse.setDirection(2);
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiSettingsGet(deviceSettingsResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::setCenterFrequency: not a sample source device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::setCenterFrequency: no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::setCenterFrequency: get device frequency error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    // Patch centerFrequency
    QJsonObject *jsonObj = deviceSettingsResponse.asJsonObject();
    double freq;
    if (WebAPIUtils::getSubObjectDouble(*jsonObj, "centerFrequency", freq))
    {
        WebAPIUtils::setSubObjectDouble(*jsonObj, "centerFrequency", frequencyInHz);
        QStringList deviceSettingsKeys;
        deviceSettingsKeys.append("centerFrequency");
        deviceSettingsResponse.init();
        deviceSettingsResponse.fromJsonObject(*jsonObj);
        SWGSDRangel::SWGErrorResponse errorResponse2;

        DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();

        httpRC = source->webapiSettingsPutPatch(false, deviceSettingsKeys, deviceSettingsResponse, *errorResponse2.getMessage());

        if (httpRC/100 == 2)
        {
            qDebug("ChannelWebAPIUtils::setCenterFrequency: set device frequency %f OK", frequencyInHz);
        }
        else
        {
            qWarning("ChannelWebAPIUtils::setCenterFrequency: set device frequency error %d: %s",
                httpRC, qPrintable(*errorResponse2.getMessage()));
            return false;
        }
    }
    else
    {
        qWarning("ChannelWebAPIUtils::setCenterFrequency: no centerFrequency key in device settings");
        return false;
    }

    return true;
}

// Start acquisition
bool ChannelWebAPIUtils::run(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(1, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(1, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::run - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::run - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::run: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Stop acquisition
bool ChannelWebAPIUtils::stop(unsigned int deviceIndex, int subsystemIndex)
{
    SWGSDRangel::SWGDeviceState runResponse;
    QString errorResponse;
    int httpRC;
    DeviceSet *deviceSet;

    std::vector<DeviceSet*> deviceSets = MainCore::instance()->getDeviceSets();
    if (deviceIndex < deviceSets.size())
    {
        runResponse.setState(new QString());
        deviceSet = deviceSets[deviceIndex];
        if (deviceSet->m_deviceSourceEngine)
        {
            DeviceSampleSource *source = deviceSet->m_deviceAPI->getSampleSource();
            httpRC = source->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceSinkEngine)
        {
            DeviceSampleSink *sink = deviceSet->m_deviceAPI->getSampleSink();
            httpRC = sink->webapiRun(0, runResponse, errorResponse);
        }
        else if (deviceSet->m_deviceMIMOEngine)
        {
            DeviceSampleMIMO *mimo = deviceSet->m_deviceAPI->getSampleMIMO();
            httpRC = mimo->webapiRun(0, subsystemIndex, runResponse, errorResponse);
        }
        else
        {
            qDebug() << "ChannelWebAPIUtils::stop - unknown device " << deviceIndex;
            return false;
        }
    }
    else
    {
        qDebug() << "ChannelWebAPIUtils::stop - no device " << deviceIndex;
        return false;
    }

    if (httpRC/100 != 2)
    {
        qWarning("ChannelWebAPIUtils::stop: run error %d: %s",
            httpRC, qPrintable(errorResponse));
        return false;
    }

    return true;
}

// Get input frequency offset for a channel
bool ChannelWebAPIUtils::getFrequencyOffset(unsigned int deviceIndex, int channelIndex, int& offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;
    double offsetD;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::getFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();
        if (WebAPIUtils::getSubObjectDouble(*jsonObj, "inputFrequencyOffset", offsetD))
        {
            offset = (int)offsetD;
            return true;
        }
    }
    return false;
}

// Set input frequency offset for a channel
bool ChannelWebAPIUtils::setFrequencyOffset(unsigned int deviceIndex, int channelIndex, int offset)
{
    SWGSDRangel::SWGChannelSettings channelSettingsResponse;
    QString errorResponse;
    int httpRC;
    QJsonObject *jsonObj;

    ChannelAPI *channel = MainCore::instance()->getChannel(deviceIndex, channelIndex);
    if (channel != nullptr)
    {
        httpRC = channel->webapiSettingsGet(channelSettingsResponse, errorResponse);
        if (httpRC/100 != 2)
        {
            qWarning("ChannelWebAPIUtils::setFrequencyOffset: get channel settings error %d: %s",
                httpRC, qPrintable(errorResponse));
            return false;
        }

        jsonObj = channelSettingsResponse.asJsonObject();

        if (WebAPIUtils::setSubObjectDouble(*jsonObj, "inputFrequencyOffset", (double)offset))
        {
            QStringList keys;
            keys.append("inputFrequencyOffset");
            channelSettingsResponse.init();
            channelSettingsResponse.fromJsonObject(*jsonObj);
            httpRC = channel->webapiSettingsPutPatch(false, keys, channelSettingsResponse, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::setFrequencyOffset: patch channel settings error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }

            return true;
        }
    }
    return false;
}

// Start or stop all file sinks in a given device set
bool ChannelWebAPIUtils::startStopFileSinks(unsigned int deviceIndex, bool start)
{
    MainCore *mainCore = MainCore::instance();
    ChannelAPI *channel;
    int channelIndex = 0;
    while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
    {
        if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.filesink"))
        {
            QStringList channelActionKeys = {"record"};
            SWGSDRangel::SWGChannelActions channelActions;
            SWGSDRangel::SWGFileSinkActions *fileSinkAction = new SWGSDRangel::SWGFileSinkActions();
            QString errorResponse;
            int httpRC;

            fileSinkAction->setRecord(start);
            channelActions.setFileSinkActions(fileSinkAction);
            httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
            if (httpRC/100 != 2)
            {
                qWarning("ChannelWebAPIUtils::startStopFileSinks: webapiActionsPost error %d: %s",
                    httpRC, qPrintable(errorResponse));
                return false;
            }
        }
        channelIndex++;
    }
    return true;
}

// Send AOS actions to all channels that support it
bool ChannelWebAPIUtils::satelliteAOS(const QString name, bool northToSouthPass)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"aos"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_aos *aosAction = new SWGSDRangel::SWGAPTDemodActions_aos();
                QString errorResponse;
                int httpRC;

                aosAction->setSatelliteName(new QString(name));
                aosAction->setNorthToSouthPass(northToSouthPass);
                aptDemodAction->setAos(aosAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteAOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}

// Send LOS actions to all channels that support it
bool ChannelWebAPIUtils::satelliteLOS(const QString name)
{
    MainCore *mainCore = MainCore::instance();
    std::vector<DeviceSet*> deviceSets = mainCore->getDeviceSets();
    for (unsigned int deviceIndex = 0; deviceIndex < deviceSets.size(); deviceIndex++)
    {
        ChannelAPI *channel;
        int channelIndex = 0;
        while(nullptr != (channel = mainCore->getChannel(deviceIndex, channelIndex)))
        {
            if (ChannelUtils::compareChannelURIs(channel->getURI(), "sdrangel.channel.aptdemod"))
            {
                QStringList channelActionKeys = {"los"};
                SWGSDRangel::SWGChannelActions channelActions;
                SWGSDRangel::SWGAPTDemodActions *aptDemodAction = new SWGSDRangel::SWGAPTDemodActions();
                SWGSDRangel::SWGAPTDemodActions_los *losAction = new SWGSDRangel::SWGAPTDemodActions_los();
                QString errorResponse;
                int httpRC;

                losAction->setSatelliteName(new QString(name));
                aptDemodAction->setLos(losAction);

                channelActions.setAptDemodActions(aptDemodAction);
                httpRC = channel->webapiActionsPost(channelActionKeys, channelActions, errorResponse);
                if (httpRC/100 != 2)
                {
                    qWarning("ChannelWebAPIUtils::satelliteLOS: webapiActionsPost error %d: %s",
                        httpRC, qPrintable(errorResponse));
                    return false;
                }
            }
            channelIndex++;
        }
    }
    return true;
}
